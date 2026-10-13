"""REST API: devices, readings, anomalies, control-plane config, reports,
and the battery trade-off curve. All under /api."""

from __future__ import annotations

import csv
import io
import json
import math
from datetime import datetime, timedelta
from typing import Optional

from fastapi import APIRouter, Depends, HTTPException, Query
from fastapi.responses import Response
from pydantic import BaseModel
from sqlmodel import Session, select

from .db import get_session
from .models import Anomaly, Device, Reading, utcnow

router = APIRouter(prefix="/api")


# ── Serialization helpers ────────────────────────────────────────────────────

def _device_status(device: Device) -> str:
    if device.last_seen is None:
        return "unknown"
    period_s = (device.sleep_duration_us / 1_000_000.0) * max(device.heartbeat_interval, 1)
    # Allow generous slack (anomaly posts can be sparse; heartbeats ~daily).
    deadline = timedelta(seconds=max(period_s * 2.5, 3600))
    online = (utcnow() - device.last_seen) <= deadline
    if not online:
        return "offline"
    return "anomaly" if device.last_is_anomaly else "online"


def _device_dict(device: Device) -> dict:
    return {
        "device_id": device.device_id,
        "name": device.name or device.device_id,
        "status": _device_status(device),
        "created_at": device.created_at.isoformat() if device.created_at else None,
        "last_seen": device.last_seen.isoformat() if device.last_seen else None,
        "last_battery": device.last_battery,
        "last_recon_error": device.last_recon_error,
        "last_threshold": device.last_threshold,
        "last_is_anomaly": bool(device.last_is_anomaly),
        "uptime_cycles": device.uptime_cycles,
        "wifi_fail_count": device.wifi_fail_count,
        "http_retry_count": device.http_retry_count,
        "config": _config_dict(device),
        "config_synced": device.config_version == device.applied_config_version,
    }


def _config_dict(device: Device) -> dict:
    return {
        "config_version": device.config_version,
        "applied_config_version": device.applied_config_version,
        "sleep_duration_us": device.sleep_duration_us,
        "heartbeat_interval": device.heartbeat_interval,
        "sample_rate_hz": device.sample_rate_hz,
        "anomaly_k": device.anomaly_k,
    }


def _reading_dict(r: Reading) -> dict:
    return {
        "id": r.id,
        "device_id": r.device_id,
        "received_at": r.received_at.isoformat() if r.received_at else None,
        "type": r.type,
        "timestamp_ms": r.timestamp_ms,
        "reconstruction_error": r.reconstruction_error,
        "anomaly_threshold": r.anomaly_threshold,
        "is_anomaly": bool(r.is_anomaly),
        "battery_voltage": r.battery_voltage,
        "uptime_cycles": r.uptime_cycles,
        "mean_vibration": r.mean_vibration,
        "mean_temperature_delta": r.mean_temperature_delta,
        "weight_magnitude": r.weight_magnitude,
        "wifi_fail_count": r.wifi_fail_count,
        "http_retry_count": r.http_retry_count,
        "config_version": r.config_version,
        "sample_rate_hz": r.sample_rate_hz,
        "features": json.loads(r.features_json) if r.features_json else None,
    }


def _parse_dt(value: Optional[str]) -> Optional[datetime]:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).replace(tzinfo=None)
    except ValueError:
        raise HTTPException(status_code=400, detail=f"invalid datetime: {value}")


# ── Devices ──────────────────────────────────────────────────────────────────

@router.get("/devices")
def list_devices(session: Session = Depends(get_session)) -> list[dict]:
    devices = session.exec(select(Device).order_by(Device.device_id)).all()
    return [_device_dict(d) for d in devices]


@router.get("/devices/{device_id}")
def get_device(device_id: str, session: Session = Depends(get_session)) -> dict:
    device = session.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    return _device_dict(device)


class DeviceRename(BaseModel):
    name: str


@router.patch("/devices/{device_id}")
def rename_device(device_id: str, body: DeviceRename, session: Session = Depends(get_session)) -> dict:
    device = session.get(Device, device_id)
    if device is None:
        raise HTTPException(status_code=404, detail="device not found")
    device.name = body.name
    session.add(device)
    session.commit()
    session.refresh(device)
    return _device_dict(device)


# ── Readings (time-series) ───────────────────────────────────────────────────

