#include "statistics.h"
#include "utilities.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

void tp_stats_init(tp_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->started_wall_ns = tp_now_wall_ns();
}

void tp_stats_inc(_Atomic uint64_t *field) { atomic_fetch_add(field, 1); }
void tp_stats_add(_Atomic uint64_t *field, uint64_t value) { atomic_fetch_add(field, value); }
uint64_t tp_stats_get(const _Atomic uint64_t *field) { return atomic_load(field); }

double tp_stats_average_latency_ms(const tp_stats_t *stats)
{
    uint64_t processed = atomic_load(&stats->processed);
    return processed ? (double)atomic_load(&stats->total_processing_ns) / (double)processed / 1000000.0 : 0.0;
}

void tp_stats_update_buffer_peak(tp_stats_t *stats, unsigned utilization_pct)
{
    uint64_t old = atomic_load(&stats->max_buffer_utilization_pct);
    while (utilization_pct > old && !atomic_compare_exchange_weak(&stats->max_buffer_utilization_pct, &old, utilization_pct)) {}
}

void tp_stats_print_summary(const tp_stats_t *s, FILE *out)
{
    uint64_t elapsed_ns = tp_now_wall_ns() - s->started_wall_ns;
    double elapsed = elapsed_ns ? (double)elapsed_ns / 1000000000.0 : 0.0;
    fprintf(out, "Execution summary\n");
    fprintf(out, "duration_seconds=%.3f\nreceived=%llu\nprocessed=%llu\nexported=%llu\nstored=%llu\n"
                 "discarded=%llu\nduplicates=%llu\nanomalies=%llu\nwarnings=%llu\nerrors=%llu\n"
                 "dropped=%llu\nrecovered=%llu\ncrc_failures=%llu\naverage_latency_ms=%.3f\n"
                 "average_throughput=%.2f\nmax_buffer_utilization_pct=%llu\n",
            elapsed,
            (unsigned long long)atomic_load(&s->received), (unsigned long long)atomic_load(&s->processed),
            (unsigned long long)atomic_load(&s->exported), (unsigned long long)atomic_load(&s->stored),
            (unsigned long long)atomic_load(&s->discarded), (unsigned long long)atomic_load(&s->duplicates),
            (unsigned long long)atomic_load(&s->anomalies), (unsigned long long)atomic_load(&s->warnings),
            (unsigned long long)atomic_load(&s->errors), (unsigned long long)atomic_load(&s->dropped),
            (unsigned long long)atomic_load(&s->recovered), (unsigned long long)atomic_load(&s->crc_failures),
            tp_stats_average_latency_ms(s), elapsed > 0.0 ? (double)atomic_load(&s->processed) / elapsed : 0.0,
            (unsigned long long)atomic_load(&s->max_buffer_utilization_pct));
    fprintf(out, "ingress_bytes=%llu\ningress_frames=%llu\ningress_accepted=%llu\n"
                 "ingress_rejected=%llu\ningress_dropped=%llu\ningress_duplicates=%llu\n"
                 "ingress_gaps=%llu\ningress_out_of_order=%llu\ningress_oversize=%llu\n"
                 "ingress_truncated=%llu\ningress_transport_errors=%llu\n",
            (unsigned long long)atomic_load(&s->ingress_bytes),
            (unsigned long long)atomic_load(&s->ingress_frames),
            (unsigned long long)atomic_load(&s->ingress_accepted),
            (unsigned long long)atomic_load(&s->ingress_rejected),
            (unsigned long long)atomic_load(&s->ingress_dropped),
            (unsigned long long)atomic_load(&s->ingress_duplicates),
            (unsigned long long)atomic_load(&s->ingress_gaps),
            (unsigned long long)atomic_load(&s->ingress_out_of_order),
            (unsigned long long)atomic_load(&s->ingress_oversize),
            (unsigned long long)atomic_load(&s->ingress_truncated),
            (unsigned long long)atomic_load(&s->ingress_transport_errors));
}

int tp_stats_write_json_summary(const tp_stats_t *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return TP_ERR_IO;
    uint64_t elapsed_ns = tp_now_wall_ns() - s->started_wall_ns;
    fprintf(f, "{\n\"duration_seconds\":%.3f,\n\"received\":%llu,\n\"processed\":%llu,\n"
               "\"exported\":%llu,\n\"stored\":%llu,\n\"discarded\":%llu,\n\"duplicates\":%llu,\n"
               "\"anomalies\":%llu,\n\"warnings\":%llu,\n\"errors\":%llu,\n\"dropped\":%llu,\n"
               "\"average_latency_ms\":%.3f,\n\"max_buffer_utilization_pct\":%llu,\n"
               "\"ingress_bytes\":%llu,\n\"ingress_frames\":%llu,\n\"ingress_accepted\":%llu,\n"
               "\"ingress_rejected\":%llu,\n\"ingress_dropped\":%llu,\n\"ingress_duplicates\":%llu,\n"
               "\"ingress_gaps\":%llu,\n\"ingress_out_of_order\":%llu,\n\"ingress_oversize\":%llu,\n"
               "\"ingress_truncated\":%llu,\n\"ingress_transport_errors\":%llu\n}\n",
            (double)elapsed_ns / 1000000000.0,
            (unsigned long long)atomic_load(&s->received), (unsigned long long)atomic_load(&s->processed),
            (unsigned long long)atomic_load(&s->exported), (unsigned long long)atomic_load(&s->stored),
            (unsigned long long)atomic_load(&s->discarded), (unsigned long long)atomic_load(&s->duplicates),
            (unsigned long long)atomic_load(&s->anomalies), (unsigned long long)atomic_load(&s->warnings),
            (unsigned long long)atomic_load(&s->errors), (unsigned long long)atomic_load(&s->dropped),
            tp_stats_average_latency_ms(s), (unsigned long long)atomic_load(&s->max_buffer_utilization_pct),
            (unsigned long long)atomic_load(&s->ingress_bytes),
            (unsigned long long)atomic_load(&s->ingress_frames),
            (unsigned long long)atomic_load(&s->ingress_accepted),
            (unsigned long long)atomic_load(&s->ingress_rejected),
            (unsigned long long)atomic_load(&s->ingress_dropped),
            (unsigned long long)atomic_load(&s->ingress_duplicates),
            (unsigned long long)atomic_load(&s->ingress_gaps),
            (unsigned long long)atomic_load(&s->ingress_out_of_order),
            (unsigned long long)atomic_load(&s->ingress_oversize),
            (unsigned long long)atomic_load(&s->ingress_truncated),
            (unsigned long long)atomic_load(&s->ingress_transport_errors));
    fclose(f);
    return TP_OK;
}
