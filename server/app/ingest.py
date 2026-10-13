"""Device ingest endpoint. Backwards-compatible with the firmware webhook:
accepts the same JSON the M5StickC posts to /ingest, but persists to SQLite
(devices, readings, anomalies) instead of CSV."""

from __future__ import annotations

import json
from datetime import datetime
from typing import Any, Optional

from fastapi import APIRouter, Depends, Request
from sqlmodel import Session, select

from .db import get_session
from .models import Anomaly, Device, Reading, utcnow

router = APIRouter()


def _num(payload: dict, *keys) -> Optional[float]:
    for k in keys:
        if k in payload and payload[k] not in (None, ""):
            try:
                return float(payload[k])
            except (TypeError, ValueError):
                continue
    return None


def _int(payload: dict, *keys) -> Optional[int]:
    v = _num(payload, *keys)
    return int(v) if v is not None else None


def _severity(recon: Optional[float], threshold: Optional[float]) -> str:
    if recon is None or not threshold:
        return "low"
    ratio = recon / threshold if threshold else 0.0
    if ratio >= 3.0:
        return "high"
    if ratio >= 1.5:
        return "medium"
    return "low"


def ingest_payload(payload: dict[str, Any], session: Session) -> dict:
    """Persist one device payload. Returns a small status dict."""
    device_id = str(payload.get("device_id") or "unknown")
    ptype = str(payload.get("type") or "heartbeat")
    now = utcnow()

    recon = _num(payload, "reconstruction_error", "cycle_reconstruction_error")
    threshold = _num(payload, "anomaly_threshold")
    is_anomaly = 1 if (ptype == "anomaly" or _int(payload, "is_anomaly", "cycle_is_anomaly")) else 0
    battery = _num(payload, "battery_voltage")
    uptime = _int(payload, "uptime_cycles")

    features = payload.get("features")
    features_json = json.dumps(features) if isinstance(features, list) else None

    reading = Reading(
        device_id=device_id,
        received_at=now,
        type=ptype,
        timestamp_ms=_int(payload, "timestamp_ms"),
        reconstruction_error=recon,
        anomaly_threshold=threshold,
        is_anomaly=is_anomaly,
        battery_voltage=battery,
        uptime_cycles=uptime,
        mean_vibration=_num(payload, "mean_vibration"),
        mean_temperature_delta=_num(payload, "mean_temperature_delta"),
        weight_magnitude=_num(payload, "weight_magnitude"),
        wifi_fail_count=_int(payload, "wifi_fail_count"),
        http_retry_count=_int(payload, "http_retry_count"),
        config_version=_int(payload, "config_version"),
        sleep_duration_us=_int(payload, "sleep_duration_us"),
        heartbeat_interval=_int(payload, "heartbeat_interval"),
        sample_rate_hz=_int(payload, "sample_rate_hz"),
        features_json=features_json,
        raw_json=json.dumps(payload),
    )
    session.add(reading)

    device = session.get(Device, device_id)
    if device is None:
        device = Device(device_id=device_id)
        session.add(device)

    device.last_seen = now
    if battery is not None:
        device.last_battery = battery
    if recon is not None:
        device.last_recon_error = recon
    if threshold is not None:
        device.last_threshold = threshold
    device.last_is_anomaly = is_anomaly
    if uptime is not None:
        device.uptime_cycles = uptime
    wf = _int(payload, "wifi_fail_count")
    if wf is not None:
        device.wifi_fail_count = wf
    hr = _int(payload, "http_retry_count")
    if hr is not None:
        device.http_retry_count = hr
    applied = _int(payload, "config_version")
    if applied is not None:
        device.applied_config_version = applied

    session.commit()
    session.refresh(reading)

    anomaly_id = None
    if is_anomaly:
        anomaly = Anomaly(
            device_id=device_id,
            reading_id=reading.id,
            created_at=now,
            reconstruction_error=recon,
            anomaly_threshold=threshold,
            severity=_severity(recon, threshold),
        )
        session.add(anomaly)
        session.commit()
        session.refresh(anomaly)
        anomaly_id = anomaly.id

    return {"status": "ok", "reading_id": reading.id, "anomaly_id": anomaly_id}


@router.post("/ingest")
@router.post("/api/ingest")
async def ingest(request: Request, session: Session = Depends(get_session)) -> dict:
    payload = await request.json()
    if not isinstance(payload, dict):
        return {"status": "error", "detail": "expected a JSON object"}
    return ingest_payload(payload, session)
