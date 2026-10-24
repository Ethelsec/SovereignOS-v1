#include "anomaly_detector.h"
#include "parser.h"
#include "utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tp_anomaly_init(tp_anomaly_detector_t *detector)
{
    memset(detector, 0, sizeof(*detector));
}

int tp_anomaly_process(tp_anomaly_detector_t *detector, tp_event_t *event, double *confidence)
{
    if (!detector || !event) return 0;
    if (confidence) *confidence = 0.0;
    char payload[TP_MAX_PAYLOAD + 1];
    tp_decode_payload_text(event, payload, sizeof(payload));
    int anomalous = 0;
    if (event->type == TP_TYPE_RESOURCE_SPIKE || event->severity == TP_SEV_CRITICAL) {
        anomalous = 1;
        if (confidence) *confidence = 0.95;
    } else if (event->type == TP_TYPE_PROCESS_START || event->type == TP_TYPE_FORK) {
        detector->repeated_process_events++;
        if (detector->repeated_process_events > 100) {
            anomalous = 1;
            if (confidence) *confidence = 0.80;
        }
    } else if (strstr(payload, "auth failed") || strstr(payload, "permission denied") || strstr(payload, "oom")) {
        anomalous = 1;
        if (confidence) *confidence = 0.70;
    } else {
        detector->repeated_process_events = 0;
    }
    if (anomalous) {
        event->severity = event->severity < TP_SEV_HIGH ? TP_SEV_HIGH : event->severity;
        char prior[128];
        const char *src = event->analysis[0] ? event->analysis : "analysis=available";
        size_t n = strnlen(src, sizeof(prior) - 1);
        memcpy(prior, src, n);
        prior[n] = '\0';
        snprintf(event->analysis, sizeof(event->analysis), "%.128s,anomaly.confidence=%.2f",
                 prior, confidence ? *confidence : 0.0);
    }
    return anomalous;
}
