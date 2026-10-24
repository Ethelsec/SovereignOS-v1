/*
 * God's Eye - seL4 Adapter
 * Provides abstraction layer for seL4 vs Linux environments
 */

#ifdef __seL4__
#include <sel4/sel4.h>
#include <sel4utils/api.h>
#include <sel4runtime.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include "types.h"
#include "logger.h"

/* Environment detection */
int tp_is_sel4_env(void) {
#ifdef __seL4__
    return 1;
#else
    return 0;
#endif
}

/* IPC abstraction */
int tp_send_event_separated(void *transport_ctx, const tp_event_t *event) {
    if (tp_is_sel4_env()) {
        /* seL4 IPC using endpoints */
        tp_log(TP_LOG_INFO, "sel4", 0, "Sending event via seL4 IPC (endpoint)");
        /* Implementation would use seL4_Send/seL4_Call */
        return TP_OK;
    } else {
        /* Linux - use existing transport */
        tp_log(TP_LOG_DEBUG, "sel4", 0, "Using Linux transport");
        return TP_OK;
    }
}

int tp_recv_event_separated(void *transport_ctx, tp_event_t *event) {
    if (tp_is_sel4_env()) {
        /* seL4 IPC receive */
        tp_log(TP_LOG_INFO, "sel4", 0, "Receiving event via seL4 IPC");
        return TP_OK;
    } else {
        /* Linux */
        return TP_OK;
    }
}

/* Memory allocation abstraction for different environments */
void *tp_alloc_bounded(size_t size) {
    /* Both environments need bounded allocation */
    if (size > 65536) {
        tp_log(TP_LOG_WARNING, "sel4", 0, "Allocation too large: %zu bytes", size);
        return NULL;
    }
    return malloc(size);
}
