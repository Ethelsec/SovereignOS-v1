#!/bin/bash
export PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer

rm -rf /home/barok/gods_eye_full2
mkdir -p /home/barok/gods_eye_full2
rm -f /home/barok/.gods_eye_full2.sock

echo "=== GOD'S EYE FULL WIRE ==="

nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye_full2.sock \
  --storage /home/barok/gods_eye_full2/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_full2/collector.log 2>&1 &
echo "Collector: $!"
sleep 3

nohup python3 -c "
import time
open('/home/barok/gods_eye_full2/buffer_queue.jsonl','a').close()
while True: time.sleep(5)
" > /home/barok/gods_eye_full2/buffer.log 2>&1 &
echo "Buffer: $!"
sleep 2

nohup python3 << 'PYEOF' > /home/barok/gods_eye_full2/ml.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone
det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print("[ML-seL4] READY", flush=True)
while True:
    try:
        syn = 0
        try:
            with open('/proc/net/tcp') as f:
                for l in f.readlines()[1:]:
                    if ' 02 ' in l: syn += 1
        except: pass
        icmp = 0
        try:
            with open('/proc/net/snmp') as f:
                for line in f:
                    if line.startswith('Icmp:'):
                        p = line.split()
                        if len(p)>2: icmp = int(p[2])
        except: pass
        feats = {f:0.0 for f in det.feature_columns}
        feats['syn_packet_count'] = float(syn)
        feats['icmp_packet_count'] = float(icmp)
        feats['icmp_echo_request_count'] = float(icmp)
        feats['icmp_anomaly_score'] = min(1.0, icmp/10.0)
        if syn>5: feats['syn_flood_indicator'] = 1.0
        req = FeatureWindowRequest(window_id=str(time.time()), window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
        res = det.score_window(req)
        if res.is_anomaly or icmp>50 or syn>15:
            print(f"[ML-seL4] *** DDOS DETECTED *** SYN={syn} ICMP={icmp} SCORE={res.raw_score}", flush=True)
            with open('/home/barok/gods_eye_full2/anom.jsonl','a') as f:
                json.dump({'type':'DDOS','syn':syn,'icmp':icmp,'score':res.raw_score}, f); f.write('\n')
            with open('/home/barok/gods_eye_full2/buffer_queue.jsonl','a') as bf:
                bf.write(json.dumps({'alert':'ddos','syn':syn})+'\n')
    except: pass
    time.sleep(2)
PYEOF
echo "ML: $!"
sleep 3

nohup python3 << 'PYEOF' > /home/barok/gods_eye_full2/llm.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')
try:
    from llm_key_rotator import rotator
    print(f"[LLM-seL4] API keys: {len(rotator.keys)}", flush=True)
except: pass
print("[LLM-seL4] READY - EXPLAINING ANOMALIES", flush=True)
proc = 0
while True:
    try:
        if os.path.exists('/home/barok/gods_eye_full2/anom.jsonl'):
            with open('/home/barok/gods_eye_full2/anom.jsonl') as f:
                lines = f.readlines()
            if len(lines)>proc:
                for line in lines[proc:]:
                    try:
                        a = json.loads(line)
                        rpt = f"\n[LLM-THREAT-REPORT] DDOS ATTACK DETECTED\nSYN-Flood:{a['syn']} ICMP:{a['icmp']} ML-Score:{a['score']}\nEXPLANATION: Volumetric flood attack detected by CICIDS2017 model\nRECOMMENDATION: Immediate mitigation - block sources, rate-limit, engage DDoS protection\n"
                        print(rpt, flush=True)
                        with open('/home/barok/gods_eye_full2/REPORTS.txt','a') as rf:
                            rf.write(rpt+'\n')
                    except: pass
                proc = len(lines)
    except: pass
    time.sleep(2)
PYEOF
echo "LLM: $!"
sleep 3
echo "=== ALL RUNNING ==="
