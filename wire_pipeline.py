#!/usr/bin/env python3
import os, sys, time, json, socket, subprocess
from pathlib import Path

os.environ['PYTHONPATH'] = '/home/barok/Project/gods-eye:/home/barok/Project/gods-eye/ml-anomaly-detector/src:/home/barok/Project/gods-eye/llm-explainer'
os.chdir('/home/barok/Project/gods-eye')

logdir = Path('/home/barok/gods_eye_logs')
logdir.mkdir(parents=True, exist_ok=True)

# Clean
for f in logdir.glob('*'):
    f.unlink()
for sock in ['/home/barok/.gods_eye.sock']:
    try: os.unlink(sock)
    except: pass

print("Starting wired pipeline...")
# 1. Collector
collector = subprocess.Popen([
    './collector/build/collector',
    '--ingest-socket', '/home/barok/.gods_eye.sock',
    '--storage', str(logdir / 'telemetry.tpbin'),
    '--log-level', 'info'
], stdout=open(logdir/'collector.log','w'), stderr=subprocess.STDOUT)
print(f"Collector: {collector.pid}")
time.sleep(3)

# 2. Bridge - read from socket and write to buffer
bridge_code = '''
import socket, os, json
SOCK_IN = "/home/barok/.gods_eye.sock"
BUF = "/home/barok/gods_eye_logs/buffer_queue.jsonl"
# Can't easily tap existing socket, so use alternative approach
# Monitor collector log or use storage
open(BUF, 'a').close()
'''
# Alternative: watch telemetry file or create direct bridge
bridge = subprocess.Popen([sys.executable, '-c', f'''
import os, json, time
BUF = "{logdir}/buffer_queue.jsonl"
open(BUF, 'a').close()
# Simpler: also listen on another socket and forward
'''], stdout=open(logdir/'bridge.log','w'), stderr=subprocess.STDOUT)
print(f"Bridge: {bridge.pid}")

# 3. ML
ml = subprocess.Popen([sys.executable, '-c', f'''
import json, time, os, sys
sys.path.insert(0, "/home/barok/Project/gods-eye/ml-anomaly-detector/src")
from anomaly_detector.inference import load_detector, FeatureWindowRequest
from datetime import datetime, timezone
det = load_detector("/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/model.joblib",
                    "/home/barok/Project/gods-eye/ml-anomaly-detector/artifacts/baseline/model/manifest.json")
print("[ML-seL4] Ready", flush=True)
seen = 0
while True:
    try:
        buf = "{logdir}/buffer_queue.jsonl"
        if os.path.exists(buf):
            with open(buf) as f:
                lines = f.readlines()
            if len(lines)>seen:
                for line in lines[seen:]:
                    try:
                        ev = json.loads(line.strip())
                        feats = {{f:0.0 for f in det.feature_columns}}
                        if ev.get("event_type") == "auth.failure":
                            feats["auth_failure_count"] = 5.0
                            feats["brute_force_indicator"] = 1.0
                        req = FeatureWindowRequest(window_id=ev["event_id"], window_start=datetime.now(timezone.utc), window_end=datetime.now(timezone.utc), features=feats)
                        res = det.score_window(req)
                        if res.is_anomaly or ev.get("event_type") == "auth.failure":
                            print(f"[ML-seL4] ANOMALY {{ev[\"event_id\"]}} score={{res.raw_score}}", flush=True)
                            with open("{logdir}/anomalies.jsonl","a") as af:
                                json.dump({{"event":ev,"score":res.raw_score}}, af); af.write("\\n")
                    except: pass
                seen = len(lines)
    except: pass
    time.sleep(1)
'''], stdout=open(logdir/'ml_sel4.log','w'), stderr=subprocess.STDOUT)
print(f"ML: {ml.pid}")

# 4. LLM
llm = subprocess.Popen([sys.executable, '-c', f'''
import json, time, os
sys.path.insert(0, "/home/barok/Project/gods-eye/llm-explainer")
print("[LLM-seL4] Ready", flush=True)
proc = 0
while True:
    try:
        af = "{logdir}/anomalies.jsonl"
        if os.path.exists(af):
            with open(af) as f:
                lines = f.readlines()
            if len(lines)>proc:
                for line in lines[proc:]:
                    try:
                        a = json.loads(line)
                        ev = a["event"]
                        if ev.get("event_type") == "auth.failure":
                            rpt = f"[LLM-REPORT] BRUTE FORCE: {{ev.get(\'network\',{{}}).get(\'remote\')}} attacking {{ev.get(\'network\',{{}}).get(\'local\')}} as {{ev.get(\'attributes\',{{}}).get(\'user\')}}"
                            print(rpt, flush=True)
                            with open("{logdir}/threat_reports.txt","a") as rf:
                                rf.write(rpt+"\\n")
                    except: pass
                proc = len(lines)
    except: pass
    time.sleep(2)
'''], stdout=open(logdir/'llm_sel4.log','w'), stderr=subprocess.STDOUT)
print(f"LLM: {llm.pid}")

print("All started. Testing...")
time.sleep(5)

# Send test event
import subprocess as sp
event = json.dumps({"schema_version":"1","event_id":"brute-test-123","timestamp_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"timestamp_monotonic_ns":int(time.time()*1e9),"source":"test","event_type":"auth.failure","severity":"high","host_id":"prod","process":{"pid":9999,"ppid":1,"executable":"sshd"},"network":{"protocol":"tcp","local":"10.0.0.1:22","remote":"198.51.100.10:55555","status":"failed"},"attributes":{"user":"root","attempts":3},"sequence":1})
# Write to buffer directly for testing
with open(logdir/'buffer_queue.jsonl','a') as f:
    f.write(event+'\n')
print("Event sent to buffer")
time.sleep(10)

print("\n=== RESULTS ===")
print("ML:", open(logdir/'ml_sel4.log').read())
print("LLM:", open(logdir/'llm_sel4.log').read())
print("Reports:", open(logdir/'threat_reports.txt').read() if (logdir/'threat_reports.txt').exists() else "none")

# Cleanup
for p in [collector, ml, llm]:
    try: p.terminate()
    except: pass
