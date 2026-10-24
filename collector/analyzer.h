#pragma once
#include "anomaly_detector.h"

typedef struct {
    tp_anomaly_detector_t anomaly;
} tp_analyzer_t;

void tp_analyzer_init(tp_analyzer_t *analyzer);
int tp_analyzer_process(tp_analyzer_t *analyzer, tp_event_t *event, double *confidence, int *anomalous);
int tp_analyzer_mark_security_observation(tp_event_t *event, const char *message);
