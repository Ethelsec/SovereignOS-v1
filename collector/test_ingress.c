#include "analyzer.h"
#include "config.h"
#include "ingress.h"
#include "ring_buffer.h"
#include "statistics.h"
#include "storage.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (line %d)\n", #expr, __LINE__); return 1; } } while (0)

static int mkframe(char *buf, size_t cap, unsigned id, unsigned seq,
                   const char *type, const char *severity, const char *attrs)
{
    int n = snprintf(buf, cap,
                     "{\"schema_version\":\"1\",\"event_id\":\"evt-%u\","
                     "\"timestamp_utc\":\"2026-10-23T12:00:00Z\","
                     "\"timestamp_monotonic_ns\":%u,\"source\":\"replay\","
                     "\"event_type\":\"%s\",\"severity\":\"%s\",\"host_id\":\"guest-01\","
                     "\"process\":{\"pid\":100,\"ppid\":1,\"executable\":\"/usr/bin/demo\"},"
                     "\"network\":null,\"attributes\":%s,\"sequence\":%u}",
                     id, 1000000000u + id, type, severity, attrs, seq);
    return n > 0 && (size_t)n < cap;
}

static void path_for(char *out, size_t cap, const char *suffix)
{
    snprintf(out, cap, "/tmp/collector-ingress-%ld-%s", (long)getpid(), suffix);
}

/* One file: two valid events, one blank line, CRLF handling. */
static int test_file_framing(void)
{
    char path[160], f1[1024], f2[1024];
    path_for(path, sizeof(path), "framing.jsonl");
    CHECK(mkframe(f1, sizeof(f1), 1, 1, "process.start", "info", "{}"));
    CHECK(mkframe(f2, sizeof(f2), 2, 2, "process.exit", "info", "{}"));

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs(f1, f);
    fputs("\n\n", f);
    fputs(f2, f);
    fputs("\r\n", f);
    fclose(f);

    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 8) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);
    CHECK(tp_ingress_run_file(&ing, path) == TP_OK);

    CHECK(tp_stats_get(&stats.ingress_bytes) == (uint64_t)(strlen(f1) + strlen(f2) + 4));
    CHECK(tp_stats_get(&stats.ingress_frames) == 2);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 2);
    CHECK(tp_stats_get(&stats.ingress_rejected) == 0);
    CHECK(tp_stats_get(&stats.ingress_duplicates) == 0);
    CHECK(tp_stats_get(&stats.ingress_truncated) == 0);
    CHECK(tp_ring_size(&ring) == 2);

    tp_event_t first, second;
    CHECK(tp_ring_pop(&ring, &first) == TP_OK);
    CHECK(tp_ring_pop(&ring, &second) == TP_OK);
    CHECK(first.sequence == 1);
    CHECK(second.sequence == 2);
    CHECK(first.type == TP_TYPE_PROCESS_START);
    CHECK(second.type == TP_TYPE_PROCESS_EXIT);
    CHECK(first.pid == 100);
    CHECK(strcmp(first.process_name, "demo") == 0);
    CHECK(strcmp(first.source_probe, "replay") == 0);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    unlink(path);
    return 0;
}

/* Oversize line is discarded, malformed line is rejected, final partial frame
 * is counted as truncated, and nothing crashes. */
static int test_framing_errors(void)
{
    char path[160], good[1024];
    path_for(path, sizeof(path), "errors.jsonl");
    CHECK(mkframe(good, sizeof(good), 3, 1, "demo.marker", "info", "{}"));

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs(good, f);
    fputc('\n', f);
    for (unsigned i = 0; i < TP_CANONICAL_MAX_FRAME + 100u; ++i) fputc('x', f);
    fputc('\n', f);
    fputs("{\"schema_version\":\"1\"", f);
    fputc('\n', f);
    fputs("{\"schema_version\":\"1\"", f); /* no newline: truncated */
    fclose(f);

    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 8) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);
    CHECK(tp_ingress_run_file(&ing, path) == TP_OK);

    CHECK(tp_stats_get(&stats.ingress_oversize) == 1);
    CHECK(tp_stats_get(&stats.ingress_truncated) == 1);
    CHECK(tp_stats_get(&stats.ingress_rejected) == 1);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 1);
    CHECK(tp_stats_get(&stats.ingress_frames) == 2); /* good + malformed */
    CHECK(tp_ring_size(&ring) == 1);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    unlink(path);
    return 0;
}

