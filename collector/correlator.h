#pragma once
#include "constants.h"
#include "types.h"
#include <pthread.h>

typedef struct {
    pid_t pid;
    pid_t ppid;
    uint64_t first_seen_ns;
    uint64_t last_seen_ns;
    uint64_t events;
    uint64_t bytes_io;
    char name[64];
    char executable[TP_MAX_PATH];
} tp_process_timeline_t;

typedef struct {
    pthread_mutex_t lock;
    tp_process_timeline_t processes[TP_MAX_PROCESSES];
    size_t process_count;
    uint64_t correlations;
} tp_correlator_t;

int tp_correlator_init(tp_correlator_t *corr);
void tp_correlator_destroy(tp_correlator_t *corr);
int tp_correlator_process(tp_correlator_t *corr, tp_event_t *event);
void tp_correlator_write_report(tp_correlator_t *corr, FILE *out);
