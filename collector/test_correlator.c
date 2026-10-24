#include "correlator.h"
#include "normalizer.h"
#include "parser.h"

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_correlator(void)
{
    tp_correlator_t corr;
    tp_event_t event;
    CHECK(tp_correlator_init(&corr) == TP_OK);
    CHECK(tp_parse_raw_event("process_start|test|start", &event) == TP_OK);
    CHECK(tp_normalize_event(&event) == TP_OK);
    CHECK(tp_correlator_process(&corr, &event) == TP_OK);
    CHECK(corr.process_count >= 1);
    tp_correlator_destroy(&corr);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_correlator(); }
#endif
