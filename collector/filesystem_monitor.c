#include "filesystem_monitor.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int tp_filesystem_open_event(const char *path, tp_event_t *event)
{
    if (!path || !event) return TP_ERR_INVAL;
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_FILE_OPEN;
    event->pid = getpid();
    event->tid = getpid();
    event->severity = TP_SEV_INFO;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload), "path=%s", path);
    return TP_OK;
}
