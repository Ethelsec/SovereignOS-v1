#include "file_writer.h"

int tp_file_writer_append(tp_storage_t *storage, const tp_event_t *event)
{
    return tp_storage_write(storage, event);
}
