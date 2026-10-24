#pragma once
#include "ring_buffer.h"
#include "types.h"
#include <signal.h>

typedef struct {
    tp_ring_buffer_t *ring;
    volatile sig_atomic_t *running;
    unsigned interval_ms;
} tp_probe_context_t;

int tp_ebpf_probe_status(char *out, size_t out_len);
int tp_emit_startup_events(tp_ring_buffer_t *ring);
void *tp_procfs_probe_thread(void *arg);
