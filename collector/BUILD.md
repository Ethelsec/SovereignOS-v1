# Build

Use CMake 3.20 or newer:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Useful options:

```sh
cmake -S . -B build -DTP_ENABLE_TESTS=ON -DTP_ENABLE_WARNINGS_AS_ERRORS=ON
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
```

The build creates `collector` and `collector_core`. Optional libbpf, protobuf-c, and zlib are detected through pkg-config/CMake and linked when available.

Direct compiler smoke build:

```sh
cc -std=c17 -D_GNU_SOURCE -Wall -Wextra -Werror -pthread \
  analyzer.c anomaly_detector.c buffer.c classifier.c cli.c config.c correlator.c dashboard.c \
  decoder.c encoder.c exporter.c file_reader.c file_writer.c filter.c filesystem_monitor.c \
  health.c logger.c memory_pool.c metrics.c network_monitor.c normalizer.c otlp_client.c \
  parser.c platform.c process_tracker.c protobuf_serializer.c resource_monitor.c ring_buffer.c \
  scheduler.c statistics.c storage.c syscall_monitor.c telemetry.c terminal_ui.c timeline.c utilities.c main.c \
  -o collector
```
