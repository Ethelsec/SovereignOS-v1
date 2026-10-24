# How to Start and Use God's Eye

## Starting the Program

### Option 1: Run as a service/collector (Live Mode)
The collector can run continuously, listening for events:

```bash
cd /home/barok/Project/gods-eye
./collector/build/collector \
  --ingest-socket /tmp/gods_eye.sock \
  --storage /var/log/gods_eye/telemetry.tpbin \
  --log-level info
```

This will start collecting events from Unix socket in real-time.

### Option 2: Process from file (Batch Mode)
```bash
./collector/build/collector \
  --ingest-file /path/to/events.jsonl \
  --storage /var/log/gods_eye/telemetry.tpbin \
  --run-seconds 60
```

### Option 3: Use with your own applications
Send events to the socket:
```bash
echo '{"schema_version":"1","event_id":"test1","timestamp_utc":"2026-10-24T16:00:00Z","timestamp_monotonic_ns":123456789000,"source":"app","event_type":"process.start","severity":"info","host_id":"localhost","process":{"pid":1234,"ppid":1,"executable":"myapp"},"network":null,"attributes":{},"sequence":1}' | nc -U /tmp/gods_eye.sock
```

## Analyzing Results

### View stored events
```bash
./collector/build/collector --replay /var/log/gods_eye/telemetry.tpbin
```

### Verify integrity
```bash
./collector/build/collector --verify /var/log/gods_eye/telemetry.tpbin
```

### ML Analysis
```bash
cd ml-anomaly-detector
python3 -c "
# Use the tuned model
import sys
sys.path.insert(0, 'src')
from anomaly_detector.inference import load_detector
det = load_detector('artifacts/baseline/model/model.joblib', 'artifacts/baseline/model/manifest.json')
# Analyze your events
"
```

### LLM Explanation
```bash
python3 llm-explainer/llm_explainer.py
```

## Quick Start Right Now
```bash
# Create log directory
mkdir -p /var/log/gods_eye

# Start collector in background
./collector/build/collector --ingest-socket /tmp/gods_eye.sock --storage /var/log/gods_eye/telemetry.tpbin --log-level info &
COLLECTOR_PID=$!

# Send some test events
sleep 2
echo '{"schema_version":"1","event_id":"evt1","timestamp_utc":"'$(date -u +%Y-%m-%dT%H:%M:%SZ)'","timestamp_monotonic_ns":'$(date +%s%N)',"source":"test","event_type":"auth.failure","severity":"high","host_id":"localhost","process":{"pid":1000,"ppid":1,"executable":"sshd"},"network":{"protocol":"tcp","local":"127.0.0.1:22","remote":"192.168.1.100:1234","status":"failed"},"attributes":{"user":"admin"},"sequence":1}' | nc -U /tmp/gods_eye.sock 2>/dev/null

sleep 5
kill $COLLECTOR_PID
./collector/build/collector --replay /var/log/gods_eye/telemetry.tpbin
```
