#pragma once
#include "types.h"

typedef struct {
    uint32_t hashes[TP_MAX_TEXT];
    uint64_t times[TP_MAX_TEXT];
    size_t next;
    unsigned window_ms;
} tp_filter_t;

void tp_filter_init(tp_filter_t *filter, unsigned window_ms);
int tp_filter_should_keep(tp_filter_t *filter, const tp_config_t *cfg, const tp_event_t *event, int *duplicate);
