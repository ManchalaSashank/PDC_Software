import net from "node:net";

const TYPE_DATA = 0x01;
const TYPE_CFG2 = 0x31;
const TYPE_CMD = 0x41;
const CMD_TURN_OFF_TX = 0x0001;
const CMD_TURN_ON_TX = 0x0002;
const CMD_SEND_CFG2 = 0x0005;

export const PHASOR_CHANNELS = ["VA", "VB", "VC", "IA", "IB", "IC"];
export const AUX_CHANNELS = ["F", "DFDT"];

function calculateCrc(buffer) {
  let crc = 0xffff;
  for (const byte of buffer) {
    crc ^= byte << 8;
    for (let index = 0; index < 8; index += 1) {
      crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
    }
  }
  return crc;
}

function appendUInt16(buffer, value) {
  const chunk = Buffer.alloc(2);
  chunk.writeUInt16BE(value, 0);
  return Buffer.concat([buffer, chunk]);
}

function appendUInt32(buffer, value) {
  const chunk = Buffer.alloc(4);
  chunk.writeUInt32BE(value, 0);
  return Buffer.concat([buffer, chunk]);
}

function appendFloat(buffer, value) {
  const chunk = Buffer.alloc(4);
  chunk.writeFloatBE(value, 0);
  return Buffer.concat([buffer, chunk]);
}

function appendText(buffer, text, width) {
  const chunk = Buffer.alloc(width, 0x20);
  chunk.write(String(text || "").slice(0, width), 0, "ascii");
  return Buffer.concat([buffer, chunk]);
}

function randomInRange(min, max) {
  return min + Math.random() * (max - min);
}

function phasorValueForLabel(label) {
  const voltageBase = { VA: 230, VB: 228, VC: 232 };
  const currentBase = { IA: 12, IB: 11.5, IC: 12.4 };
  const isVoltage = label.startsWith("V");
  const magnitudeBase = isVoltage ? (voltageBase[label] || 230) : (currentBase[label] || 12);
  const magnitudeSwing = isVoltage ? 4 : 0.8;
  const angleDeg = { VA: 0, VB: -120, VC: 120, IA: -8, IB: -128, IC: 112 }[label] ?? 0;
  return {
    magnitude: magnitudeBase + randomInRange(-magnitudeSwing, magnitudeSwing),
    angleRad: (angleDeg + randomInRange(-2, 2)) * Math.PI / 180
  };
}

export function normalizeIpAddress(value) {
  if (typeof value !== "string" || !value.trim()) {
    return "";
  }
  const trimmed = value.trim().toLowerCase();
  if (trimmed === "localhost" || trimmed === "::1") {
    return "127.0.0.1";
  }
  if (trimmed.startsWith("::ffff:")) {
    return trimmed.slice(7);
  }
  return trimmed;
}

function parseCommandCode(frame) {
  if (frame.length >= 16) {
    return frame.readUInt16BE(14);
  }
  if (frame.length >= 8) {
    return frame.readUInt16BE(6);
  }
  return 0;
}

