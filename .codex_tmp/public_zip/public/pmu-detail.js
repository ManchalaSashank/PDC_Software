const key = new URLSearchParams(window.location.search).get("key");

const elements = {
  title: document.querySelector("#pmuTitle"),
  subtitle: document.querySelector("#pmuSubtitle"),
  statusBadge: document.querySelector("#statusBadge"),
  statusMeta: document.querySelector("#statusMeta"),
  frequencyValue: document.querySelector("#frequencyValue"),
  rocofValue: document.querySelector("#rocofValue"),
  packetsValue: document.querySelector("#packetsValue"),
  stationValue: document.querySelector("#stationValue"),
  phasorList: document.querySelector("#phasorList"),
  analogList: document.querySelector("#analogList"),
  configTable: document.querySelector("#configTable"),
  frequencyChart: document.querySelector("#frequencyChart"),
  rocofChart: document.querySelector("#rocofChart"),
  frequencyStats: document.querySelector("#frequencyStats"),
  rocofStats: document.querySelector("#rocofStats"),
  refreshSelect: document.querySelector("#refreshSelect"),
  windowSelect: document.querySelector("#windowSelect"),
  pauseChartsBtn: document.querySelector("#pauseChartsBtn")
};

let pendingChartPmu = null;
let pendingChannelPmu = null;
let chartsInitialized = false;
let chartRefreshMs = 3000;
let chartWindowSize = 40;
let chartsPaused = false;
let lastChartRenderAt = 0;
let channelRefreshMs = 2500;
let lastChannelRenderAt = 0;
const smoothedChannelValues = new Map();

function formatNumber(value, digits = 2) {
  return Number.isFinite(value) ? value.toFixed(digits) : "--";
}

