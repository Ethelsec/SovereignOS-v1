#pragma once
#include "types.h"

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t sequence;
    uint64_t wall_ns;
    uint64_t monotonic_ns;
    uint32_t event_size;
    uint32_t crc32;
} tp_storage_record_header_t;

typedef struct {
    FILE *file;
    char path[TP_MAX_PATH];
    uint64_t records;
    uint64_t bytes;
} tp_storage_t;

int tp_storage_open(tp_storage_t *storage, const char *path);
int tp_storage_write(tp_storage_t *storage, const tp_event_t *event);
int tp_storage_close(tp_storage_t *storage);
int tp_storage_verify(const char *path, uint64_t *records_out);
int tp_storage_replay(const char *path, int (*callback)(const tp_event_t *, void *), void *user);
