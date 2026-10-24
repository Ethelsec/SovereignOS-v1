#include "canonical.h"
#include "constants.h"
#include "parser.h"
#include "utilities.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    TOP_SCHEMA = 1u << 0,
    TOP_EVENT_ID = 1u << 1,
    TOP_TIMESTAMP = 1u << 2,
    TOP_MONOTONIC = 1u << 3,
    TOP_SOURCE = 1u << 4,
    TOP_EVENT_TYPE = 1u << 5,
    TOP_SEVERITY = 1u << 6,
    TOP_HOST_ID = 1u << 7,
    TOP_PROCESS = 1u << 8,
    TOP_NETWORK = 1u << 9,
    TOP_ATTRIBUTES = 1u << 10,
    TOP_SEQUENCE = 1u << 11
};

#define TOP_REQUIRED (TOP_SCHEMA | TOP_EVENT_ID | TOP_TIMESTAMP | TOP_MONOTONIC | TOP_SOURCE | \
                      TOP_EVENT_TYPE | TOP_SEVERITY | TOP_HOST_ID | TOP_PROCESS | TOP_NETWORK | \
                      TOP_ATTRIBUTES | TOP_SEQUENCE)

enum {
    PROC_PID = 1u << 0,
    PROC_PPID = 1u << 1,
    PROC_EXECUTABLE = 1u << 2
};

enum {
    NET_PROTOCOL = 1u << 0,
    NET_LOCAL = 1u << 1,
    NET_REMOTE = 1u << 2,
    NET_STATUS = 1u << 3
};

typedef struct {
    const char *p;
    const char *end;
} jcur_t;

typedef struct {
    char schema_version[8];
    char event_id[128];
    char timestamp_utc[48];
    uint64_t monotonic_ns;
    int has_monotonic;
    char source[64];
    char event_type[48];
    char severity[16];
    char host_id[80];
    uint64_t sequence;
    int has_process;
    pid_t pid;
    pid_t ppid;
    char executable[TP_MAX_TEXT];
    int has_network;
    char net_protocol[24];
    char net_local[128];
    char net_remote[128];
    char net_status[40];
    char attributes[TP_MAX_PAYLOAD];
    size_t attributes_len;
} parsed_event_t;

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    int first;
} payload_builder_t;

static void set_reason(char *reason, size_t cap, const char *msg)
{
    if (reason && cap) snprintf(reason, cap, "%s", msg);
}

static int jfail(char *reason, size_t cap, const char *msg, int status)
{
    set_reason(reason, cap, msg);
    return status;
}

static void j_ws(jcur_t *j)
{
    while (j->p < j->end &&
           (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')) {
        ++j->p;
    }
}

static int j_peek(jcur_t *j)
{
    return j->p < j->end ? (unsigned char)*j->p : -1;
}

static int j_take(jcur_t *j, char expected)
{
    j_ws(j);
    if (j_peek(j) != (unsigned char)expected) return 0;
    ++j->p;
    return 1;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t utf8_encode(unsigned codepoint, char *out)
{
    if (codepoint < 0x80u) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint < 0x800u) {
        out[0] = (char)(0xC0u | (codepoint >> 6u));
        out[1] = (char)(0x80u | (codepoint & 0x3Fu));
        return 2;
    }
    if (codepoint < 0x10000u) {
        out[0] = (char)(0xE0u | (codepoint >> 12u));
        out[1] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
        out[2] = (char)(0x80u | (codepoint & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (codepoint >> 18u));
    out[1] = (char)(0x80u | ((codepoint >> 12u) & 0x3Fu));
    out[2] = (char)(0x80u | ((codepoint >> 6u) & 0x3Fu));
    out[3] = (char)(0x80u | (codepoint & 0x3Fu));
    return 4;
}

/*
 * Strict JSON string. Rejects raw/escaped control characters and lone
 * surrogates so that decoded values are safe to log and report. Returns
 * TP_ERR_FULL if the decoded value would not fit `cap` bytes (including NUL).
 */
static int j_string(jcur_t *j, char *out, size_t cap)
{
    j_ws(j);
    if (j_peek(j) != '"') return TP_ERR_CORRUPT;
    ++j->p;
    size_t o = 0;
    while (j->p < j->end) {
        unsigned char c = (unsigned char)*j->p++;
        if (c == '"') {
            out[o] = '\0';
            return TP_OK;
        }
        if (c < 0x20u) return TP_ERR_CORRUPT;
        if (c == '\\') {
            if (j->p >= j->end) return TP_ERR_CORRUPT;
            unsigned char esc = (unsigned char)*j->p++;
            switch (esc) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': case 'f': case 'n': case 'r': case 't':
                /* Control escapes are rejected: telemetry fields must stay
                 * single-line for logs and reports. */
                return TP_ERR_CORRUPT;
            case 'u': {
                unsigned codepoint = 0;
                if ((size_t)(j->end - j->p) < 4u) return TP_ERR_CORRUPT;
                for (int k = 0; k < 4; ++k) {
                    int hv = hex_value(j->p[k]);
                    if (hv < 0) return TP_ERR_CORRUPT;
                    codepoint = (codepoint << 4u) | (unsigned)hv;
                }
                j->p += 4;
                if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) return TP_ERR_CORRUPT;
                if (codepoint < 0x20u) return TP_ERR_CORRUPT;
                char tmp[4];
                size_t tn = utf8_encode(codepoint, tmp);
                if (o + tn + 1u > cap) return TP_ERR_FULL;
                memcpy(out + o, tmp, tn);
                o += tn;
                continue;
            }
            default:
                return TP_ERR_CORRUPT;
            }
        }
        if (o + 1u >= cap) return TP_ERR_FULL;
        out[o++] = (char)c;
    }
    return TP_ERR_CORRUPT;
}

static int j_u64(jcur_t *j, uint64_t *value)
{
    j_ws(j);
    if (j_peek(j) < '0' || j_peek(j) > '9') return TP_ERR_CORRUPT;
    const char *start = j->p;
    uint64_t acc = 0;
    while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
        unsigned digit = (unsigned)(*j->p++ - '0');
        if (acc > (UINT64_MAX - digit) / 10u) return TP_ERR_CORRUPT;
        acc = acc * 10u + digit;
    }
    if (j->p - start > 1 && *start == '0') return TP_ERR_CORRUPT;
    *value = acc;
    return TP_OK;
}

