#include "storage.h"
#include "constants.h"
#include "logger.h"
#include "utilities.h"
#include <errno.h>
#include <string.h>

int tp_storage_open(tp_storage_t *storage, const char *path)
{
    if (!storage || !path) return TP_ERR_INVAL;
    memset(storage, 0, sizeof(*storage));
    tp_safe_copy(storage->path, sizeof(storage->path), path);
    storage->file = fopen(path, "ab+");
    if (!storage->file) return TP_ERR_IO;
    setvbuf(storage->file, NULL, _IOFBF, 1024 * 1024);
    return TP_OK;
}

int tp_storage_write(tp_storage_t *storage, const tp_event_t *event)
{
    if (!storage || !storage->file || !event) return TP_ERR_INVAL;
    tp_storage_record_header_t h = {
        .magic = TP_STORAGE_MAGIC,
        .version = TP_STORAGE_VERSION,
        .header_size = sizeof(h),
        .sequence = event->sequence,
        .wall_ns = event->timestamp.wall_ns,
        .monotonic_ns = event->timestamp.monotonic_ns,
        .event_size = sizeof(*event),
        .crc32 = tp_crc32(event, sizeof(*event))
    };
    if (fwrite(&h, sizeof(h), 1, storage->file) != 1) return TP_ERR_IO;
    if (fwrite(event, sizeof(*event), 1, storage->file) != 1) return TP_ERR_IO;
    storage->records++;
    storage->bytes += sizeof(h) + sizeof(*event);
    if ((storage->records & 63u) == 0) fflush(storage->file);
    return TP_OK;
}

int tp_storage_close(tp_storage_t *storage)
{
    if (!storage) return TP_ERR_INVAL;
    int rc = TP_OK;
    if (storage->file) {
        if (fflush(storage->file) != 0) rc = TP_ERR_IO;
        if (fclose(storage->file) != 0) rc = TP_ERR_IO;
    }
    storage->file = NULL;
    return rc;
}

int tp_storage_replay(const char *path, int (*callback)(const tp_event_t *, void *), void *user)
{
    FILE *f = fopen(path, "rb");
    if (!f) return TP_ERR_IO;
    int rc = TP_OK;
    while (1) {
        tp_storage_record_header_t h;
        size_t n = fread(&h, 1, sizeof(h), f);
        if (n == 0 && feof(f)) break;
        if (n != sizeof(h) || h.magic != TP_STORAGE_MAGIC || h.version != TP_STORAGE_VERSION || h.event_size != sizeof(tp_event_t)) {
            rc = TP_ERR_CORRUPT;
            break;
        }
        tp_event_t event;
        if (fread(&event, 1, sizeof(event), f) != sizeof(event)) {
            rc = TP_ERR_CORRUPT;
            break;
        }
        if (tp_crc32(&event, sizeof(event)) != h.crc32) {
            rc = TP_ERR_CRC;
            break;
        }
        if (callback && callback(&event, user) != TP_OK) {
            rc = TP_ERR;
            break;
        }
    }
    fclose(f);
    return rc;
}

static int count_cb(const tp_event_t *event, void *user)
{
    (void)event;
    uint64_t *n = user;
    (*n)++;
    return TP_OK;
}

int tp_storage_verify(const char *path, uint64_t *records_out)
{
    uint64_t n = 0;
    int rc = tp_storage_replay(path, count_cb, &n);
    if (records_out) *records_out = n;
    return rc;
}