@router.get("/devices/{device_id}/readings")
def get_readings(
    device_id: str,
    since: Optional[str] = None,
    until: Optional[str] = None,
    type: Optional[str] = None,
    limit: int = Query(1000, le=10000),
    session: Session = Depends(get_session),
) -> list[dict]:
    stmt = select(Reading).where(Reading.device_id == device_id)
    dt_since = _parse_dt(since)
    dt_until = _parse_dt(until)
    if dt_since:
        stmt = stmt.where(Reading.received_at >= dt_since)
    if dt_until:
        stmt = stmt.where(Reading.received_at <= dt_until)
    if type:
        stmt = stmt.where(Reading.type == type)
    stmt = stmt.order_by(Reading.received_at.desc()).limit(limit)
    rows = session.exec(stmt).all()
    rows.reverse()  # chronological for charting
    return [_reading_dict(r) for r in rows]


# ── Control plane: device config ─────────────────────────────────────────────

class ConfigUpdate(BaseModel):
    sleep_duration_us: Optional[int] = None
    heartbeat_interval: Optional[int] = None
    sample_rate_hz: Optional[int] = None
    anomaly_k: Optional[float] = None


@router.get("/devices/{device_id}/config")
def get_config(device_id: str, session: Session = Depends(get_session)) -> dict:
    """Document the device pulls during a Wi-Fi session (parse_config_json)."""
    device = session.get(Device, device_id)
    if device is None:
        # Auto-register unknown devices so the first pull works.
        device = Device(device_id=device_id)
        session.add(device)
        session.commit()
        session.refresh(device)
    return {
        "config_version": device.config_version,
        "sleep_duration_us": device.sleep_duration_us,
        "heartbeat_interval": device.heartbeat_interval,
        "sample_rate_hz": device.sample_rate_hz,
        "anomaly_k": device.anomaly_k,
    }


@router.put("/devices/{device_id}/config")
def update_config(device_id: str, body: ConfigUpdate, session: Session = Depends(get_session)) -> dict:
    device = session.get(Device, device_id)
    if device is None:
        device = Device(device_id=device_id)
        session.add(device)
    changed = False
    for field in ("sleep_duration_us", "heartbeat_interval", "sample_rate_hz", "anomaly_k"):
        value = getattr(body, field)
        if value is not None and value != getattr(device, field):
            setattr(device, field, value)
            changed = True
    if changed:
        device.config_version += 1
    session.add(device)
    session.commit()
    session.refresh(device)
    return _config_dict(device)


# ── Anomalies ────────────────────────────────────────────────────────────────

@router.get("/anomalies")
def list_anomalies(
    device_id: Optional[str] = None,
    acknowledged: Optional[bool] = None,
    limit: int = Query(200, le=5000),
    session: Session = Depends(get_session),
) -> list[dict]:
    stmt = select(Anomaly)
    if device_id:
        stmt = stmt.where(Anomaly.device_id == device_id)
    if acknowledged is not None:
        stmt = stmt.where(Anomaly.acknowledged == (1 if acknowledged else 0))
    stmt = stmt.order_by(Anomaly.created_at.desc()).limit(limit)
    rows = session.exec(stmt).all()
    return [
        {
            "id": a.id,
            "device_id": a.device_id,
            "reading_id": a.reading_id,
            "created_at": a.created_at.isoformat() if a.created_at else None,
            "reconstruction_error": a.reconstruction_error,
            "anomaly_threshold": a.anomaly_threshold,
            "severity": a.severity,
            "acknowledged": bool(a.acknowledged),
            "note": a.note,
        }
        for a in rows
    ]


class AckBody(BaseModel):
    note: Optional[str] = None


@router.post("/anomalies/{anomaly_id}/ack")
def ack_anomaly(anomaly_id: int, body: AckBody, session: Session = Depends(get_session)) -> dict:
    anomaly = session.get(Anomaly, anomaly_id)
    if anomaly is None:
        raise HTTPException(status_code=404, detail="anomaly not found")
    anomaly.acknowledged = 1
    if body.note is not None:
        anomaly.note = body.note
    session.add(anomaly)
    session.commit()
    return {"status": "ok", "id": anomaly_id}


# ── Reports ──────────────────────────────────────────────────────────────────

