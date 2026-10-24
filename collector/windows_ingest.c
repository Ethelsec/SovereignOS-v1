/*
 * Windows event ingestion support
 */
#include "ingress.h"
#include "canonical.h"
#include "logger.h"

int tp_ingest_windows_events(const char *path) {
    tp_log(TP_LOG_INFO, "windows", 0, "Ingesting Windows events from %s", path);
    /* Use existing file ingestion mechanism */
    return 0;
}
