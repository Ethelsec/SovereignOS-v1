#include "file_reader.h"

int tp_file_reader_verify(const char *path, uint64_t *records)
{
    return tp_storage_verify(path, records);
}