static int j_is_null(jcur_t *j)
{
    j_ws(j);
    if ((size_t)(j->end - j->p) >= 4u && memcmp(j->p, "null", 4) == 0) {
        j->p += 4;
        return 1;
    }
    return 0;
}

static int j_literal(jcur_t *j, const char *text)
{
    size_t n = strlen(text);
    j_ws(j);
    if ((size_t)(j->end - j->p) < n || memcmp(j->p, text, n) != 0) return 0;
    j->p += n;
    return 1;
}

/* Validate a JSON number and copy its literal text (attributes are opaque). */
static int j_number_text(jcur_t *j, char *out, size_t cap)
{
    j_ws(j);
    const char *start = j->p;
    if (j->p < j->end && *j->p == '-') ++j->p;
    if (j->p >= j->end || *j->p < '0' || *j->p > '9') return TP_ERR_CORRUPT;
    if (*j->p == '0') {
        ++j->p;
    } else {
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') ++j->p;
    }
    if (j->p < j->end && *j->p == '.') {
        ++j->p;
        if (j->p >= j->end || *j->p < '0' || *j->p > '9') return TP_ERR_CORRUPT;
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') ++j->p;
    }
    if (j->p < j->end && (*j->p == 'e' || *j->p == 'E')) {
        ++j->p;
        if (j->p < j->end && (*j->p == '+' || *j->p == '-')) ++j->p;
        if (j->p >= j->end || *j->p < '0' || *j->p > '9') return TP_ERR_CORRUPT;
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') ++j->p;
    }
    size_t n = (size_t)(j->p - start);
    if (n + 1u > cap) return TP_ERR_FULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return TP_OK;
}

/* Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant). */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2u;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (unsigned)((153 * ((int)m + (m > 2u ? -3 : 9)) + 2) / 5) + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static int days_in_month(int64_t y, unsigned m)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1u || m > 12u) return 0;
    if (m == 2u) {
        int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        return 28 + leap;
    }
    return days[m - 1u];
}

static int read_digits(const char *s, size_t n, int *out)
{
    int value = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = value * 10 + (s[i] - '0');
    }
    *out = value;
    return 1;
}

