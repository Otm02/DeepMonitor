"""DeepMonitor server entrypoint: REST API + ingest + bundled SPA."""

from __future__ import annotations

from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

from . import __version__
from .api import router as api_router
from .db import init_db
from .ingest import router as ingest_router

FRONTEND_DIST = Path(__file__).resolve().parent.parent.parent / "frontend" / "dist"


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


app = FastAPI(title="DeepMonitor", version=__version__, lifespan=lifespan)

# CORS so the Vite dev server (5173) can call the API during development.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(ingest_router)
app.include_router(api_router)


@app.get("/health")
def health() -> dict:
    return {"status": "ok", "version": __version__}


# ── Static SPA (served when frontend/dist exists) ─────────────────────────────
if FRONTEND_DIST.exists():
    assets_dir = FRONTEND_DIST / "assets"
    if assets_dir.exists():
        app.mount("/assets", StaticFiles(directory=assets_dir), name="assets")

    @app.get("/")
    def spa_index() -> FileResponse:
        return FileResponse(FRONTEND_DIST / "index.html")

    @app.get("/{full_path:path}")
    def spa_catch_all(full_path: str):
        # Never shadow API/ingest routes.
        if full_path.startswith(("api/", "ingest", "health")):
            return JSONResponse({"detail": "not found"}, status_code=404)
        candidate = FRONTEND_DIST / full_path
        if candidate.is_file():
            return FileResponse(candidate)
        return FileResponse(FRONTEND_DIST / "index.html")
else:
    @app.get("/")
    def placeholder() -> dict:
        return {
            "status": "ok",
            "message": "DeepMonitor API is running. Build the frontend (frontend/dist) to serve the dashboard.",
            "docs": "/docs",
        }
