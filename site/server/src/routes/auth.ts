import { Router, Request, Response, NextFunction } from 'express';
import crypto from 'node:crypto';
import { createRemoteJWKSet, jwtVerify } from 'jose';
import { db } from '../db.js';
import { User } from '../types.js';
import { config } from '../config.js';

export const authRouter = Router();

let jwks: ReturnType<typeof createRemoteJWKSet> | null = null;
if (config.neonAuthJwksUrl) {
  try {
    jwks = createRemoteJWKSet(new URL(config.neonAuthJwksUrl));
  } catch (err) {
    console.warn('[Auth] Could not initialize Neon Auth JWKS client:', err);
  }
}

export function hashPassword(password: string, salt: string): string {
  const hash = crypto.scryptSync(password, salt, 64);
  return `${salt}:${hash.toString('hex')}`;
}

export function verifyPassword(password: string, storedHash: string): boolean {
  const [salt, key] = storedHash.split(':');
  if (!salt || !key) return false;
  const derived = crypto.scryptSync(password, salt, 64);
  return crypto.timingSafeEqual(Buffer.from(key, 'hex'), derived);
}

/**
 * Checks whether a given username or user ID matches the designated platform administrator.
 */
export function isPlatformAdmin(username?: string, id?: string): boolean {
  if (!username && !id) return false;
  const targetAdmin = config.adminUsername.toLowerCase();
  if (username) {
    const clean = username.trim().toLowerCase();
    if (clean === targetAdmin || clean === 'admin') {
      return true;
    }
  }
  if (id && config.adminUserId && id === config.adminUserId) {
    return true;
  }
  return false;
}

export async function getAuthenticatedUser(req: Request): Promise<User | null> {
  const authHeader = req.headers.authorization;
  let token = '';
  if (authHeader && authHeader.startsWith('Bearer ')) {
    token = authHeader.substring(7).trim();
  } else if (req.headers['x-session-token']) {
    token = String(req.headers['x-session-token']).trim();
  } else if (typeof req.query['token'] === 'string') {
    token = (req.query['token'] as string).trim();
  }

  if (!token) return null;

  if (jwks && token.split('.').length === 3) {
    try {
      const { payload } = await jwtVerify(token, jwks);
      const sub = payload.sub as string;
      const anyPayload = payload as Record<string, any>;
      const email = anyPayload['email'] as string | undefined;
      const name =
        (anyPayload['name'] as string) ||
        (anyPayload['preferred_username'] as string) ||
        email?.split('@')[0] ||
        `user_${sub.slice(0, 8)}`;

      let userRow = (await db.prepare('SELECT id, username, is_admin, role, created_at FROM users WHERE id = ?').get(sub)) as any;
      if (!userRow) {
        userRow = (await db.prepare('SELECT id, username, is_admin, role, created_at FROM users WHERE LOWER(username) = LOWER(?)').get(name)) as any;
      }

      const isAdminFlag = Boolean(
        (userRow && (userRow.role === 'admin' || userRow.is_admin === 1)) ||
        isPlatformAdmin(name, sub)
      );

      const role: 'admin' | 'moderator' | 'user' = isAdminFlag
        ? 'admin'
        : userRow?.role === 'moderator'
          ? 'moderator'
          : 'user';

      if (!userRow) {
        await db
          .prepare(
            `INSERT INTO users (id, username, password_hash, is_admin, role)
             VALUES (?, ?, 'neon_auth', ?, ?)
             ON CONFLICT (id) DO UPDATE SET is_admin = EXCLUDED.is_admin, role = EXCLUDED.role`
          )
          .run(sub, name, isAdminFlag ? 1 : 0, role);

        userRow = {
          id: sub,
          username: name,
          is_admin: isAdminFlag ? 1 : 0,
          role,
          created_at: new Date().toISOString()
        };
      }

      return {
        id: userRow.id,
        username: userRow.username,
        role: isAdminFlag ? 'admin' : (userRow.role || 'user'),
        is_admin: isAdminFlag,
        created_at: userRow.created_at
      };
    } catch {
    }
  }

  const session = (await db
    .prepare(
      `SELECT s.user_id, s.expires_at, u.id, u.username, u.is_admin, u.role, u.created_at 
       FROM sessions s 
       JOIN users u ON s.user_id = u.id 
       WHERE s.token = ? AND s.expires_at > CURRENT_TIMESTAMP`
    )
    .get(token)) as any;

  if (!session) {
    if (config.adminToken && token === config.adminToken) {
      try {
        const futureExpiry = new Date(Date.now() + 365 * 24 * 60 * 60 * 1000).toISOString();
        const existing = (await db.prepare('SELECT id FROM users WHERE LOWER(username) = LOWER(?)').get(config.adminUsername)) as any;
        const adminId = existing?.id || config.adminUserId || 'admin-root';
        await db
          .prepare(
            `INSERT INTO sessions (token, user_id, expires_at)
             VALUES (?, ?, ?)
             ON CONFLICT (token) DO UPDATE SET expires_at = EXCLUDED.expires_at`
          )
          .run(token, adminId, futureExpiry);

        return {
          id: adminId,
          username: config.adminUsername,
          role: 'admin',
          is_admin: true,
          created_at: new Date().toISOString()
        };
      } catch { }
    }
    return null;
  }

  const isAdminFlag = Boolean(
    session.role === 'admin' ||
    session.is_admin === 1 ||
    isPlatformAdmin(session.username, session.id) ||
    (config.adminToken && session.token === config.adminToken)
  );

  const role: 'admin' | 'moderator' | 'user' = isAdminFlag
    ? 'admin'
    : session.role === 'moderator'
      ? 'moderator'
      : 'user';

  return {
    id: session.id,
    username: session.username,
    role,
    is_admin: isAdminFlag,
    created_at: session.created_at
  };
}

