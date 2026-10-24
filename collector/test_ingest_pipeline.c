#include "analyzer.h"
#include "constants.h"
#include "ring_buffer.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) return 1; } while (0)

typedef struct {
    uint64_t count;
    tp_event_t last;
} replay_state_t;

static int capture_cb(const tp_event_t *event, void *user)
{
    replay_state_t *state = user;
    state->last = *event;
    state->count++;
    return TP_OK;
}

static int cleanup_path(const char *path, int rc)
{
    unlink(path);
    return rc;
}

int tp_test_ingest_pipeline(void)
{
    char path[128];
    snprintf(path, sizeof(path), "/tmp/collector-ingest-%ld.tpbin", (long)getpid());

    tp_ring_buffer_t ring;
    CHECK(tp_ring_init(&ring, 8) == TP_OK);

    tp_event_t raw;
    memset(&raw, 0, sizeof(raw));
    raw.type = TP_TYPE_SECURITY;
    raw.severity = TP_SEV_INFO;
    raw.payload_size = (uint32_t)snprintf((char *)raw.payload, sizeof(raw.payload), "auth failed for user=test");

    int rc = 0;
    if (tp_ring_push(&ring, &raw) != TP_OK) rc = 1;

    tp_event_t event;
    if (!rc && tp_ring_pop(&ring, &event) != TP_OK) rc = 1;

    tp_analyzer_t analyzer;
    tp_analyzer_init(&analyzer);
    double confidence = 0.0;
    int anomalous = 0;
    if (!rc && tp_analyzer_process(&analyzer, &event, &confidence, &anomalous) != TP_OK) rc = 1;

    if (!rc && event.version != TP_EVENT_VERSION) rc = 1;
    if (!rc && event.timestamp.monotonic_ns == 0) rc = 1;
    if (!rc && event.timestamp.wall_ns == 0) rc = 1;
    if (!rc && event.event_id == 0) rc = 1;
    if (!rc && event.sequence != 1) rc = 1;
    if (!rc && event.categories != TP_EVENT_SECURITY) rc = 1;
    if (!rc && event.correlation_id == 0) rc = 1;
    if (!rc && event.trace_id[0] == '\0') rc = 1;
    if (!rc && event.span_id[0] == '\0') rc = 1;
    if (!rc && event.resource_attributes[0] == '\0') rc = 1;
    if (!rc && event.analysis[0] == '\0') rc = 1;
    if (!rc && event.pid <= 0) rc = 1;
    if (!rc && event.source_probe[0] == '\0') rc = 1;
    if (!rc && !anomalous) rc = 1;
    if (!rc && confidence < 0.69) rc = 1;
    if (!rc && event.severity < TP_SEV_HIGH) rc = 1;

    tp_storage_t storage;
    if (!rc && tp_storage_open(&storage, path) != TP_OK) rc = 1;
    if (!rc && tp_storage_write(&storage, &event) != TP_OK) rc = 1;
    if (!rc && tp_storage_close(&storage) != TP_OK) rc = 1;

    uint64_t records = 0;
    if (!rc && tp_storage_verify(path, &records) != TP_OK) rc = 1;
    if (!rc && records != 1) rc = 1;

    replay_state_t replay;
    memset(&replay, 0, sizeof(replay));
    if (!rc && tp_storage_replay(path, capture_cb, &replay) != TP_OK) rc = 1;
    if (!rc && replay.count != 1) rc = 1;
    if (!rc && replay.last.version != TP_EVENT_VERSION) rc = 1;
    if (!rc && replay.last.categories != TP_EVENT_SECURITY) rc = 1;
    if (!rc && strcmp(replay.last.trace_id, event.trace_id) != 0) rc = 1;
    if (!rc && memcmp(replay.last.payload, event.payload, event.payload_size) != 0) rc = 1;

    tp_ring_destroy(&ring);
    return cleanup_path(path, rc);
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_ingest_pipeline(); }
#endif
