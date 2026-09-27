import dotenv from 'dotenv';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

dotenv.config({ path: path.resolve(__dirname, '../.env') });
dotenv.config();

const databaseUrl =
  process.env['DATABASE_URL'] ||
  process.env['POSTGRES_URL'] ||
  process.env['POSTGRES_PRISMA_URL'] ||
  process.env['DATABASE_URL_UNPOOLED'] ||
  process.env['POSTGRES_URL_NON_POOLING'] ||
  '';

const neonAuthBaseUrl = (
  process.env['NEON_AUTH_BASE_URL'] ||
  process.env['VITE_NEON_AUTH_URL'] ||
  process.env['NEON_AUTH_URL'] ||
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
