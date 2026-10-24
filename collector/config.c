#include "config.h"
#include "constants.h"
#include "logger.h"
#include "utilities.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static void apply_pair(tp_config_t *cfg, const char *key, const char *val)
{
    uint64_t n = 0;
    if (strcmp(key, "buffer_size") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->ring_capacity = (size_t)n;
    else if (strcmp(key, "worker_count") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->worker_count = (size_t)n;
    else if (strcmp(key, "storage_path") == 0) tp_safe_copy(cfg->storage_path, sizeof(cfg->storage_path), val);
    else if (strcmp(key, "export_endpoint") == 0) tp_safe_copy(cfg->export_endpoint, sizeof(cfg->export_endpoint), val);
    else if (strcmp(key, "ingest_file") == 0) {
        tp_safe_copy(cfg->ingest_path, sizeof(cfg->ingest_path), val);
        cfg->ingest_is_socket = false;
    } else if (strcmp(key, "ingest_socket") == 0) {
        tp_safe_copy(cfg->ingest_path, sizeof(cfg->ingest_path), val);
        cfg->ingest_is_socket = true;
    }
    else if (strcmp(key, "log_level") == 0) cfg->log_level = tp_log_level_from_string(val);
    else if (strcmp(key, "dashboard") == 0) cfg->dashboard = strcmp(val, "false") != 0 && strcmp(val, "0") != 0;
    else if (strcmp(key, "export_enabled") == 0) cfg->export_enabled = strcmp(val, "false") != 0 && strcmp(val, "0") != 0;
    else if (strcmp(key, "dashboard_interval_ms") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->dashboard_interval_ms = (unsigned)n;
    else if (strcmp(key, "run_seconds") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->run_seconds = (unsigned)n;
    else if (strcmp(key, "retry_max") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->retry_max = (unsigned)n;
    else if (strcmp(key, "retry_backoff_ms") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->retry_backoff_ms = (unsigned)n;
    else if (strcmp(key, "retention_hours") == 0 && tp_parse_u64(val, &n) == TP_OK) cfg->retention_hours = (unsigned)n;
}

void tp_config_defaults(tp_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->ring_capacity = TP_DEFAULT_RING_CAPACITY;
    cfg->worker_count = TP_DEFAULT_WORKERS;
    cfg->log_level = TP_LOG_INFO;
    cfg->color_logs = true;
    cfg->dashboard = true;
    cfg->export_enabled = true;
    cfg->dashboard_interval_ms = TP_DEFAULT_DASHBOARD_MS;
    cfg->correlation_windows_ms[0] = 10;
    cfg->correlation_windows_ms[1] = 50;
    cfg->correlation_windows_ms[2] = 100;
    cfg->correlation_windows_ms[3] = 250;
    cfg->correlation_windows_ms[4] = 500;
    cfg->correlation_windows_ms[5] = 1000;
    cfg->correlation_windows_ms[6] = 5000;
    cfg->correlation_windows_ms[7] = 10000;
    cfg->correlation_window_count = 8;
    cfg->retention_hours = 24;
    cfg->retry_max = 3;
    cfg->retry_backoff_ms = 200;
    cfg->run_seconds = 0;
    cfg->min_store_severity = TP_SEV_TRACE;
    tp_safe_copy(cfg->storage_path, sizeof(cfg->storage_path), TP_DEFAULT_STORAGE_PATH);
    tp_safe_copy(cfg->export_endpoint, sizeof(cfg->export_endpoint), TP_DEFAULT_EXPORT_ENDPOINT);
}

int tp_config_load_file(tp_config_t *cfg, const char *path)
{
    if (!cfg || !path || !*path) return TP_ERR_INVAL;
    FILE *f = fopen(path, "r");
    if (!f) return TP_ERR_IO;
    char line[1024];
    unsigned lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        ++lineno;
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            tp_log(TP_LOG_WARNING, "config", 0, "ignoring malformed config line %u", lineno);
            continue;
        }
        *eq = '\0';
        apply_pair(cfg, trim(s), trim(eq + 1));
    }
    fclose(f);
    return TP_OK;
}

int tp_config_apply_env(tp_config_t *cfg)
{
    const char *pairs[][2] = {
        {"TP_BUFFER_SIZE", "buffer_size"}, {"TP_WORKERS", "worker_count"},
        {"TP_STORAGE_PATH", "storage_path"}, {"TP_EXPORT_ENDPOINT", "export_endpoint"},
        {"TP_INGEST_FILE", "ingest_file"}, {"TP_INGEST_SOCKET", "ingest_socket"},
        {"TP_LOG_LEVEL", "log_level"}, {"TP_DASHBOARD", "dashboard"}, {"TP_EXPORT", "export_enabled"}
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        const char *v = getenv(pairs[i][0]);
        if (v) apply_pair(cfg, pairs[i][1], v);
    }
    return TP_OK;
}

void tp_config_print(const tp_config_t *cfg)
{
    printf("buffer_size=%zu\nworker_count=%zu\nstorage_path=%s\nexport_endpoint=%s\nlog_level=%d\n"
           "dashboard=%s\nexport_enabled=%s\ndashboard_interval_ms=%u\nrun_seconds=%u\n"
           "ingest_file=%s\ningest_socket=%s\n",
           cfg->ring_capacity, cfg->worker_count, cfg->storage_path, cfg->export_endpoint,
           cfg->log_level, cfg->dashboard ? "true" : "false", cfg->export_enabled ? "true" : "false",
           cfg->dashboard_interval_ms, cfg->run_seconds,
           !cfg->ingest_is_socket ? cfg->ingest_path : "",
           cfg->ingest_is_socket ? cfg->ingest_path : "");
}