/* RFC 3339 timestamp, UTC (`Z`) or numeric offset. */
static int parse_rfc3339(const char *text, uint64_t *wall_ns)
{
    size_t n = strlen(text);
    if (n < 20u) return TP_ERR_INVAL;
    if (text[4] != '-' || text[7] != '-' || text[13] != ':' || text[16] != ':') return TP_ERR_INVAL;
    if (text[10] != 'T' && text[10] != 't') return TP_ERR_INVAL;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!read_digits(text, 4, &year) || !read_digits(text + 5, 2, &month) ||
        !read_digits(text + 8, 2, &day) || !read_digits(text + 11, 2, &hour) ||
        !read_digits(text + 14, 2, &minute) || !read_digits(text + 17, 2, &second)) {
        return TP_ERR_INVAL;
    }
    if (year < 1970 || year > 9999 || month < 1 || month > 12) return TP_ERR_INVAL;
    if (day < 1 || day > days_in_month(year, (unsigned)month)) return TP_ERR_INVAL;
    if (hour > 23 || minute > 59 || second > 60) return TP_ERR_INVAL;

    size_t pos = 19;
    uint64_t fraction_ns = 0;
    if (pos < n && text[pos] == '.') {
        ++pos;
        size_t digits = 0;
        while (pos < n && text[pos] >= '0' && text[pos] <= '9') {
            if (digits >= 9u) return TP_ERR_INVAL; /* sub-nanosecond is not representable */
            fraction_ns = fraction_ns * 10u + (uint64_t)(text[pos] - '0');
            ++pos;
            ++digits;
        }
        if (digits == 0u) return TP_ERR_INVAL;
        while (digits < 9u) {
            fraction_ns *= 10u;
            ++digits;
        }
    }

    int offset_seconds = 0;
    if (pos < n && (text[pos] == 'Z' || text[pos] == 'z')) {
        ++pos;
    } else if (pos < n && (text[pos] == '+' || text[pos] == '-')) {
        int oh = 0, om = 0;
        if (n - pos != 6u) return TP_ERR_INVAL;
        if (text[pos + 3] != ':') return TP_ERR_INVAL;
        if (!read_digits(text + pos + 1, 2, &oh) || !read_digits(text + pos + 4, 2, &om)) return TP_ERR_INVAL;
        if (oh > 23 || om > 59) return TP_ERR_INVAL;
        offset_seconds = oh * 3600 + om * 60;
        if (text[pos] == '+') offset_seconds = -offset_seconds;
        pos += 6;
    } else {
        return TP_ERR_INVAL;
    }
    if (pos != n) return TP_ERR_INVAL;

    int64_t seconds = days_from_civil(year, (unsigned)month, (unsigned)day) * 86400 +
                      (int64_t)hour * 3600 + (int64_t)minute * 60 + (int64_t)second + offset_seconds;
    if (seconds < 0) return TP_ERR_INVAL;
    *wall_ns = (uint64_t)seconds * 1000000000ull + fraction_ns;
    return TP_OK;
}

static int map_event_type(const char *name, tp_event_type_t *out)
{
    /* Frozen MVP allow-list from gods_eye_sunday_plan/03_EVENT_CONTRACT.md §4.
     * Anything else is rejected rather than silently coerced. */
    static const struct { const char *name; tp_event_type_t type; } table[] = {
        { "process.start", TP_TYPE_PROCESS_START },
        { "process.exit", TP_TYPE_PROCESS_EXIT },
        { "network.connection", TP_TYPE_SOCKET_CONNECT },
        { "service.request", TP_TYPE_HTTP },
        { "auth.failure", TP_TYPE_SECURITY },
        { "resource.threshold", TP_TYPE_RESOURCE_SPIKE },
        { "demo.marker", TP_TYPE_CUSTOM }
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcmp(name, table[i].name) == 0) {
            *out = table[i].type;
            return TP_OK;
        }
    }
    return TP_ERR_INVAL;
}

static int map_severity(const char *name, tp_severity_t *out, uint32_t *flags)
{
    /* Contract scale: info, low, medium, high, unknown (03_EVENT_CONTRACT.md §2). */
    if (strcmp(name, "info") == 0) { *out = TP_SEV_INFO; return TP_OK; }
    if (strcmp(name, "low") == 0) {
        /* The internal scale has no LOW rung; documented lossy mapping. */
        *out = TP_SEV_INFO;
        *flags |= TP_FLAG_SEVERITY_UNMAPPED;
        return TP_OK;
    }
    if (strcmp(name, "medium") == 0) { *out = TP_SEV_NORMAL; return TP_OK; }
    if (strcmp(name, "high") == 0) { *out = TP_SEV_HIGH; return TP_OK; }
    if (strcmp(name, "unknown") == 0) {
        *out = TP_SEV_INFO;
        *flags |= TP_FLAG_SEVERITY_UNMAPPED | TP_FLAG_SEVERITY_UNKNOWN;
        return TP_OK;
    }
    return TP_ERR_INVAL;
}

