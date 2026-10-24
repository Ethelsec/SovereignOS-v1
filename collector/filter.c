#include "filter.h"
#include "utilities.h"
#include <string.h>

void tp_filter_init(tp_filter_t *filter, unsigned window_ms)
{
    memset(filter, 0, sizeof(*filter));
    filter->window_ms = window_ms ? window_ms : 1000;
}

int tp_filter_should_keep(tp_filter_t *filter, const tp_config_t *cfg, const tp_event_t *event, int *duplicate)
{
    if (!filter || !event) return 0;
    if (duplicate) *duplicate = 0;
    uint64_t now = event->timestamp.monotonic_ns;
    uint32_t hash = tp_hash_event_identity(event);
    uint64_t window_ns = (uint64_t)filter->window_ms * 1000000ull;
    for (size_t i = 0; i < TP_MAX_TEXT; ++i) {
        if (filter->hashes[i] == hash && now >= filter->times[i] && now - filter->times[i] <= window_ns) {
            if (duplicate) *duplicate = 1;
            return event->severity >= TP_SEV_HIGH || event->type == TP_TYPE_SECURITY;
        }
    }
    filter->hashes[filter->next] = hash;
    filter->times[filter->next] = now;
    filter->next = (filter->next + 1u) % TP_MAX_TEXT;
    return !cfg || event->severity >= cfg->min_store_severity || event->type == TP_TYPE_SECURITY;
}
