#include "utilities.h"
#include <string.h>

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

int tp_test_utilities(void)
{
    char host[64];
    CHECK(tp_now_monotonic_ns() > 0);
    CHECK(tp_crc32("abc", 3) == 0x352441c2u);
    CHECK(tp_get_hostname(host, sizeof(host)) == TP_OK || strlen(host) > 0);
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_utilities(); }
#endif
