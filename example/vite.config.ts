import path from 'path';
import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

const rootDir = __dirname;
const packageRoot = path.resolve(rootDir, '..');

export default defineConfig({
  root: path.resolve(rootDir, 'web'),
  plugins: [react()],
  resolve: {
    alias: {
      'react-native': 'react-native-web',
      '@op-engineering/op-sqlite': path.resolve(packageRoot, 'src/index.web.ts'),
    },
    extensions: ['.web.tsx', '.web.ts', '.tsx', '.ts', '.jsx', '.js', '.mjs', '.json'],
  },
  server: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
    fs: {
      allow: [packageRoot],
    },
  },
  optimizeDeps: {
    exclude: ['@op-engineering/op-sqlite'],
  },
  build: {
    outDir: path.resolve(rootDir, 'web-build'),
    emptyOutDir: true,
  },
});
