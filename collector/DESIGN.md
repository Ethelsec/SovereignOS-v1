# Design

The design favors deterministic behavior, clear ownership, and minimal steady-state allocation. Events use a forward-compatible fixed struct with versioning, fixed text fields, inline payload storage, resource attributes, analysis metadata, trace IDs, span IDs, and correlation IDs.

## Ring Buffer

The ring is fixed-capacity and circular. Slots carry state, sequence number, timestamp, payload size, event, and CRC. Sequence validation detects overwritten or out-of-order slots. CRC validation catches memory corruption and partial writes.

## Processing

Every worker runs the same mandatory stages. Validation happens before normalization. Normalization assigns timestamps, categories, host/process attributes, trace IDs, and span IDs. Filtering removes repeated low-value events while retaining security and high-severity signals. Correlation updates process timelines. Analysis flags resource spikes and suspicious patterns.

## Export

The exporter serializes events into OTLP-shaped JSON records with resource logs. This keeps the project deployable without generated protobuf code while preserving the fields needed by OTLP gateways and later protobuf-c integration.
