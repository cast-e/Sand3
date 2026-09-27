import { Router, Request, Response, NextFunction } from 'express';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { db } from '../db.js';
import { isAdminUser, isModeratorUser, getAuthenticatedUser } from './auth.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const UPLOADS_DIR = process.env['UPLOADS_DIR'] || (process.env['VERCEL'] ? '/tmp/sand3-uploads' : path.resolve(__dirname, '../../uploads'));
const THUMBNAILS_DIR = path.resolve(UPLOADS_DIR, 'thumbnails');

export const adminRouter = Router();

export async function requireAdmin(req: Request, res: Response, next: NextFunction): Promise<void> {
  if (!(await isAdminUser(req))) {
    res.status(403).json({
      error: 'Admin access required. Please log in as an administrator.'
    });
    return;
  }
  next();
}

export async function requireModerator(req: Request, res: Response, next: NextFunction): Promise<void> {
  if (!(await isModeratorUser(req))) {
    res.status(403).json({
      error: 'Moderator or Admin access required. Please log in with a moderator or administrator account.'
    });
    return;
  }
  next();
}

adminRouter.get('/check', async (req: Request, res: Response): Promise<void> => {
  const is_admin = await isAdminUser(req);
  const is_moderator = await isModeratorUser(req);
  const user = await getAuthenticatedUser(req);
  res.json({
    is_admin,
    is_moderator,
    role: user?.role || (is_admin ? 'admin' : (is_moderator ? 'moderator' : 'user')),
    user: user ? { id: user.id, username: user.username, role: user.role } : null
  });
});

