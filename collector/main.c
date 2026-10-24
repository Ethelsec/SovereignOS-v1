#include "analyzer.h"
#include "cli.h"
#include "config.h"
#include "correlator.h"
#include "dashboard.h"
#include "exporter.h"
#include "filter.h"
#include "health.h"
#include "ingress.h"
#include "logger.h"
#include "normalizer.h"
#include "parser.h"
#include "platform.h"
#include "ring_buffer.h"
#include "statistics.h"
#include "storage.h"
#include "telemetry.h"
#include "utilities.h"
#include "version.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

typedef struct {
    tp_config_t cfg;
    tp_ring_buffer_t ring;
    tp_storage_t storage;
    tp_exporter_t exporter;
    tp_stats_t stats;
    tp_correlator_t correlator;
    tp_analyzer_t analyzer;
    tp_filter_t filter;
    tp_health_t health;
    tp_ingress_t ingress;
    pthread_mutex_t sink_lock;
} tp_app_t;

static int replay_print_cb(const tp_event_t *event, void *user)
{
    uint64_t *count = user;
    if (*count < 10) {
        char payload[TP_MAX_PAYLOAD + 1];
        tp_decode_payload_text(event, payload, sizeof(payload));
        printf("seq=%llu type=%s pid=%d severity=%s payload=%s\n",
               (unsigned long long)event->sequence, tp_event_type_name(event->type), event->pid,
               tp_severity_name(event->severity), payload);
    }
    (*count)++;
    return TP_OK;
}

static void *worker_thread(void *arg)
{
    tp_app_t *app = arg;
    while (g_running || tp_ring_size(&app->ring) > 0) {
        tp_event_t event;
        int rc = tp_ring_pop(&app->ring, &event);
        if (rc == TP_ERR_EMPTY) {
            usleep(1000);
            continue;
        }
        if (rc == TP_ERR_CRC) {
            tp_stats_inc(&app->stats.crc_failures);
            continue;
        }
        uint64_t start = tp_now_monotonic_ns();
        tp_stats_inc(&app->stats.received);
        double confidence = 0.0;
        int anomalous = 0;
        if (tp_analyzer_process(&app->analyzer, &event, &confidence, &anomalous) != TP_OK) {
            tp_stats_inc(&app->stats.discarded);
            continue;
        }
        tp_stats_inc(&app->stats.normalized);
        int duplicate = 0;
        if (!tp_filter_should_keep(&app->filter, &app->cfg, &event, &duplicate)) {
            tp_stats_inc(duplicate ? &app->stats.duplicates : &app->stats.discarded);
            continue;
        }
        if (duplicate) tp_stats_inc(&app->stats.duplicates);
        tp_correlator_process(&app->correlator, &event);
        tp_stats_inc(&app->stats.correlated);
        if (anomalous) {
            tp_stats_inc(&app->stats.anomalies);
            tp_log(TP_LOG_WARNING, "analysis", event.correlation_id, "anomaly detected confidence=%.2f", confidence);
        }
        pthread_mutex_lock(&app->sink_lock);
        if (tp_storage_write(&app->storage, &event) == TP_OK) tp_stats_inc(&app->stats.stored);
        else tp_stats_inc(&app->stats.errors);
        if (tp_exporter_export(&app->exporter, &event) == TP_OK) {
            if (tp_exporter_is_enabled(&app->exporter)) tp_stats_inc(&app->stats.exported);
        } else {
            tp_stats_inc(&app->stats.errors);
        }
        pthread_mutex_unlock(&app->sink_lock);
        tp_stats_inc(&app->stats.processed);
        tp_stats_add(&app->stats.total_processing_ns, tp_now_monotonic_ns() - start);
    }
    return NULL;
}

