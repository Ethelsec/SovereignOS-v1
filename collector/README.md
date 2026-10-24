# Collector Telemetry Platform

Collector is a C17 Linux telemetry collection, processing, correlation, analysis, storage, replay, dashboard, and OpenTelemetry export platform. It targets Linux 6.x on x86_64 and ARM64 and is written around POSIX/Linux primitives: pthreads, atomics, procfs, signalfd, checksummed append-only files, and a bounded process-local ring buffer.

The implementation is designed to run without root for userspace and procfs telemetry. The current runtime does **not** attach an eBPF probe; it only reports CO-RE readiness through BTF detection. Systems with libbpf/protobuf-c/zlib available can link those dependencies through CMake feature detection, but current runtime code does not use them. The exporter emits OTLP-shaped newline-delimited JSON (not protobuf OTLP) so telemetry remains usable without a collector endpoint.

## Features

- Event collection from procfs/userspace sources. eBPF probe attachment is not implemented; only BTF capability detection and single-root BPF skeleton compatibility files exist.
- Canonical JSONL ingress from a file or AF_UNIX socket, with strict validation, bounded deduplication, producer sequence-gap counters, and documented lossy field mappings (see `INGEST.md`).
- Multiple-producer/multi-consumer ring buffer with sequence numbers, CRC validation, CAS consumer claim, overflow accounting, and utilization reporting.
- Mandatory processing stages: validation, decode, normalization, classification, enrichment, correlation, anomaly analysis, filtering, export, storage, statistics, and cleanup.
- Process timeline reconstruction with parent/child correlation, event counts, byte totals, and final reports.
- Behavioral analysis for resource spikes, security-like payloads, repeated process activity, and confidence scoring.
- Append-only telemetry file format with per-record headers, checksums, replay, and verification.
- OpenTelemetry log export model with trace IDs, span IDs, resource attributes, severity, event name, PID/TID, and correlation IDs.
- Live terminal dashboard with throughput, buffer, health, anomaly, warning, and latency fields.
- Configuration through defaults, key=value config files, environment variables, and command-line arguments.
- CMake build, CTest unit tests, installation rules, and CPack packaging metadata.

## Architecture

```text
 procfs sampler / userspace events      canonical JSONL (file or AF_UNIX)
                 |                                  |
                 |                     decode -> validate -> dedup
                 |                                  |
                 +----------------+-----------------+
                                  |
                                  v
                 Ring buffer with sequence and CRC checks
                                  |
                                  v
        Worker pipeline: validate -> decode -> normalize -> classify -> enrich
                                  |
                                  v
        Filter and deduplicate -> correlate -> analyze anomalies
                                  |
          +-----------------------+-----------------------+
          |                       |                       |
          v                       v                       v
  Append-only storage      OTLP-shaped JSON export   Live dashboard
          |
          v
  Replay, verification, final human and JSON summaries
```

The canonical ingress path is the Member A receiver boundary. It is implemented
for a file (replay) and a local `AF_UNIX` stream; see `INGEST.md` for the exact
contract, mappings, counters, and limitations.

## File Layout

Every project file lives directly in this directory. Core runtime files include `main.c`, `types.h`, `constants.h`, `logger.c`, `config.c`, `cli.c`, `ring_buffer.c`, `memory_pool.c`, `parser.c`, `normalizer.c`, `classifier.c`, `filter.c`, `correlator.c`, `anomaly_detector.c`, `resource_monitor.c`, `telemetry.c`, `storage.c`, `protobuf_serializer.c`, `exporter.c`, `dashboard.c`, `health.c`, `platform.c`, and `statistics.c`.

Canonical ingress files are `canonical.c`/`canonical.h` (strict JSON decode and field mapping) and `ingress.c`/`ingress.h` (framing, file and Unix-socket transports, deduplication, counters).

Compatibility and extension-facing modules include `buffer.c`, `decoder.c`, `encoder.c`, `analyzer.c`, `process_tracker.c`, `timeline.c`, `scheduler.c`, `filesystem_monitor.c`, `network_monitor.c`, `syscall_monitor.c`, `file_writer.c`, `file_reader.c`, `otlp_client.c`, `metrics.c`, `terminal_ui.c`, `telemetry.bpf.c`, and `telemetry.skel.h`.

