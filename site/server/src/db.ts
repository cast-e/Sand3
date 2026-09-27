import { DatabaseSync } from 'node:sqlite';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const DATA_DIR = process.env.DATA_DIR || (process.env.VERCEL ? '/tmp/sand3-data' : path.resolve(__dirname, '../data'));
if (!fs.existsSync(DATA_DIR)) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
}

const DB_PATH = process.env.DB_PATH || path.join(DATA_DIR, 'workshop.db');

// If on Vercel or custom DATA_DIR, copy bundled db if present and DB_PATH does not exist yet
const bundledDbPath = path.resolve(__dirname, '../data/workshop.db');
if (DB_PATH !== bundledDbPath && !fs.existsSync(DB_PATH) && fs.existsSync(bundledDbPath)) {
  try {
    fs.copyFileSync(bundledDbPath, DB_PATH);
  } catch (err) {
    console.warn('[DB] Could not copy bundled database:', err);
  }
}

export const db = new DatabaseSync(DB_PATH);

// Enable WAL mode and foreign keys for durability and performance
db.exec('PRAGMA journal_mode = WAL;');
db.exec('PRAGMA foreign_keys = ON;');

// Initialize users and sessions tables
db.exec(`
  CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    username TEXT UNIQUE NOT NULL COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    expires_at DATETIME NOT NULL
  );
  CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
`);

// Add is_admin and role columns to users if not present
const userCols = db.prepare("PRAGMA table_info(users)").all() as any[];
if (!userCols.some((c: any) => c.name === 'is_admin')) {
  try {
    db.exec('ALTER TABLE users ADD COLUMN is_admin INTEGER DEFAULT 0;');
  } catch {}
}
if (!userCols.some((c: any) => c.name === 'role')) {
  try {
    db.exec("ALTER TABLE users ADD COLUMN role TEXT DEFAULT 'user';");
    db.exec("UPDATE users SET role = 'admin' WHERE is_admin = 1;");
  } catch {}
}

// Ensure Cast_E and admin token e3b8937e-ac6c-4c8c-a59a-f3f243c8182d exist and have admin rights
try {
  const adminUser = db.prepare('SELECT id FROM users WHERE id = ? OR username = ?').get('c7831b24-de86-45ae-b049-9ecdf88a3219', 'Cast_E') as any;
  if (!adminUser) {
    db.prepare(`
      INSERT OR IGNORE INTO users (id, username, password_hash, is_admin, role)
      VALUES (?, ?, ?, 1, 'admin')
    `).run('c7831b24-de86-45ae-b049-9ecdf88a3219', 'Cast_E', 'bootstrap:bootstrap');
  } else {
    db.prepare("UPDATE users SET is_admin = 1, role = 'admin' WHERE id = ?").run(adminUser.id);
  }

  const adminSession = db.prepare('SELECT token FROM sessions WHERE token = ?').get('e3b8937e-ac6c-4c8c-a59a-f3f243c8182d');
  if (!adminSession) {
    const futureExpiry = new Date(Date.now() + 365 * 24 * 60 * 60 * 1000).toISOString();
    db.prepare(`
      INSERT OR REPLACE INTO sessions (token, user_id, expires_at)
      VALUES (?, ?, ?)
    `).run('e3b8937e-ac6c-4c8c-a59a-f3f243c8182d', 'c7831b24-de86-45ae-b049-9ecdf88a3219', futureExpiry);
  }

  // Also ensure if e3b8937e-ac6c-4c8c-a59a-f3f243c8182d was registered as user ID, it is set as admin
  db.prepare("UPDATE users SET is_admin = 1, role = 'admin' WHERE id = 'e3b8937e-ac6c-4c8c-a59a-f3f243c8182d'").run();
} catch (e) {
  console.warn('[DB] Admin bootstrap warning:', e);
}

