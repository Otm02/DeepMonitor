"""Database engine and session management (SQLite via SQLModel)."""

from __future__ import annotations

import os
from pathlib import Path

from sqlmodel import SQLModel, Session, create_engine

# Storage location. Override with DEEPMONITOR_DB_URL (e.g. for tests).
DATA_DIR = Path(__file__).resolve().parent.parent / "data"
DATA_DIR.mkdir(parents=True, exist_ok=True)
_DEFAULT_URL = f"sqlite:///{(DATA_DIR / 'deepmonitor.db').as_posix()}"

DB_URL = os.environ.get("DEEPMONITOR_DB_URL", _DEFAULT_URL)

engine = create_engine(DB_URL, echo=False, connect_args={"check_same_thread": False})


def init_db() -> None:
    # Import models so they register on SQLModel.metadata before create_all.
    from . import models  # noqa: F401
    SQLModel.metadata.create_all(engine)


def get_session():
    with Session(engine) as session:
        yield session
