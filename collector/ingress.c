#include "ingress.h"
#include "logger.h"
#include "statistics.h"
#include "utilities.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define TP_INGRESS_READ_CHUNK 4096u
#define TP_INGRESS_POLL_MS 100
#define TP_INGRESS_GAP_CAP 1000000ull

static int should_run(const tp_ingress_t *ing)
{
    return !ing->running || *ing->running;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return TP_ERR_IO;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? TP_ERR_IO : TP_OK;
}

int tp_ingress_init(tp_ingress_t *ing, tp_ring_buffer_t *ring, tp_stats_t *stats,
                    volatile sig_atomic_t *running)
{
    if (!ing || !ring || !stats) return TP_ERR_INVAL;
    memset(ing, 0, sizeof(*ing));
    ing->fd = -1;
    ing->kind = TP_INGRESS_NONE;
    ing->ring = ring;
    ing->stats = stats;
    ing->running = running;
    return TP_OK;
}

int tp_ingress_open_file(tp_ingress_t *ing, const char *path)
{
    if (!ing || !path || !*path) return TP_ERR_INVAL;
    if (ing->kind != TP_INGRESS_NONE) return TP_ERR_INVAL;
    if (strlen(path) >= sizeof(ing->path)) return TP_ERR_FULL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        tp_log(TP_LOG_ERROR, "ingress", 0, "cannot open ingest file %s", path);
        return TP_ERR_IO;
    }
    ing->fd = fd;
    ing->kind = TP_INGRESS_FILE;
    tp_safe_copy(ing->path, sizeof(ing->path), path);
    return TP_OK;
}

int tp_ingress_open_socket(tp_ingress_t *ing, const char *path)
{
    if (!ing || !path || !*path) return TP_ERR_INVAL;
    if (ing->kind != TP_INGRESS_NONE) return TP_ERR_INVAL;
    if (strlen(path) >= sizeof(ing->path)) return TP_ERR_FULL;

    struct stat st;
    if (lstat(path, &st) == 0) {
        if (!S_ISSOCK(st.st_mode)) {
            tp_log(TP_LOG_ERROR, "ingress", 0,
                   "refusing to replace non-socket path %s", path);
            return TP_ERR_INVAL;
        }
        if (unlink(path) != 0) return TP_ERR_IO; /* stale socket from a crash */
    } else if (errno != ENOENT) {
        return TP_ERR_IO;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr.sun_path)) return TP_ERR_FULL;
    memcpy(addr.sun_path, path, strlen(path) + 1);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        return TP_ERR_IO;
    }
    socklen_t addr_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(path) + 1u);
    if (bind(fd, (struct sockaddr *)&addr, addr_len) != 0 ||
        listen(fd, (int)TP_INGRESS_MAX_CLIENTS) != 0) {
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        tp_log(TP_LOG_ERROR, "ingress", 0, "cannot bind ingest socket %s", path);
        close(fd);
        unlink(path);
        return TP_ERR_IO;
    }
    ing->fd = fd;
    ing->kind = TP_INGRESS_SOCKET;
    tp_safe_copy(ing->path, sizeof(ing->path), path);
    return TP_OK;
}

int tp_ingress_open(tp_ingress_t *ing, const tp_config_t *cfg)
{
    if (!ing || !cfg) return TP_ERR_INVAL;
    if (!cfg->ingest_path[0]) return TP_ERR_EMPTY;
    return cfg->ingest_is_socket ? tp_ingress_open_socket(ing, cfg->ingest_path)
                                 : tp_ingress_open_file(ing, cfg->ingest_path);
}

void tp_ingress_close(tp_ingress_t *ing)
{
    if (!ing) return;
    if (ing->fd >= 0) {
        close(ing->fd);
        ing->fd = -1;
    }
    if (ing->kind == TP_INGRESS_SOCKET && ing->path[0]) unlink(ing->path);
    ing->kind = TP_INGRESS_NONE;
    ing->path[0] = '\0';
}

int tp_ingress_start(tp_ingress_t *ing, const tp_config_t *cfg)
{
    if (!ing || !cfg) return TP_ERR_INVAL;
    int rc = tp_ingress_open(ing, cfg);
    if (rc != TP_OK) return rc;
    if (pthread_create(&ing->thread, NULL, tp_ingress_thread, ing) != 0) {
        tp_ingress_close(ing);
        return TP_ERR;
    }
    ing->thread_started = 1;
    return TP_OK;
}

void tp_ingress_stop(tp_ingress_t *ing)
{
    if (!ing) return;
    if (ing->thread_started) {
        pthread_join(ing->thread, NULL);
        ing->thread_started = 0;
    }
    tp_ingress_close(ing);
}

static int dedup_seen(const tp_ingress_t *ing, uint64_t event_id)
{
    for (unsigned i = 0; i < TP_INGRESS_DEDUP_WINDOW; ++i) {
        if (ing->dedup[i].used && ing->dedup[i].event_id == event_id) return 1;
    }
    return 0;
}