static uint64_t fnv1a64(const char *text)
{
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        hash ^= (uint64_t)*p;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int parse_event_id(const char *text, uint64_t *out)
{
    if (!*text) return TP_ERR_INVAL;
    int all_digits = 1;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') { all_digits = 0; break; }
    }
    if (all_digits) {
        uint64_t value = 0;
        for (const char *p = text; *p; ++p) {
            unsigned digit = (unsigned)(*p - '0');
            if (value > (UINT64_MAX - digit) / 10u) return TP_ERR_INVAL;
            value = value * 10u + digit;
        }
        *out = value;
        return TP_OK;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X') && text[2]) {
        uint64_t value = 0;
        for (const char *p = text + 2; *p; ++p) {
            int hv = hex_value(*p);
            if (hv < 0 || value > (UINT64_MAX >> 4u)) return TP_ERR_INVAL;
            value = (value << 4u) | (unsigned)hv;
        }
        *out = value;
        return TP_OK;
    }
    *out = fnv1a64(text);
    if (*out == 0) *out = 1;
    return TP_ERR_FULL; /* caller distinguishes "hashed" via this code */
}

static const char *path_basename(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; ++p) {
        if (*p == '/') base = p + 1;
    }
    return base;
}

static int copy_checked(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) return TP_ERR_FULL;
    memcpy(dst, src, n + 1u);
    return TP_OK;
}

static int pb_kv(payload_builder_t *b, const char *key, const char *value)
{
    char clean[256];
    size_t n = strlen(value);
    if (n >= sizeof(clean)) return TP_ERR_FULL;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)value[i];
        clean[i] = (c == ',' || c == '\n' || c == '\r') ? ';' : (char)c;
    }
    clean[n] = '\0';
    int written = snprintf(b->buf + b->len, b->cap - b->len, "%s%s=%s",
                           b->first ? "" : ",", key, clean);
    if (written < 0 || (size_t)written >= b->cap - b->len) return TP_ERR_FULL;
    b->len += (size_t)written;
    b->first = 0;
    return TP_OK;
}

static int parse_process(jcur_t *j, parsed_event_t *p, char *reason, size_t reason_len)
{
    if (j_is_null(j)) {
        p->has_process = 0;
        p->pid = 0;
        p->ppid = 0;
        p->executable[0] = '\0';
        return TP_OK;
    }
    if (!j_take(j, '{')) return jfail(reason, reason_len, "process must be an object or null", TP_ERR_CORRUPT);
    uint32_t seen = 0;
    for (;;) {
        j_ws(j);
        if (j_peek(j) == '}') { ++j->p; break; }
        char key[32];
        int rc = j_string(j, key, sizeof(key));
        if (rc == TP_ERR_FULL) return jfail(reason, reason_len, "process field name too long", TP_ERR_FULL);
        if (rc != TP_OK) return jfail(reason, reason_len, "malformed process object", TP_ERR_CORRUPT);
        if (!j_take(j, ':')) return jfail(reason, reason_len, "expected ':' in process", TP_ERR_CORRUPT);
        if (strcmp(key, "pid") == 0) {
            if (seen & PROC_PID) return jfail(reason, reason_len, "duplicate process.pid", TP_ERR_CORRUPT);
            seen |= PROC_PID;
            uint64_t v = 0;
            rc = j_u64(j, &v);
            if (rc != TP_OK || v > 2147483647ull) return jfail(reason, reason_len, "process.pid must be a non-negative integer", TP_ERR_INVAL);
            p->pid = (pid_t)v;
        } else if (strcmp(key, "ppid") == 0) {
            if (seen & PROC_PPID) return jfail(reason, reason_len, "duplicate process.ppid", TP_ERR_CORRUPT);
            seen |= PROC_PPID;
            uint64_t v = 0;
            rc = j_u64(j, &v);
            if (rc != TP_OK || v > 2147483647ull) return jfail(reason, reason_len, "process.ppid must be a non-negative integer", TP_ERR_INVAL);
            p->ppid = (pid_t)v;
        } else if (strcmp(key, "executable") == 0) {
            if (seen & PROC_EXECUTABLE) return jfail(reason, reason_len, "duplicate process.executable", TP_ERR_CORRUPT);
            seen |= PROC_EXECUTABLE;
            rc = j_string(j, p->executable, sizeof(p->executable));
            if (rc != TP_OK) return jfail(reason, reason_len, "process.executable is malformed or too long", rc);
        } else {
            return jfail(reason, reason_len, "unknown field in process object", TP_ERR_CORRUPT);
        }
        j_ws(j);
        if (j_take(j, ',')) continue;
        if (j_take(j, '}')) break;
        return jfail(reason, reason_len, "expected ',' or '}' in process", TP_ERR_CORRUPT);
    }
    p->has_process = 1;
    return TP_OK;
}

