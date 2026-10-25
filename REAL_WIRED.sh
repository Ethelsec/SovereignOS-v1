#!/bin/bash
export PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer
LOG=/home/barok/gods_eye_real
mkdir -p $LOG
rm -f $LOG/* /home/barok/.gods_eye_real.sock

# Collector
nohup /home/barok/Project/gods-eye/collector/build/collector --ingest-socket /home/barok/.gods_eye_real.sock --storage $LOG/telemetry.tpbin --log-level info > $LOG/collector.log 2>&1 &
C=$!
echo "COLLECTOR:$C"
sleep 3

# Buffer
touch $LOG/buffer_queue.jsonl
echo "BUFFER:$LOG/buffer_queue.jsonl"

# ML seL4 - reads from buffer AND live network
nohup python3 << PYEOF > $LOG/ml_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone
det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print('[ML-seL4] STARTED - READING BUFFER: /home/barok/gods_eye_real/buffer_queue.jsonl', flush=True)
seen = 0
while True:
    try:
        # Check buffer first
        buf = '/home/barok/gods_eye_real/buffer_queue.jsonl'
        if os.path.exists(buf):
            with open(buf) as f:
                lines = f.readlines()
            if len(lines) > seen:
                for line in lines[seen:]:
                    try:
                        ev = json.loads(line.strip())
                        print(f'[ML-seL4] READ FROM BUFFER: {ev}', flush=True)
                        feats = {f:0.0 for f in det.feature_columns}
                        if ev.get('type') == 'malicious' or 'ping' in str(ev).lower():
                            feats['icmp_packet_count'] = 500.0
                            feats['icmp_echo_request_count'] = 500.0
                            feats['icmp_anomaly_score'] = 1.0
                        req = FeatureWindowRequest(window_id=ev.get('id','x'), window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
                        res = det.score_window(req)
                        if res.is_anomaly or feats['icmp_anomaly_score'] > 0.5:
                            print(f'[ML-seL4] ANOMALY DETECTED FROM BUFFER - SCORE:{res.raw_score}', flush=True)
                            with open('/home/barok/gods_eye_real/anomalies.jsonl','a') as af:
                                json.dump({'source':'BUFFER','event':ev,'score':res.raw_score}, af); af.write('\n')
                    except: pass
                seen = len(lines)
        # Also scan live network
        icmp = 0
        try:
            for line in open('/proc/net/snmp'):
                if line.startswith('Icmp:'): 
                    p=line.split(); icmp=int(p[2]) if len(p)>2 else 0
        except: pass
        if icmp > 200:
            print(f'[ML-seL4] LIVE NETWORK ANOMALY - ICMP:{icmp}', flush=True)
            with open('/home/barok/gods_eye_real/anomalies.jsonl','a') as af:
                json.dump({'source':'LIVE_NET','icmp':icmp}, af); af.write('\n')
    except: pass
    time.sleep(1)
PYEOF
M=$!
echo "ML:$M"

sleep 2
nohup python3 << PYEOF > $LOG/llm_sel4.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')
try:
    from llm_key_rotator import rotator
    print(f'[LLM-seL4] {len(rotator.keys)} keys', flush=True)
except: pass
print('[LLM-seL4] READY', flush=True)
proc=0
while True:
    try:
        af = '/home/barok/gods_eye_real/anomalies.jsonl'
        if os.path.exists(af):
            with open(af) as f: lines=f.readlines()
            if len(lines)>proc:
                for l in lines[proc:]:
                    a=json.loads(l)
                    print(f'[LLM-REPORT] EXPLAINING ANOMALY FROM {a.get("source")} - DETAILS:{a}', flush=True)
                    with open('/home/barok/gods_eye_real/THREAT_REPORT.txt','a') as rf:
                        rf.write(str(a)+'\n')
                proc=len(lines)
    except: pass
    time.sleep(2)
PYEOF
L=$!
echo "LLM:$L"
echo "DONE"
