#include "scheduler.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int tp_scheduler_context_switch_event(pid_t pid, int cpu, tp_event_t *event)
{
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_CONTEXT_SWITCH;
    event->categories = TP_EVENT_SCHEDULER | TP_EVENT_CPU;
    event->pid = pid;
    event->tid = pid;
    event->cpu_core = cpu;
    event->severity = TP_SEV_INFO;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload), "pid=%d,cpu=%d", pid, cpu);
    return TP_OK;
}
