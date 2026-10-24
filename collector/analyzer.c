#include "analyzer.h"
#include "normalizer.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>

void tp_analyzer_init(tp_analyzer_t *analyzer)
{
    if (!analyzer) return;
    memset(analyzer, 0, sizeof(*analyzer));
    tp_anomaly_init(&analyzer->anomaly);
}

int tp_analyzer_process(tp_analyzer_t *analyzer, tp_event_t *event, double *confidence, int *anomalous)
{
    if (!analyzer || !event) return TP_ERR_INVAL;
    if (confidence) *confidence = 0.0;
    if (anomalous) *anomalous = 0;

    int rc = tp_normalize_event(event);
    if (rc != TP_OK) return rc;
    rc = tp_validate_event(event);
    if (rc != TP_OK) return rc;

    int detected = tp_anomaly_process(&analyzer->anomaly, event, confidence);
    if (anomalous) *anomalous = detected ? 1 : 0;
    if (event->analysis[0] == '\0') {
        snprintf(event->analysis, sizeof(event->analysis), "analysis.status=normal");
    }
    return TP_OK;
}

int tp_analyzer_mark_security_observation(tp_event_t *event, const char *message)
{
    if (!event || !message) return TP_ERR_INVAL;
    event->type = TP_TYPE_SECURITY;
    event->severity = TP_SEV_HIGH;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload), "%s", message);
    return TP_OK;
}