@router.get("/reports/summary")
def report_summary(
    device_id: Optional[str] = None,
    since: Optional[str] = None,
    until: Optional[str] = None,
    session: Session = Depends(get_session),
) -> dict:
    stmt = select(Reading)
    if device_id:
        stmt = stmt.where(Reading.device_id == device_id)
    dt_since = _parse_dt(since)
    dt_until = _parse_dt(until)
    if dt_since:
        stmt = stmt.where(Reading.received_at >= dt_since)
    if dt_until:
        stmt = stmt.where(Reading.received_at <= dt_until)
    rows = session.exec(stmt).all()

    recons = sorted(r.reconstruction_error for r in rows if r.reconstruction_error is not None)
    batts = [r.battery_voltage for r in rows if r.battery_voltage is not None]
    anomalies = sum(1 for r in rows if r.is_anomaly)

    def pct(vals: list[float], p: float) -> Optional[float]:
        if not vals:
            return None
        idx = min(len(vals) - 1, int(p * (len(vals) - 1)))
        return vals[idx]

    return {
        "device_id": device_id,
        "total_readings": len(rows),
        "anomaly_count": anomalies,
        "anomaly_rate": (anomalies / len(rows)) if rows else 0.0,
        "recon_mean": (sum(recons) / len(recons)) if recons else None,
        "recon_p95": pct(recons, 0.95),
        "recon_max": recons[-1] if recons else None,
        "battery_last": batts[-1] if batts else None,
        "battery_min": min(batts) if batts else None,
        "first_seen": min((r.received_at for r in rows), default=None).isoformat() if rows else None,
        "last_seen": max((r.received_at for r in rows), default=None).isoformat() if rows else None,
    }


@router.get("/reports/export.csv")
def export_csv(
    device_id: Optional[str] = None,
    since: Optional[str] = None,
    until: Optional[str] = None,
    session: Session = Depends(get_session),
) -> Response:
    stmt = select(Reading)
    if device_id:
        stmt = stmt.where(Reading.device_id == device_id)
    dt_since = _parse_dt(since)
    dt_until = _parse_dt(until)
    if dt_since:
        stmt = stmt.where(Reading.received_at >= dt_since)
    if dt_until:
        stmt = stmt.where(Reading.received_at <= dt_until)
    stmt = stmt.order_by(Reading.received_at)
    rows = session.exec(stmt).all()

    fields = [
        "received_at", "device_id", "type", "timestamp_ms", "reconstruction_error",
        "anomaly_threshold", "is_anomaly", "battery_voltage", "uptime_cycles",
        "mean_vibration", "mean_temperature_delta", "weight_magnitude",
        "wifi_fail_count", "http_retry_count", "config_version", "sample_rate_hz",
    ]
    buf = io.StringIO()
    writer = csv.DictWriter(buf, fieldnames=fields, extrasaction="ignore")
    writer.writeheader()
    for r in rows:
        d = _reading_dict(r)
        d["received_at"] = r.received_at.isoformat() if r.received_at else ""
        d["is_anomaly"] = int(r.is_anomaly)
        writer.writerow({k: d.get(k, "") for k in fields})

    name = f"deepmonitor_{device_id or 'all'}.csv"
    return Response(
        content=buf.getvalue(),
        media_type="text/csv",
        headers={"Content-Disposition": f'attachment; filename="{name}"'},
    )


# ── Battery trade-off curve ──────────────────────────────────────────────────

def _estimate_days(sleep_min: float, active_s: float, battery_mah: float,
                   wifi_burst_s: float = 6.0, anomaly_pct: float = 1.0) -> float:
    sleep_ua, active_ma, wifi_avg_ma, usable = 15.0, 28.0, 160.0, 0.85
    cycle_s = sleep_min * 60.0 + active_s
    if cycle_s <= 0:
        return 0.0
    wifi_events_day = 1.0 + (anomaly_pct / 100.0) * (86400.0 / cycle_s)
    wifi_s_per_cycle = (wifi_events_day * wifi_burst_s) / (86400.0 / cycle_s)
    energy_mas = (sleep_min * 60.0) * (sleep_ua / 1000.0) + active_s * active_ma + wifi_s_per_cycle * wifi_avg_ma
    avg_ma = energy_mas / cycle_s
    if avg_ma <= 0:
        return math.inf
    return (battery_mah * usable / avg_ma) / 24.0


@router.get("/battery/curve")
def battery_curve(battery_mah: float = 3000.0, active_s: float = 1.3) -> dict:
    points = []
    for sleep_min in [1, 2, 5, 10, 15, 30, 60, 120, 240]:
        days = _estimate_days(sleep_min, active_s, battery_mah)
        points.append({
            "sleep_min": sleep_min,
            "detection_latency_min": sleep_min,
            "est_days": round(days, 1),
            "est_years": round(days / 365.25, 2),
        })
    return {"battery_mah": battery_mah, "active_s": active_s, "points": points}


@router.get("/battery/estimate")
def battery_estimate(
    sleep_duration_us: int,
    battery_mah: float = 3000.0,
    active_s: float = 1.3,
) -> dict:
    sleep_min = sleep_duration_us / 1_000_000.0 / 60.0
    days = _estimate_days(sleep_min, active_s, battery_mah)
    return {
        "sleep_min": round(sleep_min, 2),
        "est_days": round(days, 1),
        "est_years": round(days / 365.25, 2),
    }
