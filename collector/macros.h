#pragma once

#include <errno.h>
#include <stddef.h>

#define TP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define TP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define TP_ALIGN_UP(v, a) (((v) + ((a) - 1u)) & ~((a) - 1u))
#define TP_UNUSED(x) ((void)(x))
#define TP_RETRY_EINTR(expr) ({ typeof(expr) _r; do { _r = (expr); } while (_r == -1 && errno == EINTR); _r; })
