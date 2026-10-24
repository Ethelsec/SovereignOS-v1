#pragma once
#include "types.h"

void tp_stats_init(tp_stats_t *stats);
void tp_stats_inc(_Atomic uint64_t *field);
void tp_stats_add(_Atomic uint64_t *field, uint64_t value);
uint64_t tp_stats_get(const _Atomic uint64_t *field);
double tp_stats_average_latency_ms(const tp_stats_t *stats);
void tp_stats_update_buffer_peak(tp_stats_t *stats, unsigned utilization_pct);
void tp_stats_print_summary(const tp_stats_t *stats, FILE *out);
int tp_stats_write_json_summary(const tp_stats_t *stats, const char *path);
