#pragma once

#include "types.h"
#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    _Atomic uint64_t sequence;
    _Atomic uint32_t state;
    uint32_t crc32;
    uint32_t payload_size;
    tp_timestamp_t timestamp;
    tp_event_t event;
} tp_ring_slot_t;

typedef struct {
    size_t capacity;
    tp_ring_slot_t *slots;
    _Atomic uint64_t write_seq;
    _Atomic uint64_t read_seq;
    _Atomic uint64_t dropped;
    _Atomic uint64_t corrupt;
    pthread_mutex_t producer_lock;
} tp_ring_buffer_t;

int tp_ring_init(tp_ring_buffer_t *ring, size_t capacity);
void tp_ring_destroy(tp_ring_buffer_t *ring);
int tp_ring_push(tp_ring_buffer_t *ring, const tp_event_t *event);
int tp_ring_pop(tp_ring_buffer_t *ring, tp_event_t *event);
size_t tp_ring_size(const tp_ring_buffer_t *ring);
uint64_t tp_ring_dropped(const tp_ring_buffer_t *ring);
unsigned tp_ring_utilization_pct(const tp_ring_buffer_t *ring);
