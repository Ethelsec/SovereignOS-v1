#pragma once
#include "types.h"

typedef struct {
    uint64_t cpu_total;
    uint64_t cpu_idle;
    uint64_t mem_total_kb;
    uint64_t mem_available_kb;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    double cpu_pct;
} tp_resource_sample_t;

int tp_resource_sample(tp_resource_sample_t *sample);
int tp_resource_event(const tp_resource_sample_t *prev, const tp_resource_sample_t *cur, tp_event_t *event);
