import type { ReactNode } from "react";
import type { DeviceStatus, Severity } from "../types";

export function Card({ title, children, actions }: { title?: string; children: ReactNode; actions?: ReactNode }) {
  return (
    <div className="card">
      {(title || actions) && (
        <div className="card-head">
          {title && <h3>{title}</h3>}
          {actions && <div className="card-actions">{actions}</div>}
        </div>
      )}
      <div className="card-body">{children}</div>
    </div>
  );
}

export function Stat({ label, value, hint }: { label: string; value: ReactNode; hint?: string }) {
  return (
    <div className="stat">
      <div className="stat-label">{label}</div>
      <div className="stat-value">{value}</div>
      {hint && <div className="stat-hint">{hint}</div>}
    </div>
  );
}

export function StatusBadge({ status }: { status: DeviceStatus }) {
  return (
    <span className={`badge status-${status}`}>
      <span className="dot" /> {status}
    </span>
  );
}

export function SeverityBadge({ severity }: { severity: Severity }) {
  return <span className={`badge sev-${severity}`}>{severity}</span>;
}

export function Loading({ what = "data" }: { what?: string }) {
  return <div className="muted">Loading {what}…</div>;
}

export function ErrorNote({ error }: { error: string }) {
  return <div className="error-note">⚠ {error}</div>;
}
