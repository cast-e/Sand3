let app: any;
try {
  const mod = await import('../server/dist/index.js');
  app = mod.default || mod.app || mod;
} catch {
  // @ts-ignore
  const mod = await import('../server/src/index.js');
  app = mod.default || mod.app || mod;
}

export default app;