static void dedup_insert(tp_ingress_t *ing, uint64_t event_id)
{
    ing->dedup[ing->dedup_next].event_id = event_id;
    ing->dedup[ing->dedup_next].used = 1;
    ing->dedup_next = (ing->dedup_next + 1u) % TP_INGRESS_DEDUP_WINDOW;
}

int tp_ingress_handle_frame(tp_ingress_t *ing, const char *frame, size_t len,
                            char *reason, size_t reason_len)
{
    if (!ing || !frame) return TP_ERR_INVAL;
    if (reason && reason_len) reason[0] = '\0';
    tp_stats_inc(&ing->stats->ingress_frames);

    tp_canonical_event_t decoded;
    int rc = tp_canonical_decode(frame, len, &decoded, reason, reason_len);
    if (rc != TP_OK) {
        tp_stats_inc(&ing->stats->ingress_rejected);
        /* Never log the raw frame: it may contain untrusted or sensitive data. */
        tp_log(TP_LOG_DEBUG, "ingress", 0, "rejected frame (%s): %s",
               tp_canonical_status_name(rc), reason && reason_len ? reason : "no detail");
        return rc;
    }

    if (dedup_seen(ing, decoded.event.event_id)) {
        tp_stats_inc(&ing->stats->ingress_duplicates);
        tp_log(TP_LOG_DEBUG, "ingress", decoded.event.event_id,
               "duplicate event_id inside the %u-frame window", (unsigned)TP_INGRESS_DEDUP_WINDOW);
        return TP_INGRESS_DUPLICATE;
    }

    /* The ring assigns its own sequence on push; the producer sequence is kept
     * only for gap accounting here (see INGEST.md). */
    if (tp_ring_push(ing->ring, &decoded.event) != TP_OK) {
        /* Drop-newest. The event ID is deliberately NOT inserted into the
         * dedup window so a producer retry can still be accepted. */
        tp_stats_inc(&ing->stats->ingress_dropped);
        tp_stats_inc(&ing->stats->dropped);
        return TP_INGRESS_QUEUE_FULL;
    }

    if (ing->have_sequence) {
        if (decoded.producer_sequence > ing->last_sequence) {
            uint64_t missing = decoded.producer_sequence - ing->last_sequence - 1u;
            if (missing > TP_INGRESS_GAP_CAP) missing = TP_INGRESS_GAP_CAP;
            tp_stats_add(&ing->stats->ingress_gaps, missing);
            ing->last_sequence = decoded.producer_sequence;
        } else {
            tp_stats_inc(&ing->stats->ingress_out_of_order);
        }
    } else {
        ing->last_sequence = decoded.producer_sequence;
        ing->have_sequence = 1;
    }

    dedup_insert(ing, decoded.event.event_id);
    tp_stats_inc(&ing->stats->ingress_accepted);
    tp_log(TP_LOG_TRACE, "ingress", decoded.event.event_id, "accepted type=%s sequence=%llu",
           tp_event_type_name(decoded.event.type),
           (unsigned long long)decoded.producer_sequence);
    return TP_INGRESS_ACCEPTED;
}

/* Feed raw bytes into the framing state machine. Only complete lines reach
 * tp_ingress_handle_frame(); oversize and partial frames are counted. */
static void stream_consume(tp_ingress_t *ing, tp_ingress_stream_t *stream,
                           const char *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        char c = data[i];
        tp_stats_inc(&ing->stats->ingress_bytes);
        if (c == '\n') {
            if (stream->discarding) {
                stream->discarding = 0;
                stream->line_len = 0;
                continue;
            }
            size_t n = stream->line_len;
            if (n > 0 && stream->line[n - 1] == '\r') --n;
            stream->line_len = 0;
            if (n == 0) continue; /* blank line */
            stream->line[n] = '\0';
            char reason[TP_CANONICAL_MAX_REASON];
            (void)tp_ingress_handle_frame(ing, stream->line, n, reason, sizeof(reason));
            continue;
        }
        if (stream->discarding) continue;
        if (stream->line_len >= TP_CANONICAL_MAX_FRAME) {
            stream->discarding = 1;
            stream->line_len = 0;
            tp_stats_inc(&ing->stats->ingress_oversize);
            continue;
        }
        stream->line[stream->line_len++] = c;
    }
}

static void stream_finish(tp_ingress_t *ing, tp_ingress_stream_t *stream)
{
    if (stream->discarding) {
        stream->discarding = 0;
        stream->line_len = 0;
        return;
    }
    if (stream->line_len > 0) {
        tp_stats_inc(&ing->stats->ingress_truncated);
        stream->line_len = 0;
    }
}

