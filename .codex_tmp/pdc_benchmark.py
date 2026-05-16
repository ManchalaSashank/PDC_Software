import csv
import math
import os
import socket
import statistics
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path


CMD_SEND_CFG2 = 0x0005
CMD_TURN_ON_TX = 0x0002
TYPE_DATA = 0x01
TYPE_CFG2 = 0x31
TYPE_CMD = 0x41
SYNC = 0xAA
TIME_BASE_DEFAULT = 1_000_000


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def command_frame(pmu_id: int, command: int) -> bytes:
    now = int(time.time())
    frame = bytearray()
    frame.extend([SYNC, TYPE_CMD])
    frame.extend((0).to_bytes(2, "big"))
    frame.extend(pmu_id.to_bytes(2, "big"))
    frame.extend(now.to_bytes(4, "big"))
    frame.extend((0).to_bytes(4, "big"))
    frame.extend(command.to_bytes(2, "big"))
    size = len(frame) + 2
    frame[2:4] = size.to_bytes(2, "big")
    frame.extend(crc16(frame).to_bytes(2, "big"))
    return bytes(frame)


def recv_exact_frame(sock: socket.socket, timeout: float = 5.0) -> bytes:
    sock.settimeout(timeout)
    buffer = bytearray()
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("socket closed")
        buffer.extend(chunk)
        while buffer and buffer[0] != SYNC:
            del buffer[0]
        if len(buffer) >= 4:
            size = int.from_bytes(buffer[2:4], "big")
            if size < 10 or size > 4096:
                del buffer[0]
                continue
            while len(buffer) < size:
                chunk = sock.recv(4096)
                if not chunk:
                    raise ConnectionError("socket closed")
                buffer.extend(chunk)
            return bytes(buffer[:size])


class FrameReader:
    def __init__(self, sock: socket.socket) -> None:
        self.sock = sock
        self.buffer = bytearray()

    def next_frame(self, timeout: float = 5.0) -> bytes:
        self.sock.settimeout(timeout)
        while True:
            while self.buffer and self.buffer[0] != SYNC:
                del self.buffer[0]
            if len(self.buffer) >= 4:
                size = int.from_bytes(self.buffer[2:4], "big")
                if size < 10 or size > 4096:
                    del self.buffer[0]
                    continue
                if len(self.buffer) >= size:
                    frame = bytes(self.buffer[:size])
                    del self.buffer[:size]
                    return frame
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("socket closed")
            self.buffer.extend(chunk)


def read_u16(frame: bytes, index: int) -> int:
    return int.from_bytes(frame[index:index + 2], "big")


def read_u32(frame: bytes, index: int) -> int:
    return int.from_bytes(frame[index:index + 4], "big")


def read_f32(frame: bytes, index: int) -> float:
    import struct

    return struct.unpack(">f", frame[index:index + 4])[0]


@dataclass
class Config:
    pmu_id: int
    station: str
    phasors: int
    analogs: int
    digitals: int
    data_rate: int
    time_base: int


def parse_cfg2(frame: bytes) -> Config:
    if frame[0] != SYNC or frame[1] != TYPE_CFG2:
        raise ValueError("not CFG2")
    if crc16(frame[:-2]) != read_u16(frame, len(frame) - 2):
        raise ValueError("CFG CRC failed")
    time_base = read_u32(frame, 14) & 0x00FFFFFF
    if time_base == 0:
        time_base = TIME_BASE_DEFAULT
    index = 20
    station = frame[index:index + 16].decode("ascii", "ignore").strip()
    index += 16
    pmu_id = read_u16(frame, index)
    index += 2
    index += 2
    phasors = read_u16(frame, index)
    index += 2
    analogs = read_u16(frame, index)
    index += 2
    digitals = read_u16(frame, index)
    index += 2
    index += 16 * (phasors + analogs + (16 * digitals))
    index += 4 * (phasors + analogs + digitals)
    index += 2
    index += 2
    data_rate = abs(int.from_bytes(frame[index:index + 2], "big", signed=True)) or 50
    return Config(pmu_id, station, phasors, analogs, digitals, data_rate, time_base)


@dataclass
class PmuMetrics:
    pmu_id: int
    port: int
    connected: bool = False
    cfg_ms: float = 0.0
    frames: int = 0
    parse_errors: int = 0
    latencies_ms: list[float] = field(default_factory=list)
    intervals_ms: list[float] = field(default_factory=list)
    frequencies: list[float] = field(default_factory=list)
    rocofs: list[float] = field(default_factory=list)
    first_receive: float | None = None
    last_receive: float | None = None
    error: str = ""
    config: Config | None = None


