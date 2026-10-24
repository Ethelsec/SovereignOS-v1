#include "process_tracker.h"

size_t tp_process_tracker_count(tp_process_tracker_t *tracker)
{
    return tracker ? tracker->process_count : 0;
}
