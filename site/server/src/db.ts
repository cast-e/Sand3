import { neon } from '@neondatabase/serverless';
import { config } from './config.js';

if (!config.databaseUrl) {
  console.warn('[DB] Warning: DATABASE_URL environment variable is not set. Set it in Vercel or your .env file.');
}

function cleanUrl(url: string): string {
  if (!url) return '';
  return url.replace(/([?&])channel_binding=[^&]+(&|$)/, '$1').replace(/[?&]$/, '');
}

const DUMMY_URL = 'postgresql://dummy:dummy@localhost/dummy';
let activeDatabaseUrl = cleanUrl(config.databaseUrl);
export let sql = neon(activeDatabaseUrl || DUMMY_URL);

export function getActiveDatabaseUrl(): string {
  return activeDatabaseUrl;
}

export function translateSql(query: string): string {
  let paramIdx = 1;
  let converted = query.replace(/\?/g, () => `$${paramIdx++}`);

  converted = converted.replace(/datetime\('now'\)/gi, 'CURRENT_TIMESTAMP');
  converted = converted.replace(/datetime\(([^)]+)\)/gi, '$1');

  converted = converted.replace(/COLLATE\s+NOCASE/gi, '');

  converted = converted.replace(/COUNT\(([^)]+)\)(?!::int)/gi, 'COUNT($1)::int');
  converted = converted.replace(/COALESCE\(SUM\(([^)]+)\),\s*0\)/gi, 'COALESCE(SUM($1)::int, 0)');

  if (/INSERT\s+OR\s+REPLACE\s+INTO\s+sessions/i.test(converted)) {
    converted = converted.replace(
      /INSERT\s+OR\s+REPLACE\s+INTO\s+sessions\s*\(([^)]+)\)\s*VALUES\s*\(([^)]+)\)/i,
      'INSERT INTO sessions ($1) VALUES ($2) ON CONFLICT (token) DO UPDATE SET user_id = EXCLUDED.user_id, expires_at = EXCLUDED.expires_at'
    );
  }
  if (/INSERT\s+OR\s+IGNORE\s+INTO\s+users/i.test(converted)) {
    converted = converted.replace(
      /INSERT\s+OR\s+IGNORE\s+INTO\s+users\s*\(([^)]+)\)\s*VALUES\s*\(([^)]+)\)/i,
      'INSERT INTO users ($1) VALUES ($2) ON CONFLICT (id) DO NOTHING'
    );
  }

  return converted;
}

export interface PreparedStatement {
  get(...params: any[]): Promise<any | undefined>;
  all(...params: any[]): Promise<any[]>;
  run(...params: any[]): Promise<{ changes: number }>;
}

function checkDatabaseConfigured() {
  if (!activeDatabaseUrl && !config.databaseUrl) {
    throw new Error('DATABASE_URL is not configured. Please ensure DATABASE_URL or POSTGRES_URL is set in Vercel Environment Variables.');
  }
}

export async function runSqlQuery(translated: string, flatParams: any[] = []): Promise<any[]> {
  checkDatabaseConfigured();

  const queryPromise = sql.query(translated, flatParams) as Promise<any[]>;
  const timeoutPromise = new Promise<never>((_, reject) =>
    setTimeout(() => reject(new Error('Database query timed out after 10000ms. Check Neon compute status and IP allowlist.')), 10000)
  );

  try {
    return (await Promise.race([queryPromise, timeoutPromise])) as any[];
  } catch (err: any) {
    const errMsg = err?.message || String(err);


    if (errMsg.includes('database "neondb" does not exist') && activeDatabaseUrl) {
      try {
        const parsed = new URL(activeDatabaseUrl);
        if (parsed.pathname === '/neondb' || parsed.pathname === '/neondb/') {
          console.warn('[DB Auto-Heal] Database "neondb" does not exist. Switching target to "sand3"...');
          parsed.pathname = '/sand3';
          activeDatabaseUrl = parsed.toString();
          config.databaseUrl = activeDatabaseUrl;
          sql = neon(activeDatabaseUrl);
          return (await sql.query(translated, flatParams)) as any[];
        }
      } catch (switchErr) {
        console.error('[DB Auto-Heal] Failed switching database URL:', switchErr);
      }
    }

    throw err;
  }
}

