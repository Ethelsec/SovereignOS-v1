#pragma once
#include "types.h"

typedef struct {
    double last_cpu_pct;
    uint64_t last_memory_kb;
    uint64_t last_network_bytes;
    uint64_t repeated_process_events;
    char last_message[TP_MAX_TEXT];
} tp_anomaly_detector_t;

void tp_anomaly_init(tp_anomaly_detector_t *detector);
int tp_anomaly_process(tp_anomaly_detector_t *detector, tp_event_t *event, double *confidence);
