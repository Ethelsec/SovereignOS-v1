#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#define TP_MAX_PAYLOAD 1024u
#define TP_MAX_TEXT 256u
#define TP_MAX_PATH 512u
#define TP_TRACE_ID_LEN 33u
#define TP_SPAN_ID_LEN 17u

typedef enum {
    TP_OK = 0,
    TP_ERR = -1,
    TP_ERR_NOMEM = -2,
    TP_ERR_INVAL = -3,
    TP_ERR_IO = -4,
    TP_ERR_FULL = -5,
    TP_ERR_EMPTY = -6,
    TP_ERR_CRC = -7,
    TP_ERR_CORRUPT = -8
} tp_status_t;

typedef enum {
    TP_LOG_FATAL = 0,
    TP_LOG_ERROR,
    TP_LOG_WARNING,
    TP_LOG_SUCCESS,
    TP_LOG_INFO,
    TP_LOG_DEBUG,
    TP_LOG_TRACE
} tp_log_level_t;

typedef enum {
    TP_EVENT_PROCESS = 1u << 0,
    TP_EVENT_FILESYSTEM = 1u << 1,
    TP_EVENT_NETWORK = 1u << 2,
    TP_EVENT_SCHEDULER = 1u << 3,
    TP_EVENT_SECURITY = 1u << 4,
    TP_EVENT_MEMORY = 1u << 5,
    TP_EVENT_CPU = 1u << 6,
    TP_EVENT_STORAGE = 1u << 7,
    TP_EVENT_IPC = 1u << 8,
    TP_EVENT_SIGNAL = 1u << 9,
    TP_EVENT_KERNEL = 1u << 10,
    TP_EVENT_USERSPACE = 1u << 11,
    TP_EVENT_SYSTEM = 1u << 12,
    TP_EVENT_CUSTOM = 1u << 13
} tp_event_category_t;

typedef enum {
    TP_TYPE_UNKNOWN = 0,
    TP_TYPE_PROCESS_START,
    TP_TYPE_PROCESS_EXIT,
    TP_TYPE_THREAD_START,
    TP_TYPE_THREAD_EXIT,
    TP_TYPE_EXEC,
    TP_TYPE_FORK,
    TP_TYPE_SIGNAL,
    TP_TYPE_FILE_OPEN,
    TP_TYPE_FILE_WRITE,
    TP_TYPE_FILE_CLOSE,
    TP_TYPE_SOCKET_CONNECT,
    TP_TYPE_SOCKET_ACCEPT,
    TP_TYPE_SOCKET_CLOSE,
    TP_TYPE_TCP_STATE,
    TP_TYPE_DNS,
    TP_TYPE_HTTP,
    TP_TYPE_MEMORY_ALLOC,
    TP_TYPE_PAGE_FAULT,
    TP_TYPE_CPU_SAMPLE,
    TP_TYPE_CONTEXT_SWITCH,
    TP_TYPE_SECURITY,
    TP_TYPE_KERNEL_WARNING,
    TP_TYPE_RESOURCE_SPIKE,
    TP_TYPE_HEALTH,
    TP_TYPE_LOG,
    TP_TYPE_CUSTOM,
    TP_TYPE_METRIC
} tp_event_type_t;

typedef enum {
    TP_SEV_TRACE = 0,
    TP_SEV_DEBUG,
    TP_SEV_INFO,
    TP_SEV_NORMAL,
    TP_SEV_HIGH,
    TP_SEV_CRITICAL
} tp_severity_t;

typedef struct {
    uint64_t monotonic_ns;
    uint64_t wall_ns;
} tp_timestamp_t;

