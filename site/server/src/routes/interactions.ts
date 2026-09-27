import { Router } from 'express';
import { db } from '../db.js';

export const interactionsRouter = Router();

// 1. Toggle Like on an item
interactionsRouter.post('/:id/like', (req, res) => {
  const id = String(req.params.id);
  const { client_uuid } = req.body;

  if (!client_uuid) {
    return res.status(400).json({ error: 'client_uuid is required' });
  }

  const item = db.prepare('SELECT id, likes_count FROM items WHERE id = ?').get(id) as any;
  if (!item) {
    return res.status(404).json({ error: 'Item not found' });
  }

  const existing = db
    .prepare('SELECT id FROM interactions WHERE item_id = ? AND client_uuid = ? AND interaction_type = ?')
    .get(id, client_uuid, 'like') as any;

  let is_liked = false;
  if (existing) {
    // Remove like
    db.prepare('DELETE FROM interactions WHERE id = ?').run(existing.id);
    db.prepare('UPDATE items SET likes_count = MAX(0, likes_count - 1) WHERE id = ?').run(id);
    is_liked = false;
  } else {
    // Add like
    db.prepare('INSERT INTO interactions (item_id, client_uuid, interaction_type) VALUES (?, ?, ?)').run(
      id,
      client_uuid,
      'like'
    );
    db.prepare('UPDATE items SET likes_count = likes_count + 1 WHERE id = ?').run(id);
    is_liked = true;
  }

  const updated = db.prepare('SELECT likes_count FROM items WHERE id = ?').get(id) as any;
  res.json({
    item_id: id,
    is_liked,
    likes_count: updated.likes_count
  });
});

// 2. Toggle Favorite on an item
interactionsRouter.post('/:id/favorite', (req, res) => {
  const id = String(req.params.id);
  const { client_uuid } = req.body;

  if (!client_uuid) {
    return res.status(400).json({ error: 'client_uuid is required' });
  }

  const item = db.prepare('SELECT id, favorites_count FROM items WHERE id = ?').get(id) as any;
  if (!item) {
    return res.status(404).json({ error: 'Item not found' });
  }

  const existing = db
    .prepare('SELECT id FROM interactions WHERE item_id = ? AND client_uuid = ? AND interaction_type = ?')
    .get(id, client_uuid, 'favorite') as any;

  let is_favorited = false;
  if (existing) {
    // Remove favorite
    db.prepare('DELETE FROM interactions WHERE id = ?').run(existing.id);
    db.prepare('UPDATE items SET favorites_count = MAX(0, favorites_count - 1) WHERE id = ?').run(id);
    is_favorited = false;
  } else {
    // Add favorite
    db.prepare('INSERT INTO interactions (item_id, client_uuid, interaction_type) VALUES (?, ?, ?)').run(
      id,
      client_uuid,
      'favorite'
    );
    db.prepare('UPDATE items SET favorites_count = favorites_count + 1 WHERE id = ?').run(id);
    is_favorited = true;
  }

  const updated = db.prepare('SELECT favorites_count FROM items WHERE id = ?').get(id) as any;
  res.json({
    item_id: id,
    is_favorited,
    favorites_count: updated.favorites_count
  });
});

// 3. Report an item
interactionsRouter.post('/:id/report', (req, res) => {
  const id = String(req.params.id);
  const { client_uuid, reason = 'other', details = '' } = req.body;

  if (!client_uuid) {
    return res.status(400).json({ error: 'client_uuid is required' });
  }
  if (!['broken', 'offensive', 'spam', 'other'].includes(reason)) {
    return res.status(400).json({ error: 'Invalid report reason' });
  }

  const item = db.prepare('SELECT id, reports_count FROM items WHERE id = ?').get(id) as any;
  if (!item) {
    return res.status(404).json({ error: 'Item not found' });
  }

  // Insert report
  db.prepare('INSERT INTO reports (item_id, client_uuid, reason, details) VALUES (?, ?, ?, ?)').run(
    id,
    client_uuid,
    reason,
    details.substring(0, 500)
  );

  // Increment report count; auto-hide if threshold exceeded
  db.prepare('UPDATE items SET reports_count = reports_count + 1 WHERE id = ?').run(id);
  const updated = db.prepare('SELECT reports_count FROM items WHERE id = ?').get(id) as any;

  if (updated.reports_count >= 5) {
    db.prepare('UPDATE items SET is_hidden = 1 WHERE id = ?').run(id);
  }

  res.json({
    message: 'Report submitted successfully. Thank you for helping keep the workshop clean and safe.',
    reports_count: updated.reports_count
  });
});

// 4. Get User's Favorited Items
interactionsRouter.get('/user/favorites', (req, res) => {
  const { client_uuid } = req.query as { client_uuid: string };
  if (!client_uuid) {
    return res.status(400).json({ error: 'client_uuid query parameter is required' });
  }

  const query = `
    SELECT i.*,
      (SELECT title FROM items p WHERE p.id = i.parent_set_id) as parent_set_title
    FROM items i
    JOIN interactions inter ON inter.item_id = i.id
    WHERE inter.client_uuid = ? AND inter.interaction_type = 'favorite' AND i.is_hidden = 0
    ORDER BY inter.created_at DESC
  `;

  const rows = db.prepare(query).all(client_uuid);
  res.json(rows);
});
