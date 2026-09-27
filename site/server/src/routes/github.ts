import { Router } from 'express';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export const githubRouter = Router();

const GITHUB_REPO = 'cast-e/Sand3';
const LOCAL_README_PATH = path.resolve(__dirname, '../../../../README.md');

interface CacheEntry<T> {
  data: T;
  timestamp: number;
}

const cache = new Map<string, CacheEntry<any>>();

function getCached<T>(key: string, ttlMs: number): T | null {
  const entry = cache.get(key);
  if (!entry) return null;
  if (Date.now() - entry.timestamp > ttlMs) {
    cache.delete(key);
    return null;
  }
  return entry.data as T;
}

function setCached<T>(key: string, data: T): void {
  cache.set(key, { data, timestamp: Date.now() });
}

githubRouter.get('/latest-release', async (req, res) => {
  const cacheKey = 'github:latest-release';
  const cached = getCached(cacheKey, 5 * 60 * 1000); if (cached) {
    return res.json(cached);
  }

  try {
    const response = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases`, {
      headers: { 'User-Agent': 'Sand3-Web-Server' }
    });

    if (!response.ok) {
      throw new Error(`GitHub API returned status ${response.status}`);
    }

    const releases: any[] = await response.json();
    if (!releases || releases.length === 0) {
      return res.status(404).json({ error: 'No releases found' });
    }

    const latest = releases[0];
    const result = {
      tag_name: latest.tag_name,
      name: latest.name || latest.tag_name,
      prerelease: latest.prerelease,
      published_at: latest.published_at,
      html_url: latest.html_url,
      body: latest.body || '',
      assets: (latest.assets || []).map((a: any) => ({
        name: a.name,
        browser_download_url: a.browser_download_url,
        size: a.size,
        download_count: a.download_count,
        created_at: a.created_at
      }))
    };

    setCached(cacheKey, result);
    res.json(result);
  } catch (err: any) {
    console.error('Error fetching latest release from GitHub:', err.message);
    res.json({
      tag_name: 'v1.0.0-alpha.7',
      name: 'Mostly fixes but some QoL',
      prerelease: true,
      published_at: new Date().toISOString(),
      html_url: `https://github.com/${GITHUB_REPO}/releases/tag/v1.0.0-alpha.7`,
      body: 'Most changes are fixes to some degree, but a bunch of quality of life features too!\n\n**Full Changelog**: https://github.com/cast-e/Sand3/compare/v1.0.0-alpha.6...v1.0.0-alpha.7',
      assets: [
        {
          name: 'sand3-v1.0.0-alpha.7-linux.zip',
          browser_download_url: `https://github.com/${GITHUB_REPO}/releases/download/v1.0.0-alpha.7/sand3-v1.0.0-alpha.7-linux.zip`,
          size: 3499574
        },
        {
          name: 'sand3-v1.0.0-alpha.7-win64.zip',
          browser_download_url: `https://github.com/${GITHUB_REPO}/releases/download/v1.0.0-alpha.7/sand3-v1.0.0-alpha.7-win64.zip`,
          size: 8552290
        }
      ]
    });
  }
});

githubRouter.get('/releases', async (req, res) => {
  const cacheKey = 'github:all-releases';
  const cached = getCached(cacheKey, 10 * 60 * 1000); if (cached) {
    return res.json(cached);
  }

  try {
    const response = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases`, {
      headers: { 'User-Agent': 'Sand3-Web-Server' }
    });

    if (!response.ok) {
      throw new Error(`GitHub API returned status ${response.status}`);
    }

    const releases: any[] = await response.json();
    const result = releases.map((r: any) => ({
      tag_name: r.tag_name,
      name: r.name || r.tag_name,
      prerelease: r.prerelease,
      published_at: r.published_at,
      html_url: r.html_url,
      body: r.body || '',
      assets: (r.assets || []).map((a: any) => ({
        name: a.name,
        browser_download_url: a.browser_download_url,
        size: a.size,
        download_count: a.download_count
      }))
    }));

    setCached(cacheKey, result);
    res.json(result);
  } catch (err: any) {
    console.error('Error fetching releases from GitHub:', err.message);
    res.json([]);
  }
});

githubRouter.get('/readme', async (req, res) => {
  const cacheKey = 'github:readme';
  const cached = getCached(cacheKey, 15 * 60 * 1000); if (cached) {
    return res.json({ content: cached, source: 'cache' });
  }

  try {
    const response = await fetch(`https://raw.githubusercontent.com/${GITHUB_REPO}/main/README.md`);
    if (response.ok) {
      const content = await response.text();
      setCached(cacheKey, content);
      return res.json({ content, source: 'github' });
    }
  } catch (err: any) {
    console.warn('Could not fetch raw README from GitHub, falling back to local file:', err.message);
  }

  if (fs.existsSync(LOCAL_README_PATH)) {
    const localContent = fs.readFileSync(LOCAL_README_PATH, 'utf-8');
    setCached(cacheKey, localContent);
    return res.json({ content: localContent, source: 'local' });
  }

  res.status(404).json({ error: 'README not found' });
});

githubRouter.get('/instructions', (req, res) => {
  res.json({
    linux: {
      prerequisites: 'Vulkan drivers (Mesa / NVIDIA / AMD), ALSA or PulseAudio',
      steps: [
        'Download and extract `sand3-v*-linux.zip`',
        'Open terminal in the extracted directory',
        'Make executable: `chmod +x sand3`',
        'Run: `./sand3`'
      ]
    },
    windows: {
      prerequisites: 'Windows 10/11 64-bit, Vulkan-compatible GPU drivers',
      steps: [
        'Download and extract `sand3-v*-win64.zip`',
        'Open the extracted folder',
        'Double-click `sand3.exe` to run'
      ]
    }
  });
});
