"""SQLModel tables for devices, telemetry readings, and anomaly events."""

from __future__ import annotations

from datetime import datetime
from typing import Optional

from sqlmodel import Field, SQLModel


def utcnow() -> datetime:
    return datetime.utcnow()


class Device(SQLModel, table=True):
    """One physical sensor. Holds the desired (control-plane) config plus a
    rolling summary of the latest telemetry."""

    device_id: str = Field(primary_key=True)
    name: Optional[str] = None
    created_at: datetime = Field(default_factory=utcnow)
    last_seen: Optional[datetime] = None

    # Latest telemetry summary (updated on each ingest).
    last_battery: Optional[float] = None
    last_recon_error: Optional[float] = None
    last_threshold: Optional[float] = None
    last_is_anomaly: int = 0
    uptime_cycles: int = 0
    wifi_fail_count: int = 0
    http_retry_count: int = 0

    # Desired runtime configuration (pushed to the device via config-pull).
    sleep_duration_us: int = 900_000_000
    heartbeat_interval: int = 96
    sample_rate_hz: int = 200
    anomaly_k: float = 3.0
    config_version: int = 0           # desired version (bumped on edit)
    applied_config_version: int = 0   # version the device last reported applying


class Reading(SQLModel, table=True):
    """A single telemetry payload (heartbeat or anomaly alert)."""

    id: Optional[int] = Field(default=None, primary_key=True)
    device_id: str = Field(index=True, foreign_key="device.device_id")
    received_at: datetime = Field(default_factory=utcnow, index=True)
    type: str = "heartbeat"
    timestamp_ms: Optional[int] = None

    reconstruction_error: Optional[float] = None
    anomaly_threshold: Optional[float] = None
    is_anomaly: int = 0
    battery_voltage: Optional[float] = None
    uptime_cycles: Optional[int] = None
    mean_vibration: Optional[float] = None
    mean_temperature_delta: Optional[float] = None
    weight_magnitude: Optional[float] = None
    wifi_fail_count: Optional[int] = None
    http_retry_count: Optional[int] = None

    config_version: Optional[int] = None
    sleep_duration_us: Optional[int] = None
    heartbeat_interval: Optional[int] = None
    sample_rate_hz: Optional[int] = None

    features_json: Optional[str] = None   # JSON-encoded feature vector
    raw_json: Optional[str] = None


class Anomaly(SQLModel, table=True):
    """An anomaly event raised from a reading. Supports acknowledgement."""

    id: Optional[int] = Field(default=None, primary_key=True)
    device_id: str = Field(index=True, foreign_key="device.device_id")
    reading_id: Optional[int] = Field(default=None, foreign_key="reading.id")
    created_at: datetime = Field(default_factory=utcnow, index=True)
    reconstruction_error: Optional[float] = None
    anomaly_threshold: Optional[float] = None
    severity: str = "low"
    acknowledged: int = 0
    note: Optional[str] = None
