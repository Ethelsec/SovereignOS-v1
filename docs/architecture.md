# God's Eye Architecture

## Overview

God's Eye implements a defensive telemetry pipeline:
1. **Guest/Source** - Generates or observes events (Linux procfs currently, with replay fallback)
2. **Producer/Adapter** - Converts to canonical event format
3. **Transport** - Moves events across boundary (local file/socket currently, seL4 integration planned)
4. **Collector** - Validates, processes, analyzes events (existing component in `collector/`)
5. **Analysis & Report** - Rule-based detection with explainable findings

## Current Implementation

The collector in `collector/` is already functional with:
- Canonical JSONL ingestion (`--ingest-file`, `--ingest-socket`)
- Ring buffer for event processing
- Validation, normalization, classification
- Anomaly detection
- Storage and export
- 12 passing tests

## seL4 Integration

seL4 environment components are set up in `~/seL4/`. Integration is in progress to adapt the collector for seL4's execution model where POSIX/Linux APIs may not be available.