/* Duplicate IDs are caught inside the window; producer gaps and late frames
 * are counted but never silently repaired. */
static int test_duplicates_and_sequences(void)
{
    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 16) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);

    char a[1024], b[1024], c[1024], d[1024];
    CHECK(mkframe(a, sizeof(a), 10, 10, "process.start", "info", "{}"));
    CHECK(mkframe(b, sizeof(b), 11, 13, "process.start", "info", "{}"));
    CHECK(mkframe(c, sizeof(c), 12, 12, "process.start", "info", "{}"));
    CHECK(mkframe(d, sizeof(d), 13, 14, "process.start", "info", "{}"));

    char reason[TP_CANONICAL_MAX_REASON];
    CHECK(tp_ingress_handle_frame(&ing, a, strlen(a), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_ingress_handle_frame(&ing, a, strlen(a), reason, sizeof(reason)) == TP_INGRESS_DUPLICATE);
    CHECK(tp_ingress_handle_frame(&ing, b, strlen(b), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_ingress_handle_frame(&ing, c, strlen(c), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_ingress_handle_frame(&ing, d, strlen(d), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);

    CHECK(tp_stats_get(&stats.ingress_frames) == 5);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 4);
    CHECK(tp_stats_get(&stats.ingress_duplicates) == 1);
    CHECK(tp_stats_get(&stats.ingress_gaps) == 2); /* 11 and 12 never arrived before 13 */
    CHECK(tp_stats_get(&stats.ingress_out_of_order) == 1);
    CHECK(tp_stats_get(&stats.ingress_rejected) == 0);
    CHECK(tp_ring_size(&ring) == 4);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    return 0;
}

/* Queue-full policy is documented drop-newest; the event ID is not burned so a
 * producer retry can still land. */
static int test_queue_full(void)
{
    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 2) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);

    char a[1024], b[1024], c[1024];
    CHECK(mkframe(a, sizeof(a), 20, 1, "process.start", "info", "{}"));
    CHECK(mkframe(b, sizeof(b), 21, 2, "process.start", "info", "{}"));
    CHECK(mkframe(c, sizeof(c), 22, 3, "process.start", "info", "{}"));

    char reason[TP_CANONICAL_MAX_REASON];
    CHECK(tp_ingress_handle_frame(&ing, a, strlen(a), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_ingress_handle_frame(&ing, b, strlen(b), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_ingress_handle_frame(&ing, c, strlen(c), reason, sizeof(reason)) == TP_INGRESS_QUEUE_FULL);
    CHECK(tp_stats_get(&stats.ingress_dropped) == 1);
    CHECK(tp_stats_get(&stats.dropped) == 1);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 2);

    tp_event_t drained;
    CHECK(tp_ring_pop(&ring, &drained) == TP_OK);
    CHECK(tp_ingress_handle_frame(&ing, c, strlen(c), reason, sizeof(reason)) == TP_INGRESS_ACCEPTED);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 3);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    return 0;
}

/* AF_UNIX stream: split write proves framing does not assume one read == one
 * frame, and shutdown removes the socket path. */
static int test_unix_socket(void)
{
    char path[100], f1[1024], f2[1024];
    path_for(path, sizeof(path), "ingest.sock");
    unlink(path);
    CHECK(mkframe(f1, sizeof(f1), 30, 1, "process.start", "info", "{}"));
    CHECK(mkframe(f2, sizeof(f2), 31, 2, "process.exit", "info", "{}"));

    volatile sig_atomic_t running = 1;
    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_config_t cfg;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 8) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, &running) == TP_OK);
    tp_config_defaults(&cfg);
    snprintf(cfg.ingest_path, sizeof(cfg.ingest_path), "%s", path);
    cfg.ingest_is_socket = true;
    CHECK(tp_ingress_start(&ing, &cfg) == TP_OK);

    int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(cfd >= 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    CHECK(connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    size_t half = strlen(f1) / 2;
    CHECK(write(cfd, f1, half) == (ssize_t)half);
    usleep(20000);
    CHECK(write(cfd, f1 + half, strlen(f1) - half) == (ssize_t)(strlen(f1) - half));
    CHECK(write(cfd, "\n", 1) == 1);
    CHECK(write(cfd, f2, strlen(f2)) == (ssize_t)strlen(f2));
    CHECK(write(cfd, "\n", 1) == 1);
    shutdown(cfd, SHUT_WR);
    close(cfd);

    for (int i = 0; i < 200 && tp_stats_get(&stats.ingress_accepted) < 2; ++i) usleep(10000);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 2);
    CHECK(tp_stats_get(&stats.ingress_frames) == 2);
    CHECK(tp_stats_get(&stats.ingress_truncated) == 0);
    CHECK(tp_ring_size(&ring) == 2);

    running = 0;
    tp_ingress_stop(&ing);
    CHECK(access(path, F_OK) != 0); /* socket file removed */
    tp_ring_destroy(&ring);
    return 0;
}

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

