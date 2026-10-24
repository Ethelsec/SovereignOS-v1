#include "memory_pool.h"

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_memory_pool(void)
{
    tp_memory_pool_t pool;
    CHECK(tp_memory_pool_init(&pool, 32, 2) == TP_OK);
    void *a = tp_memory_pool_acquire(&pool);
    void *b = tp_memory_pool_acquire(&pool);
    void *c = tp_memory_pool_acquire(&pool);
    CHECK(a && b && !c);
    tp_memory_pool_release(&pool, a);
    CHECK(tp_memory_pool_acquire(&pool) != NULL);
    tp_memory_pool_destroy(&pool);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_memory_pool(); }
#endif
