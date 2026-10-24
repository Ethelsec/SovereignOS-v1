# Canonical Event Ingress (Member A)

Receiver-side integration for the God's Eye Sunday plan:
`gods_eye_sunday_plan/02_ARCHITECTURE.md` §6 (interface boundaries),
`03_EVENT_CONTRACT.md` (canonical event), and
`06_INTEGRATION_AND_TEST_PLAN.md` (test matrix).

This document states exactly what is implemented, what is lossy, and what is
**not** implemented. Do not upgrade any statement here into a live-guest or
seL4 claim.

## 1. Status

| Boundary | Status | Notes |
|---|---|---|
| Canonical JSONL decode + validation | **Implemented** | `canonical.c`, strict allow-list |
| File transport (newline-delimited JSON) | **Implemented** | `--ingest-file`; reads once to EOF (replay fallback) |
| AF_UNIX stream transport | **Implemented** | `--ingest-socket`; local only, up to 8 clients |
| TCP / vsock transport | Not implemented | no topology or endpoint agreed |
| Shared memory / ring handoff from guest | Not implemented | collector ring is process-local |
| seL4 / virtio / VM boundary | Not implemented | no seL4 artifacts exist in this tree |
| Producer authentication / encryption | Not implemented | local socket relies on filesystem permissions |

The collector's existing probes (procfs, startup events) are unchanged. Ingress
events enter the **same** ring buffer and worker pipeline.

## 2. Boundary and data flow

```text
transport bytes
   |
   v
ingress.c      framing: '\n' delimiter, 8192-byte cap, CRLF, truncation
   |
   v
canonical.c    strict JSON decode -> mapping -> tp_validate_event()
   |
   v
ingress.c      event_id dedup (256) -> producer gap counters -> ring push
   |
   v
ring_buffer.c  collector sequence assigned on push (CAS consumer pop)
   |
   v
worker pipeline: normalize -> validate -> anomaly -> filter -> correlate
                 -> storage + export
```

Only events that pass `tp_canonical_decode()` reach `tp_ring_push()`. Invalid,
oversized, truncated, and duplicate frames are counted and dropped; they never
reach the analyzer as "normal" data.

## 3. Contract handling

### 3.1 Required fields

All twelve fields from `03_EVENT_CONTRACT.md` §2 are required:
`schema_version`, `event_id`, `timestamp_utc`, `timestamp_monotonic_ns`,
`source`, `event_type`, `severity`, `host_id`, `process`, `network`,
`attributes`, `sequence`.

`process` and `network` must be present and be an object or `null`.
`attributes` must be present and be a **flat object** (`{}` is valid; `null`
is rejected). `timestamp_monotonic_ns` may be `null`.

### 3.2 Field mapping

| Canonical | Internal (`tp_event_t`) | Lossy? |
|---|---|---|
| `schema_version` | not stored | must equal `"1"` |
| `event_id` | `event_id` (`uint64_t`) | decimal/`0x` hex pass through; other strings are FNV-1a hashed |
| `timestamp_utc` | `timestamp.wall_ns` | RFC 3339 → epoch ns; sub-nanosecond precision rejected |
| `timestamp_monotonic_ns` | `timestamp.monotonic_ns` | `null`/0 uses receiver clock and sets a flag |
| `source` | `source_probe` | max 31 bytes; `source_module` is forced to `"ingress"` |
| `event_type` | `type` | seven-name MVP allow-list |
| `severity` | `severity` | see 3.3 |
| `host_id` | `host_id` | max 63 bytes |
| `process` | `pid`, `ppid`, `process_name`, `executable_path` | basename only; never reads local `/proc/<pid>/exe` |
| `network` | payload `network.*` | flattened into payload |
| `attributes` | payload `key=value,...` | flattened; keys ≤31 bytes, values ≤255 bytes |
| `sequence` | payload `canonical.sequence` | internal `sequence` is the ring sequence |

The contract has no free-form payload field. The internal payload is therefore
**synthesized provenance**, not producer-supplied bytes:

- always: `canonical.sequence=<producer sequence>`
- hashed ID: `canonical.event_id=<original string>`
- `low`/`unknown` severity: `canonical.severity=<original>`
- receiver clock fallback: `canonical.monotonic=receiver_clock`
- unknown process: `canonical.pid=unknown`
- network object: `network.protocol`, `network.local`, `network.remote`, `network.status`
- attributes: `key=value` pairs, comma-separated

Payload is capped at 1024 bytes. A frame whose provenance does not fit is
rejected; nothing is silently truncated.

