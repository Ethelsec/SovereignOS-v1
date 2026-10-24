#include "resource_monitor.h"
#include "constants.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_cpu(tp_resource_sample_t *s)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return TP_ERR_IO;
    char cpu[8];
    uint64_t user, nice, sys, idle, iowait, irq, softirq, steal;
    int ok = fscanf(f, "%7s %llu %llu %llu %llu %llu %llu %llu %llu", cpu,
                    (unsigned long long *)&user, (unsigned long long *)&nice,
                    (unsigned long long *)&sys, (unsigned long long *)&idle,
                    (unsigned long long *)&iowait, (unsigned long long *)&irq,
                    (unsigned long long *)&softirq, (unsigned long long *)&steal) == 9;
    fclose(f);
    if (!ok) return TP_ERR_IO;
    s->cpu_idle = idle + iowait;
    s->cpu_total = user + nice + sys + idle + iowait + irq + softirq + steal;
    return TP_OK;
}

static int read_mem(tp_resource_sample_t *s)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return TP_ERR_IO;
    char key[64], unit[16];
    uint64_t value;
    while (fscanf(f, "%63s %llu %15s", key, (unsigned long long *)&value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) s->mem_total_kb = value;
        else if (strcmp(key, "MemAvailable:") == 0) s->mem_available_kb = value;
    }
    fclose(f);
    return s->mem_total_kb ? TP_OK : TP_ERR_IO;
}

static void read_net(tp_resource_sample_t *s)
{
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;
    char line[512];
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        char iface[64];
        uint64_t rx, tx;
        if (sscanf(line, " %63[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu",
                   iface, (unsigned long long *)&rx, (unsigned long long *)&tx) == 3) {
            if (strcmp(iface, "lo") != 0) {
                s->net_rx_bytes += rx;
                s->net_tx_bytes += tx;
            }
        }
    }
    fclose(f);
}

int tp_resource_sample(tp_resource_sample_t *sample)
{
    memset(sample, 0, sizeof(*sample));
    int rc = read_cpu(sample);
    if (read_mem(sample) != TP_OK) rc = TP_ERR_IO;
    read_net(sample);
    return rc;
}

int tp_resource_event(const tp_resource_sample_t *prev, const tp_resource_sample_t *cur, tp_event_t *event)
{
    if (!cur || !event) return TP_ERR_INVAL;
    memset(event, 0, sizeof(*event));
    event->version = TP_EVENT_VERSION;
    event->timestamp = tp_timestamp_now();
    event->event_id = event->timestamp.monotonic_ns;
    event->type = TP_TYPE_METRIC;
    event->severity = TP_SEV_INFO;
    event->pid = getpid();
    event->tid = getpid();
    event->ppid = getppid();
    event->uid = getuid();
    event->gid = getgid();
    tp_safe_copy(event->source_probe, sizeof(event->source_probe), "procfs");
    tp_safe_copy(event->source_module, sizeof(event->source_module), "resource_monitor");
    double cpu_pct = 0.0;
    if (prev && cur->cpu_total > prev->cpu_total) {
        uint64_t total = cur->cpu_total - prev->cpu_total;
        uint64_t idle = cur->cpu_idle - prev->cpu_idle;
        cpu_pct = total ? 100.0 * (double)(total - idle) / (double)total : 0.0;
    }
    uint64_t used_kb = cur->mem_total_kb > cur->mem_available_kb ? cur->mem_total_kb - cur->mem_available_kb : 0;
    int spike = cpu_pct > 85.0;
    event->type = spike ? TP_TYPE_RESOURCE_SPIKE : TP_TYPE_METRIC;
    event->severity = spike ? TP_SEV_HIGH : TP_SEV_INFO;
    event->payload_size = (uint32_t)snprintf((char *)event->payload, sizeof(event->payload),
        "cpu_pct=%.2f,mem_used_kb=%llu,net_rx_bytes=%llu,net_tx_bytes=%llu",
        cpu_pct, (unsigned long long)used_kb, (unsigned long long)cur->net_rx_bytes,
        (unsigned long long)cur->net_tx_bytes);
    return TP_OK;
}