static int pump_fd(tp_ingress_t *ing, int fd)
{
    tp_ingress_stream_t stream;
    memset(&stream, 0, sizeof(stream));
    stream.fd = fd;
    char buf[TP_INGRESS_READ_CHUNK];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            stream_consume(ing, &stream, buf, (size_t)n);
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        tp_log(TP_LOG_ERROR, "ingress", 0, "read failed on %s", ing->path);
        stream_finish(ing, &stream);
        return TP_ERR_IO;
    }
    stream_finish(ing, &stream);
    return TP_OK;
}

int tp_ingress_run_file(tp_ingress_t *ing, const char *path)
{
    if (!ing || !path || !*path) return TP_ERR_INVAL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        return TP_ERR_IO;
    }
    int rc = pump_fd(ing, fd);
    close(fd);
    return rc;
}

static void accept_clients(tp_ingress_t *ing, tp_ingress_stream_t *clients)
{
    for (;;) {
        int fd = accept(ing->fd, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            tp_stats_inc(&ing->stats->ingress_transport_errors);
            break;
        }
        if (set_nonblocking(fd) != TP_OK) {
            close(fd);
            tp_stats_inc(&ing->stats->ingress_transport_errors);
            continue;
        }
        int slot = -1;
        for (unsigned i = 0; i < TP_INGRESS_MAX_CLIENTS; ++i) {
            if (clients[i].fd < 0) { slot = (int)i; break; }
        }
        if (slot < 0) {
            close(fd);
            tp_stats_inc(&ing->stats->ingress_transport_errors);
            tp_log(TP_LOG_WARNING, "ingress", 0,
                   "client limit reached (%u); closing new connection",
                   (unsigned)TP_INGRESS_MAX_CLIENTS);
            continue;
        }
        memset(&clients[slot], 0, sizeof(clients[slot]));
        clients[slot].fd = fd;
    }
}

static void drain_client(tp_ingress_t *ing, tp_ingress_stream_t *client)
{
    char buf[TP_INGRESS_READ_CHUNK];
    for (;;) {
        ssize_t n = read(client->fd, buf, sizeof(buf));
        if (n > 0) {
            stream_consume(ing, client, buf, (size_t)n);
            continue;
        }
        if (n == 0) {
            stream_finish(ing, client);
            close(client->fd);
            client->fd = -1;
            return;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        tp_stats_inc(&ing->stats->ingress_transport_errors);
        stream_finish(ing, client);
        close(client->fd);
        client->fd = -1;
        return;
    }
}

void *tp_ingress_thread(void *arg)
{
    tp_ingress_t *ing = arg;
    if (!ing) return NULL;

    if (ing->kind == TP_INGRESS_FILE) {
        int rc = ing->fd >= 0 ? pump_fd(ing, ing->fd) : TP_ERR_IO;
        tp_log(rc == TP_OK ? TP_LOG_SUCCESS : TP_LOG_WARNING, "ingress", 0,
               "file ingest finished path=%s", ing->path);
        return NULL;
    }
    if (ing->kind != TP_INGRESS_SOCKET || ing->fd < 0) return NULL;
    if (!ing->running) {
        tp_log(TP_LOG_ERROR, "ingress", 0, "socket ingest requires a running flag");
        return NULL;
    }

    tp_ingress_stream_t clients[TP_INGRESS_MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));
    for (unsigned i = 0; i < TP_INGRESS_MAX_CLIENTS; ++i) clients[i].fd = -1;

    while (should_run(ing)) {
        struct pollfd pfds[TP_INGRESS_MAX_CLIENTS + 1];
        int slots[TP_INGRESS_MAX_CLIENTS + 1];
        nfds_t count = 0;
        pfds[count].fd = ing->fd;
        pfds[count].events = POLLIN;
        pfds[count].revents = 0;
        slots[count] = -1;
        ++count;
        for (unsigned i = 0; i < TP_INGRESS_MAX_CLIENTS; ++i) {
            if (clients[i].fd < 0) continue;
            pfds[count].fd = clients[i].fd;
            pfds[count].events = POLLIN;
            pfds[count].revents = 0;
            slots[count] = (int)i;
            ++count;
        }
        int ready = poll(pfds, count, TP_INGRESS_POLL_MS);
        if (ready < 0) {
            if (errno == EINTR) continue;
            tp_stats_inc(&ing->stats->ingress_transport_errors);
            break;
        }
        if (ready == 0) continue;
        if (pfds[0].revents & POLLIN) accept_clients(ing, clients);
        for (nfds_t k = 1; k < count; ++k) {
            if (!(pfds[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            drain_client(ing, &clients[slots[k]]);
        }
    }

    for (unsigned i = 0; i < TP_INGRESS_MAX_CLIENTS; ++i) {
        if (clients[i].fd < 0) continue;
        stream_finish(ing, &clients[i]);
        close(clients[i].fd);
        clients[i].fd = -1;
    }
    return NULL;
}
