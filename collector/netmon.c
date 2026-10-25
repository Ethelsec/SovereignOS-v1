#include "ring_buffer.h"
#include "logger.h"
#include "utilities.h"
#include "types.h"
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

typedef struct {
    tp_ring_buffer_t *ring;
    volatile sig_atomic_t *running;
} tp_netmon_ctx_t;

static void emit_net_event(tp_ring_buffer_t *ring, const char *desc, int severity) {
    tp_event_t e;
    memset(&e, 0, sizeof(e));
    e.version = TP_EVENT_VERSION;
    e.timestamp = tp_timestamp_now();
    e.event_id = (uint64_t)tp_timestamp_now().monotonic_ns;
    e.type = TP_TYPE_SOCKET_CONNECT;
    e.severity = severity;
    e.pid = getpid();
    e.ppid = getppid();
    tp_safe_copy(e.source_probe, sizeof(e.source_probe), "netmon");
    tp_safe_copy(e.source_module, sizeof(e.source_module), "network_monitor");
    snprintf((char *)e.payload, sizeof(e.payload), "%s", desc);
    e.payload_size = (uint32_t)strlen((char *)e.payload);
    tp_ring_push(ring, &e);
}

void *tp_netmon_thread(void *arg) {
    tp_netmon_ctx_t *ctx = (tp_netmon_ctx_t *)arg;
    tp_log(TP_LOG_SUCCESS, "netmon", 0, "real network monitoring started");
    
    while (*ctx->running) {
        // Monitor connections
        FILE *f = fopen("/proc/net/tcp", "r");
        if (f) {
            char line[512];
            int count = 0;
            fgets(line, sizeof(line), f);
            while (fgets(line, sizeof(line), f) && count < 100) {
                unsigned long rxq, txq, tr, tmwhen;
                unsigned int local_ip, local_port, rem_ip, rem_port, state;
                if (sscanf(line, "%*d: %x:%x %x:%x %x %lx:%lx %lx:%lx %*X %*d %*d %*d %*d %*d %*u %*d %*u %*d",
                           &local_ip, &local_port, &rem_ip, &rem_port, &state,
                           &rxq, &txq, &tr, &tmwhen) >= 10) {
                    count++;
                    if (rem_ip != 0 && state == 1) { // ESTABLISHED
                        struct in_addr laddr = {local_ip}, raddr = {rem_ip};
                        char desc[256];
                        snprintf(desc, sizeof(desc), "net.tcp.established local=%s:%u remote=%s:%u state=established",
                                inet_ntoa(laddr), local_port, inet_ntoa(raddr), rem_port);
                        emit_net_event(ctx->ring, desc, TP_SEV_INFO);
                    } else if (rem_ip != 0 && state == 6) { // TIME_WAIT or SYN_SENT patterns
                        if (count > 50) { // High connection count
                            char desc[256];
                            struct in_addr raddr = {rem_ip};
                            snprintf(desc, sizeof(desc), "net.conn.flood remote=%s connections=high", inet_ntoa(raddr));
                            emit_net_event(ctx->ring, desc, TP_SEV_HIGH);
                        }
                    }
                }
            }
            fclose(f);
        }
        
        // Monitor UDP
        f = fopen("/proc/net/udp", "r");
        if (f) {
            char line[512];
            fgets(line, sizeof(line), f);
            int udp_count = 0;
            while (fgets(line, sizeof(line), f) && udp_count < 50) {
                udp_count++;
            }
            fclose(f);
        }
        
        sleep(2);
    }
    return NULL;
}