function buildSparklinePath(values, width, height) {
  if (!values.length) {
    return "";
  }

  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;

  return values
    .map((value, index) => {
      const x = (index / Math.max(values.length - 1, 1)) * width;
      const y = height - ((value - min) / range) * (height - 4) - 2;
      return `${index === 0 ? "M" : "L"} ${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
}

function getSmoothedValue(channelKey, rawValue, alpha = 0.35) {
  const previous = smoothedChannelValues.get(channelKey);
  const smoothed = Number.isFinite(previous)
    ? previous + (rawValue - previous) * alpha
    : rawValue;
  smoothedChannelValues.set(channelKey, smoothed);
  return smoothed;
}

function renderBars(container, items, history, type, accessor, maxValue, unit = "") {
  container.innerHTML = "";
  if (!items.length) {
    container.innerHTML = `<article class="bar-item"><header><strong>No live data yet</strong><span>--</span></header></article>`;
    return;
  }

  items.forEach((item) => {
    const rawValue = accessor(item);
    const channelKey = `${key}:${type}:${item.label}`;
    const value = getSmoothedValue(channelKey, rawValue);
    const width = Math.max(8, Math.min(100, (value / maxValue) * 100));
    const series = history
      .slice(-12)
      .map((frame) => {
        const match = type === "phasor"
          ? frame.phasors?.find((entry) => entry.label === item.label)?.magnitude
          : frame.analogs?.find((entry) => entry.label === item.label)?.value;
        return match;
      })
      .filter(Number.isFinite);
    const previous = series.length > 1 ? series[series.length - 2] : rawValue;
    const delta = rawValue - previous;
    const deltaText = `${delta >= 0 ? "+" : ""}${formatNumber(delta, 2)}`;
    const deltaClass = delta > 0.02 ? "up" : delta < -0.02 ? "down" : "steady";
    const sparklinePath = buildSparklinePath(series, 140, 32);
    const wrapper = document.createElement("article");
    wrapper.className = "bar-item";
    wrapper.innerHTML = `
      <header>
        <div>
          <strong>${item.label}</strong>
          <div class="bar-subline">
            <span class="delta-chip ${deltaClass}">${deltaText}</span>
            <span>Raw ${formatNumber(rawValue, 2)}${unit ? ` ${unit}` : ""}</span>
          </div>
        </div>
        <span>${formatNumber(value, 2)}${unit ? ` ${unit}` : ""}</span>
      </header>
      <svg class="mini-sparkline" viewBox="0 0 140 32" preserveAspectRatio="none">
        <path d="${sparklinePath}" fill="none" stroke="rgba(83,167,255,0.95)" stroke-width="2.5" stroke-linecap="round"></path>
      </svg>
      <div class="bar-track">
        <div class="bar-fill" style="width: ${width}%"></div>
      </div>
    `;
    container.appendChild(wrapper);
  });
}

function renderChannelPanels(pmu) {
  if (!pmu) {
    renderBars(elements.phasorList, [], [], "phasor", (item) => item.magnitude, 260, "V");
    renderBars(elements.analogList, [], [], "analog", (item) => item.value, 10, "");
    return;
  }

  renderBars(
    elements.phasorList,
    pmu.latestFrame?.phasors || [],
    pmu.history || [],
    "phasor",
    (item) => item.magnitude,
    260,
    "V"
  );
  renderBars(
    elements.analogList,
    pmu.latestFrame?.analogs || [],
    pmu.history || [],
    "analog",
    (item) => item.value,
    10,
    ""
  );
  lastChannelRenderAt = Date.now();
}

function buildPath(points, width, height) {
  if (!points.length) {
    return "";
  }

  const values = points.map((point) => point.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;

  return points
    .map((point, index) => {
      const x = (index / Math.max(points.length - 1, 1)) * width;
      const y = height - ((point.value - min) / range) * (height - 20) - 10;
      return `${index === 0 ? "M" : "L"} ${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
}

function buildTicks(min, max, count = 4) {
  const ticks = [];
  const range = max - min || 1;

  for (let index = 0; index < count; index += 1) {
    const ratio = index / (count - 1);
    ticks.push({
      value: max - range * ratio,
      ratio
    });
  }

  return ticks;
}

function getVisibleHistory(history) {
  return history.slice(-chartWindowSize);
}

function buildTrendStats(history, keyName) {
  const values = history.map((item) => item[keyName]).filter(Number.isFinite);
  if (!values.length) {
    return null;
  }

  const total = values.reduce((sum, value) => sum + value, 0);
  return {
    latest: values[values.length - 1],
    min: Math.min(...values),
    max: Math.max(...values),
    avg: total / values.length
  };
}

function renderTrendStats(container, stats, unit = "") {
  if (!stats) {
    container.innerHTML = `<div class="trend-stat"><span>No stats yet</span><strong>--</strong></div>`;
    return;
  }

  const entries = [
    ["Latest", stats.latest],
    ["Min", stats.min],
    ["Max", stats.max],
    ["Average", stats.avg]
  ];

  container.innerHTML = entries.map(([label, value]) => `
    <div class="trend-stat">
      <span>${label}</span>
      <strong>${formatNumber(value, 4)}${unit ? ` ${unit}` : ""}</strong>
    </div>
  `).join("");
}

function renderChart(svg, history, keyName, color, unit = "") {
  const width = 640;
  const height = 240;
  const left = 56;
  const right = 18;
  const top = 12;
  const bottom = 32;
  const plotWidth = width - left - right;
  const plotHeight = height - top - bottom;
  const values = history.map((item) => item[keyName]).filter(Number.isFinite);

  if (!values.length) {
    svg.innerHTML = `
      <text x="${width / 2}" y="${height / 2}" text-anchor="middle" fill="#96afc2" font-size="14">
        Waiting for enough samples...
      </text>
    `;
    return;
  }

  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const points = history.map((item, index) => {
    const x = left + (index / Math.max(history.length - 1, 1)) * plotWidth;
    const y = top + plotHeight - ((item[keyName] - min) / range) * plotHeight;
    return { x, y, value: item[keyName] };
  });
  const path = points
    .map((point, index) => `${index === 0 ? "M" : "L"} ${point.x.toFixed(2)} ${point.y.toFixed(2)}`)
    .join(" ");
  const yTicks = buildTicks(min, max);
  const xTickIndexes = [0, Math.max(0, Math.floor((history.length - 1) / 2)), Math.max(0, history.length - 1)];

  svg.innerHTML = `
    ${yTicks.map((tick) => {
      const y = top + tick.ratio * plotHeight;
      return `
        <line x1="${left}" y1="${y}" x2="${width - right}" y2="${y}" stroke="rgba(255,255,255,0.08)" stroke-width="1"></line>
        <text x="${left - 8}" y="${y + 4}" text-anchor="end" fill="#96afc2" font-size="12">
          ${formatNumber(tick.value, 4)}${unit ? ` ${unit}` : ""}
        </text>
      `;
    }).join("")}
    <line x1="${left}" y1="${top}" x2="${left}" y2="${height - bottom}" stroke="rgba(255,255,255,0.18)" stroke-width="1.2"></line>
    <line x1="${left}" y1="${height - bottom}" x2="${width - right}" y2="${height - bottom}" stroke="rgba(255,255,255,0.18)" stroke-width="1.2"></line>
    ${xTickIndexes.map((tickIndex) => {
      const point = points[tickIndex];
      const time = new Date(history[tickIndex].timestamp).toLocaleTimeString([], {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
      });
      return `
        <line x1="${point.x}" y1="${height - bottom}" x2="${point.x}" y2="${height - bottom + 6}" stroke="rgba(255,255,255,0.18)" stroke-width="1"></line>
        <text x="${point.x}" y="${height - 8}" text-anchor="middle" fill="#96afc2" font-size="11">${time}</text>
      `;
    }).join("")}
    <path d="${path}" fill="none" stroke="${color}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"></path>
    ${points.map((point) => `
      <circle cx="${point.x}" cy="${point.y}" r="2.4" fill="${color}"></circle>
    `).join("")}
  `;
}

function renderCharts(pmu) {
  if (!pmu) {
    renderChart(elements.frequencyChart, [], "frequency", "#53a7ff", "Hz");
    renderChart(elements.rocofChart, [], "rocof", "#3fd0b4");
    renderTrendStats(elements.frequencyStats, null, "Hz");
    renderTrendStats(elements.rocofStats, null);
    return;
  }

  const visibleHistory = getVisibleHistory(pmu.history || []);
  renderChart(elements.frequencyChart, visibleHistory, "frequency", "#53a7ff", "Hz");
  renderChart(elements.rocofChart, visibleHistory, "rocof", "#3fd0b4");
  renderTrendStats(elements.frequencyStats, buildTrendStats(visibleHistory, "frequency"), "Hz");
  renderTrendStats(elements.rocofStats, buildTrendStats(visibleHistory, "rocof"));
  lastChartRenderAt = Date.now();
}

function renderConfig(config, connection) {
  const entries = {
    "Station Name": config.stationName,
    "Frame Format": config.format,
    "Phasors": config.phasorCount,
    "Analogs": config.analogCount,
    "Digital": config.digitalCount,
    "Time Base": config.timeBase,
    "PMU Count": config.numPmu,
    "Data Rate": `${connection.dataRate} fps`
  };

  elements.configTable.innerHTML = "";
  Object.entries(entries).forEach(([label, value]) => {
    const item = document.createElement("div");
    item.className = "config-item";
    item.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
    elements.configTable.appendChild(item);
  });
}

function renderPmu(pmu) {
  if (!pmu) {
    elements.title.textContent = "PMU Not Found";
    elements.subtitle.textContent = "Open this window from the PDC page so it receives a valid PMU key.";
    renderCharts(null);
    renderChannelPanels(null);
    return;
  }

  elements.title.textContent = `PMU ${pmu.connection.pmuId}`;
  elements.subtitle.textContent = `${pmu.connection.ipAddress}:${pmu.connection.port}`;
  elements.statusBadge.textContent = pmu.connection.status;
  elements.statusBadge.className = `status-badge ${pmu.connection.status}`;
  elements.statusMeta.textContent = pmu.connection.lastError || pmu.connection.message || "Waiting for data.";
  elements.packetsValue.textContent = pmu.connection.packetsReceived;
  elements.stationValue.textContent = pmu.config.stationName;

  if (!pmu.latestFrame) {
    elements.frequencyValue.textContent = "--";
    elements.rocofValue.textContent = "--";
  } else {
    elements.frequencyValue.textContent = `${formatNumber(pmu.latestFrame.frequency, 4)} Hz`;
    elements.rocofValue.textContent = formatNumber(pmu.latestFrame.rocof, 4);
  }

  renderConfig(pmu.config, pmu.connection);
  if (!chartsInitialized) {
    renderCharts(pmu);
    renderChannelPanels(pmu);
    chartsInitialized = true;
    pendingChartPmu = null;
    pendingChannelPmu = null;
  } else {
    pendingChartPmu = pmu;
    pendingChannelPmu = pmu;
  }
}

function updatePauseButton() {
  elements.pauseChartsBtn.textContent = chartsPaused ? "Resume Charts" : "Pause Charts";
}

async function loadPmu() {
  if (!key) {
    renderPmu(null);
    return;
  }

  const response = await fetch(`/api/pmus/${encodeURIComponent(key)}`);
  if (!response.ok) {
    renderPmu(null);
    return;
  }

  renderPmu(await response.json());
}

const events = new EventSource("/api/stream");
events.addEventListener("dashboard", (event) => {
  if (!key) {
    return;
  }
  const snapshot = JSON.parse(event.data);
  renderPmu(snapshot.pmus.find((pmu) => pmu.key === key) || null);
});

elements.refreshSelect.addEventListener("change", () => {
  chartRefreshMs = Number.parseInt(elements.refreshSelect.value, 10);
  channelRefreshMs = Math.max(1500, chartRefreshMs - 500);
});

elements.windowSelect.addEventListener("change", () => {
  chartWindowSize = Number.parseInt(elements.windowSelect.value, 10);
  if (pendingChartPmu) {
    renderCharts(pendingChartPmu);
  }
});

elements.pauseChartsBtn.addEventListener("click", () => {
  chartsPaused = !chartsPaused;
  updatePauseButton();
  if (!chartsPaused && pendingChartPmu) {
    renderCharts(pendingChartPmu);
    pendingChartPmu = null;
  }
});

setInterval(() => {
  if (!chartsPaused && pendingChartPmu && Date.now() - lastChartRenderAt >= chartRefreshMs) {
    renderCharts(pendingChartPmu);
    pendingChartPmu = null;
  }
}, 400);

setInterval(() => {
  if (pendingChannelPmu && Date.now() - lastChannelRenderAt >= channelRefreshMs) {
    renderChannelPanels(pendingChannelPmu);
    pendingChannelPmu = null;
  }
}, 400);

updatePauseButton();
loadPmu();
