#include "utilities.h"
#include "macros.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint64_t tp_now_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t tp_now_wall_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

tp_timestamp_t tp_timestamp_now(void)
{
    tp_timestamp_t ts = { tp_now_monotonic_ns(), tp_now_wall_ns() };
    return ts;
}

uint32_t tp_crc32(const void *data, size_t len)
{
    static uint32_t table[256];
    static int ready;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1u) ? (0xedb88320u ^ (c >> 1u)) : (c >> 1u);
            table[i] = c;
        }
        ready = 1;
    }
    uint32_t crc = 0xffffffffu;
    const unsigned char *p = data;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ p[i]) & 0xffu] ^ (crc >> 8u);
    return crc ^ 0xffffffffu;
}

void tp_make_id_hex(char *out, size_t len, uint64_t a, uint64_t b)
{
    if (!out || len == 0) return;
    if (len >= TP_TRACE_ID_LEN) snprintf(out, len, "%016llx%016llx", (unsigned long long)a, (unsigned long long)b);
    else if (len >= TP_SPAN_ID_LEN) snprintf(out, len, "%016llx", (unsigned long long)a);
    else snprintf(out, len, "%llx", (unsigned long long)a);
}

int tp_get_hostname(char *out, size_t len)
{
    if (!out || len == 0) return TP_ERR_INVAL;
    if (gethostname(out, len) != 0) {
        tp_safe_copy(out, len, "unknown-host");
        return TP_ERR_IO;
    }
    out[len - 1] = '\0';
    return TP_OK;
}

int tp_read_first_line(const char *path, char *out, size_t len)
{
    if (!path || !out || len == 0) return TP_ERR_INVAL;
    FILE *f = fopen(path, "r");
    if (!f) return TP_ERR_IO;
    if (!fgets(out, (int)len, f)) {
        fclose(f);
        return TP_ERR_IO;
    }
    fclose(f);
    out[strcspn(out, "\r\n")] = '\0';
    return TP_OK;
}

double tp_timespec_diff_ms(uint64_t start_ns, uint64_t end_ns)
{
    return end_ns >= start_ns ? (double)(end_ns - start_ns) / 1000000.0 : 0.0;
}

const char *tp_event_type_name(tp_event_type_t type)
{
    switch (type) {
    case TP_TYPE_PROCESS_START: return "process.start";
    case TP_TYPE_PROCESS_EXIT: return "process.exit";
    case TP_TYPE_THREAD_START: return "thread.start";
    case TP_TYPE_THREAD_EXIT: return "thread.exit";
    case TP_TYPE_EXEC: return "process.exec";
    case TP_TYPE_FORK: return "process.fork";
    case TP_TYPE_SIGNAL: return "signal";
    case TP_TYPE_FILE_OPEN: return "file.open";
    case TP_TYPE_FILE_WRITE: return "file.write";
    case TP_TYPE_FILE_CLOSE: return "file.close";
    case TP_TYPE_SOCKET_CONNECT: return "socket.connect";
    case TP_TYPE_SOCKET_ACCEPT: return "socket.accept";
    case TP_TYPE_SOCKET_CLOSE: return "socket.close";
    case TP_TYPE_TCP_STATE: return "tcp.state";
    case TP_TYPE_DNS: return "dns";
    case TP_TYPE_HTTP: return "http";
    case TP_TYPE_MEMORY_ALLOC: return "memory.alloc";
    case TP_TYPE_PAGE_FAULT: return "page_fault";
    case TP_TYPE_CPU_SAMPLE: return "cpu.sample";
    case TP_TYPE_CONTEXT_SWITCH: return "context_switch";
    case TP_TYPE_SECURITY: return "security";
    case TP_TYPE_KERNEL_WARNING: return "kernel.warning";
    case TP_TYPE_RESOURCE_SPIKE: return "resource.spike";
    case TP_TYPE_HEALTH: return "health";
    case TP_TYPE_LOG: return "log";
    case TP_TYPE_METRIC: return "metric";
    default: return "unknown";
    }
}

const char *tp_severity_name(tp_severity_t severity)
{
    switch (severity) {
    case TP_SEV_TRACE: return "TRACE";
    case TP_SEV_DEBUG: return "DEBUG";
    case TP_SEV_INFO: return "INFO";
    case TP_SEV_NORMAL: return "NORMAL";
    case TP_SEV_HIGH: return "HIGH";
    case TP_SEV_CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

uint32_t tp_hash_event_identity(const tp_event_t *event)
{
    if (!event) return 0;
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)event->type) * 16777619u;
    h = (h ^ (uint32_t)event->pid) * 16777619u;
    h = (h ^ event->payload_size) * 16777619u;
    h = (h ^ tp_crc32(event->payload, event->payload_size)) * 16777619u;
    return h;
}

void tp_safe_copy(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_len, "%s", src);
}

int tp_parse_u64(const char *text, uint64_t *value)
{
    if (!text || !value) return TP_ERR_INVAL;
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(text, &end, 10);
    if (errno || end == text || (*end && !isspace((unsigned char)*end))) return TP_ERR_INVAL;
    *value = (uint64_t)v;
    return TP_OK;
}
