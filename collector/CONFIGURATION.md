# Configuration

Collector starts from safe defaults and applies config file, environment, and CLI overrides.

## File Format

The config file is line-oriented `key=value`. Blank lines and lines beginning with `#` are ignored.

Supported keys:

- `buffer_size`: event capacity of the ring.
- `worker_count`: number of processing workers.
- `storage_path`: append-only binary telemetry file.
- `export_endpoint`: OTLP JSON export file path.
- `ingest_file`: read canonical newline-delimited JSON from this file once to EOF.
- `ingest_socket`: listen for canonical newline-delimited JSON on this `AF_UNIX` path.
- `log_level`: fatal, error, warning, success, info, debug, or trace.
- `dashboard`: true/false.
- `export_enabled`: true/false.
- `dashboard_interval_ms`: dashboard refresh interval.
- `run_seconds`: bounded runtime.
- `retry_max`: configured retry count for exporters that implement network retries.
- `retry_backoff_ms`: retry backoff.
- `retention_hours`: retention policy value stored with configuration.

## Environment

`TP_BUFFER_SIZE`, `TP_WORKERS`, `TP_STORAGE_PATH`, `TP_EXPORT_ENDPOINT`, `TP_INGEST_FILE`, `TP_INGEST_SOCKET`, `TP_LOG_LEVEL`, `TP_DASHBOARD`, and `TP_EXPORT` override the corresponding file settings.

## Precedence Note

The intended precedence is defaults, config file, environment, then CLI. The
current `main.c` wiring applies the config file and environment after CLI
parsing, so an environment variable can override a CLI flag. Use a clean
environment for reproducible runs. See `INGEST.md`.
