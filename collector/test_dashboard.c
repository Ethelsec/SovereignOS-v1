#include "dashboard.h"
#include "config.h"

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_dashboard(void)
{
    tp_config_t cfg;
    tp_stats_t stats;
    tp_ring_buffer_t ring;
    tp_health_t health = { .healthy = 1 };
    tp_config_defaults(&cfg);
    cfg.dashboard = false;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 8) == TP_OK);
    tp_dashboard_render(&cfg, &stats, &ring, &health);
    tp_ring_destroy(&ring);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_dashboard(); }
#endif