let ensureBlobsPromise: Promise<void> | null = null;
export async function ensureItemBlobsTable(): Promise<void> {
  if (!ensureBlobsPromise) {
    ensureBlobsPromise = db
      .exec(
        `CREATE TABLE IF NOT EXISTS item_blobs (
          item_id TEXT PRIMARY KEY REFERENCES items(id) ON DELETE CASCADE,
          thumbnail_data TEXT,
          file_data TEXT,
          created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
        );`
      )
      .catch((err) => {
        ensureBlobsPromise = null;
        console.warn('[DB] Could not ensure item_blobs table:', err);
      });
  }
  return ensureBlobsPromise;
}

export const db = {
  prepare(queryText: string): PreparedStatement {
    return {
      async get(...params: any[]): Promise<any | undefined> {
        const translated = translateSql(queryText);
        const flatParams = params.flat();
        const rows = await runSqlQuery(translated, flatParams);
        return rows && rows.length > 0 ? rows[0] : undefined;
      },
      async all(...params: any[]): Promise<any[]> {
        const translated = translateSql(queryText);
        const flatParams = params.flat();
        const rows = await runSqlQuery(translated, flatParams);
        return rows || [];
      },
      async run(...params: any[]): Promise<{ changes: number }> {
        const translated = translateSql(queryText);
        const flatParams = params.flat();
        await runSqlQuery(translated, flatParams);
        return { changes: 1 };
      }
    };
  },
  async query(queryText: string, params: any[] = []): Promise<any[]> {
    const translated = translateSql(queryText);
    const flatParams = params.flat();
    return await runSqlQuery(translated, flatParams);
  },
  async exec(statement: string): Promise<void> {
    const stmts = statement
      .split(';')
      .map((s) => s.trim())
      .filter((s) => s.length > 0);
    for (const s of stmts) {
      await runSqlQuery(s);
    }
  }
};

