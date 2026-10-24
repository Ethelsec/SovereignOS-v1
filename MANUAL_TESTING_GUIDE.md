# God's Eye - Manual Testing Guide

This guide provides step-by-step instructions for manually testing the God's Eye telemetry platform.

## Prerequisites

- Linux system (tested on Kali Linux)
- GCC, CMake, Make installed
- Python 3 installed
- Git installed

## Setup

### 1. Clone/Navigate to the Repository

```bash
cd /home/barok/Project/gods-eye
```

### 2. Build All Components

```bash
cd scripts
./build.sh
```

Expected output:
- Collector built successfully
- Guest agent built successfully  
- Analyzer built successfully

## Testing Procedures

### Test 1: Verify Collector Tests Pass

```bash
cd /home/barok/Project/gods-eye/collector/build
ctest --output-on-failure
```

**Expected**: All 12 tests should pass (test_buffer, test_parser, test_storage, test_exporter, test_correlator, test_dashboard, test_configuration, test_memory_pool, test_utilities, test_ingest_pipeline, test_canonical, test_ingress)

---

### Test 2: Process Normal Events

```bash
cd /home/barok/Project/gods-eye
./collector/build/collector \
  --ingest-file samples/normal-events.jsonl \
  --storage /tmp/test_normal.tpbin \
  --disable-dashboard \
  --disable-export \
  --run-seconds 2
```

**Expected**:
- Should see "ingesting canonical JSONL from file" message
- Should process events successfully
- Post-run verification should show OK status

**Verify**:
```bash
./collector/build/collector --verify /tmp/test_normal.tpbin
./collector/build/collector --replay /tmp/test_normal.tpbin | head -15
```

---

### Test 3: Process Suspicious Authentication Failures

```bash
cd /home/barok/Project/gods-eye
./collector/build/collector \
  --ingest-file samples/suspicious-pattern.jsonl \
  --storage /tmp/test_suspicious.tpbin \
  --disable-dashboard \
  --disable-export \
  --run-seconds 2
```

**Expected**:
- Should ingest 3 auth.failure events
- Events should have severity HIGH
- Events should be classified as security type

**Verify**:
```bash
./collector/build/collector --replay /tmp/test_suspicious.tpbin | grep -A2 "security"
```

You should see entries like:
```
seq=X type=security pid=2001 severity=HIGH payload=...canonical.event_id=susp-001...network.protocol=tcp...
```

---

### Test 4: Test Guest Agent

```bash
cd /home/barok/Project/gods-eye/guest-agent
./guest_probe /tmp/guest_output.jsonl
cat /tmp/guest_output.jsonl
```

**Expected**: Should generate a canonical JSONL event with schema_version "1"

---

### Test 5: Test Shared Buffer (Guest <-> seL4)

```bash
cd /home/barok/Project/gods-eye/shared-buffer
gcc -o test_buffer guest_sel4_buffer.c -pthread
./test_buffer
```

Create a quick test:
```bash
cat > /tmp/test_shared.c << 'EOF'
#include "guest_sel4_buffer.h"
#include <stdio.h>
#include <string.h>

int main() {
    gs_buffer_init();
    char msg1[] = "Hello from Guest OS";
    char msg2[] = "Hello from seL4";
    gs_buffer_push_guest(msg1, strlen(msg1));
    gs_buffer_push_sel4(msg2, strlen(msg2));
    gs_buffer_stats();
    return 0;
}
EOF
gcc -o /tmp/test_shared /tmp/test_shared.c guest_sel4_buffer.c -pthread && /tmp/test_shared
```

**Expected**: Should show buffer stats with pushes from both sides

---

### Test 6: Test LLM Explainer

```bash
cd /home/barok/Project/gods-eye/llm-explainer
python3 llm_explainer.py
```

**Expected**: Should display a verbose UI report with:
- System overview
- Telemetry statistics
- Detected findings
- LLM explanation with evidence chain
- Recommendations
- Limitations disclaimer

---

### Test 7: Run Full Demo Script

```bash
cd /home/barok/Project/gods-eye/scripts
./run_demo.sh
```

**Expected**:
- Demo 1: Normal events processed successfully
- Demo 2: Suspicious pattern processed successfully
- Both should show verification OK

---

### Test 8: Test ML Anomaly Detector

```bash
cd /home/barok/Project/gods-eye/ml-anomaly-detector
# Check if tests can run
python3 -m pytest tests/test_baseline_model.py -v 2>&1 | tail -20
```

**Note**: May need dependencies installed. Check requirements if tests fail.

---

## Manual Inspection Tests

### Check Event Schema Compliance

```bash
cat /home/barok/Project/gods-eye/samples/normal-events.jsonl | python3 -m json.tool | head -30
```

Verify all required fields are present: schema_version, event_id, timestamp_utc, timestamp_monotonic_ns, source, event_type, severity, host_id, process, network, attributes, sequence

### Check seL4 Components

```bash
ls -la ~/seL4/
```

Should show: kernel, tools, capdl, util_libs, sel4runtime

### Check seL4 Adapter

```bash
cat /home/barok/Project/gods-eye/collector/sel4_adapter.c
```

Verify it has environment detection and abstraction layers

## Troubleshooting

### Build Fails
```bash
cd /home/barok/Project/gods-eye/collector
rm -rf build
mkdir build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
make -j4
```

### Tests Fail
- Check if all dependencies are installed
- Ensure running from correct directory
- Check file permissions

### Permission Issues
```bash
chmod +x /home/barok/Project/gods-eye/scripts/*.sh
```

## Expected Results Summary

| Test | Status | Notes |
|------|--------|-------|
| Collector Unit Tests (12) | PASS | All tests pass |
| Normal Events Ingestion | PASS | 3 events + procfs metrics processed |
| Suspicious Auth Failures | PASS | 3 HIGH severity security events |
| Guest Agent | PASS | Generates canonical JSONL |
| Shared Buffer | PASS | Guest<->seL4 communication works |
| LLM Explainer | PASS | Verbose UI report generated |
| Demo Script | PASS | Both demos run successfully |
| ML Detector | Available | Code included, may need deps |

## Notes

- This is a PROTOTYPE system - not production ready
- File ingestion is replay mode, not live collection
- seL4 integration is in development/prototype stage
- ML components are included but may require additional setup
- All findings are rule-based indicators, not confirmed attacks
