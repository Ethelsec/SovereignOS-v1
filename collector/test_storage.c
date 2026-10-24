#include "parser.h"
#include "storage.h"
#include <stdio.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_storage(void)
{
    char path[128];
    snprintf(path, sizeof(path), "/tmp/collector-storage-%ld.tpbin", (long)getpid());
    tp_storage_t storage;
    tp_event_t event;
    CHECK(tp_parse_raw_event("file_open|test|path=/tmp/a", &event) == TP_OK);
    CHECK(tp_storage_open(&storage, path) == TP_OK);
    CHECK(tp_storage_write(&storage, &event) == TP_OK);
    CHECK(tp_storage_close(&storage) == TP_OK);
    uint64_t records = 0;
    CHECK(tp_storage_verify(path, &records) == TP_OK);
    CHECK(records == 1);
    unlink(path);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_storage(); }
#endif
