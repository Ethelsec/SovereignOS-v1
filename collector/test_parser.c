#include "normalizer.h"
#include "parser.h"

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_parser(void)
{
    tp_event_t event;
    CHECK(tp_parse_raw_event("socket_connect|curl|remote=127.0.0.1,port=4317", &event) == TP_OK);
    CHECK(event.type == TP_TYPE_SOCKET_CONNECT);
    CHECK(tp_normalize_event(&event) == TP_OK);
    CHECK(event.categories & TP_EVENT_NETWORK);
    CHECK(event.trace_id[0] != '\0');
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_parser(); }
#endif
