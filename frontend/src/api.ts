import type {
  Anomaly,
  BatteryCurve,
  BatteryEstimate,
  Device,
  DeviceConfig,
  Reading,
  ReportSummary,
} from "./types";

async function http<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...init,
  });
  if (!res.ok) {
    const text = await res.text().catch(() => "");
    throw new Error(`${res.status} ${res.statusText}: ${text}`);
  }
  return (await res.json()) as T;
}

function qs(params: Record<string, string | number | boolean | undefined>): string {
  const entries = Object.entries(params).filter(([, v]) => v !== undefined && v !== "");
  if (entries.length === 0) return "";
  return "?" + entries.map(([k, v]) => `${k}=${encodeURIComponent(String(v))}`).join("&");
}

export const api = {
  listDevices: () => http<Device[]>("/api/devices"),
  getDevice: (id: string) => http<Device>(`/api/devices/${encodeURIComponent(id)}`),
  renameDevice: (id: string, name: string) =>
    http<Device>(`/api/devices/${encodeURIComponent(id)}`, {
      method: "PATCH",
      body: JSON.stringify({ name }),
    }),
  getReadings: (
    id: string,
    opts: { since?: string; until?: string; type?: string; limit?: number } = {}
  ) => http<Reading[]>(`/api/devices/${encodeURIComponent(id)}/readings${qs(opts)}`),

  getConfig: (id: string) => http<DeviceConfig>(`/api/devices/${encodeURIComponent(id)}/config`),
  updateConfig: (id: string, body: Partial<DeviceConfig>) =>
    http<DeviceConfig>(`/api/devices/${encodeURIComponent(id)}/config`, {
      method: "PUT",
      body: JSON.stringify(body),
    }),

  listAnomalies: (opts: { device_id?: string; acknowledged?: boolean; limit?: number } = {}) =>
    http<Anomaly[]>(`/api/anomalies${qs(opts)}`),
  ackAnomaly: (anomalyId: number, note?: string) =>
    http<{ status: string }>(`/api/anomalies/${anomalyId}/ack`, {
      method: "POST",
      body: JSON.stringify({ note }),
    }),

  reportSummary: (opts: { device_id?: string; since?: string; until?: string } = {}) =>
    http<ReportSummary>(`/api/reports/summary${qs(opts)}`),
  exportCsvUrl: (opts: { device_id?: string; since?: string; until?: string } = {}) =>
    `/api/reports/export.csv${qs(opts)}`,

  batteryCurve: (battery_mah = 3000, active_s = 1.3) =>
    http<BatteryCurve>(`/api/battery/curve${qs({ battery_mah, active_s })}`),
  batteryEstimate: (sleep_duration_us: number, battery_mah = 3000) =>
    http<BatteryEstimate>(`/api/battery/estimate${qs({ sleep_duration_us, battery_mah })}`),
};
