#!/bin/bash
# Test secure buffer gateway

cd "$(dirname "$0")/../shared-buffer"
gcc -o test_gateway buffer_gateway.c guest_sel4_buffer.c -pthread 2>&1 | tail -10