### 3.3 Severity mapping (documented lossy)

| Canonical | Internal | Flag |
|---|---|---|
| `info` | `TP_SEV_INFO` | — |
| `low` | `TP_SEV_INFO` | `TP_FLAG_SEVERITY_UNMAPPED` |
| `medium` | `TP_SEV_NORMAL` | — |
| `high` | `TP_SEV_HIGH` | — |
| `unknown` | `TP_SEV_INFO` | `TP_FLAG_SEVERITY_UNMAPPED \| TP_FLAG_SEVERITY_UNKNOWN` |

`critical` and `trace`/`debug` are **rejected** because the frozen contract
does not define them. This is a contract decision to revisit, not a bug.

### 3.4 Event type allow-list

| Canonical | Internal type | Collector category |
|---|---|---|
| `process.start` | `TP_TYPE_PROCESS_START` | PROCESS + USERSPACE |
| `process.exit` | `TP_TYPE_PROCESS_EXIT` | PROCESS + USERSPACE |
| `network.connection` | `TP_TYPE_SOCKET_CONNECT` | NETWORK |
| `service.request` | `TP_TYPE_HTTP` | NETWORK |
| `auth.failure` | `TP_TYPE_SECURITY` | SECURITY |
| `resource.threshold` | `TP_TYPE_RESOURCE_SPIKE` | SYSTEM |
| `demo.marker` | `TP_TYPE_CUSTOM` | SYSTEM + CUSTOM |

`--replay` prints the collector type name, so `service.request` appears as
`http`. That is the existing collector's naming, not a contract change.

### 3.5 Rejection rules

The frame is rejected (no ring push) when any of these hold:

- frame exceeds 8192 bytes; top level is not a JSON object; malformed JSON
- trailing bytes after the object
- duplicate JSON keys, or any unknown field (strict allow-list)
- missing required field
- `schema_version != "1"`
- `event_type` or `severity` outside the allow-list
- invalid RFC 3339 timestamp, wrong JSON type, or impossible date/time
- `event_id` empty or not representable (bad hex, overflow)
- `source`/`host_id` empty; `source` > 31 bytes; `host_id` > 63 bytes
- `pid`/`ppid` negative, non-integer, or > 2^31-1
- overlong process basename (> 63 bytes)
- attribute key > 31 bytes, value > 255 bytes, nested value, or payload budget exceeded
- control characters (raw or escaped `\n`, `\r`, `\t`, …)

Rejections increment a counter and log only a short reason at DEBUG level.
Raw frames are never logged.

## 4. Framing and transport

- One JSON object per line, terminated by `\n`.
- A trailing `\r` is stripped (CRLF producers work); blank lines are ignored.
- Maximum frame: **8192 bytes**. Larger lines are discarded up to the next
  newline, counted in `ingress_oversize`, and never partially parsed.
- A partial line at EOF or connection close is counted in
  `ingress_truncated` and not parsed. Files must end with a newline.
- There is no declared-length field, so "truncated" means "no closing newline",
  not "declared length exceeds bytes received". This is the line-delimited
  option from `03_EVENT_CONTRACT.md` §5.
- File mode reads once to EOF. It is a replay fallback, not `tail -f`, and
  assumes a regular file (a FIFO can block shutdown).
- Socket mode is `AF_UNIX`/`SOCK_STREAM`, local only. It accepts up to 8
  concurrent clients, polls with a 100 ms timeout, and removes its socket file
  on shutdown.
- One `read()` is never assumed to equal one frame; writes may split anywhere.

## 5. Counters

All counters are exported in the human summary and `telemetry-summary.json`.

| Counter | Meaning |
|---|---|
| `ingress_bytes` | transport bytes consumed, including newlines |
| `ingress_frames` | complete non-blank frames handed to the decoder |
| `ingress_accepted` | decoded, validated, unique, and pushed to the ring |
| `ingress_rejected` | decode/validation failure |
| `ingress_duplicates` | `event_id` already seen in the 256-event window |
| `ingress_dropped` | ring full (drop-newest) |
| `ingress_gaps` | missing producer sequence numbers (capped at 10^6 per gap) |
| `ingress_out_of_order` | accepted frame whose producer sequence did not advance |
| `ingress_oversize` | lines larger than 8192 bytes |
| `ingress_truncated` | partial final line at EOF/disconnect |
| `ingress_transport_errors` | open/bind/accept/read/poll failure or client-limit refusal |

`received`/`processed` are worker-side counters. At shutdown, `dropped` is
reconciled with the ring's authoritative drop counter.

