#include "decoder.h"

int tp_decoder_decode_record(const char *record, tp_event_t *event)
{
    return tp_parse_raw_event(record, event);
}
