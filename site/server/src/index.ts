import express from 'express';
import cors from 'cors';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { githubRouter } from './routes/github.js';
import { itemsRouter } from './routes/items.js';
import { interactionsRouter } from './routes/interactions.js';
import { downloadsRouter } from './routes/downloads.js';

import { authRouter } from './routes/auth.js';
import { adminRouter } from './routes/admin.js';
import { usersRouter } from './routes/users.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = process.env.PORT || 3000;

// Security Headers & CORS
app.use((_req, res, next) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('X-Frame-Options', 'SAMEORIGIN');
  res.setHeader('Referrer-Policy', 'strict-origin-when-cross-origin');
  res.removeHeader('X-Powered-By');
  next();
});

app.use(cors());

// Parse JSON & URL-encoded bodies with 25MB limits
app.use(express.json({ limit: '25mb' }));
app.use(express.urlencoded({ extended: true, limit: '25mb' }));

// API Router
const apiRouter = express.Router();

// Health Check
apiRouter.get('/workshop/health', (_req, res) => {
  res.json({
    status: 'ok',
    version: '1.0.0',
    service: 'Sand3 Workshop API',
    uptime: process.uptime(),
    timestamp: new Date().toISOString()
  });
});

// API Routes
apiRouter.use('/github', githubRouter);
apiRouter.use('/workshop/auth', authRouter);
apiRouter.use('/workshop/admin', adminRouter);
apiRouter.use('/workshop/users', usersRouter);
apiRouter.use('/workshop/items', itemsRouter);
apiRouter.use('/workshop/items', interactionsRouter);
apiRouter.use('/workshop/items', downloadsRouter);

app.use('/api', apiRouter);
app.use(apiRouter);

// Static uploads directory (for previews/attachments)
const UPLOADS_DIR = process.env.UPLOADS_DIR || (process.env.VERCEL ? '/tmp/sand3-uploads' : path.resolve(__dirname, '../uploads'));
if (!fs.existsSync(UPLOADS_DIR)) {
  fs.mkdirSync(UPLOADS_DIR, { recursive: true });
}
app.use('/uploads', express.static(UPLOADS_DIR));
app.use('/api/uploads', express.static(UPLOADS_DIR));

// Serve Angular static frontend build if present
const DIST_DIR = path.resolve(__dirname, '../../dist/sand3-site/browser');
if (fs.existsSync(DIST_DIR)) {
  app.use(express.static(DIST_DIR));
  app.use((req, res, next) => {
    if (req.method !== 'GET') return next();
    if (req.path.startsWith('/api') || req.path.startsWith('/uploads')) {
      return next();
    }
    res.sendFile(path.join(DIST_DIR, 'index.html'));
  });
}

export { app };
export default app;

if (!process.env.VERCEL && process.env.NODE_ENV !== 'test') {
  app.listen(PORT, () => {
    console.log(`[Sand3 Workshop & Portal] Server running on http://localhost:${PORT}`);
  });
}
