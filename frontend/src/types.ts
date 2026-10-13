export type DeviceStatus = "online" | "offline" | "anomaly" | "unknown";

export interface DeviceConfig {
  config_version: number;
  applied_config_version: number;
  sleep_duration_us: number;
  heartbeat_interval: number;
  sample_rate_hz: number;
  anomaly_k: number;
}

export interface Device {
  device_id: string;
  name: string;
  status: DeviceStatus;
  created_at: string | null;
  last_seen: string | null;
  last_battery: number | null;
  last_recon_error: number | null;
  last_threshold: number | null;
  last_is_anomaly: boolean;
  uptime_cycles: number;
  wifi_fail_count: number;
  http_retry_count: number;
  config: DeviceConfig;
  config_synced: boolean;
}

export interface Reading {
  id: number;
  device_id: string;
  received_at: string;
  type: string;
  timestamp_ms: number | null;
  reconstruction_error: number | null;
  anomaly_threshold: number | null;
  is_anomaly: boolean;
  battery_voltage: number | null;
  uptime_cycles: number | null;
  mean_vibration: number | null;
  mean_temperature_delta: number | null;
  weight_magnitude: number | null;
  wifi_fail_count: number | null;
  http_retry_count: number | null;
  config_version: number | null;
  sample_rate_hz: number | null;
  features: number[] | null;
}

export type Severity = "low" | "medium" | "high";

export interface Anomaly {
  id: number;
  device_id: string;
  reading_id: number | null;
  created_at: string;
  reconstruction_error: number | null;
  anomaly_threshold: number | null;
  severity: Severity;
  acknowledged: boolean;
  note: string | null;
}

export interface ReportSummary {
  device_id: string | null;
  total_readings: number;
  anomaly_count: number;
  anomaly_rate: number;
  recon_mean: number | null;
  recon_p95: number | null;
  recon_max: number | null;
  battery_last: number | null;
  battery_min: number | null;
  first_seen: string | null;
  last_seen: string | null;
}

export interface BatteryPoint {
  sleep_min: number;
  detection_latency_min: number;
  est_days: number;
  est_years: number;
}

export interface BatteryCurve {
  battery_mah: number;
  active_s: number;
  points: BatteryPoint[];
}

export interface BatteryEstimate {
  sleep_min: number;
  est_days: number;
  est_years: number;
}
