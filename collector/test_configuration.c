#include "config.h"

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_configuration(void)
{
    tp_config_t cfg;
    tp_config_defaults(&cfg);
    CHECK(cfg.ring_capacity > 0);
    CHECK(cfg.worker_count > 0);
    CHECK(cfg.storage_path[0] != '\0');
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_configuration(); }
#endif
