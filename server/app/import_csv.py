"""Import legacy capture CSVs (tools/out/captures/*.csv from local_ingest.py)
into the SQLite database.

Usage:
    python -m app.import_csv ../tools/out/captures
    python -m app.import_csv path/to/file.csv
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

from sqlmodel import Session

from .db import engine, init_db
from .ingest import ingest_payload


def import_file(path: Path, session: Session) -> int:
    count = 0
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            payload = {k: v for k, v in row.items() if v not in (None, "")}
            # Map legacy column names to the current ingest schema.
            if "type" not in payload:
                payload["type"] = "anomaly" if payload.get("is_anomaly") in ("1", "true", "True") else "heartbeat"
            ingest_payload(payload, session)
            count += 1
    return count


def import_path(target: Path) -> int:
    init_db()
    total = 0
    files = [target] if target.is_file() else sorted(target.glob("*.csv"))
    with Session(engine) as session:
        for f in files:
            n = import_file(f, session)
            print(f"  {f.name}: {n} rows")
            total += n
    return total


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 1
    target = Path(argv[1])
    if not target.exists():
        print(f"path not found: {target}")
        return 1
    total = import_path(target)
    print(f"Imported {total} rows.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
