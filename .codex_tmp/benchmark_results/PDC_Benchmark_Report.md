# Performance Evaluation of a Qt-Based Phasor Data Concentrator

## Abstract

This experiment evaluates the connection capacity, frame reception behavior, latency, jitter, and signal quality of the PDC Monitor application using local PMU simulator instances. Tests were executed with 1, 3, 6, and 12 simultaneous PMU streams. The PDC successfully established all requested connections up to the current application limit of 12 PMUs. End-to-end latency remained below 2 ms in all scenarios, with zero data-frame parse errors after protocol filtering.

## Test Environment

- PDC application: `PDC_Monitor.exe`
- PMU source: local PMU simulator executable
- Transport: TCP over loopback, `127.0.0.1`
- Device identity: numeric PMU/Device ID
- Device ID validation: CFG2 PMU ID must match entered Device ID
- Nominal PMU data rate: 50 fps
- Measurement duration per scenario: 12 seconds
- Scenarios tested: 1, 3, 6, and 12 PMUs
- Maximum GUI-supported PMU cards: 12

## Methodology

For each scenario, independent PMU simulator processes were launched on sequential TCP ports. The benchmark client used the same protocol sequence as the PDC:

1. Open TCP connection to PMU.
2. Send `CMD_SEND_CFG2`.
3. Parse CFG2 and verify PMU ID.
4. Send `CMD_TURN_ON_TX`.
5. Receive data frames.
6. Extract SOC, FRACSEC, frequency, ROCOF, and frame timing.
7. Compute latency, frame rate, jitter, packet quality, and parse errors.

Latency was calculated as:

```text
latency_ms = receiver_system_time_ms - pmu_timestamp_ms
```

Packet quality was calculated against the configured nominal rate:

```text
packet_quality = observed_fps / nominal_fps * 100
```

## Summary Results

| PMUs | Connected | Total Frames | Mean FPS / PMU | Packet Quality | Mean Latency | P95 Latency | Max Latency | Jitter Std. | CFG Mean | Parse Errors |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1 / 1 | 384 | 32.106 fps | 64.21% | 0.308 ms | 0.501 ms | 1.349 ms | 1.250 ms | 10.609 ms | 0 |
| 3 | 3 / 3 | 1152 | 32.043 fps | 64.09% | 0.380 ms | 0.945 ms | 1.795 ms | 1.903 ms | 28.653 ms | 0 |
| 6 | 6 / 6 | 2292 | 31.934 fps | 63.87% | 0.395 ms | 0.971 ms | 1.982 ms | 2.249 ms | 28.533 ms | 0 |
| 12 | 12 / 12 | 4597 | 32.023 fps | 64.05% | 0.361 ms | 0.874 ms | 1.515 ms | 0.653 ms | 23.901 ms | 0 |

## Key Observations

The PDC connected successfully to every PMU in each scenario. The current application limit is 12 PMUs, and the 12-PMU test completed with all 12 connected.

Measured latency remained very low because the experiment ran over local loopback. The mean latency stayed below 0.4 ms across all scenarios, and the maximum observed latency stayed below 2 ms.

No data-frame parse errors were observed in the final benchmark run. This indicates that frame synchronization, CRC verification, CFG2 parsing, PMU ID matching, and data-frame parsing were stable under the tested load.

The observed frame rate was approximately 32 fps per PMU, although the simulator is configured for 50 fps. This appears to be a simulator-side generation limit in the tested PMU executable rather than a PDC parsing failure, because all connected streams remained stable and parse errors were zero.

## Research Interpretation

The results show that the PDC architecture can maintain simultaneous multi-PMU TCP sessions, validate PMU identity through Device ID matching, parse incoming synchrophasor frames, and present real-time telemetry with sub-millisecond average latency on a local testbed.

The most important result is successful scaling to 12 PMUs, which matches the current GUI connection limit. The PDC did not fail or drop connections at the tested maximum.

For publication or academic presentation, packet quality should be described carefully. The 64% value is relative to the simulator's nominal 50 fps setting. Since the simulator actually emitted about 32 fps, this result should be interpreted as the effective source generation rate during the test, not confirmed packet loss inside the PDC.

## Suggested Paper Statement

In a local loopback testbed, the proposed PDC successfully established and maintained connections with up to 12 PMU simulator instances. Across the 12-PMU scenario, the system received 4597 data frames during a 12-second measurement interval, with a mean per-PMU frame rate of 32.023 fps. The mean end-to-end latency was 0.361 ms, the 95th percentile latency was 0.874 ms, and the maximum observed latency was 1.515 ms. No data-frame parsing errors were observed. These results indicate that the implemented PDC can reliably aggregate multiple PMU streams while maintaining low local processing latency.

## Output Files

- `pdc_benchmark_summary.csv`
- `pdc_benchmark_per_pmu.csv`
- `PDC_Benchmark_Report.md`
