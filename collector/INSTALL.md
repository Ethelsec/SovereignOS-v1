# Install

Install dependencies on Debian-like systems:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libbpf-dev protobuf-c-compiler libprotobuf-c-dev zlib1g-dev
```

Build and install:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Create runtime storage when running as a service:

```sh
sudo install -d -m 0750 /var/lib/collector
sudo install -d -m 0750 /var/log/collector
collector --storage /var/lib/collector/telemetry.tpbin --export /var/lib/collector/otlp-export.jsonl
```

For eBPF attachment on locked-down systems, run with the required capabilities for your distribution, commonly `CAP_BPF`, `CAP_PERFMON`, `CAP_SYS_RESOURCE`, and sometimes `CAP_SYS_ADMIN`.
