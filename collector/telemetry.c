#include "telemetry.h"
#include "logger.h"
#include "parser.h"
#include "resource_monitor.h"
#include "utilities.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

int tp_ebpf_probe_status(char *out, size_t out_len)
{
    struct utsname u;
    if (uname(&u) != 0) {
        snprintf(out, out_len, "kernel capability check failed: %s", strerror(errno));
        return TP_ERR_IO;
    }
    FILE *f = fopen("/sys/kernel/btf/vmlinux", "rb");
    int btf = f != NULL;
    if (f) fclose(f);
    snprintf(out, out_len, "kernel=%s arch=%s btf=%s core_ready=%s",
             u.release, u.machine, btf ? "available" : "missing", btf ? "yes" : "limited");
    return btf ? TP_OK : TP_ERR;
}

int tp_emit_startup_events(tp_ring_buffer_t *ring)
{
    char status[TP_MAX_TEXT];
    tp_ebpf_probe_status(status, sizeof(status));
    tp_event_t event;
    tp_parse_raw_event("health|collector|startup", &event);
    event.type = TP_TYPE_HEALTH;
    event.payload_size = (uint32_t)snprintf((char *)event.payload, sizeof(event.payload), "%s", status);
    return tp_ring_push(ring, &event);
}

void *tp_procfs_probe_thread(void *arg)
{
    tp_probe_context_t *ctx = arg;
    tp_resource_sample_t prev, cur;
    memset(&prev, 0, sizeof(prev));
    tp_resource_sample(&prev);
    while (*ctx->running) {
        usleep(ctx->interval_ms * 1000u);
        if (tp_resource_sample(&cur) == TP_OK) {
            tp_event_t e;
            tp_resource_event(&prev, &cur, &e);
            if (tp_ring_push(ctx->ring, &e) != TP_OK) {
                tp_log(TP_LOG_WARNING, "probe", 0, "ring buffer full while publishing procfs metric");
            }
            prev = cur;
        }
    }
    return NULL;
}
