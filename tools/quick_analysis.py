#!/usr/bin/env python3
"""
Post-test analysis for DeepMonitor capture CSVs.

Reads per-device CSV files from tools/out/captures/ and prints a
comparison table of vibration and reconstruction-error statistics.

Usage:
  python tools/quick_analysis.py
  python tools/quick_analysis.py --dir tools/out/captures
"""

from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path


def _to_float(val: str) -> float | None:
    if val is None or val == "":
        return None
    try:
        return float(val)
    except ValueError:
        return None


def analyze_file(path: Path) -> dict:
    rows = list(csv.DictReader(path.open(encoding="utf-8")))
    vibs = [_to_float(r.get("mean_vibration", "")) for r in rows]
    recons = [_to_float(r.get("reconstruction_error", "")) for r in rows]
    batts = [_to_float(r.get("battery_voltage", "")) for r in rows]

    vibs = [v for v in vibs if v is not None]
    recons = [v for v in recons if v is not None]
    batts = [v for v in batts if v is not None]

    def stats(vals: list[float]) -> tuple[float, float, float]:
        if not vals:
            return (float("nan"), float("nan"), float("nan"))
        return (
            statistics.mean(vals),
            statistics.stdev(vals) if len(vals) > 1 else 0.0,
            sorted(vals)[int(0.95 * (len(vals) - 1))],
        )

    v_mean, v_std, _ = stats(vibs)
    r_mean, r_std, r_p95 = stats(recons)
    b_mean, _, _ = stats(batts)

    anomalies = sum(1 for r in rows if r.get("type") == "anomaly")

    return {
        "device_id": path.stem,
        "rows": len(rows),
        "anomalies": anomalies,
        "vib_mean": v_mean,
        "vib_std": v_std,
        "recon_mean": r_mean,
        "recon_std": r_std,
        "recon_p95": r_p95,
        "batt_mean": b_mean,
    }


def print_table(results: list[dict]) -> None:
    if not results:
        print("No capture files found.")
        return

    print(f"\n{'='*90}")
    print("  DeepMonitor capture summary")
    print(f"{'='*90}")
    hdr = (
        f"{'device':<16} {'rows':>5} {'anom':>5} "
        f"{'vib_mean':>9} {'vib_std':>8} "
        f"{'recon_mu':>9} {'recon_p95':>10} {'batt':>6}"
    )
    print(hdr)
    print("-" * len(hdr))
    for r in results:
        print(
            f"{r['device_id']:<16} {r['rows']:>5} {r['anomalies']:>5} "
            f"{r['vib_mean']:>9.4f} {r['vib_std']:>8.4f} "
            f"{r['recon_mean']:>9.4f} {r['recon_p95']:>10.4f} "
            f"{r['batt_mean']:>6.2f}"
        )
    print()


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze DeepMonitor capture CSVs")
    parser.add_argument(
        "--dir",
        type=Path,
        default=Path(__file__).parent / "out" / "captures",
        help="Directory containing per-device CSV files",
    )
    args = parser.parse_args()

    if not args.dir.exists():
        print(f"Directory not found: {args.dir}")
        print("Run local_ingest.py during a test session first.")
        return

    csv_files = sorted(args.dir.glob("*.csv"))
    results = [analyze_file(f) for f in csv_files]
    print_table(results)


if __name__ == "__main__":
    main()