adminRouter.get('/stats', requireModerator, async (_req: Request, res: Response): Promise<void> => {
  try {
    const totalItemsRow = await db.prepare('SELECT COUNT(*) as c FROM items').get();
    const totalReportsRow = await db.prepare('SELECT COUNT(*) as c FROM reports').get();
    const totalUsersRow = await db.prepare('SELECT COUNT(*) as c FROM users').get();
    const hiddenItemsRow = await db.prepare('SELECT COUNT(*) as c FROM items WHERE is_hidden = 1').get();
    const reportedItemsRow = await db.prepare('SELECT COUNT(DISTINCT item_id) as c FROM reports').get();
    const totalDownloadsRow = await db.prepare('SELECT COALESCE(SUM(downloads_count), 0) as s FROM items').get();

    res.json({
      total_items: Number(totalItemsRow?.c || 0),
      total_reports: Number(totalReportsRow?.c || 0),
      total_users: Number(totalUsersRow?.c || 0),
      hidden_items: Number(hiddenItemsRow?.c || 0),
      reported_items: Number(reportedItemsRow?.c || 0),
      total_downloads: Number(totalDownloadsRow?.s || 0)
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.get('/reports', requireModerator, async (_req: Request, res: Response): Promise<void> => {
  try {
    const query = `
      SELECT r.id as report_id, r.item_id, r.client_uuid, r.reason, r.details, r.created_at as reported_at,
             i.title as item_title, i.type as item_type, i.author as item_author, i.reports_count,
             i.is_hidden as item_is_hidden, i.version as item_version, i.thumbnail_path as item_thumbnail_path,
             i.file_size as item_file_size, i.created_at as item_created_at
      FROM reports r
      LEFT JOIN items i ON r.item_id = i.id
      ORDER BY r.created_at DESC
    `;
    const rows = await db.prepare(query).all();
    res.json({ reports: rows });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.post('/reports/:id/dismiss', requireModerator, async (req: Request, res: Response): Promise<void> => {
  try {
    const reportId = Number(req.params['id']);
    const report = (await db.prepare('SELECT item_id FROM reports WHERE id = ?').get(reportId)) as any;
    if (!report) {
      res.status(404).json({ error: 'Report not found' });
      return;
    }

    await db.prepare('DELETE FROM reports WHERE id = ?').run(reportId);

    const countRow = (await db.prepare('SELECT COUNT(*) as c FROM reports WHERE item_id = ?').get(report.item_id)) as any;
    const remaining = countRow ? Number(countRow.c) : 0;
    await db.prepare('UPDATE items SET reports_count = ? WHERE id = ?').run(remaining, report.item_id);

    res.json({ success: true, message: 'Report dismissed', remaining_reports: remaining });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.post('/items/:id/clear-reports', requireModerator, async (req: Request, res: Response): Promise<void> => {
  try {
    const itemId = String(req.params['id']);
    await db.prepare('DELETE FROM reports WHERE item_id = ?').run(itemId);
    await db.prepare('UPDATE items SET reports_count = 0, is_hidden = 0 WHERE id = ?').run(itemId);
    res.json({ success: true, message: 'All reports cleared and item unhidden.' });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.post('/items/:id/toggle-hide', requireModerator, async (req: Request, res: Response): Promise<void> => {
  try {
    const id = String(req.params['id']);
    const item = (await db.prepare('SELECT id, is_hidden, title FROM items WHERE id = ?').get(id)) as any;
    if (!item) {
      res.status(404).json({ error: 'Item not found' });
      return;
    }

    const newHidden = item.is_hidden ? 0 : 1;
    await db.prepare('UPDATE items SET is_hidden = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?').run(newHidden, id);

    res.json({
      success: true,
      id,
      is_hidden: newHidden,
      message: newHidden ? `Item '${item.title}' is now hidden from public view.` : `Item '${item.title}' is now visible.`
    });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.delete('/items/:id', requireModerator, async (req: Request, res: Response): Promise<void> => {
  try {
    const id = String(req.params['id']);
    const item = (await db.prepare('SELECT * FROM items WHERE id = ?').get(id)) as any;
    if (!item) {
      res.status(404).json({ error: 'Item not found' });
      return;
    }

    try {
      if (item.file_path) {
        const mainFile = path.resolve(UPLOADS_DIR, item.file_path);
        if (fs.existsSync(mainFile)) fs.unlinkSync(mainFile);
      }
      if (item.thumbnail_path) {
        const thumbFile = path.resolve(THUMBNAILS_DIR, item.thumbnail_path);
        if (fs.existsSync(thumbFile)) fs.unlinkSync(thumbFile);
      }
    } catch (e) {
      console.warn('Moderator/Admin delete file cleanup error:', e);
    }

    await db.prepare('DELETE FROM items WHERE id = ?').run(id);

    res.json({ success: true, message: `Item '${item.title}' was deleted permanently by moderation.`, id });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

adminRouter.get('/items', requireModerator, async (req: Request, res: Response): Promise<void> => {
  try {
    const q = (req.query['q'] as string || '').trim().toLowerCase();
    const type = req.query['type'] as string;
    const filter = (req.query['filter'] as string || 'all').toLowerCase();

    let conditions: string[] = [];
    let params: any[] = [];

    if (q) {
      conditions.push('(LOWER(id) = ? OR LOWER(id) LIKE ? OR LOWER(title) LIKE ? OR LOWER(author) LIKE ? OR LOWER(description) LIKE ?)');
      params.push(q, `%${q}%`, `%${q}%`, `%${q}%`, `%${q}%`);
    }

    if (type && type !== 'all') {
      conditions.push('type = ?');
      params.push(type);
    }

    if (filter === 'reported') {
      conditions.push('reports_count > 0');
    } else if (filter === 'hidden') {
      conditions.push('is_hidden = 1');
    }

    const whereClause = conditions.length > 0 ? `WHERE ${conditions.join(' AND ')}` : '';
    const query = `
      SELECT id, user_id, type, title, description, author, parent_set_id, version, set_hash,
             file_path, file_size, thumbnail_path, likes_count, favorites_count, downloads_count,
             reports_count, is_hidden, created_at, updated_at
      FROM items
      ${whereClause}
      ORDER BY reports_count DESC, created_at DESC
      LIMIT 100
    `;

    const items = await db.prepare(query).all(...params);
    res.json({ items });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

import { usersRouter } from './users.js';

adminRouter.use('/users', usersRouter);
