#include "dashboard.h"
#include "utilities.h"
#include <stdio.h>
#include <stdatomic.h>

void tp_dashboard_render(const tp_config_t *cfg, const tp_stats_t *stats, const tp_ring_buffer_t *ring, const tp_health_t *health)
{
    if (!cfg->dashboard) return;
    printf("\033[H\033[J");
    printf("Collector Telemetry Platform\n");
    printf("storage=%s export=%s dashboard_interval_ms=%u\n", cfg->storage_path, cfg->export_endpoint, cfg->dashboard_interval_ms);
    printf("received=%llu processed=%llu stored=%llu exported=%llu discarded=%llu duplicates=%llu anomalies=%llu\n",
           (unsigned long long)atomic_load(&stats->received), (unsigned long long)atomic_load(&stats->processed),
           (unsigned long long)atomic_load(&stats->stored), (unsigned long long)atomic_load(&stats->exported),
           (unsigned long long)atomic_load(&stats->discarded), (unsigned long long)atomic_load(&stats->duplicates),
           (unsigned long long)atomic_load(&stats->anomalies));
    printf("buffer.size=%zu buffer.utilization=%u%% dropped=%llu crc_failures=%llu avg_latency_ms=%.3f\n",
           tp_ring_size(ring), tp_ring_utilization_pct(ring), (unsigned long long)tp_ring_dropped(ring),
           (unsigned long long)atomic_load(&stats->crc_failures), tp_stats_average_latency_ms(stats));
    printf("warnings=%llu errors=%llu max_buffer_utilization=%llu%% health=%s %s\n",
           (unsigned long long)atomic_load(&stats->warnings), (unsigned long long)atomic_load(&stats->errors),
           (unsigned long long)atomic_load(&stats->max_buffer_utilization_pct),
           health && health->healthy ? "healthy" : "unhealthy", health ? health->message : "");
    fflush(stdout);
}
