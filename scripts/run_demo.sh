#!/bin/bash
# God's Eye Demo Script

set -e

COLLECTOR_DIR="$(dirname "$0")/../collector/build"
SAMPLES_DIR="$(dirname "$0")/../samples"
REPORTS_DIR="$(dirname "$0")/../reports"

mkdir -p "$REPORTS_DIR"

echo "=== God's Eye Demo ==="
echo ""

# Demo 1: Normal events
echo "Demo 1: Processing normal events..."
$COLLECTOR_DIR/collector \
  --ingest-file "$SAMPLES_DIR/normal-events.jsonl" \
  --storage "$REPORTS_DIR/normal_demo.tpbin" \
  --disable-dashboard \
  --disable-export \
  --run-seconds 2

echo ""
echo "Verification:"
$COLLECTOR_DIR/collector --verify "$REPORTS_DIR/normal_demo.tpbin" | tail -1
echo ""

# Demo 2: Suspicious pattern
echo "Demo 2: Processing suspicious pattern..."
$COLLECTOR_DIR/collector \
  --ingest-file "$SAMPLES_DIR/suspicious-pattern.jsonl" \
  --storage "$REPORTS_DIR/suspicious_demo.tpbin" \
  --disable-dashboard \
  --disable-export \
  --run-seconds 2

echo ""
echo "Verification:"
$COLLECTOR_DIR/collector --verify "$REPORTS_DIR/suspicious_demo.tpbin" | tail -1
echo ""

echo "=== Demo Complete ==="
echo ""
echo "Results stored in: $REPORTS_DIR"
echo "- normal_demo.tpbin"
echo "- suspicious_demo.tpbin"
echo ""
echo "To replay results:"
echo "  $COLLECTOR_DIR/collector --replay $REPORTS_DIR/suspicious_demo.tpbin"