static void *dashboard_thread(void *arg)
{
    tp_app_t *app = arg;
    while (g_running) {
        tp_health_check(&app->health, &app->ring, &app->cfg);
        tp_stats_update_buffer_peak(&app->stats, tp_ring_utilization_pct(&app->ring));
        tp_dashboard_render(&app->cfg, &app->stats, &app->ring, &app->health);
        usleep(app->cfg.dashboard_interval_ms * 1000u);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    tp_app_t app;
    memset(&app, 0, sizeof(app));
    tp_config_defaults(&app.cfg);
    if (tp_cli_parse(&app.cfg, argc, argv) != TP_OK) {
        tp_cli_print_help(argv[0]);
        return 2;
    }
    if (app.cfg.config_file[0]) tp_config_load_file(&app.cfg, app.cfg.config_file);
    tp_config_apply_env(&app.cfg);
    tp_logger_init(app.cfg.log_level, app.cfg.color_logs);
    if (app.cfg.print_config) tp_config_print(&app.cfg);
    if (app.cfg.verify_mode) {
        uint64_t records = 0;
        int rc = tp_storage_verify(app.cfg.storage_path, &records);
        printf("verify path=%s records=%llu status=%s\n", app.cfg.storage_path, (unsigned long long)records, rc == TP_OK ? "ok" : "failed");
        return rc == TP_OK ? 0 : 1;
    }
    if (app.cfg.replay_mode) {
        uint64_t count = 0;
        int rc = tp_storage_replay(app.cfg.storage_path, replay_print_cb, &count);
        printf("replayed=%llu status=%s\n", (unsigned long long)count, rc == TP_OK ? "ok" : "failed");
        return rc == TP_OK ? 0 : 1;
    }

    tp_stats_init(&app.stats);
    tp_filter_init(&app.filter, 1000);
    pthread_mutex_init(&app.sink_lock, NULL);
    if (tp_ring_init(&app.ring, app.cfg.ring_capacity) != TP_OK) return 1;
    if (tp_storage_open(&app.storage, app.cfg.storage_path) != TP_OK) {
        tp_log(TP_LOG_ERROR, "storage", 0, "failed to open storage path %s", app.cfg.storage_path);
        return 1;
    }
    if (tp_exporter_open(&app.exporter, &app.cfg) != TP_OK) {
        tp_log(TP_LOG_WARNING, "exporter", 0, "exporter unavailable at %s; continuing with local storage", app.cfg.export_endpoint);
    }
    tp_correlator_init(&app.correlator);
    tp_analyzer_init(&app.analyzer);
    char ebpf[TP_MAX_TEXT];
    tp_ebpf_probe_status(ebpf, sizeof(ebpf));
    tp_log(TP_LOG_SUCCESS, "ebpf", 0, "%s", ebpf);
    int ingress_enabled = app.cfg.ingest_path[0] != '\0';
    if (ingress_enabled) {
        if (tp_ingress_init(&app.ingress, &app.ring, &app.stats, &g_running) != TP_OK ||
            tp_ingress_start(&app.ingress, &app.cfg) != TP_OK) {
            tp_log(TP_LOG_ERROR, "ingress", 0, "failed to start ingest transport at %s", app.cfg.ingest_path);
            return 1;
        }
        tp_log(TP_LOG_SUCCESS, "ingress", 0, "ingesting canonical JSONL from %s %s",
               app.cfg.ingest_is_socket ? "unix socket" : "file", app.cfg.ingest_path);
    }
    tp_emit_startup_events(&app.ring);

    pthread_t probe, dash;
    tp_probe_context_t probe_ctx = { .ring = &app.ring, .running = &g_running, .interval_ms = 250 };
    pthread_create(&probe, NULL, tp_procfs_probe_thread, &probe_ctx);
    pthread_t *workers = calloc(app.cfg.worker_count, sizeof(*workers));
    for (size_t i = 0; i < app.cfg.worker_count; ++i) pthread_create(&workers[i], NULL, worker_thread, &app);
    pthread_create(&dash, NULL, dashboard_thread, &app);

    if (app.cfg.run_seconds) {
        sleep(app.cfg.run_seconds);
    } else {
        sigset_t set;
        tp_platform_install_signals(&set);
        tp_log(TP_LOG_INFO, "main", 0, "running until SIGINT or SIGTERM");
        tp_platform_wait_signal(&set);
    }
    g_running = 0;
    if (ingress_enabled) tp_ingress_stop(&app.ingress);
    pthread_join(probe, NULL);
    for (size_t i = 0; i < app.cfg.worker_count; ++i) pthread_join(workers[i], NULL);
    pthread_join(dash, NULL);
    free(workers);
    tp_exporter_flush(&app.exporter);
    tp_storage_close(&app.storage);
    tp_exporter_close(&app.exporter);
    tp_correlator_write_report(&app.correlator, stdout);
    uint64_t ring_dropped = tp_ring_dropped(&app.ring);
    uint64_t reported_dropped = tp_stats_get(&app.stats.dropped);
    if (ring_dropped > reported_dropped) {
        tp_stats_add(&app.stats.dropped, ring_dropped - reported_dropped);
    }
    tp_stats_print_summary(&app.stats, stdout);
    tp_stats_write_json_summary(&app.stats, "telemetry-summary.json");
    uint64_t records = 0;
    int verify = tp_storage_verify(app.cfg.storage_path, &records);
    tp_log(verify == TP_OK ? TP_LOG_SUCCESS : TP_LOG_ERROR, "storage", 0, "post-run verification records=%llu", (unsigned long long)records);
    tp_correlator_destroy(&app.correlator);
    tp_ring_destroy(&app.ring);
    pthread_mutex_destroy(&app.sink_lock);
    tp_logger_close();
    return verify == TP_OK ? 0 : 1;
}
