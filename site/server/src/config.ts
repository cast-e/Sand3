import dotenv from 'dotenv';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Load environment variables from .env file if available
dotenv.config({ path: path.resolve(__dirname, '../.env') });
dotenv.config();

// Resolve database connection URL (supporting native Neon + Vercel integration variables)
const databaseUrl =
  process.env.DATABASE_URL ||
  process.env.POSTGRES_URL ||
  process.env.POSTGRES_PRISMA_URL ||
  process.env.DATABASE_URL_UNPOOLED ||
  process.env.POSTGRES_URL_NON_POOLING ||
  '';

// Resolve Neon Auth base URL (supporting native Neon Vercel integration: NEON_AUTH_BASE_URL, VITE_NEON_AUTH_URL)
const neonAuthBaseUrl = (
  process.env.NEON_AUTH_BASE_URL ||
  process.env.VITE_NEON_AUTH_URL ||
  process.env.NEON_AUTH_URL ||
  ''
).trim();

const cleanAuthBase = neonAuthBaseUrl.replace(/\/+$/, '');

// Resolve Neon Auth JWKS endpoint URL (explicit or derived from NEON_AUTH_BASE_URL)
const neonAuthJwksUrl = (
  process.env.NEON_AUTH_JWKS_URL ||
  (cleanAuthBase ? `${cleanAuthBase}/.well-known/jwks.json` : '')
).trim();

export const config = {
  // PostgreSQL / Neon database connection URL
  databaseUrl,

  // Neon Project ID (provided by Neon Vercel integration)
  neonProjectId: process.env.NEON_PROJECT_ID || '',

  // Neon Auth Base URL (provided by Neon Vercel integration as NEON_AUTH_BASE_URL)
  neonAuthBaseUrl,

  // Neon Auth JWKS Endpoint URL (derived automatically from NEON_AUTH_BASE_URL)
  neonAuthJwksUrl,

  // Master administrative secret key (sent in 'x-admin-key' header)
  adminKey: process.env.ADMIN_KEY || '',

  // Master administrative session token (for Bearer authentication)
  adminToken: process.env.ADMIN_TOKEN || '',

  // Primary platform administrator username
  adminUsername: (process.env.ADMIN_USERNAME || 'Cast_E').trim(),

  // Initial / recovery administrator password
  adminPassword: process.env.ADMIN_PASSWORD || '',

  // Administrator User ID in the database (optional stable UUID)
  adminUserId: (process.env.ADMIN_USER_ID || '').trim(),

  // Server port & environment mode
  port: process.env.PORT || 3000,
  isProduction: process.env.NODE_ENV === 'production' || !!process.env.VERCEL
};
