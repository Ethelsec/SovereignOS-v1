#include "metrics.h"
#include "constants.h"
#include "utilities.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void tp_metrics_from_stats(const tp_stats_t *stats, tp_event_t *event)
{
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_METRIC;
    event->pid = getpid();
    event->tid = getpid();
    event->severity = TP_SEV_INFO;
    tp_safe_copy(event->source_probe, sizeof(event->source_probe), "internal");
    tp_safe_copy(event->source_module, sizeof(event->source_module), "statistics");
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload),
        "received=%llu,processed=%llu,stored=%llu,exported=%llu,discarded=%llu",
        (unsigned long long)atomic_load(&stats->received), (unsigned long long)atomic_load(&stats->processed),
        (unsigned long long)atomic_load(&stats->stored), (unsigned long long)atomic_load(&stats->exported),
        (unsigned long long)atomic_load(&stats->discarded));
}
