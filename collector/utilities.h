#pragma once

#include "types.h"
#include <stddef.h>
#include <stdint.h>

uint64_t tp_now_monotonic_ns(void);
uint64_t tp_now_wall_ns(void);
tp_timestamp_t tp_timestamp_now(void);
uint32_t tp_crc32(const void *data, size_t len);
void tp_make_id_hex(char *out, size_t len, uint64_t a, uint64_t b);
int tp_get_hostname(char *out, size_t len);
int tp_read_first_line(const char *path, char *out, size_t len);
double tp_timespec_diff_ms(uint64_t start_ns, uint64_t end_ns);
const char *tp_event_type_name(tp_event_type_t type);
const char *tp_severity_name(tp_severity_t severity);
uint32_t tp_hash_event_identity(const tp_event_t *event);
void tp_safe_copy(char *dst, size_t dst_len, const char *src);
int tp_parse_u64(const char *text, uint64_t *value);