Tests are `test_buffer.c`, `test_parser.c`, `test_storage.c`, `test_exporter.c`, `test_correlator.c`, `test_dashboard.c`, `test_configuration.c`, `test_memory_pool.c`, `test_utilities.c`, `test_ingest_pipeline.c`, `test_canonical.c`, and `test_ingress.c` (12 CTest cases).

Contract fixtures are `sample_normal.jsonl` and `sample_suspicious.jsonl`. They are synthetic replay data, not captured live traffic.

Documentation files are `README.md`, `INGEST.md`, `MEMBER_A_REPORT.md`, `ARCHITECTURE.md`, `DESIGN.md`, `BUILD.md`, `INSTALL.md`, `CONFIGURATION.md`, `TELEMETRY_FORMAT.md`, `EXAMPLES.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, and `LICENSE`.

## Requirements

- Linux kernel 6.x recommended.
- GCC or Clang with C17 support.
- pthread-capable libc.
- CMake 3.20 or newer for the normal build.
- Optional: libbpf, protobuf-c, and zlib. CMake detects and links them when present.
- Root or elevated capabilities are required only for real eBPF attachment on hardened systems. The included runtime still collects procfs/userspace telemetry without those privileges.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Debug build:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DTP_ENABLE_WARNINGS_AS_ERRORS=ON
cmake --build build-debug
```

This project builds warning-free with `-Wall -Wextra -Werror` in the default
configuration. The 12-test suite is registered with CTest:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Direct compiler validation is also possible:

```sh
cc -std=c17 -D_GNU_SOURCE -Wall -Wextra -Werror -pthread *.c -o collector
```

For direct builds, exclude `test_*.c`, or compile one test standalone against
the built core library:

```sh
cc -std=c17 -D_GNU_SOURCE -DTP_TEST_STANDALONE -Wall -Wextra -Werror -pthread -I. \
  test_ingress.c -Lbuild -lcollector_core -o /tmp/test_ingress
```

## Running

```sh
./collector --storage telemetry.tpbin --export otlp-export.jsonl
./collector --run-seconds 10 --disable-dashboard --log-level debug
./collector --verify telemetry.tpbin
./collector --replay telemetry.tpbin
./collector --print-config --disable-export

# Canonical ingress: replay a JSONL file through the full pipeline
./collector --ingest-file sample_normal.jsonl --storage /tmp/normal.tpbin \
  --disable-dashboard --disable-export --run-seconds 2

# Canonical ingress: listen on a local Unix socket (newline-delimited JSON)
./collector --ingest-socket /tmp/demo.sock --storage /tmp/socket.tpbin \
  --disable-dashboard --disable-export --run-seconds 3
```

Important options:

- `--config PATH`: load key=value configuration.
- `--storage PATH`: append-only telemetry file.
- `--export PATH`: OTLP JSON export destination.
- `--enable-dashboard` / `--disable-dashboard`: control terminal dashboard.
- `--enable-export` / `--disable-export`: control export.
- `--log-level LEVEL`: fatal, error, warning, success, info, debug, or trace.
- `--buffer-size N`: ring capacity.
- `--workers N`: worker count.
- `--run-seconds N`: bounded runtime for automation.
- `--ingest-file PATH`: read canonical JSONL once to EOF, then exit the reader.
- `--ingest-socket PATH`: accept canonical JSONL on an `AF_UNIX` stream socket.
- `--verify PATH`: checksum verification.
- `--replay PATH`: replay records and print a summary.

`--ingest-file` is a replay fallback, not `tail -f`. A file must end with a
newline; a partial final line is counted and not parsed. See `INGEST.md`.

## Configuration

Configuration precedence is defaults, config file, environment variables, then CLI arguments. In the current `main.c` wiring, though, the config file and environment are applied **after** CLI parsing, so an environment variable can override a CLI flag. Use a clean environment when recording reproducible runs; this quirk is documented in `INGEST.md`.

Supported config keys include `buffer_size`, `worker_count`, `storage_path`, `export_endpoint`, `ingest_file`, `ingest_socket`, `log_level`, `dashboard`, `export_enabled`, `dashboard_interval_ms`, `run_seconds`, `retry_max`, `retry_backoff_ms`, and `retention_hours`.

