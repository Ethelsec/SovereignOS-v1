#pragma once
#include "ring_buffer.h"
#include "types.h"

int tp_health_check(tp_health_t *health, const tp_ring_buffer_t *ring, const tp_config_t *cfg);
