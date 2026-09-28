import dotenv from 'dotenv';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

dotenv.config({ path: path.resolve(__dirname, '../.env') });
dotenv.config();

let databaseUrl =
  process.env['DATABASE_URL'] ||
  process.env['POSTGRES_URL'] ||
  process.env['POSTGRES_PRISMA_URL'] ||
  process.env['DATABASE_URL_UNPOOLED'] ||
  process.env['POSTGRES_URL_NON_POOLING'] ||
  '';


if (!databaseUrl && process.env['POSTGRES_HOST'] && process.env['POSTGRES_USER'] && process.env['POSTGRES_PASSWORD']) {
  const dbName = (process.env['PGDATABASE'] || process.env['POSTGRES_DATABASE'] || 'sand3').trim();
  databaseUrl = `postgresql://${encodeURIComponent(process.env['POSTGRES_USER'])}:${encodeURIComponent(process.env['POSTGRES_PASSWORD'])}@${process.env['POSTGRES_HOST']}/${dbName}?sslmode=require`;
}


const specifiedDb = (process.env['PGDATABASE'] || process.env['POSTGRES_DATABASE'] || '').trim();
if (databaseUrl && specifiedDb) {
  try {
    const parsed = new URL(databaseUrl);
    if (parsed.pathname !== `/${specifiedDb}`) {
      parsed.pathname = `/${specifiedDb}`;
      databaseUrl = parsed.toString();
    }
  } catch { }
}

const neonAuthBaseUrl = (
  process.env['NEON_AUTH_BASE_URL'] ||
  process.env['VITE_NEON_AUTH_URL'] ||
  process.env['NEON_AUTH_URL'] ||
  process.env['_AUTH_BASE_URL'] ||
  process.env['VITE__AUTH_URL'] ||
  process.env['AUTH_BASE_URL'] ||
  ''
).trim();

const cleanAuthBase = neonAuthBaseUrl.replace(/\/+$/, '');

const neonAuthJwksUrl = (
  process.env['NEON_AUTH_JWKS_URL'] ||
  (cleanAuthBase ? `${cleanAuthBase}/.well-known/jwks.json` : '')
).trim();

export const config = {
  databaseUrl,

  neonProjectId: process.env['NEON_PROJECT_ID'] || '',

  neonAuthBaseUrl,

  neonAuthJwksUrl,

  adminKey: process.env['ADMIN_KEY'] || '',

  adminToken: process.env['ADMIN_TOKEN'] || '',

  adminUsername: (process.env['ADMIN_USERNAME'] || 'Cast_E').trim(),

  adminPassword: process.env['ADMIN_PASSWORD'] || '',

  adminUserId: (process.env['ADMIN_USER_ID'] || '').trim(),

  port: process.env['PORT'] || 3000,
  isProduction: process.env['NODE_ENV'] === 'production' || !!process.env['VERCEL']
};
