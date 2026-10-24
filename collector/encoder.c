#include "encoder.h"

int tp_encoder_encode_json(const tp_event_t *event, char *out, size_t out_len)
{
    return tp_otlp_json_serialize(event, out, out_len);
}
