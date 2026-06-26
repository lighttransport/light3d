import { defineConfig } from 'vite';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { cpSync, existsSync } from 'node:fs';

const root = dirname(fileURLToPath(import.meta.url));

// The emscripten module (lightusd.js), its .wasm, the USD/glTF sample assets, and the
// model dir are loaded at RUNTIME (script-tag injection + fetch), not statically imported,
// so Rollup doesn't see them. Copy them into dist/ on build so `vite build`/`preview` work.
function copyRuntimeAssets() {
  const assets = ['lightusd.js', 'lightusd.wasm', 'models'];
  return {
    name: 'copy-runtime-assets',
    apply: 'build',
    closeBundle() {
      for (const a of assets) {
        const src = resolve(root, a);
        if (existsSync(src)) cpSync(src, resolve(root, 'dist', a), { recursive: true, dereference: true });
      }
    },
  };
}

export default defineConfig({
  root,
  base: './',
  // Treat USD/glTF binaries as assets (don't try to parse them as JS).
  assetsInclude: ['**/*.usdc', '**/*.usda', '**/*.usdz', '**/*.glb', '**/*.gltf'],
  server: {
    port: 8000,
    open: false,           // open http://localhost:8000/harness.html (or /index.html) manually
    fs: { allow: [root, resolve(root, '../..')] },
  },
  build: {
    target: 'esnext',
    rollupOptions: {
      input: {
        index: resolve(root, 'index.html'),
        harness: resolve(root, 'harness.html'),
        tests: resolve(root, 'tests/webgpu-feature-tests.html'),
      },
    },
  },
  plugins: [copyRuntimeAssets()],
});
