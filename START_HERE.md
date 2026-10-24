# How to Run God's Eye

## Quick Start (No attacks needed)

### 1. Build (if not built)
```bash
cd collector
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
make -j4
cd ../..
```

### 2. Run Tests
```bash
cd collector/build
ctest --output-on-failure
```

### 3. Run Demo with Sample Data
```bash
./scripts/run_demo.sh
```

### 4. Check Results
```bash
# View normal events
./collector/build/collector --replay reports/normal_demo.tpbin | head -15

# View suspicious events
./collector/build/collector --replay reports/suspicious_demo.tpbin | head -15
```

### 5. Live Demo (Simulated Attacks)
```bash
./scripts/live_demo.sh
```

### 6. ML Analysis
```bash
python3 ml-anomaly-detector/src/anomaly_detector/live_ingestion.py
python3 llm-explainer/llm_explainer.py
```

## Main Components
- **Collector**: Main telemetry processor (C program)
- **ML Detector**: Trained on CICIDS2017, detects anomalies
- **LLM Explainer**: Provides verbose explanations
- **Guest Agents**: Collect from Linux/Windows

## Notes
- Works without any attacks on your system - uses sample/replay data
- All tests pass
- ML model already tuned with CICIDS2017 dataset
