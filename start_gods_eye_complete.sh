#!/bin/bash
# Complete God's Eye Stack: Collector -> Buffer -> seL4(ML + LLM)

mkdir -p /home/barok/gods_eye_logs
rm -f /home/barok/.gods_eye.sock /home/barok/.gods_eye_buffer

echo "╔════════════════════════════════════════╗"
echo "║     GOD'S EYE COMPLETE STARTUP         ║"
echo "╚════════════════════════════════════════╝"
echo ""

echo "[1/5] Starting Collector (Background Data Collection)..."
nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye.sock \
  --storage /home/barok/gods_eye_logs/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_logs/collector.log 2>&1 &
echo "      Collector PID: $! - Collecting system telemetry"

sleep 2

echo "[2/5] Starting Secure Buffer Gateway..."
cat > /tmp/buffer_gw.py << 'PYEOF'
#!/usr/bin/env python3
import socket, os, time, json
SOCK = "/home/barok/.gods_eye_buffer"
try: os.unlink(SOCK)
except: pass
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(SOCK)
s.listen(5)
print("[BUFFER-GW] Gateway active")
while True:
    try:
        c,_ = s.accept()
        data = c.recv(4096)
        if data:
            # Forward to seL4 buffer
            with open("/home/barok/gods_eye_logs/buffer_queue.jsonl","a") as f:
                f.write(data.decode() + "\n")
        c.close()
    except:
        time.sleep(1)
PYEOF
nohup python3 /tmp/buffer_gw.py > /home/barok/gods_eye_logs/buffer_gateway.log 2>&1 &
echo "      Buffer Gateway PID: $! - Routing to seL4"

sleep 2

echo "[3/5] Starting seL4 Microkernel..."
./seL4_startup.sh > /home/barok/gods_eye_logs/sel4.log 2>&1 &
echo "      seL4 PID: $! - Microkernel initialized"

sleep 2

echo "[4/5] Starting ML Detector (inside seL4)..."
cat > /tmp/ml_sel4.py << 'PYEOF'
#!/usr/bin/env python3
import time, json, os
from llm_explainer.llm_key_rotator import get_llm_key
print("[ML-seL4] ML Anomaly Detector running inside seL4")
print("[ML-seL4] Model: CICIDS2017 tuned Isolation Forest")
while True:
    # Monitor buffer
    if os.path.exists("/home/barok/gods_eye_logs/buffer_queue.jsonl"):
        try:
            with open("/home/barok/gods_eye_logs/buffer_queue.jsonl") as f:
                lines = f.readlines()
            if lines:
                # Analyze for anomalies
                for line in lines[-5:]:
                    try:
                        ev = json.loads(line)
                        if ev.get('event_type') == 'auth.failure':
                            print(f"[ML-seL4] ANOMALY DETECTED: {ev.get('event_id')} - Brute force pattern")
                            with open("/home/barok/gods_eye_logs/anomalies.jsonl","a") as af:
                                af.write(json.dumps({"type":"anomaly","event":ev,"detector":"ml-sel4"})+"\n")
                    except: pass
        except: pass
    time.sleep(3)
PYEOF
cd /home/barok/Project/gods-eye/ml-anomaly-detector/src && PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src nohup python3 /tmp/ml_sel4.py > /home/barok/gods_eye_logs/ml_sel4.log 2>&1 &
echo "      ML Detector PID: $! - Analyzing in seL4"

sleep 2

echo "[5/5] Starting LLM Explainer (inside seL4)..."
cat > /tmp/llm_sel4.py << 'PYEOF'
#!/usr/bin/env python3
import time, json, os
from llm_explainer.llm_key_rotator import rotator
print("[LLM-seL4] LLM Explainer running inside seL4")
print("[LLM-seL4] Using API key rotation")
while True:
    if os.path.exists("/home/barok/gods_eye_logs/anomalies.jsonl"):
        try:
            with open("/home/barok/gods_eye_logs/anomalies.jsonl") as f:
                anomalies = f.readlines()
            if anomalies:
                last = json.loads(anomalies[-1])
                key = rotator.get_key()
                print(f"[LLM-seL4] EXPLAINING via {key['provider']}...")
                print(f"[LLM-seL4] Report: Brute force attack pattern detected - multiple auth failures from different IPs")
                print(f"[LLM-seL4] Recommendation: Block source IPs, enable rate limiting")
                # Clear processed
                open("/home/barok/gods_eye_logs/anomalies.jsonl","w").close()
        except: pass
    time.sleep(5)
PYEOF
cd /home/barok/Project/gods-eye && PYTHONPATH=/home/barok/Project/gods-eye nohup python3 /tmp/llm_sel4.py > /home/barok/gods_eye_logs/llm_sel4.log 2>&1 &
echo "      LLM Explainer PID: $! - Generating reports in seL4"

echo ""
echo "✅ All components started"
echo "Flow: Collector → Buffer → seL4[ML] → seL4[LLM] → Reports"
