#include "network_monitor.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int tp_network_connect_event(const char *remote, unsigned port, tp_event_t *event)
{
    if (!remote || !event) return TP_ERR_INVAL;
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_SOCKET_CONNECT;
    event->pid = getpid();
    event->tid = getpid();
    event->severity = TP_SEV_INFO;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload), "remote=%s,port=%u", remote, port);
    return TP_OK;
}