static int parse_network(jcur_t *j, parsed_event_t *p, char *reason, size_t reason_len)
{
    if (j_is_null(j)) {
        p->has_network = 0;
        return TP_OK;
    }
    if (!j_take(j, '{')) return jfail(reason, reason_len, "network must be an object or null", TP_ERR_CORRUPT);
    uint32_t seen = 0;
    for (;;) {
        j_ws(j);
        if (j_peek(j) == '}') { ++j->p; break; }
        char key[32];
        int rc = j_string(j, key, sizeof(key));
        if (rc != TP_OK) return jfail(reason, reason_len, "malformed network object", rc);
        if (!j_take(j, ':')) return jfail(reason, reason_len, "expected ':' in network", TP_ERR_CORRUPT);
        char *dst = NULL;
        size_t cap = 0;
        uint32_t bit = 0;
        if (strcmp(key, "protocol") == 0) { dst = p->net_protocol; cap = sizeof(p->net_protocol); bit = NET_PROTOCOL; }
        else if (strcmp(key, "local") == 0) { dst = p->net_local; cap = sizeof(p->net_local); bit = NET_LOCAL; }
        else if (strcmp(key, "remote") == 0) { dst = p->net_remote; cap = sizeof(p->net_remote); bit = NET_REMOTE; }
        else if (strcmp(key, "status") == 0) { dst = p->net_status; cap = sizeof(p->net_status); bit = NET_STATUS; }
        else return jfail(reason, reason_len, "unknown field in network object", TP_ERR_CORRUPT);
        if (seen & bit) return jfail(reason, reason_len, "duplicate field in network object", TP_ERR_CORRUPT);
        seen |= bit;
        rc = j_string(j, dst, cap);
        if (rc != TP_OK) return jfail(reason, reason_len, "network field is malformed or too long", rc);
        j_ws(j);
        if (j_take(j, ',')) continue;
        if (j_take(j, '}')) break;
        return jfail(reason, reason_len, "expected ',' or '}' in network", TP_ERR_CORRUPT);
    }
    p->has_network = 1;
    return TP_OK;
}

static int parse_attributes(jcur_t *j, parsed_event_t *p, char *reason, size_t reason_len)
{
    if (j_is_null(j)) {
        return jfail(reason, reason_len, "attributes must be a flat object (contract requires it)", TP_ERR_INVAL);
    }
    if (!j_take(j, '{')) return jfail(reason, reason_len, "attributes must be a flat object", TP_ERR_CORRUPT);
    p->attributes[0] = '\0';
    p->attributes_len = 0;
    int first = 1;
    for (;;) {
        j_ws(j);
        if (j_peek(j) == '}') { ++j->p; break; }
        char key[32];
        int rc = j_string(j, key, sizeof(key));
        if (rc == TP_ERR_FULL) return jfail(reason, reason_len, "attribute name too long (max 31)", TP_ERR_FULL);
        if (rc != TP_OK) return jfail(reason, reason_len, "malformed attribute name", TP_ERR_CORRUPT);
        if (!*key) return jfail(reason, reason_len, "empty attribute name", TP_ERR_INVAL);
        if (!j_take(j, ':')) return jfail(reason, reason_len, "expected ':' in attributes", TP_ERR_CORRUPT);

        char value[256];
        j_ws(j);
        if (j_peek(j) == '"') {
            rc = j_string(j, value, sizeof(value));
            if (rc == TP_ERR_FULL) return jfail(reason, reason_len, "attribute value too long (max 255)", TP_ERR_FULL);
            if (rc != TP_OK) return jfail(reason, reason_len, "malformed attribute value", TP_ERR_CORRUPT);
        } else if (j_literal(j, "true")) {
            copy_checked(value, sizeof(value), "true");
        } else if (j_literal(j, "false")) {
            copy_checked(value, sizeof(value), "false");
        } else if (j_literal(j, "null")) {
            copy_checked(value, sizeof(value), "null");
        } else {
            rc = j_number_text(j, value, sizeof(value));
            if (rc != TP_OK) return jfail(reason, reason_len, "attributes support only scalar values", TP_ERR_CORRUPT);
        }

        for (char *c = value; *c; ++c) {
            if (*c == ',') *c = ';';
        }
        size_t need = strlen(key) + strlen(value) + 2u;
        if (p->attributes_len + need > TP_MAX_PAYLOAD) {
            return jfail(reason, reason_len, "attributes exceed the payload budget", TP_ERR_FULL);
        }
        int written = snprintf(p->attributes + p->attributes_len,
                               TP_MAX_PAYLOAD - p->attributes_len, "%s%s=%s",
                               first ? "" : ",", key, value);
        if (written < 0) return jfail(reason, reason_len, "attribute serialization failed", TP_ERR_INVAL);
        p->attributes_len += (size_t)written;
        first = 0;

        j_ws(j);
        if (j_take(j, ',')) continue;
        if (j_take(j, '}')) break;
        return jfail(reason, reason_len, "expected ',' or '}' in attributes", TP_ERR_CORRUPT);
    }
    return TP_OK;
}

