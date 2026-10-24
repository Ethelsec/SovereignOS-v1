#include "parser.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int tp_validate_event(const tp_event_t *event)
{
    if (!event) return TP_ERR_INVAL;
    if (event->version != TP_EVENT_VERSION) return TP_ERR_INVAL;
    if (event->payload_size > TP_MAX_PAYLOAD) return TP_ERR_INVAL;
    if (event->timestamp.monotonic_ns == 0 || event->timestamp.wall_ns == 0) return TP_ERR_INVAL;
    if (event->type < TP_TYPE_UNKNOWN || event->type > TP_TYPE_METRIC) return TP_ERR_INVAL;
    return TP_OK;
}

static tp_event_type_t parse_type(const char *s)
{
    if (strcmp(s, "process_start") == 0) return TP_TYPE_PROCESS_START;
    if (strcmp(s, "process_exit") == 0) return TP_TYPE_PROCESS_EXIT;
    if (strcmp(s, "exec") == 0) return TP_TYPE_EXEC;
    if (strcmp(s, "fork") == 0) return TP_TYPE_FORK;
    if (strcmp(s, "file_open") == 0) return TP_TYPE_FILE_OPEN;
    if (strcmp(s, "socket_connect") == 0) return TP_TYPE_SOCKET_CONNECT;
    if (strcmp(s, "cpu") == 0) return TP_TYPE_CPU_SAMPLE;
    if (strcmp(s, "memory") == 0) return TP_TYPE_MEMORY_ALLOC;
    if (strcmp(s, "security") == 0) return TP_TYPE_SECURITY;
    if (strcmp(s, "health") == 0) return TP_TYPE_HEALTH;
    return TP_TYPE_CUSTOM;
}

int tp_parse_raw_event(const char *record, tp_event_t *event)
{
    if (!record || !event) return TP_ERR_INVAL;
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_CUSTOM;
    event->severity = TP_SEV_INFO;
    event->pid = getpid();
    event->tid = getpid();
    event->ppid = getppid();
    event->uid = getuid();
    event->gid = getgid();
    event->session_id = getsid(0);
    event->numa_node = -1;
    tp_safe_copy(event->source_probe, sizeof(event->source_probe), "userspace");
    tp_safe_copy(event->source_module, sizeof(event->source_module), "parser");
    tp_get_hostname(event->host_id, sizeof(event->host_id));

    char copy[TP_MAX_PAYLOAD];
    tp_safe_copy(copy, sizeof(copy), record);
    char *save = NULL;
    char *type = strtok_r(copy, "|", &save);
    char *name = strtok_r(NULL, "|", &save);
    char *payload = strtok_r(NULL, "", &save);
    if (type) event->type = parse_type(type);
    if (name) tp_safe_copy(event->process_name, sizeof(event->process_name), name);
    if (!payload) payload = (char *)record;
    event->payload_size = (uint32_t)strnlen(payload, TP_MAX_PAYLOAD);
    memcpy(event->payload, payload, event->payload_size);
    return tp_validate_event(event);
}

int tp_decode_payload_text(const tp_event_t *event, char *out, size_t out_len)
{
    if (!event || !out || out_len == 0) return TP_ERR_INVAL;
    size_t n = event->payload_size < out_len - 1 ? event->payload_size : out_len - 1;
    memcpy(out, event->payload, n);
    out[n] = '\0';
    return TP_OK;
}
