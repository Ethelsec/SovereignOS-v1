#include "ring_buffer.h"
#include "constants.h"
#include "logger.h"
#include "utilities.h"
#include <stdlib.h>
#include <string.h>

enum { TP_SLOT_EMPTY = 0, TP_SLOT_WRITING = 1, TP_SLOT_READY = 2, TP_SLOT_READING = 3 };

int tp_ring_init(tp_ring_buffer_t *ring, size_t capacity)
{
    if (!ring || capacity < 2) return TP_ERR_INVAL;
    memset(ring, 0, sizeof(*ring));
    ring->capacity = capacity;
    ring->slots = calloc(capacity, sizeof(*ring->slots));
    if (!ring->slots) return TP_ERR_NOMEM;
    if (pthread_mutex_init(&ring->producer_lock, NULL) != 0) {
        free(ring->slots);
        return TP_ERR;
    }
    for (size_t i = 0; i < capacity; ++i) {
        atomic_store(&ring->slots[i].state, TP_SLOT_EMPTY);
        atomic_store(&ring->slots[i].sequence, i);
    }
    return TP_OK;
}

void tp_ring_destroy(tp_ring_buffer_t *ring)
{
    if (!ring) return;
    pthread_mutex_destroy(&ring->producer_lock);
    free(ring->slots);
    memset(ring, 0, sizeof(*ring));
}

int tp_ring_push(tp_ring_buffer_t *ring, const tp_event_t *event)
{
    if (!ring || !event || event->payload_size > TP_MAX_PAYLOAD) return TP_ERR_INVAL;
    pthread_mutex_lock(&ring->producer_lock);
    uint64_t w = atomic_load_explicit(&ring->write_seq, memory_order_relaxed);
    uint64_t r = atomic_load_explicit(&ring->read_seq, memory_order_acquire);
    if (w - r >= ring->capacity) {
        atomic_fetch_add(&ring->dropped, 1);
        pthread_mutex_unlock(&ring->producer_lock);
        return TP_ERR_FULL;
    }
    tp_ring_slot_t *slot = &ring->slots[w % ring->capacity];
    atomic_store_explicit(&slot->state, TP_SLOT_WRITING, memory_order_release);
    slot->event = *event;
    slot->event.sequence = w + 1;
    slot->timestamp = event->timestamp;
    slot->payload_size = event->payload_size;
    slot->crc32 = tp_crc32(&slot->event, offsetof(tp_event_t, payload) + slot->event.payload_size);
    atomic_store_explicit(&slot->sequence, w + 1, memory_order_release);
    atomic_store_explicit(&slot->state, TP_SLOT_READY, memory_order_release);
    atomic_store_explicit(&ring->write_seq, w + 1, memory_order_release);
    pthread_mutex_unlock(&ring->producer_lock);
    return TP_OK;
}

int tp_ring_pop(tp_ring_buffer_t *ring, tp_event_t *event)
{
    if (!ring || !event) return TP_ERR_INVAL;
    for (;;) {
        uint64_t r = atomic_load_explicit(&ring->read_seq, memory_order_relaxed);
        uint64_t w = atomic_load_explicit(&ring->write_seq, memory_order_acquire);
        if (r >= w) return TP_ERR_EMPTY;
        /* Claim the slot before copying. Without this CAS, two workers can pop
         * the same slot, duplicating or zeroing an event. */
        if (!atomic_compare_exchange_strong_explicit(&ring->read_seq, &r, r + 1,
                                                    memory_order_acq_rel, memory_order_relaxed)) {
            continue;
        }
        tp_ring_slot_t *slot = &ring->slots[r % ring->capacity];
        uint32_t ready = atomic_load_explicit(&slot->state, memory_order_acquire);
        uint64_t seq = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        if (ready != TP_SLOT_READY || seq != r + 1) {
            atomic_fetch_add(&ring->corrupt, 1);
            return TP_ERR_CORRUPT;
        }
        atomic_store_explicit(&slot->state, TP_SLOT_READING, memory_order_release);
        uint32_t crc = tp_crc32(&slot->event, offsetof(tp_event_t, payload) + slot->event.payload_size);
        if (crc != slot->crc32) {
            atomic_fetch_add(&ring->corrupt, 1);
            atomic_store_explicit(&slot->state, TP_SLOT_EMPTY, memory_order_release);
            return TP_ERR_CRC;
        }
        *event = slot->event;
        memset(&slot->event, 0, sizeof(slot->event));
        atomic_store_explicit(&slot->state, TP_SLOT_EMPTY, memory_order_release);
        return TP_OK;
    }
}

size_t tp_ring_size(const tp_ring_buffer_t *ring)
{
    if (!ring) return 0;
    uint64_t w = atomic_load_explicit(&ring->write_seq, memory_order_acquire);
    uint64_t r = atomic_load_explicit(&ring->read_seq, memory_order_acquire);
    return (size_t)(w >= r ? w - r : 0);
}

uint64_t tp_ring_dropped(const tp_ring_buffer_t *ring)
{
    return ring ? atomic_load(&ring->dropped) : 0;
}

unsigned tp_ring_utilization_pct(const tp_ring_buffer_t *ring)
{
    if (!ring || ring->capacity == 0) return 0;
    return (unsigned)((tp_ring_size(ring) * 100u) / ring->capacity);
}
