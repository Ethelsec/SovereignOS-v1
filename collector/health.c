#include "health.h"
#include "utilities.h"
#include <stdio.h>

int tp_health_check(tp_health_t *health, const tp_ring_buffer_t *ring, const tp_config_t *cfg)
{
    if (!health || !ring || !cfg) return TP_ERR_INVAL;
    unsigned util = tp_ring_utilization_pct(ring);
    health->last_check_ns = tp_now_monotonic_ns();
    if (util > 95) {
        health->healthy = 0;
        snprintf(health->message, sizeof(health->message), "ring buffer critical at %u%%", util);
    } else if (cfg->worker_count == 0) {
        health->healthy = 0;
        snprintf(health->message, sizeof(health->message), "worker_count must be nonzero");
    } else {
        health->healthy = 1;
        snprintf(health->message, sizeof(health->message), "all monitored components nominal");
    }
    return health->healthy ? TP_OK : TP_ERR;
}
