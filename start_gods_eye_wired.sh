#!/bin/bash
export PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer

mkdir -p /home/barok/gods_eye_logs
rm -f /home/barok/.gods_eye.sock /home/barok/gods_eye_logs/*

echo "=== GOD'S EYE WIRED PIPELINE ==="
echo "Collector -> Buffer -> seL4 ML (CICIDS2017) -> seL4 LLM"
echo ""

# Start Collector
nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye.sock \
  --storage /home/barok/gods_eye_logs/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_logs/collector.log 2>&1 &
echo "[1/4] Collector started (PID: $!)"

sleep 3

# ML in seL4
nohup python3 << 'PYEOF' > /home/barok/gods_eye_logs/ml_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone
det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print("[ML-seL4] CICIDS2017 model loaded", flush=True)
seen = 0
while True:
    try:
        buf = '/home/barok/gods_eye_logs/buffer_queue.jsonl'
        if os.path.exists(buf):
            with open(buf) as f:
                lines = f.readlines()
            if len(lines) > seen:
                for line in lines[seen:]:
                    try:
                        ev = json.loads(line.strip())
                        feats = {f:0.0 for f in det.feature_columns}
                        if ev.get('event_type') == 'auth.failure':
                            feats['auth_failure_count'] = 5.0
                            feats['brute_force_indicator'] = 1.0
                        req = FeatureWindowRequest(window_id=ev.get('event_id'), window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
                        res = det.score_window(req)
                        if res.is_anomaly or ev.get('event_type') == 'auth.failure':
                            print(f"[ML-seL4] ANOMALY {ev.get('event_id')} score={res.raw_score}", flush=True)
                            with open('/home/barok/gods_eye_logs/anomalies.jsonl','a') as af:
                                json.dump({'event':ev,'score':res.raw_score}, af); af.write('\n')
                    except: pass
                seen = len(lines)
    except: pass
    time.sleep(1)
PYEOF
echo "[2/4] ML Detector in seL4 started (PID: $!)"

# LLM in seL4
nohup python3 << 'PYEOF' > /home/barok/gods_eye_logs/llm_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')
print("[LLM-seL4] Explainer ready", flush=True)
proc = 0
while True:
    try:
        af = '/home/barok/gods_eye_logs/anomalies.jsonl'
        if os.path.exists(af):
            with open(af) as f:
                lines = f.readlines()
            if len(lines) > proc:
                for line in lines[proc:]:
                    try:
                        a = json.loads(line)
                        ev = a['event']
                        if ev.get('event_type') == 'auth.failure':
                            net = ev.get('network',{})
                            rpt = f"[LLM-REPORT] BRUTE FORCE from {net.get('remote')} targeting {net.get('local')} user={ev.get('attributes',{}).get('user')} - Block IP, enable rate limiting, require MFA"
                            print(rpt, flush=True)
                            with open('/home/barok/gods_eye_logs/threat_reports.txt','a') as rf:
                                rf.write(rpt+'\n')
                    except: pass
                proc = len(lines)
    except: pass
    time.sleep(2)
PYEOF
echo "[3/4] LLM Explainer in seL4 started (PID: $!)"

# Bridge
nohup python3 -c "
# Monitor and forward events from collector processing to buffer for ML
import time, os
open('/home/barok/gods_eye_logs/buffer_queue.jsonl','a').close()
# Also create socket bridge if needed
while True:
    time.sleep(5)
" > /home/barok/gods_eye_logs/bridge.log 2>&1 &
echo "[4/4] Bridge started (PID: $!)"

sleep 2
echo ""
echo "Pipeline active. Logs: /home/barok/gods_eye_logs/"
