# Examples

Run for ten seconds with local storage and no dashboard:

```sh
./collector --run-seconds 10 --disable-dashboard --storage telemetry.tpbin
```

Run with export enabled:

```sh
./collector --storage telemetry.tpbin --export otlp-export.jsonl --log-level info
```

Verify and replay:

```sh
./collector --verify telemetry.tpbin
./collector --replay telemetry.tpbin
```

Ingest canonical JSONL from a file, then verify the result:

```sh
./collector --ingest-file sample_suspicious.jsonl --storage /tmp/suspicious.tpbin \
  --disable-dashboard --disable-export --run-seconds 2
./collector --verify /tmp/suspicious.tpbin
```

Ingest canonical JSONL over a local Unix socket:

```sh
./collector --ingest-socket /tmp/demo.sock --storage /tmp/socket.tpbin \
  --disable-dashboard --disable-export --run-seconds 3

python3 - <<'PY'
import socket
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/demo.sock')
s.sendall(open('sample_normal.jsonl', 'rb').read())
s.shutdown(socket.SHUT_WR)
s.close()
PY
```

Both fixtures are synthetic replay data. See `INGEST.md` for the contract and
the limitations.

Example log:

```text
[+] 2026-07-08T12:00:00-0500 SUCCESS module=ebpf tid=1234 cpu=2 kernel=6.x arch=x86_64 btf=available core_ready=yes
[!] 2026-07-08T12:00:01-0500 WARNING module=analysis tid=1235 cpu=1 corr=42 anomaly detected confidence=0.95
```

Example exported body:

```json
{"resourceLogs":[{"resource":{"attributes":"host.name=node,process.pid=42,os.type=linux"},"scopeLogs":[{"logRecords":[{"severityText":"INFO","body":"cpu_pct=3.1"}]}]}]}
```