def parse_data(frame: bytes, cfg: Config) -> tuple[float, float, float]:
    if frame[0] != SYNC or frame[1] != TYPE_DATA:
        raise ValueError("not DATA")
    if crc16(frame[:-2]) != read_u16(frame, len(frame) - 2):
        raise ValueError("DATA CRC failed")
    soc = read_u32(frame, 6)
    fracsec = read_u32(frame, 10) & 0x00FFFFFF
    pmu_ts = soc + (fracsec / (cfg.time_base or TIME_BASE_DEFAULT))
    index = 16
    index += cfg.phasors * 8
    frequency = read_f32(frame, index)
    index += 4
    rocof = read_f32(frame, index)
    return pmu_ts, frequency, rocof


def client_worker(host: str, port: int, pmu_id: int, duration: float, metrics: PmuMetrics) -> None:
    try:
        start = time.perf_counter()
        with socket.create_connection((host, port), timeout=5.0) as sock:
            reader = FrameReader(sock)
            sock.sendall(command_frame(pmu_id, CMD_SEND_CFG2))
            cfg_frame = reader.next_frame(5.0)
            cfg = parse_cfg2(cfg_frame)
            if cfg.pmu_id != pmu_id:
                metrics.error = f"Device ID mismatch expected {pmu_id}, got {cfg.pmu_id}"
                return
            metrics.config = cfg
            metrics.cfg_ms = (time.perf_counter() - start) * 1000.0
            metrics.connected = True
            sock.sendall(command_frame(pmu_id, CMD_TURN_ON_TX))
            end = time.perf_counter() + duration
            sock.settimeout(2.0)
            while time.perf_counter() < end:
                try:
                    frame = reader.next_frame(2.0)
                    if len(frame) < 2 or frame[1] != TYPE_DATA:
                        continue
                    receive_ts = time.time()
                    pmu_ts, freq, rocof = parse_data(frame, cfg)
                    if metrics.last_receive is not None:
                        metrics.intervals_ms.append((receive_ts - metrics.last_receive) * 1000.0)
                    metrics.first_receive = metrics.first_receive or receive_ts
                    metrics.last_receive = receive_ts
                    metrics.latencies_ms.append((receive_ts - pmu_ts) * 1000.0)
                    metrics.frequencies.append(freq)
                    metrics.rocofs.append(rocof)
                    metrics.frames += 1
                except socket.timeout:
                    continue
                except Exception:
                    metrics.parse_errors += 1
    except Exception as exc:
        metrics.error = str(exc)


def summarize(values: list[float]) -> dict[str, float]:
    if not values:
        return {"min": math.nan, "mean": math.nan, "p95": math.nan, "max": math.nan, "std": math.nan}
    ordered = sorted(values)
    p95_index = min(len(ordered) - 1, math.ceil(0.95 * len(ordered)) - 1)
    return {
        "min": min(values),
        "mean": statistics.fmean(values),
        "p95": ordered[p95_index],
        "max": max(values),
        "std": statistics.pstdev(values) if len(values) > 1 else 0.0,
    }


