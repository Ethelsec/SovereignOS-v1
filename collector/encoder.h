#pragma once
#include "protobuf_serializer.h"

int tp_encoder_encode_json(const tp_event_t *event, char *out, size_t out_len);
