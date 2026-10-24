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
	@mkdir -p /home/barok/gods_eye_logs
	@./collector/build/collector --ingest-socket /home/barok/.gods_eye.sock --storage /home/barok/gods_eye_logs/telemetry.tpbin --log-level info &
	@echo $$! > /home/barok/.gods_eye.pid
	@echo "Started with PID: $$(cat /home/barok/.gods_eye.pid)"
	@echo "Socket: /home/barok/.gods_eye.sock"
	@echo "Storage: /home/barok/gods_eye_logs/telemetry.tpbin"

stop:
	@echo "Stopping God's Eye..."
	@if [ -f /home/barok/.gods_eye.pid ]; then kill $$(cat /home/barok/.gods_eye.pid) 2>/dev/null; rm /home/barok/.gods_eye.pid; echo "Stopped"; else echo "Not running"; fi

status:
	@echo "Checking status..."
	@if [ -f /home/barok/.gods_eye.pid ] && kill -0 $$(cat /home/barok/.gods_eye.pid) 2>/dev/null; then echo "Running (PID: $$(cat /home/barok/.gods_eye.pid))"; else echo "Not running"; fi

replay:
	@./collector/build/collector --replay /home/barok/gods_eye_logs/telemetry.tpbin 2>&1 | tail -20

verify:
	@./collector/build/collector --verify /home/barok/gods_eye_logs/telemetry.tpbin 2>&1 | tail -2

clean:
	@rm -rf collector/build reports/*.tpbin /home/barok/gods_eye_logs/* /home/barok/.gods_eye.* 2>&1 | tail -2
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
