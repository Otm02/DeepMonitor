#!/usr/bin/env python3
"""
Local laptop ingest server for DeepMonitor field tests.

Receives alert/heartbeat JSON from the M5StickC Plus2 over Wi-Fi and
appends each payload to a per-device CSV under tools/out/captures/.

Usage:
  pip install -r tools/requirements.txt
  python tools/local_ingest.py
  python tools/local_ingest.py --host 0.0.0.0 --port 8000
"""

from __future__ import annotations

import argparse
import csv
import json
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, request

app = Flask(__name__)
OUTPUT_DIR = Path(__file__).parent / "out" / "captures"

CSV_FIELDS = [
    "received_at",
    "type",
    "device_id",
    "timestamp_ms",
    "reconstruction_error",
    "anomaly_threshold",
    "mean_vibration",
    "mean_temperature_delta",
    "battery_voltage",
    "uptime_cycles",
    "norm_mean_vib",
    "norm_var_vib",
    "norm_mean_temp",
    "norm_var_temp",
    "weight_magnitude",
    "wifi_fail_count",
    "http_retry_count",
    "cycle_reconstruction_error",
    "cycle_is_anomaly",
    "raw_json",
]


def _flatten(payload: dict) -> dict:
    return {
        "received_at": datetime.now(timezone.utc).isoformat(),
        "type": payload.get("type", ""),
        "device_id": payload.get("device_id", "unknown"),
        "timestamp_ms": payload.get("timestamp_ms", ""),
        "reconstruction_error": payload.get("reconstruction_error", ""),
        "anomaly_threshold": payload.get("anomaly_threshold", ""),
        "mean_vibration": payload.get("mean_vibration", ""),
        "mean_temperature_delta": payload.get("mean_temperature_delta", ""),
        "battery_voltage": payload.get("battery_voltage", ""),
        "uptime_cycles": payload.get("uptime_cycles", ""),
        "norm_mean_vib": payload.get("norm_mean_vib", ""),
        "norm_var_vib": payload.get("norm_var_vib", ""),
        "norm_mean_temp": payload.get("norm_mean_temp", ""),
        "norm_var_temp": payload.get("norm_var_temp", ""),
        "weight_magnitude": payload.get("weight_magnitude", ""),
        "wifi_fail_count": payload.get("wifi_fail_count", ""),
        "http_retry_count": payload.get("http_retry_count", ""),
        "cycle_reconstruction_error": payload.get("cycle_reconstruction_error", ""),
        "cycle_is_anomaly": payload.get("cycle_is_anomaly", ""),
        "raw_json": json.dumps(payload),
    }


def _append_csv(row: dict) -> Path:
    device_id = row.get("device_id") or "unknown"
    path = OUTPUT_DIR / f"{device_id}.csv"
    path.parent.mkdir(parents=True, exist_ok=True)
    write_header = not path.exists()
    with path.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS, extrasaction="ignore")
        if write_header:
            writer.writeheader()
        writer.writerow(row)
    return path


def _print_summary(row: dict) -> None:
    ts = datetime.now().strftime("%H:%M:%S")
    dtype = row.get("type", "?")
    dev = row.get("device_id", "?")
    batt = row.get("battery_voltage", "")
    vib = row.get("mean_vibration", "")
    recon = row.get("reconstruction_error", "")
    wifi_fail = row.get("wifi_fail_count", "")
    http_retry = row.get("http_retry_count", "")
    cycle_recon = row.get("cycle_reconstruction_error", "")
    cycle_anom = row.get("cycle_is_anomaly", "")
    parts = [f"[{ts}] {dev} {dtype}"]
    if batt != "":
        parts.append(f"batt={batt}V")
    if vib != "":
        parts.append(f"vib={vib}")
    if recon != "":
        parts.append(f"recon={recon}")
    if wifi_fail != "":
        parts.append(f"wifi_fail={wifi_fail}")
    if http_retry != "":
        parts.append(f"http_retry={http_retry}")
    if cycle_recon != "":
        parts.append(f"cycle_recon={cycle_recon}")
    if cycle_anom != "":
        parts.append(f"cycle_anom={cycle_anom}")
    print(" ".join(parts))


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "service": "deepmonitor-ingest"})


@app.route("/ingest", methods=["POST"])
def ingest():
    if not request.is_json:
        return jsonify({"error": "expected application/json"}), 400

    payload = request.get_json(silent=True)
    if not payload:
        return jsonify({"error": "empty body"}), 400

    row = _flatten(payload)
    path = _append_csv(row)
    _print_summary(row)
    return jsonify({"status": "ok", "file": str(path)}), 200


def main() -> None:
    parser = argparse.ArgumentParser(description="DeepMonitor local ingest server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"DeepMonitor ingest listening on http://{args.host}:{args.port}")
    print(f"  POST /ingest   — device payloads")
    print(f"  GET  /health   — liveness check")
    print(f"  CSV output:    {OUTPUT_DIR.resolve()}")
    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == "__main__":
    main()
