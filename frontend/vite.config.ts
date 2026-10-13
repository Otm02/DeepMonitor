import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// During development the dashboard runs on :5173 and proxies API calls to the
// FastAPI server on :8000. In production the built assets are served directly
// by FastAPI from the same origin, so the relative "/api" paths just work.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": "http://localhost:8000",
      "/ingest": "http://localhost:8000",
      "/health": "http://localhost:8000",
    },
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
  },
});
