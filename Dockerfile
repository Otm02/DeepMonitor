# ── Stage 1: build the React dashboard ──────────────────────────────────────
FROM node:22-alpine AS frontend
WORKDIR /frontend
COPY frontend/package.json frontend/package-lock.json* ./
RUN npm install
COPY frontend/ ./
RUN npm run build

# ── Stage 2: Python runtime serving API + bundled SPA ───────────────────────
FROM python:3.12-slim AS runtime
WORKDIR /app

# Install backend deps first for better layer caching.
COPY server/requirements.txt /app/server/requirements.txt
RUN pip install --no-cache-dir -r /app/server/requirements.txt

COPY server/ /app/server/
# Place the built dashboard where FastAPI looks for it (../../frontend/dist).
COPY --from=frontend /frontend/dist /app/frontend/dist

WORKDIR /app/server
ENV DEEPMONITOR_DB_URL="sqlite:////app/server/data/deepmonitor.db"
EXPOSE 8000

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