/* Four consumers pop a pre-filled ring. Every slot must be delivered exactly
 * once: no duplicates, no zeroed events, no lost sequence numbers. */
#define RACE_EVENTS 4000u

typedef struct {
    tp_ring_buffer_t *ring;
    _Atomic uint64_t *seen;
    _Atomic uint64_t total;
    _Atomic uint64_t bad;
} race_ctx_t;

static void *race_consumer(void *arg)
{
    race_ctx_t *ctx = arg;
    tp_event_t event;
    for (;;) {
        int rc = tp_ring_pop(ctx->ring, &event);
        if (rc == TP_ERR_EMPTY) break;
        if (rc != TP_OK || event.sequence == 0 || event.sequence > RACE_EVENTS) {
            atomic_fetch_add(&ctx->bad, 1);
            continue;
        }
        if (atomic_fetch_add(&ctx->seen[event.sequence - 1], 1) != 0) {
            atomic_fetch_add(&ctx->bad, 1);
        }
        atomic_fetch_add(&ctx->total, 1);
    }
    return NULL;
}

static int test_concurrent_consumers(void)
{
    tp_ring_buffer_t ring;
    CHECK(tp_ring_init(&ring, 4096) == TP_OK);

    for (unsigned i = 0; i < RACE_EVENTS; ++i) {
        tp_event_t event;
        memset(&event, 0, sizeof(event));
        event.type = TP_TYPE_CUSTOM;
        event.severity = TP_SEV_INFO;
        event.pid = 1;
        event.event_id = i + 1u;
        event.payload_size = (uint32_t)snprintf((char *)event.payload, sizeof(event.payload), "n=%u", i);
        CHECK(tp_ring_push(&ring, &event) == TP_OK);
    }

    _Atomic uint64_t seen[RACE_EVENTS];
    for (unsigned i = 0; i < RACE_EVENTS; ++i) atomic_store(&seen[i], 0);

    race_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ring = &ring;
    ctx.seen = seen;

    pthread_t threads[4];
    for (unsigned i = 0; i < 4; ++i) CHECK(pthread_create(&threads[i], NULL, race_consumer, &ctx) == 0);
    for (unsigned i = 0; i < 4; ++i) pthread_join(threads[i], NULL);

    CHECK(tp_stats_get(&ctx.total) == RACE_EVENTS);
    CHECK(tp_stats_get(&ctx.bad) == 0);
    for (unsigned i = 0; i < RACE_EVENTS; ++i) CHECK(atomic_load(&seen[i]) == 1);
    CHECK(tp_ring_size(&ring) == 0);
    CHECK(tp_ring_dropped(&ring) == 0);

    tp_ring_destroy(&ring);
    return 0;
}

/* Missing source file is a clear transport error, not silent zero coverage. */
static int test_missing_source(void)
{
    char path[160];
    path_for(path, sizeof(path), "does-not-exist.jsonl");
    unlink(path);

    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 4) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);
    CHECK(tp_ingress_run_file(&ing, path) == TP_ERR_IO);
    CHECK(tp_stats_get(&stats.ingress_transport_errors) == 1);
    CHECK(tp_stats_get(&stats.ingress_frames) == 0);
    CHECK(tp_ring_size(&ring) == 0);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    return 0;
}

/* Simulated consumer restart: dedup/sequence state is in memory, so a fresh
 * ingress object re-delivers the same file (documented at-least-once replay). */
static int test_receiver_restart(void)
{
    char path[160], frame[1024];
    path_for(path, sizeof(path), "restart.jsonl");
    CHECK(mkframe(frame, sizeof(frame), 50, 1, "demo.marker", "info", "{}"));

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs(frame, f);
    fputc('\n', f);
    fclose(f);

    for (int run = 0; run < 2; ++run) {
        tp_ring_buffer_t ring;
        tp_stats_t stats;
        tp_ingress_t ing;
        tp_stats_init(&stats);
        CHECK(tp_ring_init(&ring, 4) == TP_OK);
        CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);
        CHECK(tp_ingress_run_file(&ing, path) == TP_OK);
        CHECK(tp_stats_get(&stats.ingress_accepted) == 1);
        CHECK(tp_stats_get(&stats.ingress_frames) == 1);
        CHECK(tp_stats_get(&stats.ingress_duplicates) == 0);
        CHECK(tp_ring_size(&ring) == 1);
        tp_ingress_close(&ing);
        tp_ring_destroy(&ring);
    }

    unlink(path);
    return 0;
}

