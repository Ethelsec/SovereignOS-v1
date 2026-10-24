#pragma once

#include "types.h"

int tp_parse_raw_event(const char *record, tp_event_t *event);
int tp_validate_event(const tp_event_t *event);
int tp_decode_payload_text(const tp_event_t *event, char *out, size_t out_len);
