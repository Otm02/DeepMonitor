"""End-to-end API tests for the DeepMonitor backend."""

from __future__ import annotations


def _heartbeat(device_id="M5-TEST", recon=0.02, anomaly=0, version=0):
    return {
        "type": "heartbeat",
        "device_id": device_id,
        "uptime_cycles": 10,
        "battery_voltage": 3.95,
        "anomaly_threshold": 0.1,
        "reconstruction_error": recon,
        "is_anomaly": anomaly,
        "weight_magnitude": 1.2,
        "wifi_fail_count": 0,
        "http_retry_count": 0,
        "config_version": version,
        "sleep_duration_us": 900000000,
        "heartbeat_interval": 96,
        "sample_rate_hz": 200,
        "features": [0.1 * i for i in range(24)],
    }


def test_health(client):
    r = client.get("/health")
    assert r.status_code == 200
    assert r.json()["status"] == "ok"


def test_ingest_heartbeat_creates_device_and_reading(client):
    r = client.post("/ingest", json=_heartbeat())
    assert r.status_code == 200
    assert r.json()["status"] == "ok"

    devices = client.get("/api/devices").json()
    assert len(devices) == 1
    assert devices[0]["device_id"] == "M5-TEST"
    assert devices[0]["status"] in ("online", "anomaly")

    readings = client.get("/api/devices/M5-TEST/readings").json()
    assert len(readings) == 1
    assert readings[0]["features"] is not None
    assert len(readings[0]["features"]) == 24


def test_ingest_anomaly_creates_anomaly(client):
    client.post("/ingest", json={
        "type": "anomaly",
        "device_id": "M5-A",
        "reconstruction_error": 0.9,
        "anomaly_threshold": 0.2,
        "battery_voltage": 3.8,
        "mean_vibration": 1.1,
        "mean_temperature_delta": 0.5,
        "features": [0.0] * 24,
    })
    anomalies = client.get("/api/anomalies").json()
    assert len(anomalies) == 1
    a = anomalies[0]
    assert a["device_id"] == "M5-A"
    assert a["severity"] in ("medium", "high")
    assert a["acknowledged"] is False

    # Acknowledge with a note.
    r = client.post(f"/api/anomalies/{a['id']}/ack", json={"note": "checked bearing"})
    assert r.status_code == 200
    acked = client.get("/api/anomalies?acknowledged=true").json()
    assert len(acked) == 1
    assert acked[0]["note"] == "checked bearing"


def test_config_control_plane(client):
    client.post("/ingest", json=_heartbeat(device_id="M5-CFG"))

    cfg = client.get("/api/devices/M5-CFG/config").json()
    assert cfg["config_version"] == 0
    assert cfg["sleep_duration_us"] == 900000000

    updated = client.put("/api/devices/M5-CFG/config", json={"sleep_duration_us": 300000000, "anomaly_k": 2.5}).json()
    assert updated["config_version"] == 1
    assert updated["sleep_duration_us"] == 300000000
    assert abs(updated["anomaly_k"] - 2.5) < 1e-6

    # The pull document reflects the new desired config.
    cfg2 = client.get("/api/devices/M5-CFG/config").json()
    assert cfg2["config_version"] == 1
    assert cfg2["sleep_duration_us"] == 300000000

    # Device reports it applied version 1 -> synced.
    client.post("/ingest", json=_heartbeat(device_id="M5-CFG", version=1))
    dev = client.get("/api/devices/M5-CFG").json()
    assert dev["config_synced"] is True


def test_reports_and_export(client):
    for i in range(5):
        client.post("/ingest", json=_heartbeat(device_id="M5-R", recon=0.01 * (i + 1)))
    client.post("/ingest", json={
        "type": "anomaly", "device_id": "M5-R",
        "reconstruction_error": 0.8, "anomaly_threshold": 0.2, "battery_voltage": 3.7,
    })

    summary = client.get("/api/reports/summary?device_id=M5-R").json()
    assert summary["total_readings"] == 6
    assert summary["anomaly_count"] == 1
    assert summary["recon_mean"] is not None

    csv_resp = client.get("/api/reports/export.csv?device_id=M5-R")
    assert csv_resp.status_code == 200
    assert "text/csv" in csv_resp.headers["content-type"]
    body = csv_resp.text.strip().splitlines()
    assert body[0].startswith("received_at,device_id,type")
    assert len(body) == 7  # header + 6 rows


def test_battery_curve(client):
    data = client.get("/api/battery/curve?battery_mah=3000").json()
    assert data["battery_mah"] == 3000
    points = data["points"]
    assert len(points) > 0
    # Longer sleep intervals must yield longer battery life.
    by_sleep = {p["sleep_min"]: p["est_days"] for p in points}
    assert by_sleep[60] > by_sleep[1]
