import { Link } from "react-router-dom";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { Card, ErrorNote, Loading, Stat, StatusBadge } from "../components/ui";
import { fmtNum, fmtRelative } from "../lib/format";

export function Overview() {
  const { data: devices, error, loading } = useFetch(() => api.listDevices(), [], 10000);

  if (loading && !devices) return <Loading what="devices" />;
  if (error) return <ErrorNote error={error} />;
  const list = devices ?? [];

  const online = list.filter((d) => d.status === "online").length;
  const anomalous = list.filter((d) => d.status === "anomaly").length;
  const offline = list.filter((d) => d.status === "offline").length;

  return (
    <div className="page">
      <h1>Fleet overview</h1>
      <div className="stat-row">
        <Stat label="Devices" value={list.length} />
        <Stat label="Online" value={online} />
        <Stat label="Anomalies" value={anomalous} hint="devices currently flagged" />
        <Stat label="Offline" value={offline} />
      </div>

      <Card title="Devices">
        {list.length === 0 ? (
          <div className="muted">
            No devices yet. Point a sensor (or the simulator) at <code>/ingest</code>.
          </div>
        ) : (
          <table className="data-table">
            <thead>
              <tr>
                <th>Device</th>
                <th>Status</th>
                <th>Battery</th>
                <th>Recon err</th>
                <th>Threshold</th>
                <th>Uptime</th>
                <th>Last seen</th>
                <th>Config</th>
              </tr>
            </thead>
            <tbody>
              {list.map((d) => (
                <tr key={d.device_id}>
                  <td>
                    <Link to={`/devices/${encodeURIComponent(d.device_id)}`}>{d.name}</Link>
                    <div className="muted small">{d.device_id}</div>
                  </td>
                  <td><StatusBadge status={d.status} /></td>
                  <td>{fmtNum(d.last_battery, 2)} V</td>
                  <td>{fmtNum(d.last_recon_error, 4)}</td>
                  <td>{fmtNum(d.last_threshold, 4)}</td>
                  <td>{d.uptime_cycles}</td>
                  <td>{fmtRelative(d.last_seen)}</td>
                  <td>
                    {d.config_synced ? (
                      <span className="muted">v{d.config.config_version} synced</span>
                    ) : (
                      <span className="badge sev-medium">pending v{d.config.config_version}</span>
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
