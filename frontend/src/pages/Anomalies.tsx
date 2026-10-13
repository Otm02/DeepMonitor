import { useState } from "react";
import { Link } from "react-router-dom";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { Card, ErrorNote, Loading, SeverityBadge } from "../components/ui";
import { fmtDateTime, fmtNum } from "../lib/format";

export function Anomalies() {
  const [showAcked, setShowAcked] = useState(false);
  const [deviceFilter, setDeviceFilter] = useState("");

  const { data, error, loading, refresh } = useFetch(
    () => api.listAnomalies({ acknowledged: showAcked ? undefined : false, device_id: deviceFilter || undefined, limit: 500 }),
    [showAcked, deviceFilter],
    15000
  );

  async function ack(id: number) {
    const note = window.prompt("Add a note for this anomaly (optional):") ?? undefined;
    await api.ackAnomaly(id, note);
    refresh();
  }

  const list = data ?? [];

  return (
    <div className="page">
      <h1>Anomalies</h1>
      <Card
        title={`${list.length} event(s)`}
        actions={
          <div className="filters">
            <input
              placeholder="Filter by device id"
              value={deviceFilter}
              onChange={(e) => setDeviceFilter(e.target.value)}
            />
            <label className="check">
              <input type="checkbox" checked={showAcked} onChange={(e) => setShowAcked(e.target.checked)} />
              include acknowledged
            </label>
          </div>
        }
      >
        {loading && !data ? (
          <Loading what="anomalies" />
        ) : error ? (
          <ErrorNote error={error} />
        ) : list.length === 0 ? (
          <div className="muted">No anomalies. The fleet looks healthy.</div>
        ) : (
          <table className="data-table">
            <thead>
              <tr>
                <th>When</th>
                <th>Device</th>
                <th>Severity</th>
                <th>Recon err</th>
                <th>Threshold</th>
                <th>Note</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {list.map((a) => (
                <tr key={a.id} className={a.acknowledged ? "row-muted" : ""}>
                  <td>{fmtDateTime(a.created_at)}</td>
                  <td><Link to={`/devices/${encodeURIComponent(a.device_id)}`}>{a.device_id}</Link></td>
                  <td><SeverityBadge severity={a.severity} /></td>
                  <td>{fmtNum(a.reconstruction_error, 4)}</td>
                  <td>{fmtNum(a.anomaly_threshold, 4)}</td>
                  <td>{a.note ?? <span className="muted">—</span>}</td>
                  <td>
                    {a.acknowledged ? (
                      <span className="muted">acked</span>
                    ) : (
                      <button className="btn small" onClick={() => ack(a.id)}>Acknowledge</button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Card>
    </div>
  );
}
