import http from "node:http";
import net from "node:net";
import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const publicDir = path.join(__dirname, "public");

const MAX_HISTORY = 120;
const preferredPort = Number.parseInt(process.env.PORT || "3000", 10);

const CMD_TURN_OFF_TX = 0x0001;
const CMD_TURN_ON_TX = 0x0002;
const CMD_SEND_CFG2 = 0x0005;

const TYPE_DATA = 0x01;
const TYPE_CFG2 = 0x31;

const pmuRegistry = new Map();
const streamClients = new Set();

function defaultConfig() {
  return {
    stationName: "--",
    format: "--",
    phasorCount: 0,
    analogCount: 0,
    digitalCount: 0,
    timeBase: 0,
    numPmu: 0,
    phasorLabels: [],
    analogLabels: []
  };
}

function createPmuRecord(input = {}) {
  const ipAddress = typeof input.ipAddress === "string" && input.ipAddress.trim()
    ? input.ipAddress.trim()
    : "127.0.0.1";
  const port = Number.isFinite(Number.parseInt(input.port, 10))
    ? Number.parseInt(input.port, 10)
    : 4712;
  const pmuId = Number.isFinite(Number.parseInt(input.pmuId, 10))
    ? Number.parseInt(input.pmuId, 10)
    : 1;
  const key = `${ipAddress}:${port}:${pmuId}`;

  return {
    key,
    socket: null,
    receiveBuffer: Buffer.alloc(0),
    waitingForConfig: false,
    connection: {
      key,
      ipAddress,
      port,
      pmuId,
      status: "disconnected",
      lastConnectedAt: null,
      packetsReceived: 0,
      dataRate: 0,
      message: "Ready to connect.",
      lastError: null
    },
    config: defaultConfig(),
    latestFrame: null,
    history: []
  };
}

function toSerializablePmu(record) {
  return {
    key: record.key,
    connection: record.connection,
    config: record.config,
    latestFrame: record.latestFrame,
    history: record.history
  };
}

function getDashboardSnapshot() {
  return {
    pmus: Array.from(pmuRegistry.values())
      .map(toSerializablePmu)
      .sort((left, right) => left.connection.pmuId - right.connection.pmuId)
  };
}

function clampHistory(record) {
  if (record.history.length > MAX_HISTORY) {
    record.history.splice(0, record.history.length - MAX_HISTORY);
  }
}

function sendJson(res, statusCode, payload) {
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store"
  });
  res.end(JSON.stringify(payload));
}

function broadcast(event, payload) {
  const body = `event: ${event}\ndata: ${JSON.stringify(payload)}\n\n`;
  for (const client of streamClients) {
    client.write(body);
  }
}

function broadcastDashboard(event = "dashboard") {
  broadcast(event, getDashboardSnapshot());
}

function calculateCrc(buffer) {
  let crc = 0xffff;

  for (const byte of buffer) {
    crc ^= byte << 8;
    for (let index = 0; index < 8; index += 1) {
      if (crc & 0x8000) {
        crc = ((crc << 1) ^ 0x1021) & 0xffff;
      } else {
        crc = (crc << 1) & 0xffff;
      }
    }
  }

  return crc;
}

function buildCommandFrame(pmuId, commandCode) {
  const frame = Buffer.alloc(18);
  frame[0] = 0xaa;
  frame[1] = 0x41;
  frame.writeUInt16BE(frame.length, 2);
  frame.writeUInt16BE(pmuId, 4);
  frame.writeUInt32BE(Math.floor(Date.now() / 1000), 6);
  frame.writeUInt32BE(0, 10);
  frame.writeUInt16BE(commandCode, 14);
  frame.writeUInt16BE(calculateCrc(frame.subarray(0, 16)), 16);
  return frame;
}

function trimPaddedText(buffer, start, length) {
  return buffer.toString("ascii", start, start + length).replace(/\0/g, "").trim();
}

function formatBitsToText(format) {
  const valueEncoding = format & (1 << 0) ? "FLOAT" : "FIXED";
  const phasorEncoding = format & (1 << 1) ? "POLAR" : "RECTANGULAR";
  return `${valueEncoding}_${phasorEncoding}`;
}

