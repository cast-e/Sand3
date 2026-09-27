import { Router, Request, Response, NextFunction } from 'express';
import { db } from '../db.js';
import { isAdminUser, isModeratorUser, getAuthenticatedUser } from './auth.js';

export const usersRouter = Router();

// Middleware to ensure administrator access (users management, deleting users, role assignment)
function requireAdmin(req: Request, res: Response, next: NextFunction) {
  if (!isAdminUser(req)) {
    return res.status(403).json({
      error: 'Administrator access required. Moderators cannot manage or delete users.'
    });
  }
  next();
}

// Middleware to ensure moderator or admin access (reading users)
function requireModerator(req: Request, res: Response, next: NextFunction) {
  if (!isModeratorUser(req)) {
    return res.status(403).json({
      error: 'Moderator or Administrator access required.'
    });
  }
  next();
}

// 1. List all users (accessible by Admin & Moderator)
usersRouter.get('/', requireModerator, (req: Request, res: Response) => {
  try {
    const q = ((req.query.q as string) || '').trim().toLowerCase();
    let whereClause = '';
    let params: any[] = [];
    if (q) {
      whereClause = 'WHERE LOWER(u.username) LIKE ? OR LOWER(u.id) LIKE ?';
      params.push(`%${q}%`, `%${q}%`);
    }

    const query = `
      SELECT u.id, u.username,
             COALESCE(u.role, CASE WHEN u.is_admin = 1 THEN 'admin' ELSE 'user' END) as role,
             u.is_admin, u.created_at,
             (SELECT COUNT(*) FROM items WHERE user_id = u.id) as items_count,
             (SELECT COUNT(*) FROM sessions WHERE user_id = u.id AND datetime(expires_at) > datetime('now')) as active_sessions_count
      FROM users u
      ${whereClause}
      ORDER BY 
        CASE COALESCE(u.role, CASE WHEN u.is_admin = 1 THEN 'admin' ELSE 'user' END)
          WHEN 'admin' THEN 1
          WHEN 'moderator' THEN 2
          ELSE 3
        END,
        u.created_at DESC
      LIMIT 100
    `;

    const users = db.prepare(query).all(...params);
    res.json({ users });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 2. Get single user details by ID or username
usersRouter.get('/:id', (req: Request, res: Response) => {
  try {
    const target = String(req.params.id);
    const user = db
      .prepare(
        `SELECT id, username,
                COALESCE(role, CASE WHEN is_admin = 1 THEN 'admin' ELSE 'user' END) as role,
                is_admin, created_at,
                (SELECT COUNT(*) FROM items WHERE user_id = users.id) as items_count
         FROM users
         WHERE id = ? OR LOWER(username) = LOWER(?)`
      )
      .get(target, target) as any;

    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    res.json({ user });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 3. Set User Role ('admin' | 'moderator' | 'user') - strictly Administrator only!
usersRouter.post('/:id/role', requireAdmin, (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const { role = 'user' } = req.body;
    const caller = getAuthenticatedUser(req);

    if (!['admin', 'moderator', 'user'].includes(role)) {
      return res.status(400).json({ error: "Invalid role. Must be 'admin', 'moderator', or 'user'." });
    }

    const user = db.prepare('SELECT id, username, is_admin, role FROM users WHERE id = ?').get(targetId) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    // Protection 1: Prevent primary administrator account from being demoted
    if (user.username.toLowerCase() === 'cast_e' || targetId === 'c7831b24-de86-45ae-b049-9ecdf88a3219') {
      if (role !== 'admin') {
        return res.status(400).json({ error: 'The primary platform administrator cannot be demoted.' });
      }
    }

    // Protection 2: Prevent caller from locking themselves out of admin
    if (caller && caller.id === targetId && role !== 'admin') {
      return res.status(400).json({ error: 'For security, you cannot revoke your own administrator privileges.' });
    }

    const newIsAdmin = role === 'admin' ? 1 : 0;
    db.prepare('UPDATE users SET role = ?, is_admin = ? WHERE id = ?').run(role, newIsAdmin, targetId);

    const roleName = role === 'admin' ? 'Administrator' : (role === 'moderator' ? 'Moderator' : 'User');
    res.json({
      success: true,
      user_id: targetId,
      username: user.username,
      role,
      is_admin: Boolean(newIsAdmin),
      message: `User '${user.username}' is now a ${roleName}.`
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 4. Toggle admin privileges for a user (backward compatibility)
usersRouter.post('/:id/toggle-admin', requireAdmin, (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const caller = getAuthenticatedUser(req);

    const user = db.prepare('SELECT id, username, is_admin, role FROM users WHERE id = ?').get(targetId) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    // Protection: prevent caller from locking themselves out
    if (caller && caller.id === targetId && user.is_admin) {
      return res.status(400).json({ error: 'For security, you cannot revoke your own administrator privileges.' });
    }

    const newRole = user.is_admin ? 'user' : 'admin';
    const newAdmin = newRole === 'admin' ? 1 : 0;
    db.prepare('UPDATE users SET role = ?, is_admin = ? WHERE id = ?').run(newRole, newAdmin, targetId);

    res.json({
      success: true,
      user_id: targetId,
      username: user.username,
      role: newRole,
      is_admin: Boolean(newAdmin),
      message: newAdmin
        ? `User '${user.username}' is now an Administrator.`
        : `Administrator privileges revoked for '${user.username}'.`
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 5. Delete a user from the platform (requires Admin or Self - NOT Moderators!)
usersRouter.delete('/:id', (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const caller = getAuthenticatedUser(req);
    const isAdmin = isAdminUser(req);

    if (!caller && !isAdmin) {
      return res.status(401).json({ error: 'Authentication required to delete a user account.' });
    }

    // Moderators CANNOT remove users — strictly Admins or the user deleting their own account
    const isSelf = caller && caller.id === targetId;
    if (!isAdmin && !isSelf) {
      return res.status(403).json({ error: 'Moderators cannot remove users. Only Administrators have permission to delete accounts.' });
    }

    const user = db.prepare('SELECT id, username, is_admin FROM users WHERE id = ?').get(targetId) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    // Protection 1: Prevent primary administrator account from being deleted
    if (user.username.toLowerCase() === 'cast_e' || targetId === 'c7831b24-de86-45ae-b049-9ecdf88a3219') {
      return res.status(400).json({ error: 'The primary platform administrator account cannot be deleted.' });
    }

    // Protection 2: Prevent active admin from accidentally deleting themselves
    if (caller && caller.id === targetId && user.is_admin) {
      return res.status(400).json({ error: 'For security, you cannot delete your own active administrator account.' });
    }

    // Step A: Terminate all active sessions for this user
    db.prepare('DELETE FROM sessions WHERE user_id = ?').run(targetId);

    // Step B: Detach user ownership from published items so items aren\'t orphaned or broken
    db.prepare('UPDATE items SET user_id = NULL WHERE user_id = ?').run(targetId);

    // Step C: Delete the user record
    db.prepare('DELETE FROM users WHERE id = ?').run(targetId);

    res.json({
      success: true,
      message: `User '${user.username}' was permanently deleted from the database.`,
      id: targetId
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});
