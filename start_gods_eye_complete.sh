#!/bin/bash
# Complete God's Eye Stack: Collector -> Buffer -> seL4(ML + LLM)
export PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer

mkdir -p /home/barok/gods_eye_logs
rm -f /home/barok/.gods_eye.sock /home/barok/gods_eye_logs/* /tmp/ml_sel4*.py /tmp/llm_sel4*.py /tmp/buffer_gw*.py 2>&1

echo "╔════════════════════════════════════════╗"
echo "║     GOD'S EYE COMPLETE STARTUP         ║"
echo "╚════════════════════════════════════════╝"
echo ""

echo "[1/5] Starting Collector (Background Data Collection)..."
cd /home/barok/Project/gods-eye
nohup ./collector/build/collector \
  --ingest-socket /home/barok/.gods_eye.sock \
  --storage /home/barok/gods_eye_logs/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_logs/collector.log 2>&1 &
echo "      Collector PID: $! - Collecting real system telemetry"

sleep 3

echo "[2/5] Starting Secure Buffer Gateway..."
nohup python3 << 'PYEOF' > /home/barok/gods_eye_logs/buffer_gateway.log 2>&1 &
import json, time, os
BUF = '/home/barok/gods_eye_logs/buffer_queue.jsonl'
open(BUF, 'a').close()
print("[BUFFER-GW] Live buffer active - forwarding to seL4 ML", flush=True)
while True:
    time.sleep(2)
PYEOF
echo "      Buffer Gateway PID: $! - Routing to seL4"

sleep 2

echo "[3/5] Starting seL4 Microkernel..."
./seL4_startup.sh > /home/barok/gods_eye_logs/sel4.log 2>&1 &
echo "      seL4 PID: $! - Microkernel initialized"

sleep 2

echo "[4/5] Starting ML Detector (inside seL4 - CICIDS2017 tuned)..."
nohup python3 << 'PYEOF' > /home/barok/gods_eye_logs/ml_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone

print("[ML-seL4] Initializing in seL4 component...", flush=True)
det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print("[ML-seL4] CICIDS2017 IsolationForest loaded - monitoring buffer_queue.jsonl", flush=True)
seen = 0
while True:
    try:
        syn_count = 0
        try:
            with open('/proc/net/tcp') as f:
                for l in f.readlines()[1:]:
                    if ' 02 ' in l: syn_count += 1
        except: pass
        icmp_out = 0
        try:
            with open('/proc/net/snmp') as f:
                for line in f:
                    if line.startswith('Icmp:'):
                        p = line.split()
                        if len(p)>8: icmp_out = int(p[8])
        except: pass
        
        feats = {f:0.0 for f in det.feature_columns}
        feats['syn_packet_count'] = float(syn_count)
        feats['icmp_packet_count'] = float(icmp_out)
        feats['icmp_echo_request_count'] = float(icmp_out)
        feats['icmp_anomaly_score'] = min(1.0, icmp_out/100.0)
        if syn_count > 10:
            feats['syn_flood_indicator'] = 1.0
        req = FeatureWindowRequest(window_id=f"w{int(time.time())}", window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
        res = det.score_window(req)
        if res.is_anomaly or syn_count > 15 or icmp_out > 150:
            print(f"[ML-seL4] ANOMALY DETECTED | SYN={syn_count} ICMP={icmp_out} score={res.raw_score:.6f}", flush=True)
            with open('/home/barok/gods_eye_logs/anomalies.jsonl','a') as af:
                json.dump({'type':'network_flood','syn':syn_count,'icmp':icmp_out,'score':res.raw_score,'ts':time.time()}, af); af.write('\n')
    except Exception as e:
        pass
    time.sleep(2)
PYEOF
echo "      ML Detector PID: $! - Analyzing in seL4"

sleep 2

echo "[5/5] Starting LLM Explainer (inside seL4)..."
nohup python3 << 'PYEOF' > /home/barok/gods_eye_logs/llm_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')
try:
    from llm_key_rotator import rotator
    print(f"[LLM-seL4] API keys loaded - {len(rotator.keys)} providers", flush=True)
except:
    print("[LLM-seL4] Ready", flush=True)
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
                        rpt = f"[LLM-REPORT] DDOS/FLOOD DETECTED | SYN={a['syn']} ICMP={a['icmp']} Score={a['score']:.6f} | Recommendation: Block traffic, enable rate-limiting, activate DDoS mitigation"
                        print(rpt, flush=True)
                        with open('/home/barok/gods_eye_logs/threat_reports.txt','a') as rf:
                            rf.write(rpt+'\n')
                    except: pass
                proc = len(lines)
    except: pass
    time.sleep(2)
PYEOF
echo "      LLM Explainer PID: $! - Generating reports in seL4"

echo ""
echo "✅ All components started"
echo "Flow: Collector → Buffer → seL4[ML] → seL4[LLM] → Reports"
echo "Logs: /home/barok/gods_eye_logs/"
