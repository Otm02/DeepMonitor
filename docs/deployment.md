# DeepMonitor deployment runbook

DeepMonitor is **local-first**: one process (FastAPI + SQLite) ingests device
telemetry and serves the React dashboard. Run it on a laptop or a Raspberry Pi
on the same Wi-Fi as your sensors. No cloud, no accounts.

```
ESP32 sensor ──HTTP POST /ingest──▶ FastAPI ──▶ SQLite
      ▲                                │
      └──GET /api/devices/{id}/config──┘   React dashboard (served by FastAPI)
```

## 1. Run with Docker (recommended)

```bash
docker compose up -d --build
# dashboard + API on http://<host>:8000
```

The SQLite database is persisted in the `deepmonitor-data` volume. To upgrade,
`git pull` then `docker compose up -d --build` again.

## 2. Run from source (development)

Backend:

```bash
cd server
python -m venv .venv && . .venv/Scripts/activate    # Windows
# source .venv/bin/activate                          # macOS/Linux
pip install -r requirements.txt
uvicorn app.main:app --reload --port 8000
```

Frontend (dev server with hot reload, proxies the API to :8000):

```bash
cd frontend
npm install
npm run dev          # http://localhost:5173
```

For a production single-origin deploy, build the SPA so FastAPI serves it:

```bash
cd frontend && npm run build      # emits frontend/dist
# now http://localhost:8000 serves the dashboard
```

## 3. Run 24/7 on a Raspberry Pi (no Docker)

```bash
sudo mkdir -p /opt/deepmonitor && sudo chown pi:pi /opt/deepmonitor
git clone <repo> /opt/deepmonitor
cd /opt/deepmonitor
python -m venv .venv && .venv/bin/pip install -r server/requirements.txt
# Build the dashboard once (needs Node) or copy a prebuilt frontend/dist:
( cd frontend && npm install && npm run build )

sudo cp deploy/deepmonitor.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now deepmonitor
journalctl -u deepmonitor -f
```

## 4. Point a device at the server

In `src/secrets.h` (copy from `src/secrets.h.example`) or via PlatformIO
`-D` build flags set:

```c
#define WIFI_SSID    "your-ssid"
#define WIFI_PASS    "your-pass"
#define WEBHOOK_URL  "http://192.168.1.50:8000/ingest"
#define CONFIG_URL   "http://192.168.1.50:8000/api/devices/M5-AB12/config"
#define DEVICE_ID    "M5-AB12"
```

`CONFIG_URL` enables the control plane: the device pulls runtime settings
(sleep interval, heartbeat, sample rate, sensitivity) during each Wi-Fi
session. Edit them in the dashboard **Controls** tab; the device applies the
new version on its next connection.

## 5. Import legacy CSV captures

```bash
cd server
python -m app.import_csv ../tools/out/captures      # a directory of CSVs
python -m app.import_csv ../tools/out/captures/run1.csv
```

## 6. Battery tuning

```bash
python tools/battery_sim.py --recommend 15 --battery-mah 3000   # longest life ≤15 min latency
python tools/battery_sim.py --curve tools/out/battery_curve.json
```

The dashboard **Controls** tab shows the same trade-off curve and an estimated
battery life for the selected sleep interval. The shipped firmware default is
the battery-optimal 15-min profile (~3.8 years on a 3000 mAh 18650).

## 7. Health & API

- `GET /health` — liveness probe.
- `GET /docs` — interactive OpenAPI docs.
- Key endpoints: `/ingest`, `/api/devices`, `/api/devices/{id}/readings`,
  `/api/devices/{id}/config`, `/api/anomalies`, `/api/reports/summary`,
  `/api/reports/export.csv`, `/api/battery/curve`.
