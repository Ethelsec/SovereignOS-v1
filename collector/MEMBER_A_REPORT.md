# Member A Implementation Report — Collector Receiver Integration

**Project:** Collector Telemetry Platform (C17, Linux)
**Plan followed:** `gods_eye_sunday_plan/` — especially `02_ARCHITECTURE.md`,
`03_EVENT_CONTRACT.md`, `04_TEAM_PLAN.md`, `06_INTEGRATION_AND_TEST_PLAN.md`,
`07_SECURITY_AND_LIMITATIONS.md`, and `12_DEFINITION_OF_DONE.md`.
**Role:** Member A — collector and platform integration, receiving side.
**Authoritative detail:** `INGEST.md` (contract, mappings, rejection rules,
counters, test matrix). This report is the implementation summary.

---

## 1. Direct answer: where did the data come from?

No new live probe or remote guest source was added by this work. Member A owns
the **receiver**, so the data used to exercise the new code came from three
places:

| # | Data source | Mechanism | What it produced | Honest label |
|---|---|---|---|---|
| 1 | Existing local procfs sampler | `telemetry.c` → `resource_monitor.c`, every 250 ms | `cpu_pct`, `mem_used_kb`, `net_rx_bytes`, `net_tx_bytes` from `/proc/stat`, `/proc/meminfo`, `/proc/net/dev` | **Live local host telemetry** (collector's pre-existing feature) |
| 2 | Existing startup/health event | `tp_emit_startup_events()` in `telemetry.c` | `type=health`, payload `kernel=… arch=… btf=… core_ready=…` from `uname()` and `/sys/kernel/btf/vmlinux` | **Live local capability check**, not eBPF attachment |
| 3 | Canonical JSONL fixtures | `--ingest-file sample_normal.jsonl` / `sample_suspicious.jsonl` | `process.start`, `service.request`, `auth.failure`, `resource.threshold` events | **Synthetic replay data** |
| 4 | Canonical JSONL over Unix socket | `--ingest-socket`, Python `AF_UNIX` client, split writes | The same synthetic events over a real local stream transport | **Synthetic replay data over a real transport** |

### What was **not** a data source

- **No eBPF probe was loaded.** `telemetry.bpf.c` is a plain C stub and
  `telemetry.skel.h` is a hand-written fake skeleton. `core_ready=yes` only
  means BTF information exists.
- **No guest VM, seL4 image, vsock, or TCP connection.** None of those
  artifacts exist in this tree.
- **No Linux audit log, filesystem monitor, or network monitor feed.** Those
  modules exist in the directory but are not started by `main.c`.
- **No production or captured traffic.** Both fixtures are hand-written
  synthetic events.

So: the **feature being implemented** is canonical event ingestion; the
**data used to prove it** is synthetic replay plus the collector's existing
procfs/health telemetry, with every fixture explicitly labelled.

---

## 2. Plan tasks → implementation status

Member A tasks from `04_TEAM_PLAN.md`:

| # | Task | Status | Where |
|---|---|---|---|
| 1 | Inspect collector and record build/run/input expectations | **DONE** | `README.md`, `INGEST.md` §6, this report |
| 2 | Identify what it can actually receive today | **DONE** | §1 above; baseline receives `tp_event_t` via ring, procfs/startup internally, no external wire protocol |
| 3 | Smallest safe adapter for the agreed event frame | **DONE** | `canonical.c` (decode/map) + `ingress.c` (frames/transport) |
| 4 | Validate length and schema; reject malformed input without crashing | **DONE** | `tp_canonical_decode()`, framing limits, rejection counters |
| 5 | Receiver-side tests, logs, counters | **DONE** | `test_canonical.c`, `test_ingress.c`, 11 `ingress_*` counters |
| 6 | Pair with Member B on a real end-to-end run | **PARTIAL / NOT RUN** | Receiver→analysis→storage→replay is proven with fixtures; no Member B live producer exists yet |

Member A deliverables:

| Deliverable | Status | Evidence |
|---|---|---|
| `collector/README.md` update | **DONE** | `README.md` updated with ingress, truthfulness, commands |
| Receiver adapter | **DONE** | `canonical.[ch]`, `ingress.[ch]` |
| Tests for valid, malformed, truncated, oversized input | **DONE** | `test_canonical`, `test_ingress`, malformed CLI demo |
| Recorded build/run commands | **DONE** | `README.md`, `INGEST.md` §6, this report |

Do **not** rewrite the collector or promise a new transport: the collector was
not rewritten. The event ABI stayed frozen and the transport is explicitly
local file/Unix socket only.

---

## 3. Runtime architecture after the change

```text
                      ┌──────────────────────────────┐
                      │ existing local host features │
                      │  procfs sampler (250 ms)     │
                      │  startup/health event        │
                      └──────────────┬───────────────┘
                                     │
┌──────────────────────────┐         │
│ canonical JSONL transport│         │
│  --ingest-file (replay)  │         │
│  --ingest-socket (AF_UNIX)│        │
└────────────┬─────────────┘         │
             │                       │
             v                       │
   ingress.c framing                 │
   (newline, 8192 cap, CRLF,         │
    oversize/truncation)             │
             │                       │
             v                       │
   canonical.c strict decode         │
   (12 required fields, allow-lists, │
    timestamps, limits)              │
             │                       │
             v                       │
   ingress.c dedup + gap counters    │
             │                       │
             +───────────┬───────────┘
                         v
              tp_ring_push() with CRC
              (ring sequence assigned here)
                         v
        worker_thread(): pop (CAS claim)
                         v
   normalize -> validate -> anomaly -> filter -> correlate
                         v
              storage + exporter + dashboard
                         v
          --verify / --replay / summaries
```

Only frames that pass `tp_canonical_decode()` are pushed to the ring. Rejected,
oversized, truncated, and duplicate frames are counted and never reach the
analyzer as normal events.

---

## 4. Files created

All files are directly in `Collector/`, preserving the flat-directory rule.

### 4.1 `canonical.h` — decoder API

Declares:

- `TP_CANONICAL_MAX_FRAME` = 8192 bytes, `TP_CANONICAL_MAX_REASON` = 160.
- `tp_canonical_event_t { tp_event_t event; uint64_t producer_sequence; }`.
- `tp_canonical_decode(frame, len, out, reason, reason_len)`.
- `tp_canonical_status_name(status)`.

Contract: received frame → mapped, validated `tp_event_t`, **before** the ring.

### 4.2 `canonical.c` — strict decoder and mapping

Responsibilities:

- Bounds-check the frame (reject `> 8192` before parsing).
- Parse JSON manually with no third-party dependency: objects, strings,
  integers, booleans, `null`; reject malformed syntax, trailing bytes,
  duplicate keys, unknown fields, invalid escapes, and control characters.
- Require all twelve contract fields:
  `schema_version`, `event_id`, `timestamp_utc`, `timestamp_monotonic_ns`,
  `source`, `event_type`, `severity`, `host_id`, `process`, `network`,
  `attributes`, `sequence`.
- Require `process` / `network` to be an object or `null`;
  require `attributes` to be a **flat object** (`{}` allowed, `null` rejected).
- Parse RFC 3339 UTC timestamps, including numeric offsets and fractional
  seconds; reject impossible dates/times and missing timezone designators.
- Map `event_id`: decimal and `0x` hex pass through; other strings are hashed
  with FNV-1a and flagged.
- Map the seven allowed event types and five allowed severities; reject
  `critical`, `trace`, `debug`, and anything outside the frozen list.
- Map `process` to `pid`/`ppid`/`process_name`/`executable_path` (basename
  only, never reads local `/proc/<pid>`).
- Map `network` and `attributes` into the internal 1024-byte payload.
- Record lossy provenance in the payload instead of silently truncating:
  `canonical.sequence`, `canonical.event_id` (hashed), `canonical.severity`
  (low/unknown), `canonical.monotonic=receiver_clock`,
  `canonical.pid=unknown`, `network.*`, and attribute `key=value` pairs.
- Reject if the payload budget would be exceeded.
- Set provenance flags: `TP_FLAG_INGEST`, `TP_FLAG_EVENT_ID_HASHED`,
  `TP_FLAG_SEVERITY_UNMAPPED`, `TP_FLAG_SEVERITY_UNKNOWN`,
  `TP_FLAG_MONOTONIC_RECEIVER_CLOCK`, `TP_FLAG_PID_UNKNOWN`.
- Finish with `tp_validate_event()` before returning.

### 4.3 `ingress.h` — transport API

Declares:

- Ingress kinds: file, socket, none.
- Outcomes: `TP_INGRESS_ACCEPTED`, `TP_INGRESS_DUPLICATE`,
  `TP_INGRESS_QUEUE_FULL`.
- Bounded dedup window (256) and client limit (8).
- Lifecycle: `tp_ingress_init`, `open`, `open_file`, `open_socket`, `start`,
  `stop`, `close`, `run_file`, `handle_frame`, `tp_ingress_thread`.

### 4.4 `ingress.c` — framing, transports, dedup, counters

Responsibilities:

- **Framing:** one JSON object per `\n`; strip trailing `\r`; ignore blank
  lines; discard over-8192-byte lines up to the next newline; count a partial
  final line as truncated and do not parse it.
- **File transport:** `--ingest-file` opens the file, reads once to EOF,
  processes frames, and exits the reader thread. It is a replay fallback, not
  `tail -f`.
- **Unix socket transport:** `AF_UNIX` / `SOCK_STREAM`, non-blocking,
  `poll()` with 100 ms timeout, up to 8 concurrent clients, stale-socket
  cleanup, refuses to replace a non-socket path, removes the socket on stop.
- **Split-write safety:** one `read()` never equals one frame; a state machine
  buffers partial frames.
- **Deduplication:** last 256 **accepted** event IDs.
- **Sequence accounting:** first producer sequence establishes a baseline;
  later gaps increment `ingress_gaps`; non-advancing frames increment
  `ingress_out_of_order`. It never silently repairs producer order.
- **Ring handoff:** `tp_ring_push()`; on full ring, drop-newest and count in
  both `ingress_dropped` and `stats.dropped`; do not insert the dropped ID
  into dedup so a retry can still land.
- **Logging:** only short reasons at DEBUG; raw untrusted frames are never
  logged.

### 4.5 `test_canonical.c` — decoder test suite

Covers:

- ABI guard: `sizeof(tp_event_t) == 3120`.
- Full valid event and minimal valid event (`process`/`network` null,
  `timestamp_monotonic_ns` null, empty attributes).
- Event ID forms: decimal, hex, hashed string.
- Severity mapping and rejection of `critical`.
- Timestamp offsets, fractional seconds, impossible dates, missing timezone,
  wrong JSON type.
- Unsupported schema version, unsupported event type, `auth.failure`.
- Missing required fields, `attributes: null`, missing `process`.
- Malformed JSON, duplicate keys, unknown fields, trailing bytes, control
  escapes.
- Frame/field limits, nested attributes, negative/string PIDs.
- Network and attribute flattening.

### 4.6 `test_ingress.c` — receiver test suite

Covers:

- File framing: two events, blank line, CRLF, bytes/frames/accepted counters.
- Framing errors: oversize line, malformed line, partial final line.
- Duplicate IDs plus producer gaps and out-of-order frames.
- Queue-full drop-newest and retry behavior.
- Concurrent consumers: 4000 events, 4 threads, each event exactly once.
- Missing source file: clear `TP_ERR_IO` and transport-error counter.
- Receiver restart: fresh object re-delivers the same file (at-least-once).
- Unix socket: split write across `sendall` calls, then shutdown and socket
  removal.
- End-to-end: decode → validate → analyze → store → verify → replay, with one
  benign and one suspicious event.

### 4.7 `sample_normal.jsonl` and `sample_suspicious.jsonl`

- `sample_normal.jsonl`: `process.start` and `service.request`, both `info`.
- `sample_suspicious.jsonl`: `auth.failure` (`high`) and
  `resource.threshold` (`high`).
- Both follow the twelve-field contract and are explicitly **synthetic replay
  data**.

### 4.8 `INGEST.md`

The authoritative receiver document: status table, boundary/data flow, field
mapping, severity mapping, event-type allow-list, rejection rules, framing,
counters, ring semantics, run commands, PASS/NOT RUN test matrix, limitations,
interface changes, and open team decisions.

---

## 5. Files modified

| File | What changed | Why |
|---|---|---|
| `types.h` | Added 11 `ingress_*` atomic counters; added `ingest_path` / `ingest_is_socket`; moved `TP_FLAG_*` macros here. `tp_event_t` layout **unchanged**. | Counters and config without breaking the 3120-byte event ABI |
| `canonical.h` | New file (see above) | Decoder API |
| `canonical.c` | New file (see above) | Strict decode and mapping |
| `ingress.h` | New file (see above) | Transport/framing API |
| `ingress.c` | New file (see above) | File and Unix-socket receiver |
| `normalizer.c` | Skip local PID/PPID/session substitution when `TP_FLAG_INGEST` is set | A producer PID must not become the collector's PID |
| `ring_buffer.c` | `tp_ring_pop()` claims `read_seq` with CAS before copying | Fix duplicate/zeroed events under multiple workers |
| `main.c` | Include and initialize ingress; start it before startup events; stop/join before workers; reconcile `dropped` with the ring at shutdown | Lifecycle and accurate counters |
| `config.c` | `ingest_file` / `ingest_socket` keys and `TP_INGEST_FILE` / `TP_INGEST_SOCKET` env vars; print config | Configuration |
| `cli.c` | `--ingest-file` / `--ingest-socket` flags | CLI |
| `statistics.c` | Human summary and JSON summary include all ingress counters | Observability |
| `CMakeLists.txt` | Add `canonical.c`, `ingress.c`, `test_canonical`, `test_ingress`; install `INGEST.md` | Build and packaging |
| `README.md` | Architecture diagram, file layout, 12 tests, build/run, ingest options, config, canonical ingress section, honest status section | Member A deliverable |
| `CONFIGURATION.md` | Ingest keys/env vars and precedence quirk | Keep config docs accurate |
| `EXAMPLES.md` | File and socket ingest examples | Usability |
| `CHANGELOG.md` | Unreleased entry for ingress, CAS fix, truthfulness docs | Change tracking |

`AGENTS.md` was intentionally left untouched; it still says “10 tests” even
though the suite now has 12.

---

## 6. Contract details implemented

### 6.1 Required fields

All twelve fields are mandatory. `process` and `network` must be present as an
object or `null`. `attributes` must be present as a flat object. Missing or
wrong-typed values are rejected.

### 6.2 Field mapping (summary)

| Canonical | Internal | Lossy behavior |
|---|---|---|
| `schema_version` | not stored | must be `"1"` |
| `event_id` | `event_id` | decimal/hex preserved; other strings FNV-1a hashed |
| `timestamp_utc` | `timestamp.wall_ns` | RFC 3339 → epoch ns |
| `timestamp_monotonic_ns` | `timestamp.monotonic_ns` | `null`/0 → receiver clock + flag |
| `source` | `source_probe` | max 31 bytes; `source_module="ingress"` |
| `event_type` | `type` | 7-name allow-list |
| `severity` | `severity` | documented low/unknown remap |
| `host_id` | `host_id` | max 63 bytes |
| `process` | `pid`, `ppid`, names | basename only |
| `network` | payload `network.*` | flattened |
| `attributes` | payload `key=value` | flat only |
| `sequence` | payload `canonical.sequence` | ring owns internal `sequence` |

### 6.3 Severity mapping

| Canonical | Internal | Flag |
|---|---|---|
| `info` | `TP_SEV_INFO` | — |
| `low` | `TP_SEV_INFO` | `TP_FLAG_SEVERITY_UNMAPPED` |
| `medium` | `TP_SEV_NORMAL` | — |
| `high` | `TP_SEV_HIGH` | — |
| `unknown` | `TP_SEV_INFO` | `TP_FLAG_SEVERITY_UNMAPPED \| TP_FLAG_SEVERITY_UNKNOWN` |
| `critical` | rejected | — |

### 6.4 Event type allow-list

`process.start`, `process.exit`, `network.connection`, `service.request`,
`auth.failure`, `resource.threshold`, `demo.marker`.

### 6.5 Rejection rules

Rejected without a ring push: overlong frame; non-object/malformed JSON;
trailing bytes; duplicate/unknown keys; missing required field; unsupported
schema; unsupported event type/severity; invalid timestamp; empty or
unrepresentable `event_id`; empty/overlong `source` or `host_id`; negative,
non-integer, or overflowing PID/PPID; overlong names; nested/oversized
attributes; payload-budget overflow; control characters.

Rejections increment counters; raw frames are never logged.

---

## 7. Counters added

| Counter | Meaning |
|---|---|
| `ingress_bytes` | Transport bytes consumed |
| `ingress_frames` | Complete non-blank frames handed to the decoder |
| `ingress_accepted` | Decoded, validated, unique, pushed |
| `ingress_rejected` | Decode/validation failure |
| `ingress_duplicates` | ID seen in the 256-event window |
| `ingress_dropped` | Ring full (drop-newest) |
| `ingress_gaps` | Missing producer sequence numbers |
| `ingress_out_of_order` | Non-advancing producer sequence |
| `ingress_oversize` | Lines larger than 8192 bytes |
| `ingress_truncated` | Partial final line |
| `ingress_transport_errors` | Open/bind/accept/read/poll failure or client-limit refusal |

All appear in the human summary and `telemetry-summary.json`.

---

## 8. Build and test evidence

Clean build:

```sh
cmake -S . -B /tmp/opencode/tp-clean -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/tp-clean -j4
ctest --test-dir /tmp/opencode/tp-clean --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 12
Build flags: -Wall -Wextra -Werror
No warnings or errors in the clean build log.
```

Direct non-test syntax check:

```sh
cc -std=c17 -D_GNU_SOURCE -Wall -Wextra -Werror -pthread -I. \
  -fsyntax-only $(ls *.c | grep -v '^test_')
```

Result: `direct-syntax: OK`.

Test suite (12):

1. `test_buffer`
2. `test_parser`
3. `test_storage`
4. `test_exporter`
5. `test_correlator`
6. `test_dashboard`
7. `test_configuration`
8. `test_memory_pool`
9. `test_utilities`
10. `test_ingest_pipeline`
11. `test_canonical` (new)
12. `test_ingress` (new)

---

## 9. Demo evidence

### 9.1 Normal fixture (file replay)

```sh
collector --ingest-file sample_normal.jsonl --storage /tmp/normal.tpbin \
  --disable-dashboard --disable-export --run-seconds 2
collector --verify /tmp/normal.tpbin
```

Observed: `ingress_frames=2`, `ingress_accepted=2`, `ingress_rejected=0`,
`anomalies=0`, `dropped=0`, `verify ... status=ok`. Replay payloads contain
`canonical.sequence=1/2` and `canonical.event_id=...`.

### 9.2 Suspicious fixture (file replay)

Observed: `ingress_frames=2`, `ingress_accepted=2`, `ingress_rejected=0`,
`ingress_bytes=817`, `duplicates=0`, `dropped=0`, `anomalies=2`, storage
verification `status=ok` with `records=10` (2 ingested + startup/procfs).
The `auth.failure` event is flagged at confidence 0.70; the resource spike at
0.95.

### 9.3 Unix socket with split write

A Python client sent `sample_normal.jsonl` in two chunks over
`AF_UNIX`/`SOCK_STREAM`, then shut down the write side.

Observed: `ingress_frames=2`, `ingress_accepted=2`, `ingress_truncated=0`,
`ingress_transport_errors=0`, `verify ... status=ok`, socket file removed after
shutdown.

### 9.4 Mixed malformed input

Input: one valid event, one unknown field, one unsupported event type, one bad
severity, one line over 8192 bytes, one final partial line.

Observed:

```text
ingress_accepted=1
ingress_rejected=3
ingress_oversize=1
ingress_truncated=1
ingress_frames=4
```

Rejected/oversized/truncated input never reached analysis or storage;
`--verify` still reported `status=ok`.

### 9.5 Storage record counts

`--verify` reports 10–14 records for the demos because the collector also
stores its startup/health event and periodic procfs metrics. Only the accepted
ingress frames come from the canonical fixture; the rest are the pre-existing
local sources described in §1.

---

## 10. Important bug found and fixed during this work

The first file-ingest demo produced duplicate records and one zeroed record
(`seq=0`, empty payload). Root cause: `tp_ring_pop()` read `read_seq` and
updated it non-atomically, so concurrent workers could pop the same slot.

Fix: claim `read_seq` with `atomic_compare_exchange_strong` before copying the
slot. Added `test_concurrent_consumers`: 4000 pre-filled events, 4 consumers,
asserting every sequence is delivered exactly once with no duplicates or zeroed
events. After the fix, the demo had `duplicates=0`, `dropped=0`, and clean
verification.

---

## 11. Limitations and NOT RUN items

### Implemented and tested

- Strict canonical JSON decode and validation.
- File replay framing (including malformed/oversized/truncated input).
- Local `AF_UNIX` stream ingest with split writes.
- Deduplication, sequence gaps, out-of-order accounting.
- Queue-full drop-newest accounting.
- Receiver→analysis→storage→verify→replay path.
- Multi-worker ring correctness.

### Not implemented / NOT RUN

| Item | Status | Reason |
|---|---|---|
| TCP / vsock / shared-memory transport | NOT IMPLEMENTED | topology not confirmed |
| seL4 / VM / virtio integration | NOT IMPLEMENTED | no artifacts in tree |
| Live remote producer end-to-end | NOT RUN | Member B producer not available |
| eBPF attachment | NOT IMPLEMENTED | stub + fake skeleton only |
| Protobuf OTLP export | NOT IMPLEMENTED | JSONL-shaped export only |
| Ingress authentication/encryption | NOT IMPLEMENTED | local file/socket permissions only |
| Ingress rate limiting | NOT IMPLEMENTED | bounded by frame size, clients, ring, drop policy |
| Permission-denied source (`EACCES`) | NOT RUN | missing-file path tested; EACCES not automated |
| OS-process restart of the collector | NOT RUN | object-level restart tested instead |
| Video/demo recording | NOT RUN | outside Member A scope |

Other honest notes:

- Dedup and sequence state are in-memory; a receiver restart re-delivers
  replayed input (at-least-once).
- File mode assumes a regular file; a FIFO can block shutdown.
- `--ingest-file` reads once to EOF; the process still runs until
  `--run-seconds` or a signal.
- The config file/environment are applied after CLI parsing in `main.c`, so an
  environment variable can override a CLI flag. Use a clean environment for
  reproducible demos.
- `--replay` prints collector type names, so `service.request` appears as
  `http` and `auth.failure` as `security`.

---

## 12. Interface changes and compatibility

- `tp_event_t` remains **3120 bytes**; `test_canonical` asserts this so an
  accidental field addition fails the suite.
- The storage header and record layout are untouched; `--verify` and
  `--replay` still read files written before this change.
- New fields live in `tp_stats_t` and `tp_config_t`, not in the stored event.
- All new files are flat in `Collector/`.

---

## 13. Reproduce in three commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Then:

```sh
./build/collector --ingest-file sample_suspicious.jsonl \
  --storage /tmp/suspicious.tpbin --disable-dashboard --disable-export \
  --run-seconds 2
./build/collector --verify /tmp/suspicious.tpbin
```

---

## 14. Bottom line

Member A's receiver work is implemented and verified: the collector can now
accept the canonical event contract from a file (replay) or a local Unix
socket, strictly validate it, count every failure mode, and pass only
validated events into the existing analysis/storage/export pipeline.

The data used to prove it is **synthetic canonical replay** plus the
collector's **existing live procfs and startup telemetry**. There is no live
guest, no eBPF attachment, and no seL4/vsock/TCP path yet. Those limitations
are documented rather than implied away.
