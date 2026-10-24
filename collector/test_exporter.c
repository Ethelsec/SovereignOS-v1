#include "exporter.h"
#include "parser.h"
#include "config.h"
#include <stdio.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_exporter(void)
{
    tp_config_t cfg;
    tp_config_defaults(&cfg);
    snprintf(cfg.export_endpoint, sizeof(cfg.export_endpoint), "/tmp/collector-export-%ld.jsonl", (long)getpid());
    tp_exporter_t exporter;
    tp_event_t event;
    CHECK(tp_parse_raw_event("health|test|export", &event) == TP_OK);
    CHECK(tp_exporter_open(&exporter, &cfg) == TP_OK);
    CHECK(tp_exporter_export(&exporter, &event) == TP_OK);
    CHECK(tp_exporter_flush(&exporter) == TP_OK);
    tp_exporter_close(&exporter);
    unlink(cfg.export_endpoint);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_exporter(); }
#endif
