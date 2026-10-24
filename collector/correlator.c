#include "correlator.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>

int tp_correlator_init(tp_correlator_t *corr)
{
    memset(corr, 0, sizeof(*corr));
    return pthread_mutex_init(&corr->lock, NULL) == 0 ? TP_OK : TP_ERR;
}

void tp_correlator_destroy(tp_correlator_t *corr)
{
    if (corr) pthread_mutex_destroy(&corr->lock);
}

static tp_process_timeline_t *get_process(tp_correlator_t *corr, const tp_event_t *event)
{
    for (size_t i = 0; i < corr->process_count; ++i) {
        if (corr->processes[i].pid == event->pid) return &corr->processes[i];
    }
    size_t idx = corr->process_count < TP_MAX_PROCESSES ? corr->process_count++ : (size_t)(event->pid % TP_MAX_PROCESSES);
    tp_process_timeline_t *p = &corr->processes[idx];
    memset(p, 0, sizeof(*p));
    p->pid = event->pid;
    p->ppid = event->ppid;
    p->first_seen_ns = event->timestamp.monotonic_ns;
    tp_safe_copy(p->name, sizeof(p->name), event->process_name);
    tp_safe_copy(p->executable, sizeof(p->executable), event->executable_path);
    return p;
}

int tp_correlator_process(tp_correlator_t *corr, tp_event_t *event)
{
    if (!corr || !event) return TP_ERR_INVAL;
    pthread_mutex_lock(&corr->lock);
    tp_process_timeline_t *p = get_process(corr, event);
    p->last_seen_ns = event->timestamp.monotonic_ns;
    p->events++;
    p->bytes_io += event->payload_size;
    if (event->parent_correlation_id == 0 && event->ppid > 1) {
        for (size_t i = 0; i < corr->process_count; ++i) {
            if (corr->processes[i].pid == event->ppid) {
                event->parent_correlation_id = ((uint64_t)(uint32_t)event->ppid << 32u) ^ corr->processes[i].first_seen_ns;
                corr->correlations++;
                break;
            }
        }
    }
    snprintf(event->analysis, sizeof(event->analysis), "timeline.events=%llu,process.first_seen_ns=%llu",
             (unsigned long long)p->events, (unsigned long long)p->first_seen_ns);
    pthread_mutex_unlock(&corr->lock);
    return TP_OK;
}

void tp_correlator_write_report(tp_correlator_t *corr, FILE *out)
{
    pthread_mutex_lock(&corr->lock);
    fprintf(out, "Process timelines: %zu tracked, correlations=%llu\n",
            corr->process_count, (unsigned long long)corr->correlations);
    for (size_t i = 0; i < corr->process_count && i < 32; ++i) {
        tp_process_timeline_t *p = &corr->processes[i];
        fprintf(out, "pid=%d ppid=%d name=%s events=%llu bytes=%llu\n", p->pid, p->ppid,
                p->name, (unsigned long long)p->events, (unsigned long long)p->bytes_io);
    }
    pthread_mutex_unlock(&corr->lock);
}
