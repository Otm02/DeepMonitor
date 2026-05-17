# First Field Tests — 2026-05-17

Vibration characterization on household objects before Russet House deployment.
This is **not** an anomaly-validation test — the goal is to confirm the sensor captures
distinguishable vibration signatures and the autoencoder produces sensible baselines.

---

## Goal and success criteria

| Criterion | Pass condition |
|-----------|----------------|
| Device boots and samples | At least one heartbeat per object within 15 minutes |
| Data reaches laptop | CSV file created under `tools/out/captures/<device_id>.csv` |
| Desk baseline | Reconstruction error stabilizes (decreases) after ~5 duty cycles on desk |
| Object differentiation | `mean_vibration` differs measurably between desk and at least one machine |
| Battery | Voltage reported in heartbeat; no unexpected resets mid-session |

---

## Bill of materials

- M5StickC Plus2 (charged)
- USB-C cable (for flashing only — device runs on battery during capture)
- Laptop on same Wi-Fi network as the device
- Masking tape or double-sided tape for mounting
- Optional: small neodymium magnet + ferrous surface

---

## Pre-flight checklist (~30 min)

### 1. Clone and configure secrets

```powershell
cd DeepMonitor
copy src\secrets.h.example src\secrets.h
```

Edit `src/secrets.h`:

```cpp
#define WIFI_SSID     "your-home-wifi"
#define WIFI_PASS     "your-wifi-password"
#define WEBHOOK_URL   "http://192.168.1.XXX:8000/ingest"   // laptop LAN IP
#define DEVICE_ID     "M5-DESK-01"                          // change per object
```

Find your laptop IP: `ipconfig` → look for IPv4 on Wi-Fi adapter.

### 2. Install tools

```powershell
pip install -r tools\requirements.txt
```

### 3. Flash firmware (onboard MPU6886 build)

```powershell
pio device list
# Pick the USB-Enhanced-SERIAL CH9102 port (M5), not Bluetooth COM3/COM4
pio run -e m5stickc-6886 -t upload --upload-port COM5
```

If PlatformIO is not installed: install the PlatformIO VS Code extension, then use the command above from a terminal in the project root.

### 4. Start ingest server

```powershell
python tools\local_ingest.py
```

Confirm in browser: `http://localhost:8000/health` → `{"status":"ok"}`

Allow port 8000 through Windows Firewall if prompted.

### 5. Smoke test

Power the M5StickC from USB briefly and confirm serial output on the correct port:
`pio device monitor -e m5stickc-6886 --port COM5`.
Then disconnect USB and let it run on battery. With test settings
(`SLEEP_DURATION_US=60000000`, `HEARTBEAT_INTERVAL=1`) you should see a
heartbeat roughly every minute.

---

## Per-object test protocol

Run objects in this order: **desk → computer → fridge → washing machine**.
Change `DEVICE_ID` in `secrets.h` (or reflash with a new ID) between objects, or use one ID and note object in the notes table below.

For each object:

1. Mount sensor rigidly (tape on flat surface; no dangling cable)
2. Orient consistently: long axis of stick parallel to surface, screen facing up
3. Let run **10 min idle** (machine off / desk undisturbed)
4. Let run **10 min active** where applicable (machine running)
5. Record observations in notes table

### Desk (control / noise floor)

| Parameter | Value |
|-----------|-------|
| Device ID | `M5-DESK-01` |
| Mount | Tape to desk surface, away from keyboard |
| Active phase | N/A — leave undisturbed entire session |
| Expected | Lowest `mean_vibration`; recon error drops as model learns idle |

### Computer

| Parameter | Value |
|-----------|-------|
| Device ID | `M5-PC-01` |
| Mount | Side panel near CPU fan exhaust (metal preferred) |
| Active phase | Run a disk benchmark or compile while capturing |
| Expected | Steady elevated vibration; occasional spikes during disk I/O |

### Fridge

