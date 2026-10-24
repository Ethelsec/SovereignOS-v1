# Changelog

## Unreleased

- Added canonical JSONL ingress (strict decoder, file and local `AF_UNIX` socket transports, framing limits, deduplication, sequence-gap and rejection counters). See `INGEST.md`.
- Added `test_canonical` and `test_ingress`, plus normal and suspicious contract fixtures.
- Fixed a multi-worker ring consumer race that could deliver duplicate or zeroed events; pop now claims slots with a CAS.
- Preserved producer sequence numbers in ingested event payloads and stopped local PID/session substitution for ingested events.
- Documented eBPF/OTLP/seL4 truthfulness gaps and the config/CLI precedence quirk.

## 1.0.0

- Initial production-oriented C17 telemetry platform.
- Added ring buffer, parser, normalizer, classifier, filter, correlator, anomaly detector, storage, replay, exporter, dashboard, health monitoring, configuration, CLI, tests, CMake, and documentation.
- Added eBPF CO-RE readiness detection and single-root BPF compatibility files.
