#include "cli.h"
#include "config.h"
#include "logger.h"
#include "utilities.h"
#include "version.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tp_cli_print_version(void)
{
    printf("collector telemetry platform %s\n", TP_VERSION_STRING);
}

void tp_cli_print_help(const char *argv0)
{
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    printf("  -h, --help                    Show help\n");
    printf("  -V, --version                 Show version\n");
    printf("  -c, --config PATH             Load key=value configuration file\n");
    printf("  -s, --storage PATH            Telemetry storage file path\n");
    printf("  -e, --export ENDPOINT         OTLP endpoint file or tcp://host:port\n");
    printf("      --enable-dashboard        Render live terminal dashboard\n");
    printf("      --disable-dashboard       Disable dashboard\n");
    printf("      --enable-export           Enable OTLP export\n");
    printf("      --disable-export          Disable OTLP export\n");
    printf("      --log-level LEVEL         fatal,error,warning,success,info,debug,trace\n");
    printf("      --buffer-size N           Ring buffer capacity in events\n");
    printf("      --workers N               Worker thread count\n");
    printf("      --ingest-file PATH        Read canonical JSONL events from a file (replay)\n");
    printf("      --ingest-socket PATH      Listen for canonical JSONL events on an AF_UNIX socket\n");
    printf("      --replay PATH             Replay and summarize a telemetry file\n");
    printf("      --verify PATH             Verify telemetry file integrity\n");
    printf("      --print-config            Print final configuration\n");
    printf("      --run-seconds N           Exit after N seconds, useful for services and tests\n");
}

int tp_cli_parse(tp_config_t *cfg, int argc, char **argv)
{
    enum { OPT_ENABLE_DASH = 1000, OPT_DISABLE_DASH, OPT_ENABLE_EXPORT, OPT_DISABLE_EXPORT,
           OPT_LOG_LEVEL, OPT_BUFFER_SIZE, OPT_WORKERS, OPT_REPLAY, OPT_VERIFY, OPT_PRINT_CONFIG,
           OPT_RUN_SECONDS, OPT_INGEST_FILE, OPT_INGEST_SOCKET };
    static const struct option opts[] = {
        {"help", no_argument, NULL, 'h'}, {"version", no_argument, NULL, 'V'},
        {"config", required_argument, NULL, 'c'}, {"storage", required_argument, NULL, 's'},
        {"export", required_argument, NULL, 'e'}, {"enable-dashboard", no_argument, NULL, OPT_ENABLE_DASH},
        {"disable-dashboard", no_argument, NULL, OPT_DISABLE_DASH}, {"enable-export", no_argument, NULL, OPT_ENABLE_EXPORT},
        {"disable-export", no_argument, NULL, OPT_DISABLE_EXPORT}, {"log-level", required_argument, NULL, OPT_LOG_LEVEL},
        {"buffer-size", required_argument, NULL, OPT_BUFFER_SIZE}, {"workers", required_argument, NULL, OPT_WORKERS},
        {"replay", required_argument, NULL, OPT_REPLAY}, {"verify", required_argument, NULL, OPT_VERIFY},
        {"print-config", no_argument, NULL, OPT_PRINT_CONFIG}, {"run-seconds", required_argument, NULL, OPT_RUN_SECONDS},
        {"ingest-file", required_argument, NULL, OPT_INGEST_FILE},
        {"ingest-socket", required_argument, NULL, OPT_INGEST_SOCKET},
        {0, 0, 0, 0}
    };
    int c;
    while ((c = getopt_long(argc, argv, "hVc:s:e:", opts, NULL)) != -1) {
        switch (c) {
        case 'h': tp_cli_print_help(argv[0]); exit(0);
        case 'V': tp_cli_print_version(); exit(0);
        case 'c': tp_safe_copy(cfg->config_file, sizeof(cfg->config_file), optarg); break;
        case 's': tp_safe_copy(cfg->storage_path, sizeof(cfg->storage_path), optarg); break;
        case 'e': tp_safe_copy(cfg->export_endpoint, sizeof(cfg->export_endpoint), optarg); break;
        case OPT_ENABLE_DASH: cfg->dashboard = true; break;
        case OPT_DISABLE_DASH: cfg->dashboard = false; break;
        case OPT_ENABLE_EXPORT: cfg->export_enabled = true; break;
        case OPT_DISABLE_EXPORT: cfg->export_enabled = false; break;
        case OPT_LOG_LEVEL: cfg->log_level = tp_log_level_from_string(optarg); break;
        case OPT_BUFFER_SIZE: cfg->ring_capacity = (size_t)strtoull(optarg, NULL, 10); break;
        case OPT_WORKERS: cfg->worker_count = (size_t)strtoull(optarg, NULL, 10); break;
        case OPT_REPLAY: cfg->replay_mode = true; tp_safe_copy(cfg->storage_path, sizeof(cfg->storage_path), optarg); break;
        case OPT_VERIFY: cfg->verify_mode = true; tp_safe_copy(cfg->storage_path, sizeof(cfg->storage_path), optarg); break;
        case OPT_PRINT_CONFIG: cfg->print_config = true; break;
        case OPT_RUN_SECONDS: cfg->run_seconds = (unsigned)strtoul(optarg, NULL, 10); break;
        case OPT_INGEST_FILE:
            tp_safe_copy(cfg->ingest_path, sizeof(cfg->ingest_path), optarg);
            cfg->ingest_is_socket = false;
            break;
        case OPT_INGEST_SOCKET:
            tp_safe_copy(cfg->ingest_path, sizeof(cfg->ingest_path), optarg);
            cfg->ingest_is_socket = true;
            break;
        default: return TP_ERR_INVAL;
        }
    }
    return TP_OK;
}