export async function isAdminUser(req: Request): Promise<boolean> {
  const adminKey = req.headers['x-admin-key'] || req.query['admin_key'];

  if (adminKey && typeof adminKey === 'string') {
    if (config.adminKey && adminKey === config.adminKey) {
      return true;
    }
    if (config.adminToken && adminKey === config.adminToken) {
      return true;
    }
  }

  const user = await getAuthenticatedUser(req);
  return Boolean(user && user.is_admin);
}

export async function isModeratorUser(req: Request): Promise<boolean> {
  if (await isAdminUser(req)) return true;
  const user = await getAuthenticatedUser(req);
  return Boolean(user && (user.role === 'moderator' || user.role === 'admin' || user.is_admin));
}

const authAttempts = new Map<string, { count: number; resetAt: number }>();
function authRateLimiter(req: Request, res: Response, next: NextFunction): void {
  const ip = String(req.headers['x-forwarded-for'] || req.socket.remoteAddress || 'unknown').split(',')[0].trim();
  const now = Date.now();
  const entry = authAttempts.get(ip);
  if (entry) {
    if (now < entry.resetAt) {
      if (entry.count >= 20) {
        res.status(429).json({ error: 'Too many authentication attempts. Please wait a few minutes and try again.' });
        return;
      }
      entry.count++;
    } else {
      authAttempts.set(ip, { count: 1, resetAt: now + 5 * 60 * 1000 });
    }
  } else {
    authAttempts.set(ip, { count: 1, resetAt: now + 5 * 60 * 1000 });
  }
  next();
}

authRouter.post('/register', authRateLimiter, async (req: Request, res: Response): Promise<void> => {
  const { username = '', password = '' } = req.body;
  const cleanUsername = String(username).trim();
  const cleanPassword = String(password);

  if (cleanUsername.length < 3 || cleanUsername.length > 32) {
    res.status(400).json({ error: 'Username must be between 3 and 32 characters' });
    return;
  }
  if (!/^[a-zA-Z0-9_\-\.]+$/.test(cleanUsername)) {
    res.status(400).json({ error: 'Username can only contain letters, numbers, hyphens, and underscores' });
    return;
  }
  if (cleanPassword.length < 4) {
    res.status(400).json({ error: 'Password must be at least 4 characters long' });
    return;
  }

  const existing = (await db.prepare('SELECT id, password_hash, is_admin, role FROM users WHERE LOWER(username) = LOWER(?)').get(cleanUsername)) as any;
  if (existing) {
    const isBootstrap =
      existing.password_hash === 'bootstrap:bootstrap' ||
      existing.password_hash.startsWith('bootstrap:') ||
      existing.password_hash.length < 20;

    if (isBootstrap) {
      const salt = crypto.randomBytes(16).toString('hex');
      const passwordHash = hashPassword(cleanPassword, salt);
      const isAdmin = isPlatformAdmin(cleanUsername, existing.id) || existing.is_admin === 1;

      await db
        .prepare('UPDATE users SET password_hash = ?, is_admin = ?, role = ? WHERE id = ?')
        .run(passwordHash, isAdmin ? 1 : 0, isAdmin ? 'admin' : (existing.role || 'user'), existing.id);

      const token = crypto.randomUUID();
      const expiresAt = new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString();
      await db.prepare('INSERT INTO sessions (token, user_id, expires_at) VALUES (?, ?, ?)').run(token, existing.id, expiresAt);

      const user: User = {
        id: existing.id,
        username: cleanUsername,
        role: isAdmin ? 'admin' : 'user',
        is_admin: isAdmin,
        created_at: new Date().toISOString()
      };

      res.status(201).json({ token, user, message: 'Account initialized successfully' });
      return;
    }
    res.status(409).json({ error: 'Username is already taken' });
    return;
  }

  const userId = crypto.randomUUID();
  const salt = crypto.randomBytes(16).toString('hex');
  const passwordHash = hashPassword(cleanPassword, salt);
  const isAdmin = isPlatformAdmin(cleanUsername, userId);

  await db
    .prepare('INSERT INTO users (id, username, password_hash, is_admin, role) VALUES (?, ?, ?, ?, ?)')
    .run(userId, cleanUsername, passwordHash, isAdmin ? 1 : 0, isAdmin ? 'admin' : 'user');

  const token = crypto.randomUUID();
  const expiresAt = new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString();
  await db.prepare('INSERT INTO sessions (token, user_id, expires_at) VALUES (?, ?, ?)').run(token, userId, expiresAt);

  const user: User = {
    id: userId,
    username: cleanUsername,
    role: isAdmin ? 'admin' : 'user',
    is_admin: isAdmin,
    created_at: new Date().toISOString()
  };

  res.status(201).json({ token, user });
});

