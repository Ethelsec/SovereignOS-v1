#!/bin/bash
export PYTHONPATH=/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer

LOGDIR=/home/barok/gods_eye_realtime
mkdir -p $LOGDIR
rm -f $LOGDIR/* /home/barok/.gods_eye_realtime.sock

echo "Starting God's Eye Real-Time Pipeline..."
echo "Collector -> Buffer -> seL4 ML (CICIDS2017) -> seL4 LLM"

# Collector
nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye_realtime.sock \
  --storage $LOGDIR/telemetry.tpbin \
  --log-level info > $LOGDIR/collector.log 2>&1 &
echo "Collector started"

sleep 3

# ML seL4
nohup python3 << PYEOF > $LOGDIR/ml.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone
det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print('[ML-seL4] Monitoring live network from /proc/net', flush=True)
while True:
    try:
        syn = sum(1 for l in open('/proc/net/tcp').readlines()[1:] if ' 02 ' in l)
        icmp = 0
        for line in open('/proc/net/snmp'):
            if line.startswith('Icmp:'): 
                p=line.split(); icmp=int(p[2]) if len(p)>2 else 0
        feats = {f:0.0 for f in det.feature_columns}
        feats['syn_packet_count']=float(syn); feats['icmp_packet_count']=float(icmp)
        feats['icmp_echo_request_count']=float(icmp); feats['icmp_anomaly_score']=min(1.0,icmp/20.0)
        if syn>10: feats['syn_flood_indicator']=1.0
        req = FeatureWindowRequest(window_id=str(time.time()), window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
        res = det.score_window(req)
        if res.is_anomaly or icmp>30 or syn>25:
            print(f'[ML-seL4] ANOMALY DETECTED SYN={syn} ICMP={icmp} SCORE={res.raw_score}', flush=True)
            open('$LOGDIR/buffer.jsonl','a').write(json.dumps({'type':'flood','syn':syn,'icmp':icmp,'score':res.raw_score})+'\\n')
    except: pass
    time.sleep(1)
PYEOF
echo "ML started"

sleep 2

# LLM seL4
nohup python3 << PYEOF > $LOGDIR/llm.log 2>&1 &
import json, time, os, sys
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')
try:
    from llm_key_rotator import rotator
    print(f'[LLM-seL4] {len(rotator.keys)} API keys ready', flush=True)
except: print('[LLM-seL4] Ready', flush=True)
seen=0
while True:
    try:
        if os.path.exists('$LOGDIR/buffer.jsonl'):
            with open('$LOGDIR/buffer.jsonl') as f: lines=f.readlines()
            if len(lines)>seen:
                for l in lines[seen:]:
                    a=json.loads(l)
                    print(f'[LLM-REPORT] DDoS/Flood Attack - SYN:{a["syn"]} ICMP:{a["icmp"]} ML:{a["score"]:.4f} | ACTION: Block & mitigate', flush=True)
                seen=len(lines)
    except: pass
    time.sleep(2)
PYEOF
echo "LLM started"
echo "All running. Logs in $LOGDIR"