| Parameter | Value |
|-----------|-------|
| Device ID | `M5-FRIDGE-01` |
| Mount | Top of compressor housing at rear (tape; do not block vents) |
| Active phase | Normal operation — compressor cycles on/off every 20–40 min |
| Expected | Periodic vibration bursts when compressor runs; quiet between cycles |

### Washing machine

| Parameter | Value |
|-----------|-------|
| Device ID | `M5-WASH-01` |
| Mount | Front panel near motor housing |
| Active phase | Run a full wash + spin cycle |
| Expected | High vibration during spin; model may flag anomalies on first spin exposure |

---

## What to watch live

The ingest server prints one line per received payload:

```
[14:32:01] M5-DESK-01 heartbeat batt=4.10V vib=0.012 recon=
[14:37:05] M5-DESK-01 anomaly batt=4.09V vib=0.845 recon=0.0523
```

| Field | Meaning |
|-------|---------|
| `batt` | Battery voltage — should decrease slowly over the day |
| `vib` | Mean vibration magnitude for the 5-second sample window |
| `recon` | Autoencoder reconstruction error (anomaly score) |

**Healthy signs:** heartbeats every ~1 minute in test mode
(`HEARTBEAT_INTERVAL=1`, 1-minute sleep). Desk recon error trending down after
~5 cycles.

**Warning signs:** no output for >20 min; `batt` below 3.5 V; every cycle flagged as anomaly on desk after warm-up.

---

## Post-test analysis

```powershell
python tools\quick_analysis.py
```

Compare `vib_mean` and `recon_p95` across devices. Desk should have the lowest vibration mean; washing machine the highest.

Optional battery planning:

```powershell
python tools\battery_sim.py --output tools\out\battery_results.csv
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| No lines in ingest terminal | Wrong `WEBHOOK_URL` IP or firewall | Verify laptop IP; allow port 8000 |
| `wifi_fail_count` rising (in serial) | Wi-Fi credentials wrong | Check `WIFI_SSID` / `WIFI_PASS` in secrets.h |
| Device resets every ~5 min but no Wi-Fi | Normal — deep sleep duty cycle | Expected behaviour |
| All cycles anomaly on desk | Model not converged or mount loose | Reflash to clear NVS; remount firmly |
| IMU init failure loop | Hardware issue | Check serial log; reseat M5StickC |
| Heartbeat not showing in serial monitor | Monitor attached to Bluetooth COM3/COM4 instead of CH9102 USB bridge | Run `pio device list`, then use `pio device monitor -e m5stickc-6886 --port COMx` with CH9102 COMx |
| Heartbeat only every 12 h | `HEARTBEAT_INTERVAL=144` with 5-min sleep | For fast feedback, set `HEARTBEAT_INTERVAL=1` and `SLEEP_DURATION_US=60000000` |

### Temporarily faster heartbeats for tomorrow only

In `src/config.h`, change for the test session:

```cpp
static const int HEARTBEAT_INTERVAL = 1;      // every cycle in test mode
static const int SLEEP_DURATION_US = 60000000; // 1 minute
```

Reflash after changing. **Revert before field deployment.**

---

## Notes template

| Object | Device ID | Mount location | Start time | End time | Ambient °C | Hand-felt vibration? | Anomalies in log? | Photo taken? |
|--------|-----------|----------------|------------|----------|------------|----------------------|-------------------|--------------|
| Desk | | | | | | | | |
| Computer | | | | | | | | |
| Fridge | | | | | | | | |
| Washing machine | | | | | | | | |

---

## Next steps after tomorrow

1. Review CSVs and `quick_analysis.py` output
2. Run `battery_sim.py` to identify duty-cycle settings for ≥1 year on production 18650 hardware
3. Wire external MPU-6050 and test `m5stickc-6050` env for sensor parity with production PCB
4. Deploy at Russet House with Supabase endpoint once plant Wi-Fi is confirmed
