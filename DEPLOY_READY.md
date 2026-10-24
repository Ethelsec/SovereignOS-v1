# God's Eye - Deployment Ready Guide

## Overview
God's Eye is now a comprehensive, deploy-ready defensive telemetry platform with:
- Core Collector (C17) with canonical ingestion
- ML Anomaly Detection with Isolation Forest and extended features
- seL4 integration framework
- Guest OS <-> seL4 shared buffer
- LLM-powered explainer with key rotation
- Detection for: Auth failures, DDoS (SYN flood), Port scans, ICMP anomalies
- Verbose UI reports

## API Keys Configuration

The system is configured with 4 API keys that rotate automatically:

```bash
export GEMINI_API_KEY_1="AQ.Ab8RN6KQ8_gfqJVdLD1qVxo29H-6t2Xll53XmEa_4oWg0SMh3Q"
export GEMINI_API_KEY_2="AQ.Ab8RN6L5r2t0KtwU485HgLW1VVtNZYvV4QzAbUKoN9HfGgIgZw"
export GROK_API_KEY="grok_04532454a5315216c7c6bcad6c269ab9_958750ef61bbf4d5aa2657a6108bcde1a4a015611ad688215e87ff50e013207e"
export KIMI_API_KEY="sk-zuYTyzN2OdfpVJPFjOPZwD8TIIl5MWtgf27uW7trIhzOWrbC"
```

The key rotator (`llm-explainer/llm_key_rotator.py`) handles automatic rotation to avoid token limits.

## Quick Start (Deploy Ready)

```bash
# 1. Build everything
cd /home/barok/Project/gods-eye/scripts
./build.sh

# 2. Run tests
./test.sh

# 3. Start monitoring with extended detection
cd /home/barok/Project/gods-eye
./collector/build/collector \
  --ingest-file samples/suspicious-pattern.jsonl \
  --storage /var/log/gods-eye/telemetry.tpbin \
  --log-level info \
  --run-seconds 60

# 4. Generate verbose LLM report
python3 llm-explainer/llm_explainer.py > /var/log/gods-eye/report.txt
```

## Detection Capabilities

| Threat Type | Detection Method | Status |
|-------------|------------------|--------|
| Brute Force/Auth Failures | Rule-based + ML baseline | ✅ Active |
| DDoS SYN Flood | Extended ML features (syn_packet_count, syn_flood_indicator) | ✅ Active |
| Port Scanning | Port scan indicator, unique_dest_ports | ✅ Active |
| ICMP Anomalies/Ping Flood | ICMP features, echo request patterns | ✅ Active |
| Connection Floods | Connection count, failed ratios | ✅ Active |
| Resource Anomalies | Existing anomaly detector | ✅ Active |

## Architecture Components

- **Collector** (`collector/`): Core C17 telemetry engine
- **ML Detector** (`ml-anomaly-detector/`): Isolation Forest + extended features
- **Shared Buffer** (`shared-buffer/`): Guest OS <-> seL4 communication
- **LLM Explainer** (`llm-explainer/`): Evidence-based explanations with key rotation
- **Guest Agent** (`guest-agent/`): Telemetry collection
- **seL4 Integration** (`~/seL4/`): Microkernel components

## Configuration Files

- `ml-anomaly-detector/configs/extended_detection.yaml` - Extended detection config
- `collector/config/` - Collector configuration
- `gods-eye/config/` - Global configuration

## Monitoring

```bash
# View real-time stats
cat /var/log/gods-eye/telemetry-summary.json | python3 -m json.tool

# Replay events
./collector/build/collector --replay /var/log/gods-eye/telemetry.tpbin

# Verify integrity
./collector/build/collector --verify /var/log/gods-eye/telemetry.tpbin
```

## Production Considerations

- Use persistent storage paths
- Configure log rotation
- Set up systemd service
- Enable proper firewall rules
- Monitor resource usage
- Regular updates to detection rules

## Testing

See `MANUAL_TESTING_GUIDE.md` for detailed step-by-step testing procedures.

## Support

This is a deploy-ready prototype. All components are integrated and tested.
