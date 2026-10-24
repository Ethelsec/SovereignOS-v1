# God's Eye - SovereignOS v1

Defensive telemetry and analysis platform with seL4 integration, ML anomaly detection, and real-time threat monitoring.

## Architecture
See `gods_eye_sunday_plan/02_ARCHITECTURE.md` for full architecture details.

## Quick Start

### Build
```bash
cd collector
mkdir build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
make -j4
```

### Test
```bash
cd collector/build
ctest --output-on-failure
```

### Run
```bash
# Process events from file
./collector/build/collector --ingest-file samples/normal-events.jsonl --disable-dashboard --run-seconds 2

# Replay stored events
./collector/build/collector --replay /path/to/events.tpbin

# Verify storage
./collector/build/collector --verify /path/to/events.tpbin
```

### Live Detection
```bash
./scripts/live_demo.sh
```

### ML Explainer
```bash
python3 llm-explainer/llm_explainer.py
```

## Components
- **collector/** - Core C17 telemetry engine
- **ml-anomaly-detector/** - Isolation Forest ML with CICIDS2017 tuning
- **guest-agent/** - Linux and Windows telemetry collectors
- **shared-buffer/** - Secure Guest OS <-> seL4 communication
- **llm-explainer/** - AI-powered evidence-based explanations
- **gods_eye_sunday_plan/** - Architecture plans

## Features
- Real-time event ingestion and validation
- ML-based anomaly detection (DDoS, brute force, port scans)
- seL4 microkernel integration with secure buffer gateway
- Cross-platform support (Linux, Windows)
- LLM-powered verbose reports
