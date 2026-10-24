#!/bin/bash
# God's Eye Build Script

set -e

echo "=== God's Eye Build ==="
echo ""

# Build collector
echo "Building collector..."
cd "$(dirname "$0")/../collector"
mkdir -p build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
make -j4
echo "Collector built successfully"
echo ""

# Build guest agent
echo "Building guest agent..."
cd "$(dirname "$0")/../guest-agent"
gcc -o guest_probe guest_probe.c 2>&1 | tail -5
if [ -f guest_probe ]; then
    echo "Guest agent built successfully"
else
    echo "Guest agent build skipped or failed"
fi
echo ""

# Build analyzer
echo "Building analyzer..."
cd "$(dirname "$0")/../analyzer"
gcc -c -o analyzer_rules.o analyzer_rules.c 2>&1 | tail -5
if [ -f analyzer_rules.o ]; then
    echo "Analyzer built successfully"
else
    echo "Analyzer build skipped or failed"
fi
echo ""

echo "=== Build Complete ==="