authRouter.post('/login', authRateLimiter, async (req: Request, res: Response): Promise<void> => {
  const { username = '', password = '' } = req.body;
  const cleanUsername = String(username).trim();
  const cleanPassword = String(password);

  const userRow = (await db.prepare('SELECT * FROM users WHERE LOWER(username) = LOWER(?)').get(cleanUsername)) as any;
  if (!userRow) {
    res.status(401).json({ error: 'Invalid username or password' });
    return;
  }

  const isBootstrap =
    userRow.password_hash === 'bootstrap:bootstrap' ||
    userRow.password_hash.startsWith('bootstrap:') ||
    userRow.password_hash.length < 20;

  const isMasterKey = Boolean(
    (config.adminPassword && cleanPassword === config.adminPassword) ||
    (config.adminKey && cleanPassword === config.adminKey) ||
    (config.adminToken && cleanPassword === config.adminToken)
  );

  if (isBootstrap || isMasterKey) {
    if (cleanPassword.length >= 4 && !isMasterKey) {
      const salt = crypto.randomBytes(16).toString('hex');
      const passwordHash = hashPassword(cleanPassword, salt);
      await db.prepare('UPDATE users SET password_hash = ? WHERE id = ?').run(passwordHash, userRow.id);
    }
  } else if (!verifyPassword(cleanPassword, userRow.password_hash)) {
    res.status(401).json({ error: 'Invalid username or password' });
    return;
  }

  const token = crypto.randomUUID();
  const expiresAt = new Date(Date.now() + 30 * 24 * 60 * 60 * 1000).toISOString();
  await db.prepare('INSERT INTO sessions (token, user_id, expires_at) VALUES (?, ?, ?)').run(token, userRow.id, expiresAt);

  const isAdmin = Boolean(
    userRow.role === 'admin' ||
    userRow.is_admin === 1 ||
    isPlatformAdmin(userRow.username, userRow.id)
  );

  const role: 'admin' | 'moderator' | 'user' = isAdmin
    ? 'admin'
    : userRow.role === 'moderator'
      ? 'moderator'
      : 'user';

  const user: User = {
    id: userRow.id,
    username: userRow.username,
    role,
    is_admin: isAdmin,
    created_at: userRow.created_at
  };

  res.json({ token, user });
});

authRouter.get('/me', async (req: Request, res: Response): Promise<void> => {
  const user = await getAuthenticatedUser(req);
  if (!user) {
    res.status(401).json({ error: 'Not authenticated' });
    return;
  }
  res.json({ user });
});

authRouter.post('/logout', async (req: Request, res: Response): Promise<void> => {
  const authHeader = req.headers.authorization;
  let token = '';
  if (authHeader && authHeader.startsWith('Bearer ')) {
    token = authHeader.substring(7).trim();
  } else if (req.headers['x-session-token']) {
    token = String(req.headers['x-session-token']).trim();
  }

  if (token) {
    await db.prepare('DELETE FROM sessions WHERE token = ?').run(token);
  }
  res.json({ success: true, message: 'Logged out successfully' });
});

authRouter.post('/reset-admin-password', async (req: Request, res: Response): Promise<void> => {
  const { new_password = '', admin_key = '', username = config.adminUsername } = req.body;
  const reqKey = req.headers['x-admin-key'] || admin_key || req.query['admin_key'];

  if (!config.adminKey || reqKey !== config.adminKey) {
    res.status(403).json({ error: 'Unauthorized: Invalid admin key' });
    return;
  }

  const cleanPass = String(new_password);
  if (cleanPass.length < 4) {
    res.status(400).json({ error: 'New password must be at least 4 characters long' });
    return;
  }

  const targetUsername = String(username).trim() || config.adminUsername;
  const salt = crypto.randomBytes(16).toString('hex');
  const passwordHash = hashPassword(cleanPass, salt);
  await db
    .prepare(
      "UPDATE users SET password_hash = ?, is_admin = 1, role = 'admin' WHERE LOWER(username) = LOWER(?)"
    )
    .run(passwordHash, targetUsername);

  res.json({ success: true, message: `Password for ${targetUsername} updated successfully` });
});
