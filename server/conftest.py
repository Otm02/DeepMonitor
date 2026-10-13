"""Pytest setup: isolate the DB to a temp file and expose a clean TestClient."""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

import pytest

# Make the `app` package importable and point the DB at a temp file BEFORE the
# app (and thus db.py) is imported anywhere.
sys.path.insert(0, str(Path(__file__).resolve().parent))

_DB_PATH = Path(tempfile.gettempdir()) / "deepmonitor_pytest.db"
os.environ["DEEPMONITOR_DB_URL"] = f"sqlite:///{_DB_PATH.as_posix()}"


@pytest.fixture
def client():
    from fastapi.testclient import TestClient
    from sqlmodel import SQLModel

    from app.db import engine, init_db
    from app.main import app

    SQLModel.metadata.drop_all(engine)
    init_db()
    with TestClient(app) as c:
        yield c
