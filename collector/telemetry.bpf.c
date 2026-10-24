#include <stdint.h>

struct telemetry_bpf_event {
    uint64_t timestamp_ns;
    uint32_t pid;
    uint32_t type;
};

int collector_bpf_core_probe(struct telemetry_bpf_event *event)
{
    if (!event) return -1;
    event->type = event->type ? event->type : 1;
    return 0;
}
