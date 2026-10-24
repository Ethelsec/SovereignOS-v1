#pragma once
#include "types.h"

typedef struct {
    FILE *file;
    char endpoint[TP_MAX_TEXT];
    uint64_t exported;
    uint64_t failures;
    int enabled;
} tp_exporter_t;

int tp_exporter_open(tp_exporter_t *exporter, const tp_config_t *cfg);
int tp_exporter_export(tp_exporter_t *exporter, const tp_event_t *event);
int tp_exporter_flush(tp_exporter_t *exporter);
void tp_exporter_close(tp_exporter_t *exporter);
int tp_exporter_is_enabled(const tp_exporter_t *exporter);
