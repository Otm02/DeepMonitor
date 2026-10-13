import { useMemo } from "react";
import { Link, useParams } from "react-router-dom";
import type { EChartsOption } from "echarts";
import { api } from "../api";
import { useFetch } from "../lib/useFetch";
import { EChart } from "../components/EChart";
import { Card, ErrorNote, Loading, Stat, StatusBadge } from "../components/ui";
import { BAND_START, FREQ_BANDS, bandEnergies, fmtNum, fmtRelative } from "../lib/format";

const ACCENT = "#5b8def";

export function DeviceDetail() {
  const { id = "" } = useParams();
  const device = useFetch(() => api.getDevice(id), [id], 15000);
  const readings = useFetch(() => api.getReadings(id, { limit: 500 }), [id], 15000);

  const rows = useMemo(() => readings.data ?? [], [readings.data]);
  const times = useMemo(() => rows.map((r) => new Date(r.received_at).toLocaleTimeString()), [rows]);

  const reconOption: EChartsOption = useMemo(() => ({
    backgroundColor: "transparent",
    tooltip: { trigger: "axis" },
    legend: { data: ["Recon error", "Threshold"] },
    grid: { left: 50, right: 20, top: 40, bottom: 40 },
    xAxis: { type: "category", data: times },
    yAxis: { type: "value", scale: true },
    series: [
      {
        name: "Recon error",
        type: "line",
        showSymbol: false,
        data: rows.map((r) => r.reconstruction_error),
        lineStyle: { color: ACCENT },
        itemStyle: { color: ACCENT },
        markPoint: {
          data: rows.flatMap((r, i) =>
            r.is_anomaly && r.reconstruction_error != null
              ? [{ name: "anomaly", coord: [i, r.reconstruction_error] as [number, number], value: "!" }]
              : []
          ),
          itemStyle: { color: "#e5534b" },
          symbolSize: 36,
        },
      },
      {
        name: "Threshold",
        type: "line",
        showSymbol: false,
        data: rows.map((r) => r.anomaly_threshold),
        lineStyle: { color: "#e0a23b", type: "dashed" },
        itemStyle: { color: "#e0a23b" },
      },
    ],
  }), [rows, times]);

  const vibTempOption: EChartsOption = useMemo(() => ({
    backgroundColor: "transparent",
    tooltip: { trigger: "axis" },
    legend: { data: ["RMS vibration", "Temperature"] },
    grid: { left: 50, right: 50, top: 40, bottom: 40 },
    xAxis: { type: "category", data: times },
    yAxis: [
      { type: "value", name: "RMS", scale: true },
      { type: "value", name: "°C", scale: true },
    ],
    series: [
      {
        name: "RMS vibration",
        type: "line",
        showSymbol: false,
        data: rows.map((r) => (r.features ? r.features[0] : null)),
        lineStyle: { color: "#43b581" },
        itemStyle: { color: "#43b581" },
      },
      {
        name: "Temperature",
        type: "line",
        yAxisIndex: 1,
        showSymbol: false,
        data: rows.map((r) => (r.features ? r.features[r.features.length - 1] : null)),
        lineStyle: { color: "#e0a23b" },
        itemStyle: { color: "#e0a23b" },
      },
    ],
  }), [rows, times]);

  const batteryOption: EChartsOption = useMemo(() => ({
    backgroundColor: "transparent",
    tooltip: { trigger: "axis" },
    grid: { left: 50, right: 20, top: 20, bottom: 40 },
    xAxis: { type: "category", data: times },
    yAxis: { type: "value", scale: true, name: "V" },
    series: [
      {
        name: "Battery",
        type: "line",
        showSymbol: false,
        areaStyle: { opacity: 0.15 },
        data: rows.map((r) => r.battery_voltage),
        lineStyle: { color: "#9b6cff" },
        itemStyle: { color: "#9b6cff" },
      },
    ],
  }), [rows, times]);

  const latestFeatures = useMemo(() => {
    for (let i = rows.length - 1; i >= 0; i--) {
      if (rows[i].features) return rows[i].features;
    }
    return null;
  }, [rows]);

  const spectrumOption: EChartsOption = useMemo(() => ({
    backgroundColor: "transparent",
    tooltip: { trigger: "axis" },
    grid: { left: 50, right: 20, top: 20, bottom: 40 },
    xAxis: { type: "category", data: Array.from({ length: FREQ_BANDS }, (_, i) => `B${i + 1}`) },
    yAxis: { type: "value", name: "log energy" },
    series: [
      {
        type: "bar",
        data: bandEnergies(latestFeatures),
        itemStyle: { color: ACCENT },
      },
    ],
  }), [latestFeatures]);

  const spectrogramOption: EChartsOption = useMemo(() => {
    const withFeat = rows.filter((r) => r.features);
    const data: [number, number, number][] = [];
    let maxVal = 0.0001;
    withFeat.forEach((r, ti) => {
      for (let b = 0; b < FREQ_BANDS; b++) {
        const v = r.features ? r.features[BAND_START + b] : 0;
        data.push([ti, b, v]);
        if (v > maxVal) maxVal = v;
      }
    });
    return {
      backgroundColor: "transparent",
      tooltip: { position: "top" },
      grid: { left: 60, right: 20, top: 20, bottom: 40 },
      xAxis: { type: "category", data: withFeat.map((r) => new Date(r.received_at).toLocaleTimeString()) },
      yAxis: { type: "category", data: Array.from({ length: FREQ_BANDS }, (_, i) => `B${i + 1}`) },
      visualMap: {
        min: 0,
        max: maxVal,
        calculable: true,
        orient: "horizontal",
        left: "center",
        bottom: 0,
        inRange: { color: ["#0b1020", "#1f3a8a", "#5b8def", "#e0a23b", "#e5534b"] },
      },
      series: [{ type: "heatmap", data, emphasis: { itemStyle: { borderColor: "#fff" } } }],
    };
  }, [rows]);

  if (device.loading && !device.data) return <Loading what="device" />;
  if (device.error) return <ErrorNote error={device.error} />;
  const d = device.data!;

  return (
    <div className="page">
      <div className="page-head">
        <div>
          <h1>{d.name}</h1>
          <div className="muted">{d.device_id} · <StatusBadge status={d.status} /></div>
        </div>
        <Link className="btn" to={`/controls?device=${encodeURIComponent(d.device_id)}`}>Configure</Link>
      </div>

      <div className="stat-row">
        <Stat label="Battery" value={`${fmtNum(d.last_battery, 2)} V`} />
        <Stat label="Recon error" value={fmtNum(d.last_recon_error, 4)} />
        <Stat label="Threshold" value={fmtNum(d.last_threshold, 4)} />
        <Stat label="Uptime" value={`${d.uptime_cycles} cyc`} />
        <Stat label="Last seen" value={fmtRelative(d.last_seen)} />
      </div>

      <Card title="Reconstruction error vs adaptive threshold">
        {rows.length ? <EChart option={reconOption} /> : <div className="muted">No readings yet.</div>}
      </Card>

      <div className="grid-2">
        <Card title="RMS vibration & temperature"><EChart option={vibTempOption} /></Card>
        <Card title="Battery"><EChart option={batteryOption} /></Card>
      </div>

      <div className="grid-2">
        <Card title="Latest FFT band spectrum"><EChart option={spectrumOption} /></Card>
        <Card title="Spectrogram (bands over time)"><EChart option={spectrogramOption} height={360} /></Card>
      </div>
    </div>
  );
}
