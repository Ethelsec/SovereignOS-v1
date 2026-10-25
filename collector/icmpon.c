#define _GNU_SOURCE
#include "ring_buffer.h"
#include "logger.h"
#include "utilities.h"
#include "types.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

void *tp_icmpmon_thread(void *arg) {
    typedef struct { tp_ring_buffer_t *ring; volatile sig_atomic_t *running; } ctx_t;
    ctx_t *ctx = (ctx_t *)arg;
    tp_log(TP_LOG_SUCCESS, "icmpmon", 0, "icmp monitoring started");
    
    while (*ctx->running) {
        FILE *f = fopen("/proc/net/snmp", "r");
        if (f) {
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Icmp:", 4) == 0) {
                    unsigned long icmp_in_msgs=0, icmp_out_msgs=0, icmp_in_echo=0, icmp_out_echo=0;
                    sscanf(line, "Icmp: %lu %lu %lu %lu %lu", &icmp_in_msgs, &icmp_in_echo, &icmp_out_msgs, &icmp_out_echo);
                    // Will be processed by feature aggregation
                }
            }
            fclose(f);
        }
        sleep(1);
    }
    return NULL;
}
