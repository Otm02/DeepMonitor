import { NavLink, Route, Routes } from "react-router-dom";
import { Overview } from "./pages/Overview";
import { DeviceDetail } from "./pages/DeviceDetail";
import { Anomalies } from "./pages/Anomalies";
import { Controls } from "./pages/Controls";
import { Reports } from "./pages/Reports";
import { DataBrowser } from "./pages/DataBrowser";

const NAV = [
  { to: "/", label: "Overview", end: true },
  { to: "/anomalies", label: "Anomalies" },
  { to: "/controls", label: "Controls" },
  { to: "/reports", label: "Reports" },
  { to: "/data", label: "Data" },
];

export default function App() {
  return (
    <div className="app">
      <aside className="sidebar no-print">
        <div className="brand">
          <span className="logo">◆</span> DeepMonitor
        </div>
        <nav>
          {NAV.map((n) => (
            <NavLink key={n.to} to={n.to} end={n.end} className={({ isActive }) => (isActive ? "active" : "")}>
              {n.label}
            </NavLink>
          ))}
        </nav>
        <div className="sidebar-foot muted small">Local-first · self-hosted</div>
      </aside>
      <main className="content">
        <Routes>
          <Route path="/" element={<Overview />} />
          <Route path="/devices/:id" element={<DeviceDetail />} />
          <Route path="/anomalies" element={<Anomalies />} />
          <Route path="/controls" element={<Controls />} />
          <Route path="/reports" element={<Reports />} />
          <Route path="/data" element={<DataBrowser />} />
        </Routes>
      </main>
    </div>
  );
}
