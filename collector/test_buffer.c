#include "ring_buffer.h"
#include "parser.h"
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_buffer(void)
{
    tp_ring_buffer_t ring;
    CHECK(tp_ring_init(&ring, 4) == TP_OK);
    tp_event_t in, out;
    CHECK(tp_parse_raw_event("process_start|test|hello", &in) == TP_OK);
    CHECK(tp_ring_push(&ring, &in) == TP_OK);
    CHECK(tp_ring_pop(&ring, &out) == TP_OK);
    CHECK(out.sequence == 1);
    CHECK(out.payload_size == in.payload_size);
    tp_ring_destroy(&ring);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_buffer(); }
#endif