def run_scenario(pmu_exe: Path, count: int, duration: float, base_port: int) -> tuple[list[PmuMetrics], list[subprocess.Popen]]:
    processes: list[subprocess.Popen] = []
    log_dir = Path(".codex_tmp") / "benchmark_logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    for idx in range(count):
        pmu_id = idx + 1
        port = base_port + idx
        p = subprocess.Popen(
            [str(pmu_exe), str(port), str(pmu_id), f"BENCH_{pmu_id}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
            cwd=str(pmu_exe.parent),
        )
        processes.append(p)
    time.sleep(1.5)

    metrics = [PmuMetrics(idx + 1, base_port + idx) for idx in range(count)]
    threads = [
        threading.Thread(target=client_worker, args=("127.0.0.1", m.port, m.pmu_id, duration, m), daemon=True)
        for m in metrics
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join(duration + 8.0)
    for p in processes:
        p.terminate()
    for p in processes:
        try:
            p.wait(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()
    return metrics, processes


def write_outputs(results: dict[int, list[PmuMetrics]], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    with open(output_dir / "pdc_benchmark_per_pmu.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "scenario_pmus", "pmu_id", "connected", "cfg_ms", "frames", "observed_fps",
            "packet_quality_pct", "latency_mean_ms", "latency_p95_ms", "latency_max_ms",
            "jitter_std_ms", "interval_mean_ms", "frequency_mean_hz", "frequency_std_hz",
            "rocof_mean", "parse_errors", "error"
        ])
        for count, items in results.items():
            for m in items:
                elapsed = (m.last_receive - m.first_receive) if m.first_receive and m.last_receive and m.last_receive > m.first_receive else 0.0
                fps = m.frames / elapsed if elapsed > 0 else 0.0
                expected_rate = m.config.data_rate if m.config else 50
                quality = (fps / expected_rate) * 100.0 if expected_rate else 0.0
                lat = summarize(m.latencies_ms)
                intervals = summarize(m.intervals_ms)
                freq = summarize(m.frequencies)
                rocof = summarize(m.rocofs)
                writer.writerow([
                    count, m.pmu_id, m.connected, f"{m.cfg_ms:.3f}", m.frames, f"{fps:.3f}",
                    f"{quality:.2f}", f"{lat['mean']:.3f}", f"{lat['p95']:.3f}", f"{lat['max']:.3f}",
                    f"{intervals['std']:.3f}", f"{intervals['mean']:.3f}",
                    f"{freq['mean']:.5f}", f"{freq['std']:.5f}", f"{rocof['mean']:.5f}",
                    m.parse_errors, m.error
                ])

    with open(output_dir / "pdc_benchmark_summary.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "scenario_pmus", "connected", "total_frames", "mean_fps_per_pmu",
            "mean_packet_quality_pct", "latency_mean_ms", "latency_p95_ms",
            "latency_max_ms", "jitter_std_ms", "cfg_mean_ms", "parse_errors"
        ])
        for count, items in results.items():
            connected = [m for m in items if m.connected]
            all_lat = [v for m in connected for v in m.latencies_ms]
            all_intervals = [v for m in connected for v in m.intervals_ms]
            fps_values = []
            quality_values = []
            for m in connected:
                elapsed = (m.last_receive - m.first_receive) if m.first_receive and m.last_receive and m.last_receive > m.first_receive else 0.0
                fps = m.frames / elapsed if elapsed > 0 else 0.0
                expected_rate = m.config.data_rate if m.config else 50
                fps_values.append(fps)
                quality_values.append((fps / expected_rate) * 100.0 if expected_rate else 0.0)
            lat = summarize(all_lat)
            intervals = summarize(all_intervals)
            cfg_mean = statistics.fmean([m.cfg_ms for m in connected]) if connected else math.nan
            writer.writerow([
                count, len(connected), sum(m.frames for m in connected),
                f"{statistics.fmean(fps_values):.3f}" if fps_values else "nan",
                f"{statistics.fmean(quality_values):.2f}" if quality_values else "nan",
                f"{lat['mean']:.3f}", f"{lat['p95']:.3f}", f"{lat['max']:.3f}",
                f"{intervals['std']:.3f}", f"{cfg_mean:.3f}",
                sum(m.parse_errors for m in connected)
            ])


def main() -> int:
    root = Path.cwd()
    pmu_exe = root / ".codex_tmp" / "public_zip" / "PMU and PDC" / "pmu" / "pmu.exe"
    if not pmu_exe.exists():
        print(f"missing PMU simulator: {pmu_exe}", file=sys.stderr)
        return 2
    scenarios = [1, 3, 6, 12]
    duration = float(os.environ.get("PDC_BENCH_DURATION", "12"))
    base_port = int(os.environ.get("PDC_BENCH_BASE_PORT", "4812"))
    results: dict[int, list[PmuMetrics]] = {}
    for scenario in scenarios:
        print(f"Running scenario: {scenario} PMU(s), {duration:.1f}s sample")
        metrics, _ = run_scenario(pmu_exe, scenario, duration, base_port)
        results[scenario] = metrics
        connected = sum(1 for m in metrics if m.connected)
        frames = sum(m.frames for m in metrics)
        print(f"  connected={connected}/{scenario}, frames={frames}")
        time.sleep(1.0)
    output_dir = root / ".codex_tmp" / "benchmark_results"
    write_outputs(results, output_dir)
    print(f"Wrote {output_dir / 'pdc_benchmark_summary.csv'}")
    print(f"Wrote {output_dir / 'pdc_benchmark_per_pmu.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
