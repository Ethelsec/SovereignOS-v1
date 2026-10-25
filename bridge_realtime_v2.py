#!/usr/bin/env python3
import os, sys, json, time
from pathlib import Path

logdir = Path('/home/barok/gods_eye_logs')
logdir.mkdir(parents=True, exist_ok=True)

sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone

det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print("[BRIDGE-V2] ML loaded, aggressive monitoring", flush=True)

last_icmp = 0
last_time = time.time()
icmp_rate = 0

while True:
    try:
        now = time.time()
        # Read ICMP
        try:
            with open('/proc/net/snmp') as f:
                for line in f:
                    if line.startswith('Icmp:'):
                        parts = line.split()
                        if len(parts) > 8:
                            icmp_out = int(parts[8])  # OutEchoReps or similar
                            if now - last_time > 0:
                                icmp_rate = (icmp_out - last_icmp) / (now - last_time)
                            last_icmp = icmp_out
                            break
        except:
            pass
        
        try:
            with open('/proc/net/tcp') as f:
                lines = f.readlines()[1:]
                syn_count = sum(1 for l in lines if ' 02 ' in l or '03' in l)
        except:
            syn_count = 0
        
        feats = {f:0.0 for f in det.feature_columns}
        feats['icmp_packet_count'] = float(last_icmp)
        feats['icmp_echo_request_count'] = float(icmp_rate * 100)
        feats['icmp_anomaly_score'] = float(min(1.0, icmp_rate))
        feats['syn_packet_count'] = float(syn_count)
        if icmp_rate > 5 or syn_count > 15:
            feats['icmp_anomaly_score'] = 1.0
            feats['syn_flood_indicator'] = 1.0
        
        req = FeatureWindowRequest(
            window_id=f"win-{int(now)}",
            window_start=datetime.now(timezone.utc),
            window_end=datetime.now(timezone.utc),
            features=feats
        )
        res = det.score_window(req)
        if res.is_anomaly or icmp_rate > 10 or syn_count > 20:
            msg = f"[ML-DETECT] *** ANOMALY *** score={res.raw_score:.4f} icmp_rate={icmp_rate:.1f}/s syn={syn_count}"
            print(msg, flush=True)
            with open(logdir/'anomalies.jsonl','a') as af:
                json.dump({'type':'icmp_flood','rate':icmp_rate,'syn':syn_count,'score':res.raw_score,'time':now}, af); af.write('\n')
            with open(logdir/'buffer_queue.jsonl','a') as bf:
                bf.write(json.dumps({'event_type':'icmp.anomaly','severity':'high'})+'\n')
        elif int(now) % 5 == 0:
            print(f"[ML-MON] icmp_rate={icmp_rate:.1f}/s syn={syn_count} score={res.raw_score:.3f}", flush=True)
        last_time = now
        time.sleep(1)
    except Exception as e:
        print(f"ERR:{e}", flush=True)
        time.sleep(1)