// Check if items table needs migration or creation
const tableInfo = db.prepare("PRAGMA table_info(items)").all() as any[];
if (tableInfo.length === 0) {
  db.exec(`
    CREATE TABLE items (
      id TEXT PRIMARY KEY,
      user_id TEXT REFERENCES users(id) ON DELETE SET NULL,
      type TEXT NOT NULL CHECK(type IN ('set', 'save', 'stamp', 'theme')),
      title TEXT NOT NULL,
      description TEXT DEFAULT '',
      author TEXT NOT NULL,
      parent_set_id TEXT REFERENCES items(id) ON DELETE SET NULL,
      version INTEGER DEFAULT 1,
      set_hash TEXT DEFAULT '',
      file_path TEXT NOT NULL,
      file_size INTEGER NOT NULL,
      thumbnail_path TEXT DEFAULT '',
      meta_json TEXT DEFAULT '{}',
      likes_count INTEGER DEFAULT 0,
      favorites_count INTEGER DEFAULT 0,
      downloads_count INTEGER DEFAULT 0,
      reports_count INTEGER DEFAULT 0,
      is_hidden INTEGER DEFAULT 0,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
      updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );
  `);
} else {
  const colNames = new Set(tableInfo.map((c) => c.name));
  if (!colNames.has('user_id')) db.exec('ALTER TABLE items ADD COLUMN user_id TEXT REFERENCES users(id) ON DELETE SET NULL;');
  if (!colNames.has('version')) db.exec('ALTER TABLE items ADD COLUMN version INTEGER DEFAULT 1;');
  if (!colNames.has('set_hash')) db.exec("ALTER TABLE items ADD COLUMN set_hash TEXT DEFAULT '';");
  if (!colNames.has('thumbnail_path')) db.exec("ALTER TABLE items ADD COLUMN thumbnail_path TEXT DEFAULT '';");

  const sqlRow = db.prepare("SELECT sql FROM sqlite_master WHERE type='table' AND name='items'").get() as any;
  if (sqlRow && sqlRow.sql && !sqlRow.sql.includes('theme')) {
    db.exec(`
      CREATE TABLE items_new (
        id TEXT PRIMARY KEY,
        user_id TEXT REFERENCES users(id) ON DELETE SET NULL,
        type TEXT NOT NULL CHECK(type IN ('set', 'save', 'stamp', 'theme')),
        title TEXT NOT NULL,
        description TEXT DEFAULT '',
        author TEXT NOT NULL,
        parent_set_id TEXT REFERENCES items_new(id) ON DELETE SET NULL,
        version INTEGER DEFAULT 1,
        set_hash TEXT DEFAULT '',
        file_path TEXT NOT NULL,
        file_size INTEGER NOT NULL,
        thumbnail_path TEXT DEFAULT '',
        meta_json TEXT DEFAULT '{}',
        likes_count INTEGER DEFAULT 0,
        favorites_count INTEGER DEFAULT 0,
        downloads_count INTEGER DEFAULT 0,
        reports_count INTEGER DEFAULT 0,
        is_hidden INTEGER DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
      );
      INSERT INTO items_new (id, user_id, type, title, description, author, parent_set_id, version, set_hash, file_path, file_size, thumbnail_path, meta_json, likes_count, favorites_count, downloads_count, reports_count, is_hidden, created_at, updated_at)
      SELECT id, user_id, type, title, description, author, parent_set_id, COALESCE(version, 1), COALESCE(set_hash, ''), file_path, file_size, COALESCE(thumbnail_path, ''), meta_json, likes_count, favorites_count, downloads_count, reports_count, is_hidden, created_at, updated_at FROM items;
      DROP TABLE items;
      ALTER TABLE items_new RENAME TO items;
    `);
  }
}

// Create indexes
db.exec(`
  CREATE INDEX IF NOT EXISTS idx_items_type ON items(type);
  CREATE INDEX IF NOT EXISTS idx_items_parent_set_id ON items(parent_set_id);
  CREATE INDEX IF NOT EXISTS idx_items_user_id ON items(user_id);
  CREATE INDEX IF NOT EXISTS idx_items_set_hash ON items(set_hash);
  CREATE INDEX IF NOT EXISTS idx_items_created_at ON items(created_at);
  CREATE INDEX IF NOT EXISTS idx_items_likes ON items(likes_count DESC);

  CREATE TABLE IF NOT EXISTS interactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
    client_uuid TEXT NOT NULL,
    interaction_type TEXT NOT NULL CHECK(interaction_type IN ('like', 'favorite')),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(item_id, client_uuid, interaction_type)
  );

  CREATE INDEX IF NOT EXISTS idx_interactions_lookup ON interactions(item_id, client_uuid);

  CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
    client_uuid TEXT NOT NULL,
    reason TEXT NOT NULL CHECK(reason IN ('broken', 'offensive', 'spam', 'other')),
    details TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );
`);
