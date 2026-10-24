#include "memory_pool.h"
#include "macros.h"
#include <stdlib.h>
#include <string.h>

int tp_memory_pool_init(tp_memory_pool_t *pool, size_t object_size, size_t object_count)
{
    if (!pool || object_size == 0 || object_count == 0) return TP_ERR_INVAL;
    memset(pool, 0, sizeof(*pool));
    pool->object_size = TP_ALIGN_UP(object_size, sizeof(void *));
    pool->object_count = object_count;
    pool->free_count = object_count;
    if (pthread_mutex_init(&pool->lock, NULL) != 0) return TP_ERR;
    pool->memory = calloc(object_count, pool->object_size);
    pool->free_stack = calloc(object_count, sizeof(size_t));
    if (!pool->memory || !pool->free_stack) {
        tp_memory_pool_destroy(pool);
        return TP_ERR_NOMEM;
    }
    for (size_t i = 0; i < object_count; ++i) pool->free_stack[i] = object_count - 1u - i;
    return TP_OK;
}

void *tp_memory_pool_acquire(tp_memory_pool_t *pool)
{
    if (!pool) return NULL;
    pthread_mutex_lock(&pool->lock);
    if (pool->free_count == 0) {
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }
    size_t idx = pool->free_stack[--pool->free_count];
    void *ptr = pool->memory + idx * pool->object_size;
    pthread_mutex_unlock(&pool->lock);
    memset(ptr, 0, pool->object_size);
    return ptr;
}

void tp_memory_pool_release(tp_memory_pool_t *pool, void *ptr)
{
    if (!pool || !ptr) return;
    uint8_t *p = ptr;
    if (p < pool->memory || p >= pool->memory + pool->object_count * pool->object_size) return;
    size_t idx = (size_t)(p - pool->memory) / pool->object_size;
    pthread_mutex_lock(&pool->lock);
    if (pool->free_count < pool->object_count) pool->free_stack[pool->free_count++] = idx;
    pthread_mutex_unlock(&pool->lock);
}

void tp_memory_pool_destroy(tp_memory_pool_t *pool)
{
    if (!pool) return;
    free(pool->memory);
    free(pool->free_stack);
    pthread_mutex_destroy(&pool->lock);
    memset(pool, 0, sizeof(*pool));
}
