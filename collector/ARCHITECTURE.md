# Architecture

Collector is organized as a set of small C modules in one root directory. The runtime is built around a bounded ring buffer that receives telemetry from probes and samplers. Worker threads consume events and enforce the processing contract before data reaches storage or export.

## Data Flow

```text
procfs/userspace/eBPF-ready probes
  -> ring_buffer.c
  -> parser.c validation
  -> normalizer.c enrichment
  -> classifier.c categories
  -> filter.c deduplication
  -> correlator.c process timelines
  -> anomaly_detector.c behavioral analysis
  -> storage.c and exporter.c
  -> dashboard.c and statistics.c
```

## Concurrency

Producers serialize writes with a short producer lock while slots are published with atomics and release/acquire ordering. Consumers read in sequence and validate per-slot CRCs. Sink writes are protected by a mutex so storage and exporter accounting remain ordered. Dashboard and health monitoring read atomics and ring state without blocking the processing path for long periods.

## Failure Handling

Invalid records are discarded, CRC failures are counted, full buffers increment dropped counters, and exporter failures do not stop local storage. Shutdown drains the ring, flushes exporters, closes storage, verifies the file, writes a JSON summary, and prints human-readable process timelines.
