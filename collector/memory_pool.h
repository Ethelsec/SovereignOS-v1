#pragma once

#include "types.h"
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    pthread_mutex_t lock;
    uint8_t *memory;
    size_t object_size;
    size_t object_count;
    size_t free_count;
    size_t *free_stack;
} tp_memory_pool_t;

int tp_memory_pool_init(tp_memory_pool_t *pool, size_t object_size, size_t object_count);
void *tp_memory_pool_acquire(tp_memory_pool_t *pool);
void tp_memory_pool_release(tp_memory_pool_t *pool, void *ptr);
void tp_memory_pool_destroy(tp_memory_pool_t *pool);
