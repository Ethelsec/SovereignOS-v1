#include "logger.h"
#include "utilities.h"
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static tp_log_level_t g_level = TP_LOG_INFO;
static int g_color = 1;

static const char *level_text(tp_log_level_t level)
{
    switch (level) {
    case TP_LOG_FATAL: return "FATAL";
    case TP_LOG_ERROR: return "ERROR";
    case TP_LOG_WARNING: return "WARNING";
    case TP_LOG_SUCCESS: return "SUCCESS";
    case TP_LOG_INFO: return "INFO";
    case TP_LOG_DEBUG: return "DEBUG";
    case TP_LOG_TRACE: return "TRACE";
    default: return "INFO";
    }
}

static const char *level_prefix(tp_log_level_t level)
{
    if (level == TP_LOG_SUCCESS) return "[+]";
    if (level == TP_LOG_WARNING) return "[!]";
    if (level == TP_LOG_ERROR || level == TP_LOG_FATAL) return "[-]";
    return "[*]";
}

static const char *level_color(tp_log_level_t level)
{
    switch (level) {
    case TP_LOG_FATAL:
    case TP_LOG_ERROR: return "\033[31m";
    case TP_LOG_WARNING: return "\033[33m";
    case TP_LOG_SUCCESS: return "\033[32m";
    case TP_LOG_DEBUG:
    case TP_LOG_TRACE: return "\033[36m";
    default: return "\033[0m";
    }
}

int tp_logger_init(tp_log_level_t level, int color_enabled)
{
    g_level = level;
    g_color = color_enabled;
    return TP_OK;
}

void tp_logger_set_level(tp_log_level_t level) { g_level = level; }
void tp_logger_set_color(int enabled) { g_color = enabled; }
void tp_logger_close(void) { fflush(stderr); }

void tp_log(tp_log_level_t level, const char *module, uint64_t correlation_id, const char *fmt, ...)
{
    if (level > g_level) return;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    time_t sec = (time_t)(tp_now_wall_ns() / 1000000000ull);
    struct tm tmv;
    localtime_r(&sec, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S%z", &tmv);

    long tid = syscall(SYS_gettid);
    int cpu = sched_getcpu();
    pthread_mutex_lock(&g_log_lock);
    if (g_color) fputs(level_color(level), stderr);
    if (correlation_id) {
        fprintf(stderr, "%s %s %-7s module=%s tid=%ld cpu=%d corr=%llu %s\n",
                level_prefix(level), ts, level_text(level), module ? module : "core",
                tid, cpu, (unsigned long long)correlation_id, msg);
    } else {
        fprintf(stderr, "%s %s %-7s module=%s tid=%ld cpu=%d %s\n",
                level_prefix(level), ts, level_text(level), module ? module : "core", tid, cpu, msg);
    }
    if (g_color) fputs("\033[0m", stderr);
    pthread_mutex_unlock(&g_log_lock);
}

tp_log_level_t tp_log_level_from_string(const char *text)
{
    if (!text) return TP_LOG_INFO;
    if (strcasecmp(text, "fatal") == 0) return TP_LOG_FATAL;
    if (strcasecmp(text, "error") == 0) return TP_LOG_ERROR;
    if (strcasecmp(text, "warning") == 0 || strcasecmp(text, "warn") == 0) return TP_LOG_WARNING;
    if (strcasecmp(text, "success") == 0) return TP_LOG_SUCCESS;
    if (strcasecmp(text, "debug") == 0) return TP_LOG_DEBUG;
    if (strcasecmp(text, "trace") == 0) return TP_LOG_TRACE;
    return TP_LOG_INFO;
}
