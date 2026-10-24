#pragma once
#include "ring_buffer.h"

typedef tp_ring_buffer_t tp_buffer_t;
static inline int tp_buffer_init(tp_buffer_t *buffer, size_t capacity) { return tp_ring_init(buffer, capacity); }
static inline int tp_buffer_push(tp_buffer_t *buffer, const tp_event_t *event) { return tp_ring_push(buffer, event); }
static inline int tp_buffer_pop(tp_buffer_t *buffer, tp_event_t *event) { return tp_ring_pop(buffer, event); }
static inline void tp_buffer_destroy(tp_buffer_t *buffer) { tp_ring_destroy(buffer); }
