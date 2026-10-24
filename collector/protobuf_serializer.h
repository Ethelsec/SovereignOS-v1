#pragma once
#include "types.h"

int tp_otlp_json_serialize(const tp_event_t *event, char *out, size_t out_len);
int tp_binary_serialize(const tp_event_t *event, uint8_t *out, size_t out_len, size_t *written);
int tp_binary_deserialize(const uint8_t *buf, size_t len, tp_event_t *event);
