import { useState } from "react";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { Card, ErrorNote, Loading } from "../components/ui";
import { fmtDateTime, fmtNum } from "../lib/format";

const PAGE_SIZE = 25;

export function DataBrowser() {
  const devices = useFetch(() => api.listDevices(), []);
  const [device, setDevice] = useState("");
  const [type, setType] = useState("");
  const [page, setPage] = useState(0);

  const readings = useFetch(
    () => (device ? api.getReadings(device, { type: type || undefined, limit: 2000 }) : Promise.resolve([])),
    [device, type]
  );

  const list = (readings.data ?? []).slice().reverse(); // newest first for browsing
  const pageRows = list.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE);
  const pages = Math.max(1, Math.ceil(list.length / PAGE_SIZE));

  const deviceList = devices.data ?? [];

  return (
    <div className="page">
      <h1>Data browser</h1>
      <Card
        title={`${list.length} reading(s)`}
        actions={
          <div className="filters">
            <select value={device} onChange={(e) => { setDevice(e.target.value); setPage(0); }}>
              <option value="">Select device…</option>
              {deviceList.map((d) => (
                <option key={d.device_id} value={d.device_id}>{d.name}</option>
              ))}
            </select>
            <select value={type} onChange={(e) => { setType(e.target.value); setPage(0); }}>
              <option value="">All types</option>
              <option value="heartbeat">heartbeat</option>
              <option value="anomaly">anomaly</option>
            </select>
            {device && <a className="btn" href={api.exportCsvUrl({ device_id: device })}>Download CSV</a>}
          </div>
        }
      >
        {!device ? (
          <div className="muted">Select a device to browse its raw readings.</div>
        ) : readings.loading && !readings.data ? (
          <Loading what="readings" />
        ) : readings.error ? (
          <ErrorNote error={readings.error} />
        ) : (
          <>
            <div className="table-scroll">
              <table className="data-table">
                <thead>
                  <tr>
                    <th>Received</th>
                    <th>Type</th>
                    <th>Recon</th>
                    <th>Thresh</th>
                    <th>Anom</th>
                    <th>Battery</th>
                    <th>Uptime</th>
                    <th>Sample Hz</th>
                  </tr>
                </thead>
                <tbody>
                  {pageRows.map((r) => (
                    <tr key={r.id} className={r.is_anomaly ? "row-anomaly" : ""}>
                      <td>{fmtDateTime(r.received_at)}</td>
                      <td>{r.type}</td>
                      <td>{fmtNum(r.reconstruction_error, 4)}</td>
                      <td>{fmtNum(r.anomaly_threshold, 4)}</td>
                      <td>{r.is_anomaly ? "yes" : ""}</td>
                      <td>{fmtNum(r.battery_voltage, 2)}</td>
                      <td>{r.uptime_cycles ?? "—"}</td>
                      <td>{r.sample_rate_hz ?? "—"}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <div className="pager">
              <button className="btn small" disabled={page === 0} onClick={() => setPage((p) => p - 1)}>Prev</button>
              <span className="muted">Page {page + 1} / {pages}</span>
              <button className="btn small" disabled={page + 1 >= pages} onClick={() => setPage((p) => p + 1)}>Next</button>
            </div>
          </>
        )}
      </Card>
    </div>
  );
}
