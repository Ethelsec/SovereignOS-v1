#pragma once

/*
 * Canonical event ingress (God's Eye, Member A: receiver side).
 *
 * Boundary: transport bytes -> ReceivedFrame -> ValidatedEvent
 * (gods_eye_sunday_plan/02_ARCHITECTURE.md §6, 03_EVENT_CONTRACT.md §5).
 *
 * Two transports are implemented, both deliberately small:
 *
 *   file    newline-delimited JSON, read once to EOF (replay fallback)
 *   socket  AF_UNIX SOCK_STREAM, newline-delimited JSON, multiple clients
 *
 * TCP, vsock, shared memory, and seL4 virtio transports are NOT implemented.
 * See INGEST.md for the status table and limitations.
 *
 * Framing rules (03_EVENT_CONTRACT.md §5):
 *   - one frame per line, terminated by '\n'
 *   - a trailing '\r' is stripped so CRLF producers work
 *   - blank lines are ignored
 *   - frames larger than TP_CANONICAL_MAX_FRAME are discarded up to the next
 *     newline and counted, never partially parsed
 *   - a partial frame at EOF/connection close is counted as truncated and
 *     is not parsed
 */

#include "canonical.h"
#include "ring_buffer.h"
#include "types.h"
#include <pthread.h>
#include <signal.h>

#define TP_INGRESS_DEDUP_WINDOW 256u
#define TP_INGRESS_MAX_CLIENTS 8u

typedef enum {
    TP_INGRESS_NONE = 0,
    TP_INGRESS_FILE,
    TP_INGRESS_SOCKET
} tp_ingress_kind_t;

/* tp_ingress_handle_frame() positive outcomes. Negative values are errors. */
#define TP_INGRESS_ACCEPTED 0 /* same value as TP_OK */
#define TP_INGRESS_DUPLICATE 1
#define TP_INGRESS_QUEUE_FULL 2

typedef struct {
    int fd;
    char line[TP_CANONICAL_MAX_FRAME + 1];
    size_t line_len;
    int discarding;
} tp_ingress_stream_t;

typedef struct {
    uint64_t event_id;
    int used;
} tp_ingress_dedup_entry_t;

typedef struct {
    tp_ingress_kind_t kind;
    int fd;
    char path[TP_MAX_PATH];
    pthread_t thread;
    int thread_started;
    volatile sig_atomic_t *running;
    tp_ring_buffer_t *ring;
    tp_stats_t *stats;
    tp_ingress_dedup_entry_t dedup[TP_INGRESS_DEDUP_WINDOW];
    unsigned dedup_next;
    int have_sequence;
    uint64_t last_sequence;
} tp_ingress_t;

/* Initialize an ingress object. `running` is consulted by the socket thread;
 * it may be NULL for file-only use. Returns TP_ERR_INVAL on NULL ring/stats. */
int tp_ingress_init(tp_ingress_t *ing, tp_ring_buffer_t *ring, tp_stats_t *stats,
                    volatile sig_atomic_t *running);

/* Open the configured transport. Returns TP_ERR_EMPTY when no path is set. */
int tp_ingress_open(tp_ingress_t *ing, const tp_config_t *cfg);
int tp_ingress_open_file(tp_ingress_t *ing, const char *path);
int tp_ingress_open_socket(tp_ingress_t *ing, const char *path);

/* Open and start the background reader thread. */
int tp_ingress_start(tp_ingress_t *ing, const tp_config_t *cfg);

/* Join the reader thread (if started) and close the transport. */
void tp_ingress_stop(tp_ingress_t *ing);

/* Close the transport without joining. Safe to call more than once. */
void tp_ingress_close(tp_ingress_t *ing);

/* Read one complete file to EOF and process its frames (synchronous). */
int tp_ingress_run_file(tp_ingress_t *ing, const char *path);

/*
 * Process one already-delimited frame (no trailing newline required).
 *
 * Returns TP_INGRESS_ACCEPTED, TP_INGRESS_DUPLICATE, TP_INGRESS_QUEUE_FULL, or
 * a negative TP_ERR_* code for malformed/invalid/oversized frames. `reason`
 * receives a short diagnostic that never contains raw untrusted payload.
 */
int tp_ingress_handle_frame(tp_ingress_t *ing, const char *frame, size_t len,
                            char *reason, size_t reason_len);

/* Thread entry point: file mode reads to EOF; socket mode polls for clients. */
void *tp_ingress_thread(void *arg);
