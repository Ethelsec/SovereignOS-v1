#pragma once

struct telemetry_bpf {
    int core_btf_available;
};

static inline struct telemetry_bpf *telemetry_bpf__open(void)
{
    static struct telemetry_bpf obj = { .core_btf_available = 1 };
    return &obj;
}

static inline int telemetry_bpf__load(struct telemetry_bpf *obj)
{
    return obj ? 0 : -1;
}

static inline int telemetry_bpf__attach(struct telemetry_bpf *obj)
{
    return obj ? 0 : -1;
}

static inline void telemetry_bpf__destroy(struct telemetry_bpf *obj)
{
    (void)obj;
}
