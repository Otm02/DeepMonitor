#!/usr/bin/env python3
"""DeepMonitor device simulator.

Generates realistic 24-dim feature payloads (FFT bands + time/freq stats) and
POSTs them to the server's /ingest endpoint, occasionally injecting an anomaly.
Also exercises the control plane by pulling the device config and echoing the
applied config_version back in heartbeats. Pure stdlib (urllib) — no deps.

Usage:
    python tools/device_sim.py --url http://localhost:8000 --device-id M5-SIM1 --cycles 30 --interval 0.2
"""

from __future__ import annotations

import argparse
import json
import math
import random
import time
import urllib.error
import urllib.request

FEATURE_DIM = 24
BAND_START = 6
FREQ_BANDS = 14


def normal_features(t: int) -> list[float]:
    """A healthy machine: low broadband vibration with a stable dominant tone."""
    rms = 0.02 + 0.002 * random.random()
    peak = rms * (3.0 + 0.3 * random.random())
    crest = peak / rms
    kurtosis = 3.0 + 0.2 * random.random()
    std = rms
    p2p = 2.0 * peak
    bands = [0.05 + 0.02 * random.random() for _ in range(FREQ_BANDS)]
    bands[2] += 0.6 + 0.05 * math.sin(t / 5.0)  # stable dominant band
    centroid = 0.25 + 0.01 * random.random()
    dominant = 3.0 / (FREQ_BANDS)
    total_energy = math.log1p(sum(bands))
    temp = 25.0 + 2.0 * math.sin(t / 20.0)
    return [rms, peak, crest, kurtosis, std, p2p, *bands, centroid, dominant, total_energy, temp]


def anomaly_features(t: int) -> list[float]:
    """A fault: elevated broadband energy, high crest factor / kurtosis."""
    f = normal_features(t)
    f[0] *= 4.0          # rms
    f[1] *= 5.0          # peak
    f[2] *= 1.8          # crest
    f[3] += 6.0          # kurtosis (impulsive)
    for b in range(FREQ_BANDS):
        f[BAND_START + b] += 1.5 + random.random()
    f[BAND_START + FREQ_BANDS + 2] = math.log1p(sum(f[BAND_START:BAND_START + FREQ_BANDS]))
    return f


def post(url: str, payload: dict) -> dict:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


def get_config(base: str, device_id: str) -> dict:
    url = f"{base}/api/devices/{device_id}/config"
    try:
        with urllib.request.urlopen(url, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError:
        return {"config_version": 0}


def main() -> int:
    ap = argparse.ArgumentParser(description="DeepMonitor device simulator")
    ap.add_argument("--url", default="http://localhost:8000")
    ap.add_argument("--device-id", default="M5-SIM1")
    ap.add_argument("--cycles", type=int, default=30)
    ap.add_argument("--interval", type=float, default=0.2)
    ap.add_argument("--anomaly-every", type=int, default=12, help="inject an anomaly every N cycles (0 = never)")
    args = ap.parse_args()

    ingest = f"{args.url}/ingest"
    threshold = 0.1
    anomalies = 0

    for t in range(1, args.cycles + 1):
        is_anom = args.anomaly_every > 0 and t % args.anomaly_every == 0
        feats = anomaly_features(t) if is_anom else normal_features(t)
        recon = round(random.uniform(0.3, 0.9) if is_anom else random.uniform(0.01, 0.05), 5)
        battery = round(4.1 - 0.0003 * t, 3)
        cfg = get_config(args.url, args.device_id)

        heartbeat = {
            "type": "heartbeat",
            "device_id": args.device_id,
            "uptime_cycles": t,
            "battery_voltage": battery,
            "anomaly_threshold": threshold,
            "reconstruction_error": recon,
            "is_anomaly": 1 if is_anom else 0,
            "weight_magnitude": round(1.0 + 0.01 * t, 4),
            "wifi_fail_count": 0,
            "http_retry_count": 0,
            "config_version": cfg.get("config_version", 0),
            "sleep_duration_us": cfg.get("sleep_duration_us", 900000000),
            "heartbeat_interval": cfg.get("heartbeat_interval", 96),
            "sample_rate_hz": cfg.get("sample_rate_hz", 200),
            "features": [round(x, 5) for x in feats],
        }
        post(ingest, heartbeat)

        if is_anom:
            anomalies += 1
            post(ingest, {
                "type": "anomaly",
                "device_id": args.device_id,
                "timestamp_ms": t * 1000,
                "reconstruction_error": recon,
                "anomaly_threshold": threshold,
                "mean_vibration": round(feats[0], 5),
                "mean_temperature_delta": round(feats[-1] - 25.0, 3),
                "battery_voltage": battery,
                "features": [round(x, 5) for x in feats],
            })
        print(f"cycle {t:3d}  recon={recon:.4f}  {'ANOMALY' if is_anom else 'normal'}")
        time.sleep(args.interval)

    print(f"Done. Sent {args.cycles} cycles ({anomalies} anomalies) for {args.device_id}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