export function createSimulatorStore({ broadcast }) {
  const simulators = new Map();

  function simulatorPhasorLabels(record) {
    return record.settings.selectedChannels.filter((channel) => PHASOR_CHANNELS.includes(channel));
  }

  function simulatorFlags(record) {
    return {
      includeFrequency: record.settings.selectedChannels.includes("F"),
      includeRocof: record.settings.selectedChannels.includes("DFDT")
    };
  }

  function toSerializable(record) {
    return {
      id: record.id,
      settings: record.settings,
      status: record.status,
      phasorLabels: simulatorPhasorLabels(record),
      auxiliaryChannels: AUX_CHANNELS.filter((channel) => record.settings.selectedChannels.includes(channel))
    };
  }

  function snapshot() {
    return {
      simulators: Array.from(simulators.values()).map(toSerializable).sort((a, b) => a.settings.pmuId - b.settings.pmuId)
    };
  }

  function announce() {
    broadcast("simulators", snapshot());
  }

  function stopTransmissionTimer(record) {
    if (record.transmissionTimer) {
      clearInterval(record.transmissionTimer);
      record.transmissionTimer = null;
    }
  }

  function buildCfg2Frame(record) {
    const phasorLabels = simulatorPhasorLabels(record);
    let frame = Buffer.from([0xaa, TYPE_CFG2]);
    frame = appendUInt16(frame, 0);
    frame = appendUInt16(frame, record.settings.pmuId);
    frame = appendUInt32(frame, Math.floor(Date.now() / 1000));
    frame = appendUInt32(frame, 0);
    frame = appendUInt32(frame, 1_000_000);
    frame = appendUInt16(frame, 1);
    frame = appendText(frame, record.settings.stationName, 16);
    frame = appendUInt16(frame, record.settings.pmuId);
    frame = appendUInt16(frame, 0x001f);
    frame = appendUInt16(frame, phasorLabels.length);
    frame = appendUInt16(frame, 0);
    frame = appendUInt16(frame, 0);
    for (const label of phasorLabels) {
      frame = appendText(frame, label, 16);
    }
    for (const label of phasorLabels) {
      frame = appendUInt32(frame, label.startsWith("I") ? 0x01000001 : 0x00000001);
    }
    frame = appendUInt16(frame, record.settings.dataRate === 60 ? 1 : 0);
    frame = appendUInt16(frame, 1);
    frame = appendUInt16(frame, record.settings.dataRate);
    frame.writeUInt16BE(frame.length + 2, 2);
    return appendUInt16(frame, calculateCrc(frame));
  }

  function buildDataFrame(record) {
    const phasorLabels = simulatorPhasorLabels(record);
    const { includeFrequency, includeRocof } = simulatorFlags(record);
    let frame = Buffer.from([0xaa, TYPE_DATA]);
    frame = appendUInt16(frame, 0);
    frame = appendUInt16(frame, record.settings.pmuId);
    frame = appendUInt32(frame, Math.floor(Date.now() / 1000));
    frame = appendUInt32(frame, 0);
    frame = appendUInt16(frame, 0xc000);
    for (const label of phasorLabels) {
      const { magnitude, angleRad } = phasorValueForLabel(label);
      frame = appendFloat(frame, magnitude);
      frame = appendFloat(frame, angleRad);
    }
    const nominalFrequency = record.settings.dataRate === 60 ? 60 : 50;
    frame = appendFloat(frame, includeFrequency ? nominalFrequency + randomInRange(-0.04, 0.04) : 0);
    frame = appendFloat(frame, includeRocof ? randomInRange(-0.2, 0.2) : 0);
    frame.writeUInt16BE(frame.length + 2, 2);
    return appendUInt16(frame, calculateCrc(frame));
  }

  function refreshTransmissionTimer(record) {
    stopTransmissionTimer(record);
    if (!record.transmitting || !record.activeSocket || record.activeSocket.destroyed) {
      return;
    }
    const intervalMs = Math.max(20, Math.round(1000 / Math.max(1, record.settings.dataRate)));
    record.transmissionTimer = setInterval(() => {
      if (!record.activeSocket || record.activeSocket.destroyed) {
        stopTransmissionTimer(record);
        record.transmitting = false;
        record.status.transmissionState = "idle";
        announce();
        return;
      }
      record.activeSocket.write(buildDataFrame(record));
      record.status.framesSent += 1;
      record.status.transmissionState = "streaming";
      record.status.message = `Streaming at ${record.settings.dataRate} fps to ${record.status.connectedPdcIp}.`;
      announce();
    }, intervalMs);
  }

  function closeSocket(record, message, keepError = false) {
    stopTransmissionTimer(record);
    record.transmitting = false;
    if (record.activeSocket && !record.activeSocket.destroyed) {
      record.activeSocket.destroy();
    }
    record.activeSocket = null;
    record.receiveBuffer = Buffer.alloc(0);
    record.status.connectedPdcIp = null;
    record.status.transmissionState = "idle";
    record.status.message = message;
    if (!keepError) {
      record.status.lastError = null;
    }
    announce();
  }

  function disconnectIfUnauthorized(record) {
    if (!record.activeSocket || record.activeSocket.destroyed) {
      return;
    }

    const remoteIp = normalizeIpAddress(record.activeSocket.remoteAddress || "");
    if (remoteIp && remoteIp !== record.settings.allowedPdcIp) {
      closeSocket(
        record,
        `Disconnected ${remoteIp} because the accepted PDC IP changed to ${record.settings.allowedPdcIp}.`
      );
    }
  }

  function handleCommand(record, frame) {
    if (frame[1] !== TYPE_CMD) {
      return;
    }
    const expectedCrc = frame.readUInt16BE(frame.length - 2);
    const actualCrc = calculateCrc(frame.subarray(0, frame.length - 2));
    if (expectedCrc !== actualCrc) {
      record.status.lastError = "Rejected command with invalid CRC.";
      record.status.message = "Received an invalid command frame.";
      announce();
      return;
    }
    const targetPmuId = frame.readUInt16BE(4);
    if (![record.settings.pmuId, 0xffff].includes(targetPmuId)) {
      return;
    }
    const commandCode = parseCommandCode(frame);
    if (commandCode === CMD_SEND_CFG2) {
      record.activeSocket?.write(buildCfg2Frame(record));
      record.status.message = "CFG2 sent to connected PDC.";
      record.status.lastError = null;
      announce();
      return;
    }
    if (commandCode === CMD_TURN_ON_TX) {
      record.transmitting = true;
      record.status.transmissionState = "starting";
      record.status.message = "Transmission enabled by connected PDC.";
      refreshTransmissionTimer(record);
      announce();
      return;
    }
    if (commandCode === CMD_TURN_OFF_TX) {
      stopTransmissionTimer(record);
      record.transmitting = false;
      record.status.transmissionState = "idle";
      record.status.message = "Transmission disabled by connected PDC.";
      announce();
    }
  }

  function processSocketData(record, chunk) {
    record.receiveBuffer = Buffer.concat([record.receiveBuffer, chunk]);
    while (record.receiveBuffer.length >= 4) {
      if (record.receiveBuffer[0] !== 0xaa) {
        const nextSync = record.receiveBuffer.indexOf(0xaa, 1);
        record.receiveBuffer = nextSync === -1 ? Buffer.alloc(0) : record.receiveBuffer.subarray(nextSync);
        continue;
      }
      const frameSize = record.receiveBuffer.readUInt16BE(2);
      if (frameSize < 10 || frameSize > 1024) {
        record.receiveBuffer = record.receiveBuffer.subarray(1);
        continue;
      }
      if (record.receiveBuffer.length < frameSize) {
        break;
      }
      const frame = record.receiveBuffer.subarray(0, frameSize);
      record.receiveBuffer = record.receiveBuffer.subarray(frameSize);
      handleCommand(record, frame);
    }
  }

  function createRecord(input = {}) {
    const pmuId = Number.isFinite(Number.parseInt(input.pmuId, 10)) ? Number.parseInt(input.pmuId, 10) : simulators.size + 1;
    const selectedChannels = Array.isArray(input.selectedChannels) ? input.selectedChannels.filter((channel) => [...PHASOR_CHANNELS, ...AUX_CHANNELS].includes(channel)) : ["VA", "VB", "VC", "IA", "IB", "IC", "F", "DFDT"];
    return {
      id: typeof input.id === "string" && input.id.trim() ? input.id.trim() : `pmu-${pmuId}`,
      server: null,
      activeSocket: null,
      receiveBuffer: Buffer.alloc(0),
      transmissionTimer: null,
      transmitting: false,
      settings: {
        name: typeof input.name === "string" && input.name.trim() ? input.name.trim() : `PMU ${pmuId}`,
        stationName: typeof input.stationName === "string" && input.stationName.trim() ? input.stationName.trim().slice(0, 16) : `SIM_PMU_${pmuId}`.slice(0, 16),
        pmuId,
        port: Number.isFinite(Number.parseInt(input.port, 10)) ? Number.parseInt(input.port, 10) : 4712 + simulators.size,
        dataRate: Number.isFinite(Number.parseInt(input.dataRate, 10)) ? Number.parseInt(input.dataRate, 10) : 50,
        allowedPdcIp: normalizeIpAddress(input.allowedPdcIp || "127.0.0.1") || "127.0.0.1",
        selectedChannels
      },
      status: {
        serverState: "stopped",
        transmissionState: "idle",
        connectedPdcIp: null,
        lastClientAt: null,
        deniedConnections: 0,
        framesSent: 0,
        message: "Ready to start.",
        lastError: null,
        lastConfigUpdatedAt: new Date().toISOString()
      }
    };
  }

  async function start(id) {
    const record = simulators.get(id);
    if (!record) {
      throw new Error("PMU simulator not found.");
    }
    if (record.server) {
      record.status.serverState = "running";
      record.status.message = `PMU simulator already listening on port ${record.settings.port}.`;
      announce();
      return toSerializable(record);
    }
    await new Promise((resolve, reject) => {
      const server = net.createServer((socket) => {
        const remoteIp = normalizeIpAddress(socket.remoteAddress || "");
        if (remoteIp !== record.settings.allowedPdcIp) {
          record.status.deniedConnections += 1;
          record.status.message = `Denied connection from ${remoteIp || "unknown"}; only ${record.settings.allowedPdcIp} is allowed.`;
          record.status.lastError = record.status.message;
          announce();
          socket.destroy();
          return;
        }
        if (record.activeSocket && !record.activeSocket.destroyed) {
          record.status.deniedConnections += 1;
          record.status.message = `Denied extra connection from ${remoteIp}; one PDC at a time is allowed.`;
          record.status.lastError = record.status.message;
          announce();
          socket.destroy();
          return;
        }
        record.activeSocket = socket;
        record.receiveBuffer = Buffer.alloc(0);
        record.status.connectedPdcIp = remoteIp;
        record.status.lastClientAt = new Date().toISOString();
        record.status.message = `PDC ${remoteIp} connected. Waiting for command.`;
        record.status.lastError = null;
        announce();
        socket.on("data", (chunk) => {
          if (record.activeSocket === socket) {
            processSocketData(record, chunk);
          }
        });
        socket.on("error", (error) => {
          if (record.activeSocket === socket) {
            record.status.lastError = error.message;
            closeSocket(record, `PDC socket error: ${error.message}`, true);
          }
        });
        socket.on("close", () => {
          if (record.activeSocket === socket) {
            closeSocket(record, "PDC disconnected from PMU simulator.");
          }
        });
      });
      server.on("error", reject);
      server.listen(record.settings.port, () => {
        record.server = server;
        record.status.serverState = "running";
        record.status.message = `Listening for PDC ${record.settings.allowedPdcIp} on port ${record.settings.port}.`;
        record.status.lastError = null;
        announce();
        resolve();
      });
    });
    return toSerializable(record);
  }

  async function stop(id) {
    const record = simulators.get(id);
    if (!record) {
      throw new Error("PMU simulator not found.");
    }
    closeSocket(record, "PMU simulator stopped.");
    if (!record.server) {
      record.status.serverState = "stopped";
      announce();
      return toSerializable(record);
    }
    const server = record.server;
    record.server = null;
    await new Promise((resolve) => server.close(resolve));
    record.status.serverState = "stopped";
    record.status.message = "PMU simulator stopped.";
    record.status.lastError = null;
    announce();
    return toSerializable(record);
  }

  function create(input = {}) {
    const record = createRecord(input);
    simulators.set(record.id, record);
    announce();
    return toSerializable(record);
  }

  function update(id, input = {}) {
    const record = simulators.get(id);
    if (!record) {
      throw new Error("PMU simulator not found.");
    }
    const nextChannels = Array.isArray(input.selectedChannels) ? input.selectedChannels.filter((channel) => [...PHASOR_CHANNELS, ...AUX_CHANNELS].includes(channel)) : record.settings.selectedChannels;
    record.settings = {
      ...record.settings,
      name: typeof input.name === "string" && input.name.trim() ? input.name.trim() : record.settings.name,
      stationName: typeof input.stationName === "string" && input.stationName.trim() ? input.stationName.trim().slice(0, 16) : record.settings.stationName,
      pmuId: Number.isFinite(Number.parseInt(input.pmuId, 10)) ? Number.parseInt(input.pmuId, 10) : record.settings.pmuId,
      port: Number.isFinite(Number.parseInt(input.port, 10)) ? Number.parseInt(input.port, 10) : record.settings.port,
      dataRate: Number.isFinite(Number.parseInt(input.dataRate, 10)) ? Number.parseInt(input.dataRate, 10) : record.settings.dataRate,
      allowedPdcIp: normalizeIpAddress(input.allowedPdcIp || record.settings.allowedPdcIp) || record.settings.allowedPdcIp,
      selectedChannels: nextChannels.length ? nextChannels : record.settings.selectedChannels
    };
    record.status.lastConfigUpdatedAt = new Date().toISOString();
    record.status.message = `Updated ${record.settings.name} configuration.`;
    record.status.lastError = null;
    if (record.transmitting) {
      refreshTransmissionTimer(record);
    }
    disconnectIfUnauthorized(record);
    if (record.activeSocket && !record.activeSocket.destroyed) {
      record.activeSocket.write(buildCfg2Frame(record));
    }
    announce();
    return toSerializable(record);
  }

  function seed() {
    [
      { id: "pmu-1", name: "Substation A", stationName: "SUB_A", pmuId: 1, port: 4712, dataRate: 50, allowedPdcIp: "127.0.0.1", selectedChannels: ["VA", "VB", "VC", "IA", "IB", "IC", "F", "DFDT"] },
      { id: "pmu-2", name: "Substation B", stationName: "SUB_B", pmuId: 2, port: 4713, dataRate: 25, allowedPdcIp: "127.0.0.1", selectedChannels: ["VA", "VB", "VC", "F"] },
      { id: "pmu-3", name: "Feeder C", stationName: "FEEDER_C", pmuId: 3, port: 4714, dataRate: 10, allowedPdcIp: "127.0.0.1", selectedChannels: ["IA", "IB", "IC", "DFDT"] }
    ].forEach((item) => create(item));
  }

  return { create, normalizeIpAddress, seed, snapshot, start, stop, update };
}