function updateRecord(record, patch) {
  Object.assign(record.connection, patch);
  broadcastDashboard("dashboard");
}

function resetTelemetry(record) {
  record.config = defaultConfig();
  record.latestFrame = null;
  record.history = [];
  record.connection.packetsReceived = 0;
  record.connection.dataRate = 0;
}

function parseCfg2Frame(frame) {
  let offset = 0;
  offset += 2;
  offset += 2;
  offset += 2;
  offset += 4;
  offset += 4;
  const timeBase = frame.readUInt32BE(offset);
  offset += 4;
  const numPmu = frame.readUInt16BE(offset);
  offset += 2;

  if (numPmu < 1) {
    return null;
  }

  const stationName = trimPaddedText(frame, offset, 16);
  offset += 16;
  const pmuId = frame.readUInt16BE(offset);
  offset += 2;
  const format = frame.readUInt16BE(offset);
  offset += 2;
  const phasorCount = frame.readUInt16BE(offset);
  offset += 2;
  const analogCount = frame.readUInt16BE(offset);
  offset += 2;
  const digitalCount = frame.readUInt16BE(offset);
  offset += 2;

  const phasorLabels = [];
  for (let index = 0; index < phasorCount; index += 1) {
    phasorLabels.push(trimPaddedText(frame, offset, 16) || `Phasor ${index + 1}`);
    offset += 16;
  }

  const analogLabels = [];
  for (let index = 0; index < analogCount; index += 1) {
    analogLabels.push(trimPaddedText(frame, offset, 16) || `Analog ${index + 1}`);
    offset += 16;
  }

  offset += phasorCount * 4;
  offset += analogCount * 4;
  offset += 2;
  offset += 2;
  const dataRate = frame.readUInt16BE(offset);

  return {
    stationName,
    pmuId,
    format: formatBitsToText(format),
    phasorCount,
    analogCount,
    digitalCount,
    timeBase,
    numPmu,
    phasorLabels,
    analogLabels,
    dataRate
  };
}

function parseDataFrame(record, frame) {
  const pmuId = frame.readUInt16BE(4);
  const soc = frame.readUInt32BE(6);
  const fracsec = frame.readUInt32BE(10);
  let offset = 16;

  const phasors = [];
  for (let index = 0; index < record.config.phasorCount; index += 1) {
    const magnitude = frame.readFloatBE(offset);
    offset += 4;
    const angleRad = frame.readFloatBE(offset);
    offset += 4;

    phasors.push({
      label: record.config.phasorLabels[index] || `Phasor ${index + 1}`,
      magnitude: Number(magnitude.toFixed(2)),
      angleDeg: Number((angleRad * 180 / Math.PI).toFixed(2))
    });
  }

  const frequency = Number(frame.readFloatBE(offset).toFixed(4));
  offset += 4;
  const rocof = Number(frame.readFloatBE(offset).toFixed(4));
  offset += 4;

  const analogs = [];
  for (let index = 0; index < record.config.analogCount; index += 1) {
    const value = Number(frame.readFloatBE(offset).toFixed(2));
    offset += 4;
    analogs.push({
      label: record.config.analogLabels[index] || `Analog ${index + 1}`,
      value
    });
  }

  return {
    timestamp: new Date(soc * 1000 + Math.floor(fracsec / 1000)).toISOString(),
    pmuId,
    soc,
    fracsec,
    frequency,
    rocof,
    phasors,
    analogs
  };
}

