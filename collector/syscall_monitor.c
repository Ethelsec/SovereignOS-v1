#include "syscall_monitor.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int tp_syscall_event(const char *name, long result, tp_event_t *event)
{
    if (!name || !event) return TP_ERR_INVAL;
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = result < 0 ? TP_TYPE_SECURITY : TP_TYPE_CUSTOM;
    event->pid = getpid();
    event->tid = getpid();
    event->severity = result < 0 ? TP_SEV_HIGH : TP_SEV_DEBUG;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload), "syscall=%s,result=%ld", name, result);
    return TP_OK;
}
