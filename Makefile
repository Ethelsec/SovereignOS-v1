# God's Eye Makefile
.PHONY: all build test run start stop clean help

all: build

build:
	@echo "Building God's Eye..."
	@mkdir -p collector/build
	@cd collector/build && cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_VERBOSE_MAKEFILE=OFF 2>&1 | tail -5
	@cd collector/build && make -j4 2>&1 | tail -10
	@echo "Build complete!"

test:
	@echo "Running tests..."
	@cd collector/build && ctest --output-on-failure

start:
	@echo "Starting God's Eye in background..."
	@mkdir -p /var/log/gods_eye
	@./collector/build/collector --ingest-socket /tmp/gods_eye.sock --storage /var/log/gods_eye/telemetry.tpbin --log-level info &
	@echo $$! > /tmp/gods_eye.pid
	@echo "Started with PID: $$(cat /tmp/gods_eye.pid)"
	@echo "Socket: /tmp/gods_eye.sock"
	@echo "Storage: /var/log/gods_eye/telemetry.tpbin"

stop:
	@echo "Stopping God's Eye..."
	@if [ -f /tmp/gods_eye.pid ]; then kill $$(cat /tmp/gods_eye.pid) 2>/dev/null; rm /tmp/gods_eye.pid; echo "Stopped"; else echo "Not running"; fi

status:
	@echo "Checking status..."
	@if [ -f /tmp/gods_eye.pid ] && kill -0 $$(cat /tmp/gods_eye.pid) 2>/dev/null; then echo "Running (PID: $$(cat /tmp/gods_eye.pid))"; else echo "Not running"; fi

replay:
	@./collector/build/collector --replay /var/log/gods_eye/telemetry.tpbin 2>&1 | tail -20

verify:
	@./collector/build/collector --verify /var/log/gods_eye/telemetry.tpbin 2>&1 | tail -2

clean:
	@rm -rf collector/build reports/*.tpbin /var/log/gods_eye/* /tmp/gods_eye.* 2>&1 | tail -2
	@echo "Cleaned"

help:
	@echo "God's Eye Commands:"
	@echo "  make build   - Build the project"
	@echo "  make test    - Run tests"
	@echo "  make start   - Start collector in background"
	@echo "  make stop    - Stop collector"
	@echo "  make status  - Check if running"
	@echo "  make replay  - Replay stored events"
	@echo "  make verify  - Verify storage integrity"
	@echo "  make clean   - Clean build artifacts"
