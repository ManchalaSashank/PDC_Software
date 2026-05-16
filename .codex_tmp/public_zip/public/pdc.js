const elements = {
  form: document.querySelector("#pmuForm"),
  ipAddress: document.querySelector("#ipAddress"),
  port: document.querySelector("#port"),
  pmuId: document.querySelector("#pmuId"),
  pmuList: document.querySelector("#pmuList"),
  totalPmus: document.querySelector("#totalPmus"),
  connectedPmus: document.querySelector("#connectedPmus"),
  totalPackets: document.querySelector("#totalPackets"),
  activeStation: document.querySelector("#activeStation"),
  pdcSummary: document.querySelector("#pdcSummary")
};

let latestSnapshot = { pmus: [] };
const pendingActions = new Map();
let renderScheduled = false;

function requestRender() {
  if (renderScheduled) {
    return;
  }

  renderScheduled = true;
  window.setTimeout(() => {
    renderScheduled = false;
    renderDashboard(latestSnapshot, { skipSnapshotWrite: true });
  }, 400);
}

function setPending(key, action) {
  pendingActions.set(key, action);
  requestRender();
}

function clearPending(key) {
  pendingActions.delete(key);
  requestRender();
}

function openPmuWindow(key) {
  const url = `/pmu?key=${encodeURIComponent(key)}`;
  window.open(url, `pmu_${key.replace(/[^\w]/g, "_")}`, "noopener");
}

async function disconnectPmu(key) {
  if (pendingActions.has(key)) {
    return;
  }

  setPending(key, "disconnecting");

  try {
    const response = await fetch(`/api/pmus/${encodeURIComponent(key)}/disconnect`, { method: "POST" });
    const data = await response.json();
    latestSnapshot = data;
    renderDashboard(data);
  } finally {
    clearPending(key);
  }
}

async function reconnectPmu(key) {
  if (pendingActions.has(key)) {
    return;
  }

  const pmu = (latestSnapshot.pmus || []).find((item) => item.key === key);

  if (!pmu) {
    window.alert("PMU not found for reconnect.");
    return;
  }

  setPending(key, "connecting");

  try {
    const response = await fetch("/api/pmus", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        ipAddress: pmu.connection.ipAddress,
        port: pmu.connection.port,
        pmuId: pmu.connection.pmuId
      })
    });

    const data = await response.json();
    latestSnapshot = data;
    renderDashboard(data);

    if (!response.ok) {
      window.alert(data.message || "Unable to reconnect PMU.");
    }
  } finally {
    clearPending(key);
  }
}

function renderRegistry(pmus) {
  elements.pmuList.innerHTML = "";

  if (!pmus.length) {
    elements.pmuList.innerHTML = `<article class="pmu-card"><h3>No PMUs yet</h3><p>Add a PMU above to start the PDC connection.</p></article>`;
    return;
  }

  pmus.forEach((pmu) => {
    const pendingAction = pendingActions.get(pmu.key);
    const isBusy = Boolean(pendingAction);
    const actionHtml = pmu.connection.status === "disconnected"
      ? `<button type="button" ${isBusy ? "disabled" : ""} onclick="window.pdcActions.reconnect('${pmu.key}')">${pendingAction === "connecting" ? "Connecting..." : "Connect"}</button>`
      : `<button type="button" class="ghost" ${isBusy ? "disabled" : ""} onclick="window.pdcActions.disconnect('${pmu.key}')">${pendingAction === "disconnecting" ? "Disconnecting..." : "Disconnect"}</button>`;
    const card = document.createElement("article");
    card.className = "pmu-card";
    card.innerHTML = `
      <div class="pmu-card-top">
        <div>
          <h3>PMU ${pmu.connection.pmuId}</h3>
          <p>${pmu.connection.ipAddress}:${pmu.connection.port}</p>
        </div>
        <span class="status-badge ${pmu.connection.status}">${pmu.connection.status}</span>
      </div>
      <div class="pmu-card-grid">
        <div><span>Station</span><strong>${pmu.config.stationName}</strong></div>
        <div><span>Packets</span><strong>${pmu.connection.packetsReceived}</strong></div>
        <div><span>Frequency</span><strong>${pmu.latestFrame ? `${pmu.latestFrame.frequency.toFixed(4)} Hz` : "--"}</strong></div>
        <div><span>ROCOF</span><strong>${pmu.latestFrame ? pmu.latestFrame.rocof.toFixed(4) : "--"}</strong></div>
      </div>
      <p class="pmu-message">${pendingAction ? `Action in progress: ${pendingAction}` : (pmu.connection.lastError || pmu.connection.message)}</p>
      <div class="form-actions">
        <a class="action-link" href="/pmu?key=${encodeURIComponent(pmu.key)}" target="_blank" rel="noopener">Open Window</a>
        ${actionHtml}
      </div>
    `;
    elements.pmuList.appendChild(card);
  });
}

function renderDashboard(snapshot, options = {}) {
  if (!options.skipSnapshotWrite) {
    latestSnapshot = snapshot;
  }

  const pmus = snapshot.pmus || [];
  const connected = pmus.filter((pmu) => pmu.connection.status === "connected").length;
  const packets = pmus.reduce((sum, pmu) => sum + pmu.connection.packetsReceived, 0);
  const active = pmus.find((pmu) => pmu.connection.status === "connected");

  elements.totalPmus.textContent = pmus.length;
  elements.connectedPmus.textContent = connected;
  elements.totalPackets.textContent = packets;
  elements.activeStation.textContent = active?.config.stationName || "--";
  elements.pdcSummary.textContent =
    pmus.length
      ? `${connected} of ${pmus.length} PMUs connected. Click any PMU to open its dedicated window.`
      : "No PMUs connected yet.";

  renderRegistry(pmus);
}

async function loadDashboard() {
  const response = await fetch("/api/dashboard");
  renderDashboard(await response.json());
}

elements.form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const submitButton = elements.form.querySelector("button[type='submit']");
  submitButton.disabled = true;
  submitButton.textContent = "Adding...";

  try {
    const response = await fetch("/api/pmus", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        ipAddress: elements.ipAddress.value,
        port: elements.port.value,
        pmuId: elements.pmuId.value
      })
    });

    const data = await response.json();
    latestSnapshot = data;
    renderDashboard(data);

    if (!response.ok) {
      window.alert(data.message || "Unable to connect PMU.");
      return;
    }

    openPmuWindow(data.pmu.key);
  } finally {
    submitButton.disabled = false;
    submitButton.textContent = "Add PMU";
  }
});

window.pdcActions = {
  disconnect: disconnectPmu,
  open: openPmuWindow,
  reconnect: reconnectPmu
};

const events = new EventSource("/api/stream");
events.addEventListener("dashboard", (event) => {
  latestSnapshot = JSON.parse(event.data);
  requestRender();
});

loadDashboard();