function handleFrame(record, frame) {
  const expectedCrc = frame.readUInt16BE(frame.length - 2);
  const actualCrc = calculateCrc(frame.subarray(0, frame.length - 2));

  if (expectedCrc !== actualCrc) {
    updateRecord(record, {
      message: "Received a frame with invalid CRC.",
      lastError: "CRC validation failed."
    });
    return;
  }

  const frameType = frame[1];

  if (frameType === TYPE_CFG2) {
    const parsed = parseCfg2Frame(frame);
    if (!parsed) {
      return;
    }

    record.config = {
      stationName: parsed.stationName,
      format: parsed.format,
      phasorCount: parsed.phasorCount,
      analogCount: parsed.analogCount,
      digitalCount: parsed.digitalCount,
      timeBase: parsed.timeBase,
      numPmu: parsed.numPmu,
      phasorLabels: parsed.phasorLabels,
      analogLabels: parsed.analogLabels
    };

    record.connection.dataRate = parsed.dataRate;
    record.connection.message = `Connected to ${parsed.stationName || "PMU"} and received CFG2.`;
    record.connection.lastError = null;
    broadcastDashboard("dashboard");

    if (record.waitingForConfig && record.socket && !record.socket.destroyed) {
      record.waitingForConfig = false;
      record.socket.write(buildCommandFrame(record.connection.pmuId, CMD_TURN_ON_TX));
    }

    return;
  }

  if (frameType === TYPE_DATA) {
    const parsed = parseDataFrame(record, frame);
    record.connection.packetsReceived += 1;
    record.latestFrame = parsed;
    record.history.push(parsed);
    clampHistory(record);
    record.connection.message = `Receiving live data from ${record.connection.ipAddress}:${record.connection.port}.`;
    record.connection.lastError = null;
    broadcastDashboard("dashboard");
  }
}

function processIncomingData(record, chunk) {
  record.receiveBuffer = Buffer.concat([record.receiveBuffer, chunk]);

  while (record.receiveBuffer.length >= 4) {
    if (record.receiveBuffer[0] !== 0xaa) {
      const nextSync = record.receiveBuffer.indexOf(0xaa, 1);
      record.receiveBuffer = nextSync === -1 ? Buffer.alloc(0) : record.receiveBuffer.subarray(nextSync);
      continue;
    }

    const frameSize = record.receiveBuffer.readUInt16BE(2);
    if (frameSize < 10 || frameSize > 65535) {
      record.receiveBuffer = record.receiveBuffer.subarray(1);
      continue;
    }

    if (record.receiveBuffer.length < frameSize) {
      break;
    }

    const frame = record.receiveBuffer.subarray(0, frameSize);
    record.receiveBuffer = record.receiveBuffer.subarray(frameSize);
    handleFrame(record, frame);
  }
}

function disconnectPmu(key, manual = true) {
  const record = pmuRegistry.get(key);
  if (!record) {
    return false;
  }

  record.waitingForConfig = false;
  record.receiveBuffer = Buffer.alloc(0);

  if (record.socket && !record.socket.destroyed) {
    try {
      record.socket.write(buildCommandFrame(record.connection.pmuId, CMD_TURN_OFF_TX));
    } catch {
      // Ignore shutdown write issues.
    }
    record.socket.destroy();
  }

  record.socket = null;
  record.connection.status = "disconnected";
  record.connection.message = manual ? "Disconnected from PMU." : "PMU connection closed.";
  if (manual) {
    record.connection.lastError = null;
  }
  broadcastDashboard("dashboard");
  return true;
}

function connectPmu(input = {}) {
  const tempRecord = createPmuRecord(input);
  const existing = pmuRegistry.get(tempRecord.key);
  const record = existing || tempRecord;

  if (!existing) {
    pmuRegistry.set(record.key, record);
  }

  disconnectPmu(record.key, false);
  resetTelemetry(record);

  return new Promise((resolve, reject) => {
    const socket = new net.Socket();
    let settled = false;

    record.socket = socket;
    record.waitingForConfig = true;

    updateRecord(record, {
      status: "connecting",
      lastConnectedAt: null,
      message: `Connecting to ${record.connection.ipAddress}:${record.connection.port}...`,
      lastError: null
    });

    const fail = (message) => {
      if (settled) {
        return;
      }
      settled = true;
      if (record.socket === socket) {
        record.socket = null;
      }
      record.waitingForConfig = false;
      record.receiveBuffer = Buffer.alloc(0);
      updateRecord(record, {
        status: "disconnected",
        message,
        lastError: message
      });
      reject(new Error(message));
    };

    socket.setTimeout(5000);

    socket.once("connect", () => {
      settled = true;
      updateRecord(record, {
        status: "connected",
        lastConnectedAt: new Date().toISOString(),
        message: "Socket connected. Requesting CFG2 from PMU...",
        lastError: null
      });

      socket.write(buildCommandFrame(record.connection.pmuId, CMD_SEND_CFG2));
      resolve(toSerializablePmu(record));
    });

    socket.on("data", (chunk) => {
      if (record.socket !== socket) {
        return;
      }
      processIncomingData(record, chunk);
    });

    socket.on("timeout", () => {
      socket.destroy(new Error("Connection timed out."));
    });

    socket.on("error", (error) => {
      if (record.socket !== socket) {
        return;
      }

      if (!settled) {
        fail(`Unable to connect: ${error.message}`);
        return;
      }

      updateRecord(record, {
        status: "disconnected",
        message: `Socket error: ${error.message}`,
        lastError: error.message
      });
    });

    socket.on("close", () => {
      if (record.socket !== socket && settled) {
        return;
      }

      if (!settled) {
        fail("Connection closed before the PMU handshake completed.");
        return;
      }

      if (record.socket === socket) {
        record.socket = null;
      }

      record.waitingForConfig = false;
      record.receiveBuffer = Buffer.alloc(0);
      updateRecord(record, {
        status: "disconnected",
        message: "PMU connection closed.",
        lastError: record.connection.lastError
      });
    });

    socket.connect(record.connection.port, record.connection.ipAddress);
  });
}

