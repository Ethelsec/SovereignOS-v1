/*
 * Distinct shared buffer for Guest OS and seL4 communication
 * Lock-free ring buffer with explicit framing
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>

#define GS_BUFFER_SIZE 4096
#define GS_MAX_FRAME_SIZE 2048
#define GS_MAGIC 0x47534559 /* "GSEY" */

typedef struct {
    uint32_t magic;
    uint32_t frame_len;
    uint64_t seq;
    uint64_t timestamp_ns;
    uint8_t source; /* 0=guest, 1=sel4 */
    uint8_t reserved[3];
    uint8_t data[GS_MAX_FRAME_SIZE];
    uint32_t crc32;
} gs_frame_t;

typedef struct {
    atomic_ulong head;
    atomic_ulong tail;
    gs_frame_t frames[GS_BUFFER_SIZE];
    pthread_mutex_t lock;
    atomic_ulong drops;
    atomic_ulong pushes_guest;
    atomic_ulong pushes_sel4;
    atomic_ulong pops_guest;
    atomic_ulong pops_sel4;
} gs_shared_buffer_t;

static gs_shared_buffer_t g_buffer;

void gs_buffer_init() {
    memset(&g_buffer, 0, sizeof(g_buffer));
    atomic_init(&g_buffer.head, 0);
    atomic_init(&g_buffer.tail, 0);
    atomic_init(&g_buffer.drops, 0);
    pthread_mutex_init(&g_buffer.lock, NULL);
    printf("[SHARED-BUF] Guest<->seL4 shared buffer initialized (size=%d)\n", GS_BUFFER_SIZE);
}

int gs_buffer_push_guest(const void *data, size_t len) {
    if (len > GS_MAX_FRAME_SIZE) return -1;
    
    unsigned long head = atomic_load_explicit(&g_buffer.head, memory_order_acquire);
    unsigned long tail = atomic_load_explicit(&g_buffer.tail, memory_order_acquire);
    
    if ((head + 1) % GS_BUFFER_SIZE == tail) {
        atomic_fetch_add(&g_buffer.drops, 1);
        return -1; /* full */
    }
    
    gs_frame_t *frame = &g_buffer.frames[head];
    frame->magic = GS_MAGIC;
    frame->frame_len = (uint32_t)len;
    frame->seq = atomic_fetch_add(&g_buffer.pushes_guest, 1);
    frame->timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;
    frame->source = 0; /* guest */
    memcpy(frame->data, data, len);
    frame->crc32 = 0; /* simplified */
    
    atomic_store_explicit(&g_buffer.head, (head + 1) % GS_BUFFER_SIZE, memory_order_release);
    return 0;
}

int gs_buffer_push_sel4(const void *data, size_t len) {
    if (len > GS_MAX_FRAME_SIZE) return -1;
    
    unsigned long head = atomic_load_explicit(&g_buffer.head, memory_order_acquire);
    unsigned long tail = atomic_load_explicit(&g_buffer.tail, memory_order_acquire);
    
    if ((head + 1) % GS_BUFFER_SIZE == tail) {
        atomic_fetch_add(&g_buffer.drops, 1);
        return -1;
    }
    
    gs_frame_t *frame = &g_buffer.frames[head];
    frame->magic = GS_MAGIC;
    frame->frame_len = (uint32_t)len;
    frame->seq = atomic_fetch_add(&g_buffer.pushes_sel4, 1);
    frame->timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;
    frame->source = 1; /* sel4 */
    memcpy(frame->data, data, len);
    
    atomic_store_explicit(&g_buffer.head, (head + 1) % GS_BUFFER_SIZE, memory_order_release);
    return 0;
}

int gs_buffer_pop_guest(void *out, size_t *out_len) {
    unsigned long tail = atomic_load_explicit(&g_buffer.tail, memory_order_acquire);
    unsigned long head = atomic_load_explicit(&g_buffer.head, memory_order_acquire);
    
    if (tail == head) return -1; /* empty */
    
    gs_frame_t *frame = &g_buffer.frames[tail];
    if (frame->magic != GS_MAGIC) return -1;
    if (frame->source != 1) return -1; /* only pop from sel4 */
    
    *out_len = frame->frame_len;
    memcpy(out, frame->data, frame->frame_len);
    
    atomic_store_explicit(&g_buffer.tail, (tail + 1) % GS_BUFFER_SIZE, memory_order_release);
    atomic_fetch_add(&g_buffer.pops_guest, 1);
    return 0;
}

int gs_buffer_pop_sel4(void *out, size_t *out_len) {
    unsigned long tail = atomic_load_explicit(&g_buffer.tail, memory_order_acquire);
    unsigned long head = atomic_load_explicit(&g_buffer.head, memory_order_acquire);
    
    if (tail == head) return -1;
    
    gs_frame_t *frame = &g_buffer.frames[tail];
    if (frame->magic != GS_MAGIC) return -1;
    if (frame->source != 0) return -1; /* only pop from guest */
    
    *out_len = frame->frame_len;
    memcpy(out, frame->data, frame->frame_len);
    
    atomic_store_explicit(&g_buffer.tail, (tail + 1) % GS_BUFFER_SIZE, memory_order_release);
    atomic_fetch_add(&g_buffer.pops_sel4, 1);
    return 0;
}

void gs_buffer_stats() {
    printf("\n=== Guest<->seL4 Shared Buffer Stats ===\n");
    printf("Pushes from Guest: %lu\n", atomic_load(&g_buffer.pushes_guest));
    printf("Pushes from seL4:  %lu\n", atomic_load(&g_buffer.pushes_sel4));
    printf("Pops by Guest:     %lu\n", atomic_load(&g_buffer.pops_guest));
    printf("Pops by seL4:      %lu\n", atomic_load(&g_buffer.pops_sel4));
    printf("Drops:             %lu\n", atomic_load(&g_buffer.drops));
    printf("=========================================\n\n");
}
