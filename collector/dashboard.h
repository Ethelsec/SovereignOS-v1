#pragma once
#include "ring_buffer.h"
#include "statistics.h"
#include "types.h"

void tp_dashboard_render(const tp_config_t *cfg, const tp_stats_t *stats, const tp_ring_buffer_t *ring, const tp_health_t *health);