Environment variables include `TP_BUFFER_SIZE`, `TP_WORKERS`, `TP_STORAGE_PATH`, `TP_EXPORT_ENDPOINT`, `TP_INGEST_FILE`, `TP_INGEST_SOCKET`, `TP_LOG_LEVEL`, `TP_DASHBOARD`, and `TP_EXPORT`.

Example config:

```text
buffer_size=8192
worker_count=6
storage_path=/var/lib/collector/telemetry.tpbin
export_endpoint=/var/lib/collector/otlp-export.jsonl
ingest_file=/var/lib/collector/events.jsonl
log_level=info
dashboard=true
export_enabled=true
dashboard_interval_ms=1000
retry_max=5
retry_backoff_ms=250
retention_hours=48
```

## Telemetry Pipeline

Each event is validated for version, timestamp, type, payload size, and checksum where applicable. The parser accepts typed records, the normalizer fills host/process/resource attributes, the classifier maps events into categories, the filter removes duplicates inside a configurable window while retaining security and high-severity data, the correlator updates process timelines, and the analyzer emits confidence-scored anomaly metadata.

Stored records carry ring sequence numbers, timestamps, payload, resource attributes, analysis metadata, trace IDs, span IDs, correlation IDs, and export status. With multiple workers, records are appended in worker completion order; use the stored sequence number, not file position, to recover ring order.

## Canonical Ingress

`canonical.c` decodes one newline-delimited JSON object per the God's Eye event
contract and maps it into the existing `tp_event_t`. `ingress.c` owns framing,
transport, deduplication, and counters. Only decoded and validated events are
pushed into the ring buffer; the rest are counted as rejected, oversized,
truncated, duplicate, or dropped.

Currently implemented transports:

- File: `--ingest-file PATH` (read once to EOF; replay fallback).
- Local Unix stream socket: `--ingest-socket PATH` (`AF_UNIX`, up to 8 clients).

Not implemented: TCP, vsock, shared memory, seL4/virtio, producer
authentication, and encryption. The fixtures are synthetic. Full contract
mapping, rejection rules, counters, and the test matrix are in `INGEST.md`.

The collector's event layout is frozen at 3120 bytes. Ingress preserves
producer sequence numbers in the event payload (`canonical.sequence=N`) because
the ring assigns its own sequence on push. Events marked as ingested are never
given the collector's local PID, PPID, or session ID, which would otherwise
attribute producer-domain events to the wrong process.

## Dashboard And Logging

The dashboard refreshes roughly once per configured interval and reports current counters, queue utilization, dropped events, CRC failures, anomalies, latency, and health status. Logs include timestamp, module, thread ID, CPU, severity, correlation ID when present, and ANSI color when enabled.

## Status And Honest Claims

- **eBPF is not attached.** `telemetry.bpf.c` is a plain C stub and
  `telemetry.skel.h` is a hand-written fake skeleton. Only BTF/CO-RE capability
  detection exists; `core_ready=yes` does not mean a probe is loaded.
- **The exporter is OTLP-shaped JSONL, not protobuf OTLP.** `TP_HAVE_LIBBPF`,
  `TP_HAVE_PROTOBUF_C`, and `TP_HAVE_ZLIB` are defined by CMake when the
  libraries exist, but current runtime code does not use them.
- **There is no seL4, VM, vsock, or remote-guest integration.** No such
  artifacts exist in this tree. The transport and topology decision is open.
- **File ingress is replay.** The sample fixtures are synthetic, not captured
  production traffic.
- **Ingress is unauthenticated.** Anyone who can read/write the configured file
  or socket can inject events; protect those paths with filesystem permissions.
- **Deduplication and sequence tracking are in-memory and bounded.** A receiver
  restart re-delivers replayed input (at-least-once behavior).

## Security

Collector handles malformed telemetry by discarding invalid records and incrementing diagnostic counters. Storage uses checksums to identify corruption. eBPF attachment should be run with the narrowest capability set available on the target distribution. Telemetry can contain process names, paths, command lines, and network metadata; protect storage and export destinations accordingly. The canonical ingress socket and file are also unauthenticated local inputs; restrict their permissions.

## Extending

Add new probes by producing `tp_event_t` records and pushing them into `tp_ring_buffer_t`. Add analyzers by processing normalized events after correlation. Add exporters by following `exporter.h` and preserving retry/error accounting. Add storage backends by preserving the replay and integrity contract in `storage.h`.
