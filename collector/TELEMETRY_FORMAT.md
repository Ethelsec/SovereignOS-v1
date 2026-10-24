# Telemetry Format

## Internal Event

`tp_event_t` contains monotonic and wall-clock timestamps, event ID, sequence, type, categories, source probe/module, PID/TID/PPID, UID/GID, CPU/NUMA, host/container/namespace identifiers, process/thread names, executable path, command line, severity, priority, flags, correlation IDs, resource attributes, analysis metadata, trace IDs, span IDs, payload size, and inline payload.

## Storage

Each record is:

```text
tp_storage_record_header_t
tp_event_t
```

The header stores magic, version, header size, sequence, timestamps, event size, and CRC32 of the event bytes. Replay validates magic, version, size, and CRC before yielding events.

## Export

`protobuf_serializer.c` emits OTLP-shaped JSON resource logs. Records include resource attributes, timestamp, severity, body, trace ID, span ID, event name, PID, TID, correlation ID, and analysis metadata.
