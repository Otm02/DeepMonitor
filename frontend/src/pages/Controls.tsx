import { useEffect, useState } from "react";
import { useSearchParams } from "react-router-dom";
import type { EChartsOption } from "echarts";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { EChart } from "../components/EChart";
import { Card, ErrorNote, Loading, Stat } from "../components/ui";
import { fmtNum, minutesToSleepUs, sleepUsToMinutes } from "../lib/format";

export function Controls() {
  const [params, setParams] = useSearchParams();
  const devices = useFetch(() => api.listDevices(), []);
  const selected = params.get("device") ?? "";

  const list = devices.data ?? [];
  const current = list.find((d) => d.device_id === selected) ?? list[0];

  const [sleepMin, setSleepMin] = useState(15);
  const [heartbeat, setHeartbeat] = useState(96);
  const [sampleRate, setSampleRate] = useState(200);
  const [anomalyK, setAnomalyK] = useState(3);
  const [saving, setSaving] = useState(false);
  const [savedMsg, setSavedMsg] = useState("");

  useEffect(() => {
    if (current) {
      setSleepMin(sleepUsToMinutes(current.config.sleep_duration_us));
      setHeartbeat(current.config.heartbeat_interval);
      setSampleRate(current.config.sample_rate_hz);
      setAnomalyK(current.config.anomaly_k);
    }
  }, [current?.device_id]); // eslint-disable-line react-hooks/exhaustive-deps

  const estimate = useFetch(() => api.batteryEstimate(minutesToSleepUs(sleepMin)), [sleepMin]);
  const curve = useFetch(() => api.batteryCurve(3000, 1.3), []);

  const curveOption: EChartsOption = {
    backgroundColor: "transparent",
    tooltip: { trigger: "axis", formatter: (p: any) => {
      const d = p[0];
      return `${d.name} min sleep<br/>${d.value} days (${(d.value / 365.25).toFixed(2)} yr)`;
    } },
    grid: { left: 60, right: 20, top: 20, bottom: 40 },
    xAxis: { type: "category", name: "sleep (min)", data: (curve.data?.points ?? []).map((p) => p.sleep_min) },
    yAxis: { type: "value", name: "days" },
    series: [
      {
        type: "line",
        smooth: true,
        data: (curve.data?.points ?? []).map((p) => p.est_days),
        areaStyle: { opacity: 0.15 },
        lineStyle: { color: "#5b8def" },
        itemStyle: { color: "#5b8def" },
      },
    ],
  };

  async function save() {
    if (!current) return;
    setSaving(true);
    setSavedMsg("");
    try {
      const cfg = await api.updateConfig(current.device_id, {
        sleep_duration_us: minutesToSleepUs(sleepMin),
        heartbeat_interval: heartbeat,
        sample_rate_hz: sampleRate,
        anomaly_k: anomalyK,
      });
      setSavedMsg(`Saved. Desired config is now v${cfg.config_version}. The device applies it on its next connection.`);
      devices.refresh();
    } catch (e) {
      setSavedMsg(`Error: ${(e as Error).message}`);
    } finally {
      setSaving(false);
    }
  }

  if (devices.loading && !devices.data) return <Loading what="devices" />;
  if (devices.error) return <ErrorNote error={devices.error} />;
  if (!current) return <div className="page"><h1>Controls</h1><div className="muted">No devices yet.</div></div>;

  const pending = current.config.config_version !== current.config.applied_config_version;

  return (
    <div className="page">
      <h1>Device controls</h1>

      <Card title="Device">
        <select
          value={current.device_id}
          onChange={(e) => setParams({ device: e.target.value })}
        >
          {list.map((d) => (
            <option key={d.device_id} value={d.device_id}>{d.name} ({d.device_id})</option>
          ))}
        </select>
        <div className="config-state">
          <span className="muted">Desired v{current.config.config_version}</span>
          <span className="muted">Applied v{current.config.applied_config_version}</span>
          {pending ? <span className="badge sev-medium">pending — awaiting device connect</span> : <span className="badge sev-low">synced</span>}
        </div>
      </Card>

      <div className="grid-2">
        <Card title="Runtime configuration">
          <div className="form">
            <label>
              Sleep interval (minutes) — detection latency
              <input type="number" min={0.5} step={0.5} value={sleepMin} onChange={(e) => setSleepMin(parseFloat(e.target.value))} />
            </label>
            <label>
              Heartbeat interval (cycles)
              <input type="number" min={1} value={heartbeat} onChange={(e) => setHeartbeat(parseInt(e.target.value, 10))} />
            </label>
            <label>
              Sample rate (Hz)
              <input type="number" min={50} max={1000} step={10} value={sampleRate} onChange={(e) => setSampleRate(parseInt(e.target.value, 10))} />
            </label>
            <label>
              Anomaly sensitivity k (lower = more sensitive)
              <input type="number" min={1} max={8} step={0.5} value={anomalyK} onChange={(e) => setAnomalyK(parseFloat(e.target.value))} />
            </label>
            <button className="btn primary" disabled={saving} onClick={save}>
              {saving ? "Saving…" : "Push config"}
            </button>
            {savedMsg && <div className="muted small">{savedMsg}</div>}
          </div>
        </Card>

        <Card title="Estimated battery life">
          <div className="stat-row">
            <Stat label="At this sleep interval" value={`${fmtNum(estimate.data?.est_years ?? null, 2)} yr`} hint={`${fmtNum(estimate.data?.est_days ?? null, 0)} days · 3000 mAh`} />
            <Stat label="Heartbeat" value={`${((heartbeat * sleepMin) / 60).toFixed(1)} h`} hint="time between uploads" />
          </div>
          <EChart option={curveOption} height={260} />
          <div className="muted small">Battery life vs sleep interval (detection latency). Longer sleep → longer life.</div>
        </Card>
      </div>
    </div>
  );
}