Deduplication is bounded and in-memory: the last 256 **accepted** event IDs.
Sequence state is also in-memory. A receiver restart therefore re-delivers a
replayed file (at-least-once); this is tested and documented, not hidden.

### 5.1 Ring buffer semantics

- Capacity is in **events** (`--buffer-size`, default 4096).
- Producers (procfs sampler, startup events, ingress) are serialized by a
  producer mutex. Consumers (`--workers`, default 4) claim read slots with an
  atomic compare-and-swap, so each event is delivered exactly once.
- The full-buffer policy is **drop-newest**. A rejected push is counted in both
  `ingress_dropped` and the ring's `dropped` counter.
- Each slot carries a CRC over the event header plus payload. Pop verifies it;
  corruption increments `crc_failures`/corrupt and the slot is skipped.
- The ring assigns the consumer-visible `sequence` on push. The producer
  sequence is not lost; it is kept in the payload as `canonical.sequence`.
- Restart behavior: ring contents, dedup state, and last producer sequence are
  process-local and reset on restart.

## 6. Running

Build and test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Replay the benign fixture through the full pipeline:

```sh
./build/collector --ingest-file sample_normal.jsonl \
  --storage /tmp/normal.tpbin --disable-dashboard --disable-export \
  --run-seconds 2
./build/collector --verify /tmp/normal.tpbin
./build/collector --replay /tmp/normal.tpbin
```

Feed the suspicious fixture (two events, two anomalies):

```sh
./build/collector --ingest-file sample_suspicious.jsonl \
  --storage /tmp/suspicious.tpbin --disable-dashboard --disable-export \
  --run-seconds 2
```

Live local socket (Python client, split write):

```sh
./build/collector --ingest-socket /tmp/demo.sock \
  --storage /tmp/socket.tpbin --disable-dashboard --disable-export \
  --run-seconds 3 &
python3 - <<'PY'
import socket, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/demo.sock')
data = open('sample_normal.jsonl', 'rb').read()
s.sendall(data[:100])
time.sleep(0.05)
s.sendall(data[100:])
s.shutdown(socket.SHUT_WR)
s.close()
PY
wait
```

The fixtures are **synthetic replay data**. `--ingest-socket` is a real local
transport, but no guest/VM/seL4 producer exists in this repository.

Configuration keys and environment variables:

```text
ingest_file=/path/to/events.jsonl    TP_INGEST_FILE
ingest_socket=/path/to/ingest.sock   TP_INGEST_SOCKET
```

CLI: `--ingest-file PATH`, `--ingest-socket PATH`.

Known quirk (pre-existing): `main.c` applies the config file and environment
**after** CLI parsing, so an env var can override a CLI flag. Use a clean
environment when recording a demo.

## 7. Test evidence

`ctest` result on 2026-10-23: **12/12 passed**, including the two new suites.
Build used `-Wall -Wextra -Werror`.

### Plan test matrix (`06_INTEGRATION_AND_TEST_PLAN.md` §3)

| Case | Covered by | Result |
|---|---|---|
| Valid event | `test_canonical` (full/minimal), `test_ingress` (file, socket, E2E) | PASS |
| Missing required field | `test_canonical::test_required_fields` | PASS |
| Unknown schema version | `test_canonical::test_schema_and_event_types` | PASS |
| Oversized frame | `test_canonical::test_limits`, `test_ingress::test_framing_errors` | PASS |
| Truncated frame | `test_ingress::test_framing_errors` | PASS |
| Bad timestamp/type | `test_canonical::test_timestamps` | PASS |
| Duplicate event | `test_ingress::test_duplicates_and_sequences` | PASS |
| Queue full | `test_ingress::test_queue_full` | PASS |
| Normal scenario | `test_ingress::test_end_to_end` (benign event) | PASS |
| Controlled pattern | `test_ingress::test_end_to_end` (`auth failed` → anomaly 0.70) | PASS |
| Receiver restart | `test_ingress::test_receiver_restart` (object-level restart) | PASS |
| Missing source | `test_ingress::test_missing_source` (clear `TP_ERR_IO`, counter) | PASS |
| Clean build | `cmake --build` with `-Werror` | PASS |
| OS-process restart | not automated | NOT RUN |
| seL4 / vsock / remote VM | not implemented | NOT RUN |
| Permission-denied source | missing-file path tested, EACCES not | NOT RUN |

### CLI evidence (this machine)

