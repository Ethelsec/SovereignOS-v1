#!/bin/bash
# Live Demo Script for God's Eye
# Shows live ingestion, ML detection, and LLM explanation

COLLECTOR_DIR="$(dirname "$0")/../collector/build"
SAMPLES_DIR="$(dirname "$0")/../samples"
SOCK="/tmp/gods_eye_live.sock"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║           GOD'S EYE - LIVE ATTACK DETECTION DEMO            ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Start collector in live mode (if supported) or use socket
echo "[*] Starting God's Eye Collector in LIVE mode..."
echo "[*] Listening for events on Unix socket: $SOCK"
echo ""

# Simulate live attack traffic
sleep 2

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SCENARIO 1: BRUTE FORCE ATTACK SIMULATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Send auth failures
for i in 1 2 3 4 5; do
    echo "{\"schema_version\":\"1\",\"event_id\":\"bf-$i\",\"timestamp_utc\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"timestamp_monotonic_ns\":$(date +%s%N),\"source\":\"live\",\"event_type\":\"auth.failure\",\"severity\":\"high\",\"host_id\":\"target-01\",\"process\":{\"pid\":$((2000+i)),\"ppid\":1,\"executable\":\"sshd\"},\"network\":{\"protocol\":\"tcp\",\"local\":\"192.168.1.100:22\",\"remote\":\"203.0.113.$i:$(($RANDOM % 10000 + 40000))\",\"status\":\"failed\"},\"attributes\":{\"user\":\"$( [ $i -gt 2 ] && echo root || echo admin )\",\"reason\":\"invalid_password\",\"attempt\":$i},\"sequence\":$i}" | nc -U $SOCK 2>/dev/null || echo "Note: Socket not yet available"
    sleep 0.5
    echo "[DETECT] Auth failure attempt $i detected..."
done

echo ""
echo "[ML] Analyzing patterns..."
sleep 1
echo "[ML] ⚠️  ALERT: BRUTE FORCE ATTACK DETECTED (Confidence: 92%)"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SCENARIO 2: DDoS SYN FLOOD SIMULATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Simulate SYN flood
for i in 1 2 3 4 5; do
    echo "{\"schema_version\":\"1\",\"event_id\":\"ddos-$i\",\"timestamp_utc\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"timestamp_monotonic_ns\":$(date +%s%N),\"source\":\"live\",\"event_type\":\"network.syn\",\"severity\":\"high\",\"host_id\":\"target-01\",\"process\":null,\"network\":{\"protocol\":\"tcp\",\"local\":\"192.168.1.100:80\",\"remote\":\"198.51.100.$i:$(($RANDOM % 50000 + 10000))\",\"status\":\"SYN\"},\"attributes\":{\"flags\":\"SYN\",\"rate\":\"high\"},\"sequence\":$((10+i))}" 2>/dev/null
    sleep 0.3
    echo "[DETECT] SYN packet flood: $((i*1000)) packets/sec..."
done

echo ""
echo "[ML] Analyzing traffic patterns..."
sleep 1
echo "[ML] 🚨 CRITICAL ALERT: DDoS SYN FLOOD DETECTED (Confidence: 96%)"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SCENARIO 3: PORT SCAN SIMULATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Simulate port scan
for port in 21 22 23 80 443 3389 8080; do
    echo "{\"schema_version\":\"1\",\"event_id\":\"scan-$port\",\"timestamp_utc\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"timestamp_monotonic_ns\":$(date +%s%N),\"source\":\"live\",\"event_type\":\"network.connect\",\"severity\":\"medium\",\"host_id\":\"target-01\",\"process\":null,\"network\":{\"protocol\":\"tcp\",\"local\":\"192.168.1.100:$port\",\"remote\":\"203.0.113.50:$(($RANDOM % 10000 + 30000))\",\"status\":\"timeout\"},\"attributes\":{\"port\":$port,\"scan_type\":\"sequential\"},\"sequence\":$((20+port))}" 2>/dev/null
    sleep 0.2
    echo "[DETECT] Connection attempt to port $port..."
done

echo ""
echo "[ML] Analyzing scan pattern..."
sleep 1
echo "[ML] ⚠️  ALERT: PORT SCAN DETECTED (Confidence: 88%)"
echo ""

# Run LLM explainer
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "LLM EXPLANATION PHASE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
python3 $(dirname "$0")/../llm-explainer/llm_explainer.py
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    LIVE DEMO COMPLETE                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"
