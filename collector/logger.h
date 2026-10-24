#pragma once

#include "types.h"
#include <stdarg.h>

int tp_logger_init(tp_log_level_t level, int color_enabled);
void tp_logger_set_level(tp_log_level_t level);
void tp_logger_set_color(int enabled);
void tp_logger_close(void);
void tp_log(tp_log_level_t level, const char *module, uint64_t correlation_id, const char *fmt, ...);
tp_log_level_t tp_log_level_from_string(const char *text);
