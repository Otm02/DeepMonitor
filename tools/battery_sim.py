#!/usr/bin/env python3
"""
Battery life estimator for DeepMonitor production hardware.

Models ESP32-WROOM-32E + MPU-6050 + W25Q32 + 18650 Li-ion duty-cycled operation.
Sweeps sleep interval, active window, Wi-Fi cadence, and anomaly rate.

Usage:
  python tools/battery_sim.py
  python tools/battery_sim.py --output tools/out/battery_results.csv
  python tools/battery_sim.py --single 60 5 1
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator


@dataclass
class CurrentProfile:
    """Per-phase current draw in milliamps."""
    sleep_ua: float = 15.0          # deep sleep (µA → stored as µA, converted in calc)
    active_ma: float = 28.0         # ESP32 + MPU-6050 sampling + FFT/AE inference
    wifi_avg_ma: float = 160.0      # connect + POST averaged over burst window
    flash_write_ma: float = 4.0     # W25Q32 page program (negligible energy)
    battery_mah: float = 2500.0
    usable_fraction: float = 0.85
    target_days: float = 365.0


@dataclass
class Scenario:
    sleep_min: float
    active_s: float
    wifi_per_day: float
    wifi_burst_s: float
    anomaly_pct: float
    battery_mah: float | None = None

    @property
    def scenario_id(self) -> str:
        return (
            f"sleep{int(self.sleep_min)}m_act{self.active_s}s"
            f"_wifi{self.wifi_per_day:.2f}d_anom{self.anomaly_pct:.0f}pct"
        )


def default_scenarios() -> list[Scenario]:
    sleep_mins = [1, 5, 15, 60, 240, 1440]
    # Active window with the FFT + autoencoder pipeline: 256 samples @ 200 Hz
    # (~1.3 s) plus inference. 2.5 s / 5 s kept for slower-sampling comparisons.
    active_ss = [1.3, 2.5, 5.0]
    # wifi_per_day: every cycle, 12h heartbeat (2/day), 24h (1/day), weekly (~0.14/day)
    wifi_cadences = [
        ("every_cycle", None),  # computed from sleep_min
        ("12h", 2.0),
        ("24h", 1.0),
        ("7d", 1.0 / 7.0),
    ]
    wifi_burst_ss = [3, 6, 10]
    anomaly_pcts = [0, 1, 5]
    battery_mahs = [2000, 2500, 3000]

    scenarios: list[Scenario] = []
    for sleep_min in sleep_mins:
        cycle_s = sleep_min * 60.0
        for active_s in active_ss:
            for _name, wifi_pd in wifi_cadences:
                if wifi_pd is None:
                    wpd = (24 * 3600) / cycle_s if cycle_s > 0 else 0
                else:
                    wpd = wifi_pd
                for wifi_burst_s in wifi_burst_ss:
                    for anomaly_pct in anomaly_pcts:
                        for batt in battery_mahs:
                            scenarios.append(
                                Scenario(
                                    sleep_min=sleep_min,
                                    active_s=active_s,
                                    wifi_per_day=wpd,
                                    wifi_burst_s=wifi_burst_s,
                                    anomaly_pct=anomaly_pct,
                                    battery_mah=batt,
                                )
                            )
    return scenarios


def estimate_days(scenario: Scenario, profile: CurrentProfile) -> dict:
    cycle_s = scenario.sleep_min * 60.0 + scenario.active_s
    if cycle_s <= 0:
        return {"error": "invalid cycle"}

    sleep_s = scenario.sleep_min * 60.0
    active_s = scenario.active_s

    # Wi-Fi events per day from heartbeat cadence + anomaly extras
    wifi_events_day = scenario.wifi_per_day
    wifi_events_day += (scenario.anomaly_pct / 100.0) * (86400.0 / cycle_s)

    wifi_s_per_cycle = (wifi_events_day * scenario.wifi_burst_s) / (86400.0 / cycle_s)

    # Energy per cycle (mA·s)
    sleep_ma = profile.sleep_ua / 1000.0
    energy_mas = (
        sleep_s * sleep_ma
        + active_s * profile.active_ma
        + wifi_s_per_cycle * profile.wifi_avg_ma
    )

    avg_current_ma = energy_mas / cycle_s
    usable_mah = (scenario.battery_mah or profile.battery_mah) * profile.usable_fraction

    if avg_current_ma <= 0:
        est_days = float("inf")
    else:
        est_hours = usable_mah / avg_current_ma
        est_days = est_hours / 24.0

    est_years = est_days / 365.25
    passes = est_days >= profile.target_days

    return {
        "scenario_id": scenario.scenario_id,
        "sleep_min": scenario.sleep_min,
        "active_s": scenario.active_s,
        "wifi_per_day": round(scenario.wifi_per_day, 4),
        "wifi_burst_s": scenario.wifi_burst_s,
        "anomaly_pct": scenario.anomaly_pct,
        "battery_mah": scenario.battery_mah or profile.battery_mah,
        "cycle_s": round(cycle_s, 1),
        "avg_current_mA": round(avg_current_ma, 4),
        "est_days": round(est_days, 1),
        "est_years": round(est_years, 2),
        "passes_1yr": passes,
    }


def load_profile(path: Path | None) -> CurrentProfile:
    if path is None or not path.exists():
        return CurrentProfile()
    data = json.loads(path.read_text(encoding="utf-8"))
    return CurrentProfile(**{k: v for k, v in data.items() if k in CurrentProfile.__dataclass_fields__})


def print_table(rows: list[dict], limit: int = 20) -> None:
    passing = [r for r in rows if r.get("passes_1yr")]
    print(f"\n{'='*72}")
    print(f"  Battery life simulation — production ESP32-WROOM + MPU-6050 + 18650")
    print(f"{'='*72}")
    print(f"  Total scenarios: {len(rows)}")
    print(f"  Passing >= 365 days: {len(passing)}")
    if passing:
        best = min(passing, key=lambda r: r["avg_current_mA"])
        print(
            f"  Best passing config: sleep={best['sleep_min']}min, "
            f"active={best['active_s']}s, wifi={best['wifi_per_day']}/day "
            f"-> {best['est_days']} days ({best['est_years']} yr)"
        )
    # Current firmware defaults reference
    ref = estimate_days(
        Scenario(sleep_min=5, active_s=5, wifi_per_day=2.0, wifi_burst_s=6, anomaly_pct=0),
        CurrentProfile(),
    )
    print(f"\n  Reference (current firmware: 5min sleep, 12h heartbeat):")
    print(f"    avg current = {ref['avg_current_mA']} mA -> {ref['est_days']} days ({ref['est_years']} yr)")
    print(f"{'='*72}\n")

    header = f"{'sleep':>6} {'active':>6} {'wifi/d':>7} {'anom%':>5} {'mAh':>5} {'mA':>8} {'days':>8} {'pass':>5}"
    print(header)
    print("-" * len(header))
    sorted_rows = sorted(rows, key=lambda r: -r["est_days"])
    for r in sorted_rows[:limit]:
        print(
            f"{r['sleep_min']:>6.0f} {r['active_s']:>6.0f} {r['wifi_per_day']:>7.2f} "
            f"{r['anomaly_pct']:>5.0f} {r['battery_mah']:>5.0f} {r['avg_current_mA']:>8.3f} "
            f"{r['est_days']:>8.1f} {'YES' if r['passes_1yr'] else 'no':>5}"
        )
    if len(rows) > limit:
        print(f"  ... and {len(rows) - limit} more (see CSV for full list)")


def recommend_config(
    profile: CurrentProfile,
    max_latency_min: float,
    battery_mah: float,
    active_s: float = 1.3,
    wifi_burst_s: float = 6.0,
    anomaly_pct: float = 1.0,
) -> dict:
    """Pick the longest-life config whose detection latency (≈ sleep interval)
    stays within max_latency_min and which still meets the day target."""
    best = None
    sleep_options = [1, 2, 5, 10, 15, 30, 60]
    for sleep_min in sleep_options:
        if sleep_min > max_latency_min:
            continue
        # Heartbeat roughly every 24 h regardless of sample cadence.
        sc = Scenario(
            sleep_min=sleep_min,
            active_s=active_s,
            wifi_per_day=1.0,
            wifi_burst_s=wifi_burst_s,
            anomaly_pct=anomaly_pct,
            battery_mah=battery_mah,
        )
        r = estimate_days(sc, profile)
        if best is None or r["est_days"] > best["est_days"]:
            best = r
    return best or {}


def battery_curve(
    profile: CurrentProfile,
    battery_mah: float,
    active_s: float = 1.3,
    wifi_burst_s: float = 6.0,
    anomaly_pct: float = 1.0,
) -> list[dict]:
    """Battery life vs sleep interval (for the dashboard trade-off chart)."""
    curve = []
    for sleep_min in [1, 2, 5, 10, 15, 30, 60, 120, 240]:
        sc = Scenario(
            sleep_min=sleep_min,
            active_s=active_s,
            wifi_per_day=1.0,
            wifi_burst_s=wifi_burst_s,
            anomaly_pct=anomaly_pct,
            battery_mah=battery_mah,
        )
        r = estimate_days(sc, profile)
        curve.append({
            "sleep_min": sleep_min,
            "detection_latency_min": sleep_min,
            "est_days": r["est_days"],
            "est_years": r["est_years"],
        })
    return curve


def main() -> None:
    parser = argparse.ArgumentParser(description="DeepMonitor battery life simulator")
    parser.add_argument("--output", "-o", type=Path, default=None, help="CSV output path")
    parser.add_argument("--profile", type=Path, default=None, help="JSON current profile override")
    parser.add_argument("--limit", type=int, default=25, help="Rows to print")
    parser.add_argument(
        "--single",
        nargs=3,
        metavar=("SLEEP_MIN", "ACTIVE_S", "WIFI_PER_DAY"),
        type=float,
        help="Evaluate one scenario",
    )
    parser.add_argument(
        "--recommend",
        type=float,
        metavar="MAX_LATENCY_MIN",
        help="Recommend the longest-life config within a detection-latency budget",
    )
    parser.add_argument("--battery-mah", type=float, default=3000.0, help="Battery capacity for --recommend/--curve")
    parser.add_argument("--curve", type=Path, default=None, help="Write battery-life vs sleep-interval JSON")
    args = parser.parse_args()

    profile = load_profile(args.profile)

    if args.single:
        sleep_min, active_s, wifi_pd = args.single
        sc = Scenario(
            sleep_min=sleep_min,
            active_s=active_s,
            wifi_per_day=wifi_pd,
            wifi_burst_s=6,
            anomaly_pct=0,
        )
        result = estimate_days(sc, profile)
        print(json.dumps(result, indent=2))
        return

    if args.recommend is not None:
        rec = recommend_config(profile, args.recommend, args.battery_mah)
        print("Recommended config (longest life within "
              f"{args.recommend:.0f} min detection latency on {args.battery_mah:.0f} mAh):")
        print(json.dumps(rec, indent=2))
        return

    if args.curve is not None:
        curve = battery_curve(profile, args.battery_mah)
        args.curve.parent.mkdir(parents=True, exist_ok=True)
        args.curve.write_text(json.dumps(curve, indent=2), encoding="utf-8")
        print(f"Wrote battery trade-off curve to {args.curve}")
        return

    scenarios = default_scenarios()
    results = [estimate_days(s, profile) for s in scenarios]
    results.sort(key=lambda r: -r["est_days"])

    print_table(results, limit=args.limit)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(results[0].keys()))
            writer.writeheader()
            writer.writerows(results)
        print(f"Wrote {len(results)} rows to {args.output}")


if __name__ == "__main__":
    main()
