import { Router, Request, Response } from 'express';
import multer from 'multer';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { db, ensureItemBlobsTable } from '../db.js';
import { WorkshopItem, ItemType } from '../types.js';
import { getAuthenticatedUser } from './auth.js';

function generateFallbackThumbnailSvg(title: string, type: string): string {
  const safeTitle = (title || 'Sand3').replace(/[<>&"']/g, '');
  const typeColor = type === 'set' ? '#58a6ff' : type === 'save' ? '#2ea043' : type === 'stamp' ? '#bc8cff' : '#d29922';
  const typeLabel = (type || 'ITEM').toUpperCase();
  const initial = safeTitle.trim().charAt(0).toUpperCase() || 'S';

  return `<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#161b22"/>
      <stop offset="100%" stop-color="#0d1117"/>
    </linearGradient>
  </defs>
  <rect width="128" height="128" fill="url(#bg)" rx="8"/>
  <rect x="6" y="6" width="116" height="116" fill="none" stroke="${typeColor}" stroke-width="1.5" stroke-opacity="0.3" rx="6"/>
  <circle cx="64" cy="54" r="26" fill="${typeColor}" fill-opacity="0.15"/>
  <text x="64" y="63" font-family="-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif" font-size="26" font-weight="bold" fill="${typeColor}" text-anchor="middle" dominant-baseline="central">${initial}</text>
  <rect x="24" y="94" width="80" height="18" fill="${typeColor}" fill-opacity="0.2" rx="4"/>
  <text x="64" y="104" font-family="-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif" font-size="10" font-weight="bold" fill="${typeColor}" text-anchor="middle" dominant-baseline="central">${typeLabel}</text>
</svg>`;
}

/**
 * Computes deterministic SHA-256 hash using strictly .mat material files.
 * Ignores set_config.ini, saves/ folder, stamps/ folder, and other metadata.
 * Strips '\r' to guarantee cross-platform CRLF/LF invariance.
 */
function computeZipSetHash(zipBuffer: Buffer): string {
  try {
    let offset = 0;
    const matFiles: { name: string; content: Buffer }[] = [];
    while (offset + 30 <= zipBuffer.length) {
      const sig = zipBuffer.readUInt32LE(offset);
      if (sig !== 0x04034b50) {
        break;
      }
      const method = zipBuffer.readUInt16LE(offset + 8);
      const compSize = zipBuffer.readUInt32LE(offset + 18);
      const fnameLen = zipBuffer.readUInt16LE(offset + 26);
      const extraLen = zipBuffer.readUInt16LE(offset + 28);
      const fname = zipBuffer.toString('utf8', offset + 30, offset + 30 + fnameLen);
      const dataStart = offset + 30 + fnameLen + extraLen;
      const rawData = zipBuffer.subarray(dataStart, dataStart + compSize);

      const baseName = fname.split('/').pop() || '';
      if (baseName.endsWith('.mat') && baseName !== 'set_config.ini' && !fname.includes('/saves/') && !fname.includes('/stamps/')) {
        let content: Buffer | null = null;
        if (method === 0) {
          content = rawData;
        } else if (method === 8) {
          content = zlib.inflateRawSync(rawData);
        }
        if (content) {
          matFiles.push({ name: baseName, content });
        }
      }
      offset = dataStart + compSize;
    }

    if (matFiles.length === 0) return '';
    matFiles.sort((a, b) => a.name.localeCompare(b.name));

    const chunks: Buffer[] = [];
    for (const f of matFiles) {
      chunks.push(Buffer.from(f.name + '\0', 'utf8'));
      const cleanBytes: number[] = [];
      for (let i = 0; i < f.content.length; ++i) {
        if (f.content[i] !== 0x0D) {
          cleanBytes.push(f.content[i]);
        }
      }
      chunks.push(Buffer.from(cleanBytes));
    }
    return crypto.createHash('sha256').update(Buffer.concat(chunks)).digest('hex');
  } catch (e) {
    console.error('Error computing zip set hash:', e);
    return '';
  }
}

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const UPLOADS_DIR = process.env['UPLOADS_DIR'] || (process.env['VERCEL'] ? '/tmp/sand3-uploads' : path.resolve(__dirname, '../../uploads'));
const THUMBNAILS_DIR = path.resolve(UPLOADS_DIR, 'thumbnails');
if (!fs.existsSync(UPLOADS_DIR)) {
  fs.mkdirSync(UPLOADS_DIR, { recursive: true });
}
if (!fs.existsSync(THUMBNAILS_DIR)) {
  fs.mkdirSync(THUMBNAILS_DIR, { recursive: true });
}

const storage = multer.diskStorage({
  destination: (_req, file, cb) => {
    if (file.fieldname === 'thumbnail') {
      cb(null, THUMBNAILS_DIR);
    } else {
      cb(null, UPLOADS_DIR);
    }
  },
  filename: (_req, file, cb) => {
    const uniqueSuffix = crypto.randomUUID();
    const ext = path.extname(file.originalname) || '.bin';
    cb(null, `${uniqueSuffix}${ext}`);
  }
});

const upload = multer({
  storage,
  limits: { fileSize: 25 * 1024 * 1024 }
});

export const itemsRouter = Router();

const uploadFields = upload.fields([
  { name: 'file', maxCount: 1 },
  { name: 'thumbnail', maxCount: 1 }
]);

itemsRouter.get('/', async (req: Request, res: Response) => {
  const {
    type = 'all',
    sort = 'popular',
    parent_set_id,
    author = '',
    user_id = '',
    q = '',
    client_uuid = '',
    page = '1',
    limit = '30'
  } = req.query as Record<string, string>;

  const pageNum = Math.max(1, parseInt(page, 10) || 1);
  const limitNum = Math.min(100, Math.max(1, parseInt(limit, 10) || 30));
  const offset = (pageNum - 1) * limitNum;

  const conditions: string[] = ['is_hidden = 0'];
  const params: any[] = [];

  if (type && type !== 'all') {
    conditions.push('type = ?');
    params.push(type);
  }

  if (parent_set_id) {
    conditions.push('parent_set_id = ?');
    params.push(parent_set_id);
  }

  if (author) {
    conditions.push('author = ?');
    params.push(author);
  }

  if (user_id) {
    conditions.push('user_id = ?');
    params.push(user_id);
  }

  if (q.trim()) {
    const trimmed = q.trim();
    const searchTerm = `%${trimmed}%`;
    conditions.push('(id = ? OR id LIKE ? OR title LIKE ? OR description LIKE ? OR author LIKE ?)');
    params.push(trimmed, searchTerm, searchTerm, searchTerm, searchTerm);
  }

  let orderBy = 'likes_count DESC, created_at DESC';
  if (sort === 'newest') {
    orderBy = 'created_at DESC';
  } else if (sort === 'liked') {
    orderBy = 'likes_count DESC';
  } else if (sort === 'favorites') {
    orderBy = 'favorites_count DESC';
  } else if (sort === 'downloads') {
    orderBy = 'downloads_count DESC';
  }

  if (q.trim()) {
    const safeQ = q.trim().replace(/'/g, "''");
    orderBy = `CASE WHEN id = '${safeQ}' THEN 0 ELSE 1 END, ${orderBy}`;
  }

  const whereClause = conditions.length > 0 ? `WHERE ${conditions.join(' AND ')}` : '';

  const countResult = (await db.prepare(`SELECT COUNT(*) as total FROM items ${whereClause}`).get(...params)) as any;
  const total = countResult ? Number(countResult.total) : 0;

  const query = `
    SELECT i.*,
      (SELECT title FROM items p WHERE p.id = i.parent_set_id) as parent_set_title,
      (SELECT COUNT(*) FROM items c WHERE c.parent_set_id = i.id AND c.type = 'save') as child_saves_count,
      (SELECT COUNT(*) FROM items c WHERE c.parent_set_id = i.id AND c.type = 'stamp') as child_stamps_count
    FROM items i
    ${whereClause}
    ORDER BY ${orderBy}
    LIMIT ? OFFSET ?
  `;

  const rows = (await db.prepare(query).all(...params, limitNum, offset)) as any[];

  let likedSet = new Set<string>();
  let favoritedSet = new Set<string>();
  if (client_uuid) {
    const interactions = (await db.prepare(`SELECT item_id, interaction_type FROM interactions WHERE client_uuid = ?`).all(client_uuid)) as any[];
    for (const inter of interactions) {
      if (inter.interaction_type === 'like') likedSet.add(inter.item_id);
      if (inter.interaction_type === 'favorite') favoritedSet.add(inter.item_id);
    }
  }

  const items: WorkshopItem[] = rows.map((r) => ({
    ...r,
    version: r.version || 1,
    is_liked: likedSet.has(r.id),
    is_favorited: favoritedSet.has(r.id)
  }));

  res.json({
    items,
    pagination: {
      page: pageNum,
      limit: limitNum,
      total,
      pages: Math.ceil(total / limitNum)
    }
  });
});

itemsRouter.get('/:id', async (req: Request, res: Response): Promise<void> => {
  const id = String(req.params['id']);
  const client_uuid = (req.query['client_uuid'] as string) || '';

  const query = `
    SELECT i.*,
      (SELECT title FROM items p WHERE p.id = i.parent_set_id) as parent_set_title,
      (SELECT COUNT(*) FROM items c WHERE c.parent_set_id = i.id AND c.type = 'save') as child_saves_count,
      (SELECT COUNT(*) FROM items c WHERE c.parent_set_id = i.id AND c.type = 'stamp') as child_stamps_count
    FROM items i
    WHERE i.id = ? AND i.is_hidden = 0
  `;

  const row = (await db.prepare(query).get(id)) as any;
  if (!row) {
    res.status(404).json({ error: 'Item not found' });
    return;
  }

  let is_liked = false;
  let is_favorited = false;
  if (client_uuid) {
    const interactions = (await db.prepare(`SELECT interaction_type FROM interactions WHERE item_id = ? AND client_uuid = ?`).all(id, client_uuid)) as any[];
    for (const inter of interactions) {
      if (inter.interaction_type === 'like') is_liked = true;
      if (inter.interaction_type === 'favorite') is_favorited = true;
    }
  }

  res.json({
    ...row,
    version: row.version || 1,
    is_liked,
    is_favorited
  });
});

itemsRouter.get('/sets/:id/saves', async (req: Request, res: Response): Promise<void> => {
  const id = String(req.params['id']);
  const query = `
    SELECT i.*
    FROM items i
    WHERE i.parent_set_id = ? AND i.type = 'save' AND i.is_hidden = 0
    ORDER BY i.likes_count DESC, i.created_at DESC
  `;
  const rows = await db.prepare(query).all(id);
  res.json(rows);
});

itemsRouter.get('/sets/:id/stamps', async (req: Request, res: Response): Promise<void> => {
  const id = String(req.params['id']);
  const query = `
    SELECT i.*
    FROM items i
    WHERE i.parent_set_id = ? AND i.type = 'stamp' AND i.is_hidden = 0
    ORDER BY i.likes_count DESC, i.created_at DESC
  `;
  const rows = await db.prepare(query).all(id);
  res.json(rows);
});

itemsRouter.get('/:id/thumbnail', async (req: Request, res: Response): Promise<void> => {
  const id = String(req.params['id']);
  const item = (await db.prepare('SELECT id, title, type, thumbnail_path FROM items WHERE id = ?').get(id)) as any;
  if (!item) {
    res.status(404).json({ error: 'Item not found' });
    return;
  }

  // 1. Check local file on disk (/tmp or uploads)
  if (item.thumbnail_path) {
    const thumbFile = path.resolve(THUMBNAILS_DIR, item.thumbnail_path);
    if (thumbFile.startsWith(THUMBNAILS_DIR) && fs.existsSync(thumbFile)) {
      res.setHeader('Cache-Control', 'public, max-age=86400, stale-while-revalidate=604800');
      res.sendFile(thumbFile);
      return;
    }
  }

  // 2. Check persistent NeonDB item_blobs
  try {
    await ensureItemBlobsTable();
    const blob = (await db.prepare('SELECT thumbnail_data FROM item_blobs WHERE item_id = ?').get(id)) as any;
    if (blob && blob.thumbnail_data) {
      const cleanData = blob.thumbnail_data.replace(/^data:image\/\w+;base64,/, '');
      const thumbBuffer = Buffer.from(cleanData, 'base64');
      const thumbFilename = item.thumbnail_path || `${id}.png`;
      try {
        fs.writeFileSync(path.join(THUMBNAILS_DIR, thumbFilename), thumbBuffer);
      } catch {}

      res.setHeader('Content-Type', 'image/png');
      res.setHeader('Cache-Control', 'public, max-age=86400, stale-while-revalidate=604800');
      res.send(thumbBuffer);
      return;
    }
  } catch (blobErr) {
    console.warn('[Thumbnail] Failed reading thumbnail from DB:', blobErr);
  }

  // 3. Fallback SVG card for items whose ephemeral containers lost their upload
  // Ensures the frontend never displays broken image icons
  const svg = generateFallbackThumbnailSvg(item.title || 'Sand3', item.type || 'item');
  res.setHeader('Content-Type', 'image/svg+xml');
  res.setHeader('Cache-Control', 'public, max-age=3600, stale-while-revalidate=86400');
  res.send(svg);
});

itemsRouter.post('/', uploadFields, async (req: Request, res: Response): Promise<void> => {
  try {
    const authUser = await getAuthenticatedUser(req);
    const body = req.body;
    const type = (body.type as ItemType) || 'save';
    const title = (body.title || '').trim();
    const description = (body.description || '').trim();
    const author = (authUser ? authUser.username : body.author || 'Anonymous').trim();
    const parent_set_id = body.parent_set_id ? String(body.parent_set_id).trim() : null;
    const set_hash = (body.set_hash || '').trim();
    let meta_json = body.meta_json || '{}';

    if (!['set', 'save', 'stamp', 'theme'].includes(type)) {
      res.status(400).json({ error: 'Invalid item type. Must be set, save, stamp, or theme' });
      return;
    }
    if (!title) {
      res.status(400).json({ error: 'Title is required' });
      return;
    }

    try {
      const parsedMeta = JSON.parse(meta_json);
      if (type === 'set') {
        delete parsedMeta.width;
        delete parsedMeta.height;
      }
      meta_json = JSON.stringify(parsedMeta);
    } catch {
      meta_json = '{}';
    }

    if (parent_set_id && (type === 'save' || type === 'stamp')) {
      const parentSet = (await db.prepare("SELECT id, title, set_hash FROM items WHERE id = ? AND type = 'set'").get(parent_set_id)) as any;
      if (!parentSet) {
        res.status(404).json({ error: `Associated set with ID '${parent_set_id}' was not found` });
        return;
      }
      if (set_hash && parentSet.set_hash && set_hash !== parentSet.set_hash) {
        res.status(409).json({
          error: `Set compatibility mismatch: the materials/rules in your local set differ from online set '${parentSet.title}'. Existing saves/stamps cannot be bound to modified sets.`,
          online_hash: parentSet.set_hash,
          provided_hash: set_hash
        });
        return;
      }
    }

    const id = crypto.randomUUID();
    let filePath = '';
    let fileSize = 0;

    const files = req.files as { [fieldname: string]: Express.Multer.File[] } | undefined;
    let fileDataBase64 = '';

    if (files && files['file'] && files['file'][0]) {
      filePath = files['file'][0].filename;
      fileSize = files['file'][0].size;
      try {
        fileDataBase64 = fs.readFileSync(path.join(UPLOADS_DIR, filePath)).toString('base64');
      } catch {}
    } else if (body.file_data) {
      fileDataBase64 = body.file_data.replace(/^data:\w+\/\w+;base64,/, '');
      const buffer = Buffer.from(fileDataBase64, 'base64');
      const ext = body.file_ext || (type === 'save' ? '.save' : type === 'stamp' ? '.stamp' : type === 'theme' ? '.theme' : '.zip');
      const filename = `${id}${ext}`;
      const dest = path.join(UPLOADS_DIR, filename);
      fs.writeFileSync(dest, buffer);
      filePath = filename;
      fileSize = buffer.length;
    } else {
      res.status(400).json({ error: 'File upload or file_data is required' });
      return;
    }

    let thumbnailPath = '';
    let thumbnailDataBase64 = '';
    if (files && files['thumbnail'] && files['thumbnail'][0]) {
      thumbnailPath = files['thumbnail'][0].filename;
      try {
        thumbnailDataBase64 = fs.readFileSync(path.join(THUMBNAILS_DIR, thumbnailPath)).toString('base64');
      } catch {}
    } else if (body.thumbnail_data) {
      thumbnailDataBase64 = body.thumbnail_data.replace(/^data:image\/\w+;base64,/, '');
      const thumbBuffer = Buffer.from(thumbnailDataBase64, 'base64');
      const thumbFilename = `${id}.png`;
      fs.writeFileSync(path.join(THUMBNAILS_DIR, thumbFilename), thumbBuffer);
      thumbnailPath = thumbFilename;
    }

    const userId = authUser ? authUser.id : null;

    let finalSetHash = set_hash;
    if (type === 'set') {
      const fullZipPath = path.join(UPLOADS_DIR, filePath);
      if (fs.existsSync(fullZipPath)) {
        const computed = computeZipSetHash(fs.readFileSync(fullZipPath));
        if (computed) {
          finalSetHash = computed;
        }
      }
    }

    await db
      .prepare(`
        INSERT INTO items (id, user_id, type, title, description, author, parent_set_id, version, set_hash, file_path, file_size, thumbnail_path, meta_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?, ?)
      `)
      .run(id, userId, type, title, description, author, parent_set_id, finalSetHash, filePath, fileSize, thumbnailPath, meta_json);

    await ensureItemBlobsTable();
    if (fileDataBase64 || thumbnailDataBase64) {
      await db
        .prepare(`
          INSERT INTO item_blobs (item_id, thumbnail_data, file_data)
          VALUES (?, ?, ?)
          ON CONFLICT (item_id) DO UPDATE SET
            thumbnail_data = COALESCE(EXCLUDED.thumbnail_data, item_blobs.thumbnail_data),
            file_data = COALESCE(EXCLUDED.file_data, item_blobs.file_data)
        `)
        .run(id, thumbnailDataBase64 || null, fileDataBase64 || null);
    }

    const created = await db.prepare('SELECT * FROM items WHERE id = ?').get(id);
    res.status(201).json(created);
  } catch (err: any) {
    console.error('Error creating workshop item:', err);
    res.status(500).json({ error: 'Failed to create item: ' + err.message });
  }
});

itemsRouter.put('/:id', uploadFields, async (req: Request, res: Response): Promise<void> => {
  try {
    const id = String(req.params['id']);
    const authUser = await getAuthenticatedUser(req);
    if (!authUser) {
      res.status(401).json({ error: 'You must be logged in to edit this item' });
      return;
    }

    const item = (await db.prepare('SELECT * FROM items WHERE id = ?').get(id)) as any;
    if (!item) {
      res.status(404).json({ error: 'Item not found' });
      return;
    }

    const isOwner =
      (item.user_id && item.user_id === authUser.id) ||
      (!item.user_id && item.author.toLowerCase() === authUser.username.toLowerCase());
    if (!isOwner) {
      res.status(403).json({ error: 'You do not own this item' });
      return;
    }

    const body = req.body;
    const title = (body.title || item.title).trim();
    const description = body.description !== undefined ? body.description.trim() : item.description;
    let metaObj: any = {};
    const rawMeta = body.meta_json !== undefined ? body.meta_json : item.meta_json;
    if (typeof rawMeta === 'object' && rawMeta !== null) {
      metaObj = { ...rawMeta };
    } else if (typeof rawMeta === 'string' && rawMeta.trim()) {
      try {
        metaObj = JSON.parse(rawMeta);
      } catch {
        metaObj = {};
      }
    }

    let set_hash = body.set_hash !== undefined ? body.set_hash.trim() : item.set_hash;
    let filePath = item.file_path;
    let fileSize = item.file_size;
    let version = item.version || 1;

    const files = req.files as { [fieldname: string]: Express.Multer.File[] } | undefined;
    let fileUpdated = false;
    let fileDataBase64 = '';

    if (files && files['file'] && files['file'][0]) {
      filePath = files['file'][0].filename;
      fileSize = files['file'][0].size;
      fileUpdated = true;
      try {
        fileDataBase64 = fs.readFileSync(path.join(UPLOADS_DIR, filePath)).toString('base64');
      } catch {}
    } else if (body.file_data) {
      fileDataBase64 = body.file_data.replace(/^data:\w+\/\w+;base64,/, '');
      const buffer = Buffer.from(fileDataBase64, 'base64');
      const ext = body.file_ext || path.extname(item.file_path) || '.bin';
      const filename = `${crypto.randomUUID()}${ext}`;
      fs.writeFileSync(path.join(UPLOADS_DIR, filename), buffer);
      filePath = filename;
      fileSize = buffer.length;
      fileUpdated = true;
    }

    if (body.version !== undefined && !isNaN(parseInt(body.version, 10))) {
      version = Math.max(1, parseInt(body.version, 10));
    } else if (fileUpdated || body.bump_version) {
      version = (item.version || 1) + 1;
    }

    if (body.changelog && typeof body.changelog === 'string' && body.changelog.trim()) {
      if (!Array.isArray(metaObj.changelog)) {
        metaObj.changelog = [];
      }
      metaObj.changelog.unshift({
        version: version,
        date: new Date().toISOString(),
        notes: body.changelog.trim()
      });
    }

    if (item.type === 'set') {
      delete metaObj.width;
      delete metaObj.height;
    }

    let meta_json = JSON.stringify(metaObj);

    if (fileUpdated && item.file_path && item.file_path !== filePath) {
      try {
        const oldFile = path.resolve(UPLOADS_DIR, item.file_path);
        if (fs.existsSync(oldFile)) fs.unlinkSync(oldFile);
      } catch (e) {
        console.warn('Could not remove previous payload file:', e);
      }
    }

    if (fileUpdated && item.type === 'set') {
      const fullZipPath = path.join(UPLOADS_DIR, filePath);
      if (fs.existsSync(fullZipPath)) {
        const computed = computeZipSetHash(fs.readFileSync(fullZipPath));
        if (computed) {
          set_hash = computed;
        }
      }
    }

    let thumbnailPath = item.thumbnail_path;
    let thumbUpdated = false;
    let thumbnailDataBase64 = '';
    if (files && files['thumbnail'] && files['thumbnail'][0]) {
      thumbnailPath = files['thumbnail'][0].filename;
      thumbUpdated = true;
      try {
        thumbnailDataBase64 = fs.readFileSync(path.join(THUMBNAILS_DIR, thumbnailPath)).toString('base64');
      } catch {}
    } else if (body.thumbnail_data) {
      thumbnailDataBase64 = body.thumbnail_data.replace(/^data:image\/\w+;base64,/, '');
      const thumbBuffer = Buffer.from(thumbnailDataBase64, 'base64');
      const thumbFilename = `${id}_v${version}.png`;
      fs.writeFileSync(path.join(THUMBNAILS_DIR, thumbFilename), thumbBuffer);
      thumbnailPath = thumbFilename;
      thumbUpdated = true;
    }

    if (thumbUpdated && item.thumbnail_path && item.thumbnail_path !== thumbnailPath) {
      try {
        const oldThumb = path.resolve(THUMBNAILS_DIR, item.thumbnail_path);
        if (fs.existsSync(oldThumb)) fs.unlinkSync(oldThumb);
      } catch (e) {
        console.warn('Could not remove previous thumbnail file:', e);
      }
    }

    await db
      .prepare(`
        UPDATE items 
        SET title = ?, description = ?, version = ?, set_hash = ?, file_path = ?, file_size = ?, thumbnail_path = ?, meta_json = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
      `)
      .run(title, description, version, set_hash, filePath, fileSize, thumbnailPath, meta_json, id);

    if (fileDataBase64 || thumbnailDataBase64) {
      await ensureItemBlobsTable();
      await db
        .prepare(`
          INSERT INTO item_blobs (item_id, thumbnail_data, file_data)
          VALUES (?, ?, ?)
          ON CONFLICT (item_id) DO UPDATE SET
            thumbnail_data = COALESCE(EXCLUDED.thumbnail_data, item_blobs.thumbnail_data),
            file_data = COALESCE(EXCLUDED.file_data, item_blobs.file_data)
        `)
        .run(id, thumbnailDataBase64 || null, fileDataBase64 || null);
    }

    const updated = await db.prepare('SELECT * FROM items WHERE id = ?').get(id);
    res.json(updated);
  } catch (err: any) {
    console.error('Error updating workshop item:', err);
    res.status(500).json({ error: 'Failed to update item: ' + err.message });
  }
});

itemsRouter.delete('/:id', async (req: Request, res: Response): Promise<void> => {
  try {
    const id = String(req.params['id']);
    const authUser = await getAuthenticatedUser(req);
    if (!authUser) {
      res.status(401).json({ error: 'You must be logged in to delete this item' });
      return;
    }

    const item = (await db.prepare('SELECT * FROM items WHERE id = ?').get(id)) as any;
    if (!item) {
      res.status(404).json({ error: 'Item not found' });
      return;
    }

    const isOwner =
      (item.user_id && item.user_id === authUser.id) ||
      (!item.user_id && item.author.toLowerCase() === authUser.username.toLowerCase());
    if (!isOwner) {
      res.status(403).json({ error: 'You do not own this item' });
      return;
    }

    try {
      const mainFile = path.resolve(UPLOADS_DIR, item.file_path);
      if (fs.existsSync(mainFile)) fs.unlinkSync(mainFile);
      if (item.thumbnail_path) {
        const thumbFile = path.resolve(THUMBNAILS_DIR, item.thumbnail_path);
        if (fs.existsSync(thumbFile)) fs.unlinkSync(thumbFile);
      }
    } catch (e) {
      console.warn('Could not remove file on delete:', e);
    }

    await db.prepare('DELETE FROM items WHERE id = ?').run(id);

    res.json({ success: true, message: 'Item deleted successfully', id });
  } catch (err: any) {
    console.error('Error deleting workshop item:', err);
    res.status(500).json({ error: 'Failed to delete item: ' + err.message });
  }
});
