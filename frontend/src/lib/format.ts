// Feature vector layout — must match src/features.cpp on the device.
export const FEATURE_LABELS: string[] = [
  "RMS",
  "Peak",
  "Crest factor",
  "Kurtosis",
  "Std dev",
  "Peak-to-peak",
  ...Array.from({ length: 14 }, (_, i) => `Band ${i + 1}`),
  "Spectral centroid",
  "Dominant freq",
  "Total energy",
  "Temp mean",
];

export const FREQ_BANDS = 14;
export const BAND_START = 6; // index of first band-energy feature

export function bandEnergies(features: number[] | null): number[] {
  if (!features) return [];
  return features.slice(BAND_START, BAND_START + FREQ_BANDS);
}

export function fmtNum(v: number | null | undefined, digits = 3): string {
  if (v === null || v === undefined || Number.isNaN(v)) return "—";
  return v.toFixed(digits);
}

export function fmtRelative(iso: string | null | undefined): string {
  if (!iso) return "never";
  const then = new Date(iso).getTime();
  const diff = Date.now() - then;
  const s = Math.floor(diff / 1000);
  if (s < 60) return `${s}s ago`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ago`;
  const d = Math.floor(h / 24);
  return `${d}d ago`;
}

export function fmtDateTime(iso: string | null | undefined): string {
  if (!iso) return "—";
  return new Date(iso).toLocaleString();
}

export function sleepUsToMinutes(us: number): number {
  return Math.round((us / 1_000_000 / 60) * 100) / 100;
}

export function minutesToSleepUs(min: number): number {
  return Math.round(min * 60 * 1_000_000);
}