typedef struct {
    uint64_t event_id;
    uint64_t sequence;
    uint64_t correlation_id;
    uint64_t parent_correlation_id;
    tp_timestamp_t timestamp;
    tp_event_type_t type;
    uint32_t categories;
    char source_probe[32];
    char source_module[32];
    pid_t pid;
    pid_t tid;
    pid_t ppid;
    uid_t uid;
    gid_t gid;
    int cpu_core;
    int numa_node;
    char host_id[64];
    char container_id[64];
    char namespace_id[64];
    char process_name[64];
    char executable_path[TP_MAX_PATH];
    char command_line[TP_MAX_PATH];
    char thread_name[64];
    pid_t session_id;
    uint8_t priority;
    tp_severity_t severity;
    uint32_t flags;
    uint16_t version;
    uint32_t payload_size;
    uint8_t payload[TP_MAX_PAYLOAD];
    char resource_attributes[TP_MAX_TEXT];
    char analysis[TP_MAX_TEXT];
    char trace_id[TP_TRACE_ID_LEN];
    char span_id[TP_SPAN_ID_LEN];
    char parent_span_id[TP_SPAN_ID_LEN];
} tp_event_t;

/* tp_event_t.flags bits. Canonical ingress sets these; they are also part of
 * the event's provenance record, not merely decoder-local state. */
#define TP_FLAG_INGEST 0x00000001u
#define TP_FLAG_SEVERITY_UNMAPPED 0x00000002u
#define TP_FLAG_SEVERITY_UNKNOWN 0x00000004u
#define TP_FLAG_MONOTONIC_RECEIVER_CLOCK 0x00000008u
#define TP_FLAG_PID_UNKNOWN 0x00000010u
#define TP_FLAG_EVENT_ID_HASHED 0x00000020u

typedef struct {
    _Atomic uint64_t received;
    _Atomic uint64_t processed;
    _Atomic uint64_t normalized;
    _Atomic uint64_t enriched;
    _Atomic uint64_t correlated;
    _Atomic uint64_t exported;
    _Atomic uint64_t stored;
    _Atomic uint64_t discarded;
    _Atomic uint64_t duplicates;
    _Atomic uint64_t anomalies;
    _Atomic uint64_t warnings;
    _Atomic uint64_t errors;
    _Atomic uint64_t dropped;
    _Atomic uint64_t recovered;
    _Atomic uint64_t crc_failures;
    /* Canonical ingress counters (Member A receiver boundary). See INGEST.md. */
    _Atomic uint64_t ingress_bytes;
    _Atomic uint64_t ingress_frames;
    _Atomic uint64_t ingress_accepted;
    _Atomic uint64_t ingress_rejected;
    _Atomic uint64_t ingress_dropped;
    _Atomic uint64_t ingress_duplicates;
    _Atomic uint64_t ingress_gaps;
    _Atomic uint64_t ingress_out_of_order;
    _Atomic uint64_t ingress_oversize;
    _Atomic uint64_t ingress_truncated;
    _Atomic uint64_t ingress_transport_errors;
    _Atomic uint64_t max_buffer_utilization_pct;
    _Atomic uint64_t total_processing_ns;
    _Atomic uint64_t peak_throughput;
    uint64_t started_wall_ns;
} tp_stats_t;

typedef struct {
    size_t ring_capacity;
    size_t worker_count;
    char storage_path[TP_MAX_PATH];
    char export_endpoint[TP_MAX_TEXT];
    char ingest_path[TP_MAX_PATH];
    bool ingest_is_socket;
    char config_file[TP_MAX_PATH];
    tp_log_level_t log_level;
    bool color_logs;
    bool dashboard;
    bool export_enabled;
    bool compression_enabled;
    bool replay_mode;
    bool verify_mode;
    bool print_config;
    unsigned dashboard_interval_ms;
    unsigned correlation_windows_ms[8];
    size_t correlation_window_count;
    unsigned retention_hours;
    unsigned retry_max;
    unsigned retry_backoff_ms;
    unsigned run_seconds;
    uint8_t min_store_severity;
} tp_config_t;

typedef struct {
    int healthy;
    char message[TP_MAX_TEXT];
    uint64_t last_check_ns;
} tp_health_t;
