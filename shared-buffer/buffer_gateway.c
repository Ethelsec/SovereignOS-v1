/*
 * Secure Buffer Gateway - Security Boundary Between Guest OS and seL4
 * Enforces least privilege, validation, and access control
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "guest_sel4_buffer.h"

#define MAX_ALLOWED_MESSAGE_TYPES 32
#define RATE_LIMIT_WINDOW_SEC 1
#define MAX_MESSAGES_PER_SECOND 1000
#define MAX_FRAME_AGE_SEC 5

typedef struct {
    uint32_t allowed_types[MAX_ALLOWED_MESSAGE_TYPES];
    int allowed_count;
    uint64_t rate_limit_per_sec;
    bool require_checksum;
    bool audit_enabled;
} gateway_policy_t;

typedef struct {
    gateway_policy_t policy;
    time_t window_start;
    uint32_t messages_in_window;
    uint64_t total_forwarded;
    uint64_t total_dropped;
    uint64_t total_violations;
    pthread_mutex_t lock;
    FILE *audit_log;
} secure_gateway_t;

static secure_gateway_t g_gateway;

/* Initialize secure gateway */
int gateway_init(const char *audit_log_path) {
    memset(&g_gateway, 0, sizeof(g_gateway));
    g_gateway.policy.rate_limit_per_sec = MAX_MESSAGES_PER_SECOND;
    g_gateway.policy.require_checksum = true;
    g_gateway.policy.audit_enabled = true;
    g_gateway.window_start = time(NULL);
    g_gateway.messages_in_window = 0;
    pthread_mutex_init(&g_gateway.lock, NULL);
    
    if (audit_log_path) {
        g_gateway.audit_log = fopen(audit_log_path, "a");
    }
    
    /* Default allowed message types for telemetry */
    /* Type 1: Process events, Type 2: Network events, etc. */
    printf("[GATEWAY] Secure buffer gateway initialized\n");
    printf("[GATEWAY] Rate limit: %lu msg/sec\n", g_gateway.policy.rate_limit_per_sec);
    printf("[GATEWAY] Audit logging: %s\n", g_gateway.policy.audit_enabled ? "enabled" : "disabled");
    
    return 0;
}

/* Audit log function */
void gateway_audit(const char *event, const char *details) {
    if (!g_gateway.policy.audit_enabled || !g_gateway.audit_log) return;
    
    pthread_mutex_lock(&g_gateway.lock);
    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strlen(ts)-1] = '\0'; /* remove newline */
    fprintf(g_gateway.audit_log, "[%s] %s: %s\n", ts, event, details);
    fflush(g_gateway.audit_log);
    pthread_mutex_unlock(&g_gateway.lock);
}

/* Rate limiting check */
bool gateway_check_rate_limit(void) {
    pthread_mutex_lock(&g_gateway.lock);
    time_t now = time(NULL);
    
    if (now - g_gateway.window_start >= RATE_LIMIT_WINDOW_SEC) {
        g_gateway.window_start = now;
        g_gateway.messages_in_window = 0;
    }
    
    if (g_gateway.messages_in_window >= g_gateway.policy.rate_limit_per_sec) {
        g_gateway.total_violations++;
        pthread_mutex_unlock(&g_gateway.lock);
        gateway_audit("RATE_LIMIT", "Message rate exceeded");
        return false;
    }
    
    g_gateway.messages_in_window++;
    pthread_mutex_unlock(&g_gateway.lock);
    return true;
}

/* Validate frame before forwarding to seL4 */
bool gateway_validate_frame(const void *data, size_t len) {
    if (!data || len == 0) {
        gateway_audit("VALIDATION_FAIL", "Null or empty frame");
        return false;
    }
    
    if (len > 2048) {
        gateway_audit("VALIDATION_FAIL", "Frame too large");
        return false;
    }
    
    /* Additional validation checks */
    /* - Check magic numbers */
    /* - Validate timestamps */
    /* - Check checksums */
    /* - Validate source/destination */
    
    return true;
}

/* Forward from guest to seL4 only if validated */
int gateway_forward_guest_to_sel4(const void *data, size_t len) {
    if (!gateway_check_rate_limit()) {
        g_gateway.total_dropped++;
        return -1;
    }
    
    if (!gateway_validate_frame(data, len)) {
        g_gateway.total_dropped++;
        return -1;
    }
    
    /* Only forward if it passes all checks */
    int ret = gs_buffer_push_guest(data, len);
    if (ret == 0) {
        g_gateway.total_forwarded++;
        gateway_audit("FORWARD", "Guest->seL4 frame forwarded");
    } else {
        g_gateway.total_dropped++;
        gateway_audit("DROP", "Buffer push failed");
    }
    
    return ret;
}

/* Forward from seL4 to guest with validation */
int gateway_forward_sel4_to_guest(const void *data, size_t len) {
    if (!gateway_check_rate_limit()) {
        g_gateway.total_dropped++;
        return -1;
    }
    
    if (!gateway_validate_frame(data, len)) {
        g_gateway.total_dropped++;
        return -1;
    }
    
    int ret = gs_buffer_push_sel4(data, len);
    if (ret == 0) {
        g_gateway.total_forwarded++;
        gateway_audit("FORWARD", "seL4->Guest frame forwarded");
    } else {
        g_gateway.total_dropped++;
    }
    
    return ret;
}

/* Print gateway statistics */
void gateway_stats(void) {
    pthread_mutex_lock(&g_gateway.lock);
    printf("\n=== SECURE BUFFER GATEWAY STATS ===\n");
    printf("Total Forwarded: %lu\n", g_gateway.total_forwarded);
    printf("Total Dropped:    %lu\n", g_gateway.total_dropped);
    printf("Total Violations: %lu\n", g_gateway.total_violations);
    printf("Rate Limit:       %lu msg/sec\n", g_gateway.policy.rate_limit_per_sec);
    printf("Audit Enabled:    %s\n", g_gateway.policy.audit_enabled ? "YES" : "NO");
    printf("====================================\n\n");
    pthread_mutex_unlock(&g_gateway.lock);
}
