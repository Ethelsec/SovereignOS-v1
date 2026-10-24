#include "normalizer.h"
#include "classifier.h"
#include "constants.h"
#include "utilities.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int tp_enrich_event(tp_event_t *event)
{
    if (!event) return TP_ERR_INVAL;
    if (event->host_id[0] == '\0') tp_get_hostname(event->host_id, sizeof(event->host_id));
    if (event->process_name[0] == '\0') {
        char path[128];
        snprintf(path, sizeof(path), "/proc/%d/comm", event->pid);
        if (tp_read_first_line(path, event->process_name, sizeof(event->process_name)) != TP_OK) {
            snprintf(event->process_name, sizeof(event->process_name), "pid-%d", event->pid);
        }
    }
    if (event->executable_path[0] == '\0') {
        char procpath[128];
        snprintf(procpath, sizeof(procpath), "/proc/%d/exe", event->pid);
        ssize_t n = readlink(procpath, event->executable_path, sizeof(event->executable_path) - 1);
        if (n > 0) event->executable_path[n] = '\0';
    }
    snprintf(event->resource_attributes, sizeof(event->resource_attributes),
             "host.name=%s,process.pid=%d,process.executable.name=%s,os.type=linux",
             event->host_id, event->pid, event->process_name);
    return TP_OK;
}

int tp_normalize_event(tp_event_t *event)
{
    if (!event) return TP_ERR_INVAL;
    int ingested = (event->flags & TP_FLAG_INGEST) != 0;
    if (event->version == 0) event->version = TP_EVENT_VERSION;
    if (event->type < TP_TYPE_UNKNOWN || event->type > TP_TYPE_METRIC) event->type = TP_TYPE_CUSTOM;
    if (event->source_probe[0] == '\0') snprintf(event->source_probe, sizeof(event->source_probe), "probe-memory");
    if (event->source_module[0] == '\0') snprintf(event->source_module, sizeof(event->source_module), "collector");
    /* Local process identity must never be substituted for a producer-domain
     * event: the PID/session would refer to the wrong machine. */
    if (event->pid == 0 && !ingested) event->pid = getpid();
    if (event->tid == 0) event->tid = event->pid;
    if (event->ppid == 0 && !ingested) event->ppid = getppid();
    if (event->session_id == 0 && !ingested) event->session_id = getsid(0);
    if (event->timestamp.monotonic_ns == 0) event->timestamp = tp_timestamp_now();
    if (event->event_id == 0) event->event_id = event->timestamp.monotonic_ns;
    event->categories = tp_classify_event(event);
    if (event->severity > TP_SEV_CRITICAL) event->severity = TP_SEV_INFO;
    if (event->correlation_id == 0) event->correlation_id = ((uint64_t)(uint32_t)event->pid << 32u) ^ event->event_id;
    if (event->trace_id[0] == '\0') tp_make_id_hex(event->trace_id, sizeof(event->trace_id), event->correlation_id, event->timestamp.wall_ns);
    if (event->span_id[0] == '\0') tp_make_id_hex(event->span_id, sizeof(event->span_id), event->event_id, 0);
    return tp_enrich_event(event);
}
