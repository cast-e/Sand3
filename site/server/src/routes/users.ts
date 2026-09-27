import { Router, Request, Response, NextFunction } from 'express';
import { db } from '../db.js';
import { isAdminUser, isModeratorUser, getAuthenticatedUser, isPlatformAdmin } from './auth.js';

export const usersRouter = Router();

async function requireAdmin(req: Request, res: Response, next: NextFunction) {
  if (!(await isAdminUser(req))) {
    return res.status(403).json({
      error: 'Administrator access required. Moderators cannot manage or delete users.'
    });
  }
  next();
}

async function requireModerator(req: Request, res: Response, next: NextFunction) {
  if (!(await isModeratorUser(req))) {
    return res.status(403).json({
      error: 'Moderator or Administrator access required.'
    });
  }
  next();
}

usersRouter.get('/', requireModerator, async (req: Request, res: Response) => {
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
             (SELECT COUNT(*) FROM sessions WHERE user_id = u.id AND expires_at > CURRENT_TIMESTAMP) as active_sessions_count
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

    const users = await db.prepare(query).all(...params);
    res.json({ users });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

usersRouter.get('/:id', async (req: Request, res: Response) => {
  try {
    const target = String(req.params.id);
    const user = (await db
      .prepare(
        `SELECT id, username,
                COALESCE(role, CASE WHEN is_admin = 1 THEN 'admin' ELSE 'user' END) as role,
                is_admin, created_at,
                (SELECT COUNT(*) FROM items WHERE user_id = users.id) as items_count
         FROM users
         WHERE id = ? OR LOWER(username) = LOWER(?)`
      )
      .get(target, target)) as any;

    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    res.json({ user });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

usersRouter.post('/:id/role', requireAdmin, async (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const { role = 'user' } = req.body;
    const caller = await getAuthenticatedUser(req);

    if (!['admin', 'moderator', 'user'].includes(role)) {
      return res.status(400).json({ error: "Invalid role. Must be 'admin', 'moderator', or 'user'." });
    }

    const user = (await db.prepare('SELECT id, username, is_admin, role FROM users WHERE id = ?').get(targetId)) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    if (isPlatformAdmin(user.username, targetId)) {
      if (role !== 'admin') {
        return res.status(400).json({ error: 'The primary platform administrator cannot be demoted.' });
      }
    }

    if (caller && caller.id === targetId && role !== 'admin') {
      return res.status(400).json({ error: 'For security, you cannot revoke your own administrator privileges.' });
    }

    const newIsAdmin = role === 'admin' ? 1 : 0;
    await db.prepare('UPDATE users SET role = ?, is_admin = ? WHERE id = ?').run(role, newIsAdmin, targetId);

    const roleName = role === 'admin' ? 'Administrator' : role === 'moderator' ? 'Moderator' : 'User';
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

usersRouter.post('/:id/toggle-admin', requireAdmin, async (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const caller = await getAuthenticatedUser(req);

    const user = (await db.prepare('SELECT id, username, is_admin, role FROM users WHERE id = ?').get(targetId)) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    if (caller && caller.id === targetId && user.is_admin) {
      return res.status(400).json({ error: 'For security, you cannot revoke your own administrator privileges.' });
    }

    const newRole = user.is_admin ? 'user' : 'admin';
    const newAdmin = newRole === 'admin' ? 1 : 0;
    await db.prepare('UPDATE users SET role = ?, is_admin = ? WHERE id = ?').run(newRole, newAdmin, targetId);

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

usersRouter.delete('/:id', async (req: Request, res: Response) => {
  try {
    const targetId = String(req.params.id);
    const caller = await getAuthenticatedUser(req);
    const isAdmin = await isAdminUser(req);

    if (!caller && !isAdmin) {
      return res.status(401).json({ error: 'Authentication required to delete a user account.' });
    }

    const isSelf = caller && caller.id === targetId;
    if (!isAdmin && !isSelf) {
      return res.status(403).json({ error: 'Moderators cannot remove users. Only Administrators have permission to delete accounts.' });
    }

    const user = (await db.prepare('SELECT id, username, is_admin FROM users WHERE id = ?').get(targetId)) as any;
    if (!user) {
      return res.status(404).json({ error: 'User not found' });
    }

    if (isPlatformAdmin(user.username, targetId)) {
      return res.status(400).json({ error: 'The primary platform administrator account cannot be deleted.' });
    }

    if (caller && caller.id === targetId && user.is_admin) {
      return res.status(400).json({ error: 'For security, you cannot delete your own active administrator account.' });
    }

    await db.prepare('DELETE FROM sessions WHERE user_id = ?').run(targetId);

    await db.prepare('UPDATE items SET user_id = NULL WHERE user_id = ?').run(targetId);

    await db.prepare('DELETE FROM users WHERE id = ?').run(targetId);

    res.json({
      success: true,
      message: `User '${user.username}' was permanently deleted from the database.`,
      id: targetId
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});