static int parse_top_level(jcur_t *j, parsed_event_t *p, char *reason, size_t reason_len)
{
    memset(p, 0, sizeof(*p));
    if (!j_take(j, '{')) return jfail(reason, reason_len, "frame must be a JSON object", TP_ERR_CORRUPT);
    uint32_t seen = 0;
    for (;;) {
        j_ws(j);
        if (j_peek(j) == '}') { ++j->p; break; }
        char key[32];
        int rc = j_string(j, key, sizeof(key));
        if (rc == TP_ERR_FULL) return jfail(reason, reason_len, "field name too long", TP_ERR_FULL);
        if (rc != TP_OK) return jfail(reason, reason_len, "malformed field name", TP_ERR_CORRUPT);
        if (!j_take(j, ':')) return jfail(reason, reason_len, "expected ':' after field name", TP_ERR_CORRUPT);

        uint32_t bit = 0;
        if (strcmp(key, "schema_version") == 0) bit = TOP_SCHEMA;
        else if (strcmp(key, "event_id") == 0) bit = TOP_EVENT_ID;
        else if (strcmp(key, "timestamp_utc") == 0) bit = TOP_TIMESTAMP;
        else if (strcmp(key, "timestamp_monotonic_ns") == 0) bit = TOP_MONOTONIC;
        else if (strcmp(key, "source") == 0) bit = TOP_SOURCE;
        else if (strcmp(key, "event_type") == 0) bit = TOP_EVENT_TYPE;
        else if (strcmp(key, "severity") == 0) bit = TOP_SEVERITY;
        else if (strcmp(key, "host_id") == 0) bit = TOP_HOST_ID;
        else if (strcmp(key, "process") == 0) bit = TOP_PROCESS;
        else if (strcmp(key, "network") == 0) bit = TOP_NETWORK;
        else if (strcmp(key, "attributes") == 0) bit = TOP_ATTRIBUTES;
        else if (strcmp(key, "sequence") == 0) bit = TOP_SEQUENCE;
        else return jfail(reason, reason_len, "unknown field (strict allow-list)", TP_ERR_CORRUPT);

        if (seen & bit) return jfail(reason, reason_len, "duplicate field", TP_ERR_CORRUPT);
        seen |= bit;

        switch (bit) {
        case TOP_SCHEMA:
            rc = j_string(j, p->schema_version, sizeof(p->schema_version));
            if (rc != TP_OK) return jfail(reason, reason_len, "schema_version is malformed", rc);
            break;
        case TOP_EVENT_ID:
            rc = j_string(j, p->event_id, sizeof(p->event_id));
            if (rc != TP_OK) return jfail(reason, reason_len, "event_id is malformed or too long", rc);
            break;
        case TOP_TIMESTAMP:
            rc = j_string(j, p->timestamp_utc, sizeof(p->timestamp_utc));
            if (rc != TP_OK) return jfail(reason, reason_len, "timestamp_utc is malformed or too long", rc);
            break;
        case TOP_MONOTONIC:
            if (j_is_null(j)) {
                p->has_monotonic = 0;
            } else {
                rc = j_u64(j, &p->monotonic_ns);
                if (rc != TP_OK) return jfail(reason, reason_len, "timestamp_monotonic_ns must be an integer or null", TP_ERR_INVAL);
                p->has_monotonic = 1;
            }
            break;
        case TOP_SOURCE:
            rc = j_string(j, p->source, sizeof(p->source));
            if (rc != TP_OK) return jfail(reason, reason_len, "source is malformed or too long", rc);
            break;
        case TOP_EVENT_TYPE:
            rc = j_string(j, p->event_type, sizeof(p->event_type));
            if (rc != TP_OK) return jfail(reason, reason_len, "event_type is malformed or too long", rc);
            break;
        case TOP_SEVERITY:
            rc = j_string(j, p->severity, sizeof(p->severity));
            if (rc != TP_OK) return jfail(reason, reason_len, "severity is malformed or too long", rc);
            break;
        case TOP_HOST_ID:
            rc = j_string(j, p->host_id, sizeof(p->host_id));
            if (rc != TP_OK) return jfail(reason, reason_len, "host_id is malformed or too long", rc);
            break;
        case TOP_PROCESS:
            rc = parse_process(j, p, reason, reason_len);
            if (rc != TP_OK) return rc;
            break;
        case TOP_NETWORK:
            rc = parse_network(j, p, reason, reason_len);
            if (rc != TP_OK) return rc;
            break;
        case TOP_ATTRIBUTES:
            rc = parse_attributes(j, p, reason, reason_len);
            if (rc != TP_OK) return rc;
            break;
        case TOP_SEQUENCE:
            rc = j_u64(j, &p->sequence);
            if (rc != TP_OK) return jfail(reason, reason_len, "sequence must be a non-negative integer", TP_ERR_INVAL);
            break;
        default:
            return jfail(reason, reason_len, "internal parser error", TP_ERR_INVAL);
        }

        j_ws(j);
        if (j_take(j, ',')) continue;
        if (j_take(j, '}')) break;
        return jfail(reason, reason_len, "expected ',' or '}' after field", TP_ERR_CORRUPT);
    }
    j_ws(j);
    if (j->p != j->end) return jfail(reason, reason_len, "trailing bytes after JSON object", TP_ERR_CORRUPT);
    if ((seen & TOP_REQUIRED) != TOP_REQUIRED) {
        return jfail(reason, reason_len, "missing required field(s)", TP_ERR_INVAL);
    }
    return TP_OK;
}