/* Decode -> validate -> analyze -> storage -> verify -> replay, with one benign
 * and one suspicious event. */
static int test_end_to_end(void)
{
    char jsonl[160], store[160], normal[1024], suspicious[1024];
    path_for(jsonl, sizeof(jsonl), "e2e.jsonl");
    path_for(store, sizeof(store), "e2e.tpbin");
    unlink(store);

    CHECK(mkframe(normal, sizeof(normal), 40, 1, "process.start", "info",
                  "{\"component\":\"demo\"}"));
    CHECK(mkframe(suspicious, sizeof(suspicious), 41, 2, "auth.failure", "high",
                  "{\"detail\":\"auth failed for user=root\"}"));

    FILE *f = fopen(jsonl, "w");
    CHECK(f != NULL);
    fputs(normal, f);
    fputc('\n', f);
    fputs(suspicious, f);
    fputc('\n', f);
    fclose(f);

    tp_ring_buffer_t ring;
    tp_stats_t stats;
    tp_ingress_t ing;
    tp_stats_init(&stats);
    CHECK(tp_ring_init(&ring, 16) == TP_OK);
    CHECK(tp_ingress_init(&ing, &ring, &stats, NULL) == TP_OK);
    CHECK(tp_ingress_run_file(&ing, jsonl) == TP_OK);
    CHECK(tp_stats_get(&stats.ingress_accepted) == 2);

    tp_analyzer_t analyzer;
    tp_analyzer_init(&analyzer);
    tp_storage_t storage;
    CHECK(tp_storage_open(&storage, store) == TP_OK);

    tp_event_t benign, hostile;
    CHECK(tp_ring_pop(&ring, &benign) == TP_OK);
    CHECK(tp_ring_pop(&ring, &hostile) == TP_OK);

    double confidence = 0.0;
    int anomalous = 0;
    CHECK(tp_analyzer_process(&analyzer, &benign, &confidence, &anomalous) == TP_OK);
    CHECK(!anomalous);
    CHECK(benign.severity == TP_SEV_INFO);
    CHECK((benign.categories & TP_EVENT_PROCESS) != 0);
    CHECK(strstr(benign.analysis, "normal") != NULL);

    CHECK(tp_analyzer_process(&analyzer, &hostile, &confidence, &anomalous) == TP_OK);
    CHECK(anomalous);
    CHECK(confidence >= 0.69);
    CHECK(hostile.severity >= TP_SEV_HIGH);
    CHECK(hostile.categories == TP_EVENT_SECURITY);
    CHECK(strstr(hostile.analysis, "anomaly.confidence=") != NULL);

    CHECK(tp_storage_write(&storage, &benign) == TP_OK);
    CHECK(tp_storage_write(&storage, &hostile) == TP_OK);
    CHECK(tp_storage_close(&storage) == TP_OK);

    uint64_t records = 0;
    CHECK(tp_storage_verify(store, &records) == TP_OK);
    CHECK(records == 2);

    replay_state_t replay;
    memset(&replay, 0, sizeof(replay));
    CHECK(tp_storage_replay(store, capture_cb, &replay) == TP_OK);
    CHECK(replay.count == 2);
    CHECK(replay.last.type == TP_TYPE_SECURITY);
    CHECK(replay.last.payload_size == hostile.payload_size);
    CHECK(memcmp(replay.last.payload, hostile.payload, hostile.payload_size) == 0);

    tp_ingress_close(&ing);
    tp_ring_destroy(&ring);
    unlink(jsonl);
    unlink(store);
    return 0;
}

int tp_test_ingress(void)
{
    if (test_file_framing()) return 1;
    if (test_framing_errors()) return 1;
    if (test_duplicates_and_sequences()) return 1;
    if (test_queue_full()) return 1;
    if (test_concurrent_consumers()) return 1;
    if (test_missing_source()) return 1;
    if (test_receiver_restart()) return 1;
    if (test_unix_socket()) return 1;
    if (test_end_to_end()) return 1;
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_ingress(); }
#endif