export async function initDb(): Promise<void> {
  if (!config.databaseUrl) {
    return;
  }

  try {
    await db.exec(`
      CREATE TABLE IF NOT EXISTS users (
        id TEXT PRIMARY KEY,
        username TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        is_admin INTEGER DEFAULT 0,
        role TEXT DEFAULT 'user',
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
    `);

    await db.exec(`
      CREATE TABLE IF NOT EXISTS sessions (
        token TEXT PRIMARY KEY,
        user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
    `);

    await db.exec(`
      CREATE TABLE IF NOT EXISTS items (
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
        forked_from_id TEXT REFERENCES items(id) ON DELETE SET NULL,
        forked_from_version INTEGER,
        likes_count INTEGER DEFAULT 0,
        favorites_count INTEGER DEFAULT 0,
        downloads_count INTEGER DEFAULT 0,
        reports_count INTEGER DEFAULT 0,
        is_hidden INTEGER DEFAULT 0,
        is_private INTEGER DEFAULT 0,
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
      ALTER TABLE items ADD COLUMN IF NOT EXISTS forked_from_id TEXT REFERENCES items(id) ON DELETE SET NULL;
      ALTER TABLE items ADD COLUMN IF NOT EXISTS forked_from_version INTEGER;
      ALTER TABLE items ADD COLUMN IF NOT EXISTS is_private INTEGER DEFAULT 0;
      CREATE INDEX IF NOT EXISTS idx_items_type ON items(type);
      CREATE INDEX IF NOT EXISTS idx_items_parent_set_id ON items(parent_set_id);
      CREATE INDEX IF NOT EXISTS idx_items_forked_from_id ON items(forked_from_id);
      CREATE INDEX IF NOT EXISTS idx_items_user_id ON items(user_id);
      CREATE INDEX IF NOT EXISTS idx_items_is_private ON items(is_private);
      CREATE INDEX IF NOT EXISTS idx_items_set_hash ON items(set_hash);
      CREATE INDEX IF NOT EXISTS idx_items_created_at ON items(created_at);
      CREATE INDEX IF NOT EXISTS idx_items_likes ON items(likes_count DESC);

      CREATE TABLE IF NOT EXISTS item_versions (
        id SERIAL PRIMARY KEY,
        item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
        version INTEGER NOT NULL,
        file_path TEXT NOT NULL,
        file_size INTEGER NOT NULL,
        set_hash TEXT DEFAULT '',
        changelog TEXT DEFAULT '',
        meta_json TEXT DEFAULT '{}',
        file_data TEXT,
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
        UNIQUE(item_id, version)
      );
      CREATE INDEX IF NOT EXISTS idx_item_versions_lookup ON item_versions(item_id, version DESC);
    `);

    await db.exec(`
      CREATE TABLE IF NOT EXISTS interactions (
        id SERIAL PRIMARY KEY,
        item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
        user_id TEXT REFERENCES users(id) ON DELETE CASCADE,
        client_uuid TEXT,
        interaction_type TEXT NOT NULL CHECK(interaction_type IN ('like', 'favorite')),
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
      ALTER TABLE interactions ADD COLUMN IF NOT EXISTS user_id TEXT REFERENCES users(id) ON DELETE CASCADE;
      ALTER TABLE interactions ALTER COLUMN client_uuid DROP NOT NULL;
      CREATE UNIQUE INDEX IF NOT EXISTS idx_interactions_user_uniq ON interactions(item_id, user_id, interaction_type) WHERE user_id IS NOT NULL;
      CREATE INDEX IF NOT EXISTS idx_interactions_user ON interactions(user_id, interaction_type);
      CREATE INDEX IF NOT EXISTS idx_interactions_lookup ON interactions(item_id, user_id);
    `);

    await db.exec(`
      CREATE TABLE IF NOT EXISTS reports (
        id SERIAL PRIMARY KEY,
        item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
        user_id TEXT REFERENCES users(id) ON DELETE SET NULL,
        client_uuid TEXT,
        reason TEXT NOT NULL CHECK(reason IN ('broken', 'offensive', 'spam', 'other')),
        details TEXT DEFAULT '',
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
      ALTER TABLE reports ADD COLUMN IF NOT EXISTS user_id TEXT REFERENCES users(id) ON DELETE SET NULL;
      ALTER TABLE reports ALTER COLUMN client_uuid DROP NOT NULL;
      CREATE INDEX IF NOT EXISTS idx_reports_user ON reports(user_id);
    `);

    await db.exec(`
      CREATE TABLE IF NOT EXISTS item_blobs (
        item_id TEXT PRIMARY KEY REFERENCES items(id) ON DELETE CASCADE,
        thumbnail_data TEXT,
        file_data TEXT,
        created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
    `);

    if (config.adminUsername) {
      const existing = (await db
        .prepare('SELECT id, is_admin, role FROM users WHERE LOWER(username) = LOWER(?)')
        .get(config.adminUsername)) as any;

      const adminId = existing?.id || config.adminUserId || 'admin-root';

      if (!existing) {
        await db
          .prepare(
            `INSERT INTO users (id, username, password_hash, is_admin, role)
             VALUES (?, ?, 'bootstrap:bootstrap', 1, 'admin')
             ON CONFLICT (id) DO UPDATE SET is_admin = 1, role = 'admin'`
          )
          .run(adminId, config.adminUsername);
      } else if (!existing.is_admin || existing.role !== 'admin') {
        await db.prepare("UPDATE users SET is_admin = 1, role = 'admin' WHERE id = ?").run(existing.id);
      }

      if (config.adminToken) {
        const futureExpiry = new Date(Date.now() + 365 * 24 * 60 * 60 * 1000).toISOString();
        await db
          .prepare(
            `INSERT INTO sessions (token, user_id, expires_at)
             VALUES (?, ?, ?)
             ON CONFLICT (token) DO UPDATE SET expires_at = EXCLUDED.expires_at, user_id = EXCLUDED.user_id`
          )
          .run(config.adminToken, adminId, futureExpiry);
      }
    }

    console.log('[DB] Neon PostgreSQL tables and bootstrap verified successfully.');
  } catch (err) {
    console.warn('[DB] Neon initDb notice:', err);
  }
}

if (!process.env['VERCEL'] && config.databaseUrl) {
  initDb().catch((err) => console.error('[DB] Failed initializing DB:', err));
}