- File ingest of `sample_suspicious.jsonl`: `ingress_frames=2`,
  `ingress_accepted=2`, `ingress_rejected=0`, `anomalies=2`,
  `verify ... records=10 status=ok`, replay shows both events with
  `canonical.sequence=3/4`.
- Socket ingest of `sample_normal.jsonl` with a split write:
  `ingress_accepted=2`, `ingress_rejected=0`, `ingress_truncated=0`,
  `verify ... records=14 status=ok`, socket file removed after shutdown.
- Mixed malformed input (one valid event, one unknown field, one unsupported
  `event_type`, one bad severity, one over-8192-byte line, one final partial
  line): `ingress_accepted=1`, `ingress_rejected=3`, `ingress_oversize=1`,
  `ingress_truncated=1`, `ingress_frames=4`; `--verify` reports `status=ok`.
  Rejected frames never reached the analyzer or storage.

Before the ring consumer fix, the same demo produced duplicate records and one
zeroed record (`seq=0`, empty payload). After the CAS fix, four concurrent
consumers deliver each of 4000 slots exactly once (`test_concurrent_consumers`).

## 8. Limitations and honest claims

- **No seL4 integration.** No seL4 image, vsock device, or guest agent exists
  in this tree. The transport decision is still open.
- **File mode is replay**, not live collection. The fixture files are synthetic.
- **No eBPF collection.** `telemetry.bpf.c` is a plain C stub and
  `telemetry.skel.h` is a hand-written fake skeleton. Only BTF capability
  detection exists; `core_ready=yes` does not mean a probe is attached.
- **OTLP is JSONL, not protobuf.** The exporter emits OTLP-shaped newline JSON.
  `TP_HAVE_LIBBPF`, `TP_HAVE_PROTOBUF_C`, and `TP_HAVE_ZLIB` are not used by
  runtime code.
- **No authentication.** Anyone who can write to the socket path or file can
  inject events. Use directory permissions.
- **No clock synchronization.** Producer monotonic timestamps are preserved as
  given; `null` falls back to the receiver clock with a flag.
- **Payload is provenance, not source bytes.** See §3.2.
- **Bounded dedup.** 256 accepted IDs; a duplicate further back is re-delivered
  and may be filtered later by the existing filter window.
- **Large attribute sets are rejected**, not truncated, because the internal
  payload is 1024 bytes.
- **No ingress rate limit.** Resource use is bounded by frame size, client
  count (8), ring capacity, and the drop-newest policy; a local writer can
  still force counted drops. Received fields are never executed or interpreted
  as commands.
- **UTF-8 is not validated.** Bytes ≥ 0x20 pass through; JSON control escapes
  are rejected. Do not treat decoded strings as trusted terminal output.

## 9. Interface changes made

Flat-directory rule preserved: all new files are in `Collector/`.

| File | Change |
|---|---|
| `canonical.[ch]` | new strict decoder + mapping |
| `ingress.[ch]` | new framing, transports, dedup, counters |
| `types.h` | ingress counters, ingest config, `TP_FLAG_*` macros; **`tp_event_t` unchanged (3120 bytes)** |
| `normalizer.c` | no local PID/session substitution for `TP_FLAG_INGEST` events |
| `ring_buffer.c` | consumer pop claims `read_seq` with CAS (fixes duplicate/zeroed events) |
| `main.c` | start/stop ingress thread; join it before workers; reconcile `dropped` |
| `config.c`, `cli.c`, `statistics.c` | keys, env, flags, summary/JSON counters |
| `CMakeLists.txt` | adds `canonical.c`, `ingress.c`, `test_canonical`, `test_ingress` |
| `sample_normal.jsonl`, `sample_suspicious.jsonl` | contract fixtures |

`--verify`/`--replay` still accept files written before this change; the event
layout and storage header are untouched. `test_canonical` asserts the 3120-byte
size so an accidental field addition fails the suite.

## 10. Open team decisions

1. **D1 topology/transport.** No TCP/vsock/seL4 transport until the VM topology
   is confirmed. Current answer: file + local Unix socket.
2. **D2 record encoding.** Newline-delimited JSON with an 8192-byte cap. Not
   length-prefixed, not protobuf.
3. **D3 contract mapping.** Severity/event-ID/sequence mappings above are the
   recommended defaults. `critical` is unsupported, and event IDs that are not
   numeric are hashed.
4. **D4 drop policy.** Drop-newest at the ring; dropped frames are counted in
   both `ingress_dropped` and `dropped`.
5. **D5 worker count.** The CAS pop fix makes multi-worker consumption safe;
   the `--workers 1` workaround is no longer required.
