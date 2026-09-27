import { Router } from 'express';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { db } from '../db.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const UPLOADS_DIR = process.env['UPLOADS_DIR'] || (process.env['VERCEL'] ? '/tmp/sand3-uploads' : path.resolve(__dirname, '../../uploads'));

export const downloadsRouter = Router();

downloadsRouter.get('/:id/download', async (req, res) => {
  const id = String(req.params['id']);

  const item = (await db.prepare('SELECT * FROM items WHERE id = ? AND is_hidden = 0').get(id)) as any;
  if (!item) {
    res.status(404).json({ error: 'Item not found' });
    return;
  }

  const resolvedPath = path.resolve(UPLOADS_DIR, item.file_path);
  if (!resolvedPath.startsWith(UPLOADS_DIR) || !fs.existsSync(resolvedPath)) {
    res.status(404).json({ error: 'Physical file not found on server' });
    return;
  }

  await db.prepare('UPDATE items SET downloads_count = downloads_count + 1 WHERE id = ?').run(id);

  const ext = path.extname(item.file_path) || (item.type === 'save' ? '.save' : item.type === 'stamp' ? '.stamp' : '.zip');
  const safeTitle = item.title.replace(/[^a-zA-Z0-9_\-\.]/g, '_');
  const downloadName = `${safeTitle}${ext}`;

  res.setHeader('Content-Disposition', `attachment; filename="${downloadName}"`);
  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('X-Item-Id', item.id);
  res.setHeader('X-Item-Type', item.type);
  res.setHeader('X-Parent-Set-Id', item.parent_set_id || '');

  const stream = fs.createReadStream(resolvedPath);
  stream.pipe(res);
});
