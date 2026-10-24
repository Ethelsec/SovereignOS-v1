#!/bin/bash
# Full God's Eye startup with Collector, ML, LLM via seL4 buffer

mkdir -p /home/barok/gods_eye_logs
rm -f /home/barok/.gods_eye.sock

echo "[1/4] Starting Collector..."
nohup /home/barok/Project/gods-eye/collector/build/collector \
  --ingest-socket /home/barok/.gods_eye.sock \
  --storage /home/barok/gods_eye_logs/telemetry.tpbin \
  --log-level info > /home/barok/gods_eye_logs/collector.log 2>&1 &
echo "Collector started (PID: $!)"

sleep 2

echo "[2/4] Starting Buffer Gateway..."
nohup python3 -c "
# Start buffer gateway
from shared_buffer import buffer_gateway
buffer_gateway.start()
" > /home/barok/gods_eye_logs/gateway.log 2>&1 &
echo "Gateway started (PID: $!)"

sleep 2

echo "[3/4] Starting ML Detector (inside seL4 component)..."
nohup python3 /home/barok/Project/gods-eye/ml-anomaly-detector/src/anomaly_detector/live_ingestion.py \
  --buffer-mode \
  --sel4-component \
  > /home/barok/gods_eye_logs/ml_detector.log 2>&1 &
echo "ML Detector started (PID: $!)"

sleep 2

echo "[4/4] Starting LLM Explainer (inside seL4 component)..."
nohup python3 /home/barok/Project/gods-eye/llm-explainer/llm_explainer.py \
  --daemon \
  --sel4-component \
  --listen-buffer \
  > /home/barok/gods_eye_logs/llm_explainer.log 2>&1 &
echo "LLM Explainer started (PID: $!)"

echo ""
echo "God's Eye Full Stack Started"
echo "Collector: /home/barok/.gods_eye.sock"
echo "Logs: /home/barok/gods_eye_logs/"
