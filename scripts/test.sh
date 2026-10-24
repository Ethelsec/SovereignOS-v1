#!/bin/bash
# God's Eye Test Script

set -e

echo "=== God's Eye Tests ==="
echo ""

# Run collector tests
echo "Running collector tests..."
cd "$(dirname "$0")/../collector/build"
ctest --output-on-failure
echo ""

# Test guest agent compilation
echo "Testing guest agent..."
cd "$(dirname "$0")/../guest-agent"
gcc -Wall -Wextra -o /tmp/guest_probe_test guest_probe.c 2>&1 | tail -3
if [ -f /tmp/guest_probe_test ]; then
    echo "Guest agent compiles: OK"
    rm /tmp/guest_probe_test
else
    echo "Guest agent compiles: FAILED"
fi
echo ""

# Test analyzer compilation
echo "Testing analyzer..."
cd "$(dirname "$0")/../analyzer"
gcc -Wall -Wextra -c analyzer_rules.c -o /tmp/analyzer_test.o 2>&1 | tail -3
if [ -f /tmp/analyzer_test.o ]; then
    echo "Analyzer compiles: OK"
    rm /tmp/analyzer_test.o
else
    echo "Analyzer compiles: FAILED"
fi
echo ""

echo "=== All Tests Complete ==="
