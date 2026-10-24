#!/bin/bash
mkdir -p /home/barok/gods_eye_logs
rm -f /home/barok/.gods_eye.sock
nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye.sock \
  --storage /home/barok/gods_eye_logs/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_logs/collector.log 2>&1 &
echo $! > /home/barok/.gods_eye.pid
echo "Started PID: $(cat /home/barok/.gods_eye.pid)"
