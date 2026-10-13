import { useState } from "react";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { Card, ErrorNote, Loading, Stat } from "../components/ui";
import { fmtDateTime, fmtNum } from "../lib/format";

export function Reports() {
  const devices = useFetch(() => api.listDevices(), []);
  const [device, setDevice] = useState("");
  const [since, setSince] = useState("");
  const [until, setUntil] = useState("");

  const opts = { device_id: device || undefined, since: since || undefined, until: until || undefined };
  const summary = useFetch(() => api.reportSummary(opts), [device, since, until]);

  const list = devices.data ?? [];

  return (
    <div className="page report-page">
      <div className="page-head no-print">
        <h1>Reports</h1>
        <div className="filters">
          <select value={device} onChange={(e) => setDevice(e.target.value)}>
            <option value="">All devices</option>
            {list.map((d) => (
              <option key={d.device_id} value={d.device_id}>{d.name}</option>
            ))}
          </select>
          <label className="inline">From <input type="datetime-local" value={since} onChange={(e) => setSince(e.target.value)} /></label>
          <label className="inline">To <input type="datetime-local" value={until} onChange={(e) => setUntil(e.target.value)} /></label>
          <a className="btn" href={api.exportCsvUrl(opts)}>Export CSV</a>
          <button className="btn" onClick={() => window.print()}>Print / PDF</button>
        </div>
      </div>

      <div className="print-only report-title">
        <h1>DeepMonitor report</h1>
        <div className="muted">{device || "All devices"} · generated {new Date().toLocaleString()}</div>
      </div>

      {summary.loading && !summary.data ? (
        <Loading what="report" />
      ) : summary.error ? (
        <ErrorNote error={summary.error} />
      ) : summary.data ? (
        <>
          <div className="stat-row">
            <Stat label="Total readings" value={summary.data.total_readings} />
            <Stat label="Anomalies" value={summary.data.anomaly_count} />
            <Stat label="Anomaly rate" value={`${(summary.data.anomaly_rate * 100).toFixed(1)}%`} />
            <Stat label="Recon mean" value={fmtNum(summary.data.recon_mean, 4)} />
            <Stat label="Recon p95" value={fmtNum(summary.data.recon_p95, 4)} />
            <Stat label="Recon max" value={fmtNum(summary.data.recon_max, 4)} />
          </div>
          <Card title="Details">
            <table className="data-table">
              <tbody>
                <tr><th>Battery (last)</th><td>{fmtNum(summary.data.battery_last, 2)} V</td></tr>
                <tr><th>Battery (min)</th><td>{fmtNum(summary.data.battery_min, 2)} V</td></tr>
                <tr><th>First seen</th><td>{fmtDateTime(summary.data.first_seen)}</td></tr>
                <tr><th>Last seen</th><td>{fmtDateTime(summary.data.last_seen)}</td></tr>
              </tbody>
            </table>
          </Card>
        </>
      ) : null}
    </div>
  );
}