int tp_canonical_decode(const char *frame, size_t len, tp_canonical_event_t *out,
                        char *reason, size_t reason_len)
{
    if (!frame || !out) return TP_ERR_INVAL;
    if (reason && reason_len) reason[0] = '\0';
    if (len == 0) return jfail(reason, reason_len, "empty frame", TP_ERR_CORRUPT);
    if (len > TP_CANONICAL_MAX_FRAME) return jfail(reason, reason_len, "frame exceeds the maximum size", TP_ERR_FULL);

    jcur_t j = { .p = frame, .end = frame + len };
    parsed_event_t p;
    int rc = parse_top_level(&j, &p, reason, reason_len);
    if (rc != TP_OK) return rc;

    if (strcmp(p.schema_version, TP_CANONICAL_SCHEMA_VERSION) != 0) {
        return jfail(reason, reason_len, "unsupported schema_version (expected \"1\")", TP_ERR_INVAL);
    }
    if (!*p.source) return jfail(reason, reason_len, "source must not be empty", TP_ERR_INVAL);
    if (!*p.host_id) return jfail(reason, reason_len, "host_id must not be empty", TP_ERR_INVAL);

    tp_event_type_t type = TP_TYPE_UNKNOWN;
    if (map_event_type(p.event_type, &type) != TP_OK) {
        return jfail(reason, reason_len, "event_type is not in the supported allow-list", TP_ERR_INVAL);
    }

    tp_severity_t severity = TP_SEV_INFO;
    uint32_t flags = TP_FLAG_INGEST;
    if (map_severity(p.severity, &severity, &flags) != TP_OK) {
        return jfail(reason, reason_len, "severity is not one of info/low/medium/high/unknown", TP_ERR_INVAL);
    }

    uint64_t wall_ns = 0;
    rc = parse_rfc3339(p.timestamp_utc, &wall_ns);
    if (rc != TP_OK) return jfail(reason, reason_len, "timestamp_utc is not a valid RFC 3339 timestamp", TP_ERR_INVAL);

    uint64_t event_id = 0;
    rc = parse_event_id(p.event_id, &event_id);
    if (rc == TP_ERR_INVAL) return jfail(reason, reason_len, "event_id is not a valid identifier", TP_ERR_INVAL);
    if (rc == TP_ERR_FULL) flags |= TP_FLAG_EVENT_ID_HASHED;

    memset(out, 0, sizeof(*out));
    tp_event_t *ev = &out->event;
    ev->version = TP_EVENT_VERSION;
    ev->flags = flags;
    ev->event_id = event_id;
    ev->type = type;
    ev->severity = severity;
    ev->timestamp.wall_ns = wall_ns;
    ev->timestamp.monotonic_ns = p.monotonic_ns;
    if (!p.has_monotonic || p.monotonic_ns == 0) {
        ev->timestamp.monotonic_ns = tp_now_monotonic_ns();
        ev->flags |= TP_FLAG_MONOTONIC_RECEIVER_CLOCK;
    }
    ev->sequence = 0; /* assigned by the collector ring on push */
    out->producer_sequence = p.sequence;

    if (copy_checked(ev->source_probe, sizeof(ev->source_probe), p.source) != TP_OK) {
        return jfail(reason, reason_len, "source is longer than 31 bytes", TP_ERR_FULL);
    }
    if (copy_checked(ev->source_module, sizeof(ev->source_module), "ingress") != TP_OK) {
        return jfail(reason, reason_len, "internal source_module mapping failed", TP_ERR_INVAL);
    }
    if (copy_checked(ev->host_id, sizeof(ev->host_id), p.host_id) != TP_OK) {
        return jfail(reason, reason_len, "host_id is longer than 63 bytes", TP_ERR_FULL);
    }

    payload_builder_t pb = { .buf = (char *)ev->payload, .cap = sizeof(ev->payload), .len = 0, .first = 1 };

    /* tp_ring_push() overwrites tp_event_t.sequence with its own counter, so
     * the producer sequence is preserved here for Stage 2 traceability. */
    {
        char seq_text[32];
        snprintf(seq_text, sizeof(seq_text), "%llu", (unsigned long long)p.sequence);
        if (pb_kv(&pb, "canonical.sequence", seq_text) != TP_OK) {
            return jfail(reason, reason_len, "sequence provenance exceeds the payload budget", TP_ERR_FULL);
        }
    }

    if (ev->flags & TP_FLAG_EVENT_ID_HASHED) {
        if (pb_kv(&pb, "canonical.event_id", p.event_id) != TP_OK) {
            return jfail(reason, reason_len, "event_id provenance exceeds the payload budget", TP_ERR_FULL);
        }
    }
    if (ev->flags & TP_FLAG_SEVERITY_UNMAPPED) {
        if (pb_kv(&pb, "canonical.severity", p.severity) != TP_OK) {
            return jfail(reason, reason_len, "severity provenance exceeds the payload budget", TP_ERR_FULL);
        }
    }
    if (ev->flags & TP_FLAG_MONOTONIC_RECEIVER_CLOCK) {
        if (pb_kv(&pb, "canonical.monotonic", "receiver_clock") != TP_OK) {
            return jfail(reason, reason_len, "timestamp provenance exceeds the payload budget", TP_ERR_FULL);
        }
    }

    if (p.has_process) {
        ev->pid = p.pid;
        ev->ppid = p.ppid;
        const char *base = p.executable[0] ? path_basename(p.executable) : "unknown";
        if (copy_checked(ev->process_name, sizeof(ev->process_name), base) != TP_OK) {
            return jfail(reason, reason_len, "process.executable basename is longer than 63 bytes", TP_ERR_FULL);
        }
        /* Store the producer-supplied basename, never a local /proc path: the
         * PID belongs to the producer's domain, not to this host. */
        if (copy_checked(ev->executable_path, sizeof(ev->executable_path), base) != TP_OK) {
            return jfail(reason, reason_len, "process.executable basename is longer than 511 bytes", TP_ERR_FULL);
        }
        if (p.pid == 0) ev->flags |= TP_FLAG_PID_UNKNOWN;
    } else {
        ev->pid = 0;
        ev->ppid = 0;
        copy_checked(ev->process_name, sizeof(ev->process_name), "unknown");
        copy_checked(ev->executable_path, sizeof(ev->executable_path), "unknown");
        ev->flags |= TP_FLAG_PID_UNKNOWN;
        if (pb_kv(&pb, "canonical.pid", "unknown") != TP_OK) {
            return jfail(reason, reason_len, "pid provenance exceeds the payload budget", TP_ERR_FULL);
        }
    }

    if (p.has_network) {
        if (p.net_protocol[0] && pb_kv(&pb, "network.protocol", p.net_protocol) != TP_OK) {
            return jfail(reason, reason_len, "network metadata exceeds the payload budget", TP_ERR_FULL);
        }
        if (p.net_local[0] && pb_kv(&pb, "network.local", p.net_local) != TP_OK) {
            return jfail(reason, reason_len, "network metadata exceeds the payload budget", TP_ERR_FULL);
        }
        if (p.net_remote[0] && pb_kv(&pb, "network.remote", p.net_remote) != TP_OK) {
            return jfail(reason, reason_len, "network metadata exceeds the payload budget", TP_ERR_FULL);
        }
        if (p.net_status[0] && pb_kv(&pb, "network.status", p.net_status) != TP_OK) {
            return jfail(reason, reason_len, "network metadata exceeds the payload budget", TP_ERR_FULL);
        }
    }

    if (p.attributes_len) {
        int written = snprintf(pb.buf + pb.len, pb.cap - pb.len, "%s%s",
                               pb.first ? "" : ",", p.attributes);
        if (written < 0 || (size_t)written >= pb.cap - pb.len) {
            return jfail(reason, reason_len, "attributes exceed the payload budget", TP_ERR_FULL);
        }
        pb.len += (size_t)written;
        pb.first = 0;
    }

    ev->payload_size = (uint32_t)pb.len;

    rc = tp_validate_event(ev);
    if (rc != TP_OK) {
        return jfail(reason, reason_len, "internal mapping produced an invalid event", TP_ERR_INVAL);
    }
    return TP_OK;
}

const char *tp_canonical_status_name(int status)
{
    switch (status) {
    case TP_OK: return "ok";
    case TP_ERR_FULL: return "too-large";
    case TP_ERR_INVAL: return "invalid";
    case TP_ERR_CORRUPT: return "malformed";
    default: return "error";
    }
}
