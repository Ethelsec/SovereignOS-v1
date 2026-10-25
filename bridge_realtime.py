#!/usr/bin/env python3
import os, sys, json, time, subprocess
from pathlib import Path

logdir = Path('/home/barok/gods_eye_logs')
logdir.mkdir(parents=True, exist_ok=True)

sys.path.insert(0, '/home/barok/Project/gods-eye/ml-anomaly-detector/src')
sys.path.insert(0, '/home/barok/Project/gods-eye/llm-explainer')

from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone

det = load_detector('/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib',
                    '/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json')
print("[BRIDGE] ML loaded, monitoring real network activity", flush=True)

# Aggregate features from /proc/net
icmp_count = 0
conn_count = 0
syn_count = 0
last_check = time.time()

while True:
    try:
        now = time.time()
        if now - last_check >= 2:
            # Read network stats
            try:
                with open('/proc/net/snmp') as f:
                    for line in f:
                        if line.startswith('Icmp:'):
                            parts = line.split()
                            if len(parts) > 2:
                                icmp_count = int(parts[2]) + int(parts[4])
            except:
                pass
            
            try:
                with open('/proc/net/tcp') as f:
                    lines = f.readlines()[1:]
                    conn_count = len(lines)
                    syn_count = sum(1 for l in lines if ' 02 ' in l)
            except:
                pass
            
            # Build features
            feats = {f:0.0 for f in det.feature_columns}
            feats['icmp_packet_count'] = float(icmp_count)
            feats['icmp_echo_request_count'] = float(icmp_count * 0.5)
            feats['icmp_anomaly_score'] = float(min(1.0, icmp_count/1000.0))
            feats['connection_count'] = float(conn_count)
            feats['syn_packet_count'] = float(syn_count)
            if syn_count > 10:
                feats['syn_flood_indicator'] = 1.0
            if conn_count > 100:
                feats['connection_attempts'] = float(conn_count)
            
            req = FeatureWindowRequest(
                window_id=f"win-{int(now)}",
                window_start=datetime.now(timezone.utc),
                window_end=datetime.now(timezone.utc),
                features=feats
            )
            res = det.score_window(req)
            if res.is_anomaly or icmp_count > 500 or syn_count > 20:
                msg = f"[ML-REAL] ANOMALY DETECTED score={res.raw_score:.3f} icmp={icmp_count} syn={syn_count} conns={conn_count}"
                print(msg, flush=True)
                anomaly = {'type':'network_anomaly','icmp':icmp_count,'syn':syn_count,'conns':conn_count,'score':res.raw_score,'ts':now}
                with open(logdir/'anomalies.jsonl','a') as af:
                    json.dump(anomaly, af); af.write('\n')
                # Write to buffer
                with open(logdir/'buffer_queue.jsonl','a') as bf:
                    bf.write(json.dumps({'event_type':'icmp.anomaly','score':res.raw_score})+'\n')
            else:
                if icmp_count > 0 or conn_count > 0:
                    print(f"[ML-REAL] Normal icmp={icmp_count} syn={syn_count} conns={conn_count} score={res.raw_score:.3f}", flush=True)
            last_check = now
    except Exception as e:
        print(f"ERR:{e}", flush=True)
    time.sleep(1)
PYEOF
chmod +x /home/barok/Project/gods-eye/bridge_realtime.py
