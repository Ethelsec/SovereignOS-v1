#pragma once

#define TP_APP_NAME "collector-telemetry-platform"
#define TP_STORAGE_MAGIC 0x54504631u
#define TP_STORAGE_VERSION 1u
#define TP_EVENT_VERSION 1u
#define TP_DEFAULT_RING_CAPACITY 4096u
#define TP_DEFAULT_WORKERS 4u
#define TP_DEFAULT_DASHBOARD_MS 1000u
#define TP_DEFAULT_STORAGE_PATH "telemetry.tpbin"
#define TP_DEFAULT_EXPORT_ENDPOINT "otlp-export.jsonl"
#define TP_ROTATE_BYTES (64u * 1024u * 1024u)
#define TP_CACHELINE 64u
#define TP_MAX_PROCESSES 4096u
#define TP_MAX_DEDUP 2048u
#define TP_MAX_ANOMALIES 128u
#define TP_MAX_BATCH 64u
