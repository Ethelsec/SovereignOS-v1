#pragma once
#include "correlator.h"

typedef tp_correlator_t tp_process_tracker_t;
static inline int tp_process_tracker_init(tp_process_tracker_t *tracker) { return tp_correlator_init(tracker); }
static inline int tp_process_tracker_update(tp_process_tracker_t *tracker, tp_event_t *event) { return tp_correlator_process(tracker, event); }
static inline void tp_process_tracker_destroy(tp_process_tracker_t *tracker) { tp_correlator_destroy(tracker); }
