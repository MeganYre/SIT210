/**
 * ThingSpeak vitals dashboard — auto-starts from thingspeak_config.js
 * Serve: npx --yes serve .
 */

const els = {
  statusPill: document.getElementById("statusPill"),
  valueBpm: document.getElementById("valueBpm"),
  valueSpo2: document.getElementById("valueSpo2"),
  lastUpdated: document.getElementById("lastUpdated"),
  canvas: document.getElementById("trendCanvas"),
};

/** @type {number | null} */
let pollTimer = null;

/** @type {{ bpm: number; spo2: number }[]} */
let history = [];
const HISTORY_MAX = 40;

function getConfig() {
  const d = window.THINGSPEAK_DEFAULTS || {};
  return {
    channelId: d.channelId,
    readKey: d.readApiKey || "",
    intervalSec: d.refreshIntervalSec ?? 5,
  };
}

function setStatus(state, label) {
  els.statusPill.dataset.state = state;
  els.statusPill.textContent = label;
}

function buildFeedsUrl(channelId, readKey, results) {
  const base = `https://api.thingspeak.com/channels/${encodeURIComponent(
    String(channelId)
  )}/feeds.json`;
  const params = new URLSearchParams();
  params.set("results", String(results));
  if (readKey) params.set("api_key", readKey);
  return `${base}?${params.toString()}`;
}

async function fetchFeeds(channelId, readKey) {
  const url = buildFeedsUrl(channelId, readKey, HISTORY_MAX);
  const res = await fetch(url);
  if (!res.ok) {
    const text = await res.text().catch(() => "");
    throw new Error(`HTTP ${res.status} ${text.slice(0, 120)}`);
  }
  return res.json();
}

function parseNumber(v) {
  if (v == null || v === "") return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

function applyLatestFeed(feeds) {
  if (!feeds || !feeds.length) {
    els.valueBpm.textContent = "—";
    els.valueSpo2.textContent = "—";
    els.lastUpdated.textContent = "No entries yet.";
    return;
  }

  const last = feeds[feeds.length - 1];
  const bpm = parseNumber(last.field1);
  const spo2 = parseNumber(last.field2);

  els.valueBpm.textContent = bpm != null ? String(Math.round(bpm)) : "—";
  els.valueSpo2.textContent = spo2 != null ? String(Math.round(spo2)) : "—";

  const created = last.created_at ? new Date(last.created_at) : null;
  els.lastUpdated.textContent = created
    ? `Last feed: ${created.toLocaleString()}`
    : "Last feed: (no timestamp)";

  history = feeds
    .map((f) => ({
      bpm: parseNumber(f.field1),
      spo2: parseNumber(f.field2),
    }))
    .filter((p) => p.bpm != null || p.spo2 != null);

  drawChart();
}

function drawChart() {
  const canvas = els.canvas;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;

  const w = canvas.width;
  const h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#0e1116";
  ctx.fillRect(0, 0, w, h);

  if (history.length < 2) {
    ctx.fillStyle = "#8b97ad";
    ctx.font = "14px Segoe UI, system-ui, sans-serif";
    ctx.fillText("Waiting for more samples…", 16, h / 2);
    return;
  }

  const padL = 44;
  const padR = 16;
  const padT = 20;
  const padB = 28;
  const innerW = w - padL - padR;
  const innerH = h - padT - padB;

  const bpms = history.map((p) => p.bpm).filter((n) => n != null);
  const spo2s = history.map((p) => p.spo2).filter((n) => n != null);
  if (!bpms.length && !spo2s.length) return;

  let minBpm = Math.min(...bpms);
  let maxBpm = Math.max(...bpms);
  if (minBpm === maxBpm) {
    minBpm -= 5;
    maxBpm += 5;
  }

  let minSp = Math.min(...spo2s);
  let maxSp = Math.max(...spo2s);
  if (minSp === maxSp) {
    minSp -= 1;
    maxSp += 1;
  }

  ctx.strokeStyle = "#2a3344";
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = padT + (innerH * i) / 4;
    ctx.beginPath();
    ctx.moveTo(padL, y);
    ctx.lineTo(padL + innerW, y);
    ctx.stroke();
  }

  function lineSeries(accessor, color, yMin, yMax) {
    const pts = [];
    for (let i = 0; i < history.length; i++) {
      const v = accessor(history[i]);
      if (v == null) continue;
      const t = i / (history.length - 1);
      const x = padL + innerW * t;
      const yn = (v - yMin) / (yMax - yMin);
      const y = padT + innerH * (1 - yn);
      pts.push({ x, y });
    }
    if (pts.length < 2) return;
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(pts[0].x, pts[0].y);
    for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
    ctx.stroke();
  }

  lineSeries((p) => p.bpm, "#7aa2ff", minBpm, maxBpm);
  lineSeries((p) => p.spo2, "#3dd6c3", minSp, maxSp);

  ctx.fillStyle = "#8b97ad";
  ctx.font = "12px Segoe UI, system-ui, sans-serif";
  ctx.fillText("BPM (blue)", padL, h - 10);
  ctx.fillText("SpO₂ % (teal)", padL + 100, h - 10);
}

async function pollOnce(channelId, readKey) {
  const data = await fetchFeeds(channelId, readKey);
  applyLatestFeed(data.feeds || []);
  setStatus("ok", "Live");
}

function startPolling() {
  const { channelId, readKey, intervalSec } = getConfig();
  if (!channelId) {
    setStatus("err", "No channel");
    return;
  }

  const ms = Math.max(1000, intervalSec * 1000);

  const tick = () => {
    pollOnce(channelId, readKey).catch((err) => {
      console.error(err);
      setStatus("err", "Error");
      els.lastUpdated.textContent = String(err.message || err);
    });
  };

  tick();
  pollTimer = window.setInterval(tick, ms);
  setStatus("warn", "Connecting…");
}

document.addEventListener("DOMContentLoaded", startPolling);
