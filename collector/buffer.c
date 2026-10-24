#include "buffer.h"

size_t tp_buffer_pending(const tp_buffer_t *buffer)
{
    return tp_ring_size(buffer);
}