async function serveStatic(urlPath, res) {
  const routeMap = new Map([
    ["/", "pdc.html"],
    ["/pdc", "pdc.html"],
    ["/pmu", "pmu.html"]
  ]);

  const assetPath = routeMap.get(urlPath) || urlPath.slice(1);
  const filePath = path.join(publicDir, assetPath);
  const file = await readFile(filePath);
  const ext = path.extname(filePath);
  const contentType = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8"
  }[ext] || "application/octet-stream";

  res.writeHead(200, { "Content-Type": contentType });
  res.end(file);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);

  if (req.method === "GET" && url.pathname === "/api/dashboard") {
    return sendJson(res, 200, getDashboardSnapshot());
  }

  if (req.method === "GET" && url.pathname.startsWith("/api/pmus/")) {
    const key = decodeURIComponent(url.pathname.replace("/api/pmus/", ""));
    const record = pmuRegistry.get(key);
    if (!record) {
      return sendJson(res, 404, { message: "PMU not found." });
    }
    return sendJson(res, 200, toSerializablePmu(record));
  }

  if (req.method === "POST" && url.pathname === "/api/pmus") {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
    });
    req.on("end", async () => {
      try {
        const payload = body ? JSON.parse(body) : {};
        const record = await connectPmu(payload);
        sendJson(res, 200, { ok: true, pmu: record, ...getDashboardSnapshot() });
      } catch (error) {
        sendJson(res, 500, {
          ok: false,
          message: error.message,
          ...getDashboardSnapshot()
        });
      }
    });
    return;
  }

  if (req.method === "POST" && url.pathname.startsWith("/api/pmus/") && url.pathname.endsWith("/disconnect")) {
    const key = decodeURIComponent(url.pathname.replace("/api/pmus/", "").replace("/disconnect", ""));
    if (!disconnectPmu(key, true)) {
      return sendJson(res, 404, { ok: false, message: "PMU not found." });
    }
    return sendJson(res, 200, { ok: true, ...getDashboardSnapshot() });
  }

  if (req.method === "GET" && url.pathname === "/api/stream") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive"
    });
    res.write(`event: dashboard\ndata: ${JSON.stringify(getDashboardSnapshot())}\n\n`);
    streamClients.add(res);
    req.on("close", () => {
      streamClients.delete(res);
    });
    return;
  }

  try {
    await serveStatic(url.pathname, res);
  } catch {
    sendJson(res, 404, { message: "Not found" });
  }
});

function listen(port) {
  server.listen(port, () => {
    console.log(`PMU/PDC dashboard running at http://localhost:${port}`);
  });
}

server.on("error", (error) => {
  if (error.code === "EADDRINUSE" && server.listening === false) {
    const fallbackPort = preferredPort + 1;
    console.log(
      `Port ${preferredPort} is already in use. Starting dashboard on http://localhost:${fallbackPort} instead.`
    );
    listen(fallbackPort);
    return;
  }

  throw error;
});

listen(preferredPort);
