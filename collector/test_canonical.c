#include "canonical.h"
#include "constants.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (line %d)\n", #expr, __LINE__); return 1; } } while (0)

static const char *D_EID = "demo-producer-000042";
static const char *D_TS = "2026-10-23T12:00:00Z";
static const char *D_MONO = "1000000000";
static const char *D_SRC = "replay";
static const char *D_TYPE = "process.start";
static const char *D_SEV = "info";
static const char *D_HOST = "demo-guest";
static const char *D_PROC = "{\"pid\":4242,\"ppid\":1000,\"executable\":\"/usr/bin/curl\"}";
static const char *D_NET = "null";
static const char *D_ATTR = "{\"component\":\"demo\",\"attempt\":3}";
static const char *D_SEQ = "42";

static int build(char *buf, size_t cap, const char *eid, const char *ts, const char *mono,
                 const char *src, const char *type, const char *sev, const char *host,
                 const char *proc, const char *net, const char *attrs, const char *seq)
{
    int n = snprintf(buf, cap,
                     "{\"schema_version\":\"1\",\"event_id\":\"%s\",\"timestamp_utc\":\"%s\","
                     "\"timestamp_monotonic_ns\":%s,\"source\":\"%s\",\"event_type\":\"%s\","
                     "\"severity\":\"%s\",\"host_id\":\"%s\",\"process\":%s,\"network\":%s,"
                     "\"attributes\":%s,\"sequence\":%s}",
                     eid, ts, mono, src, type, sev, host, proc, net, attrs, seq);
    return n > 0 && (size_t)n < cap;
}

#define BUILD_DEFAULT(buf) \
    build((buf), sizeof(buf), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST, D_PROC, D_NET, D_ATTR, D_SEQ)

static int replace_once(const char *in, const char *needle, const char *repl, char *out, size_t cap)
{
    const char *p = strstr(in, needle);
    if (!p) return 0;
    size_t head = (size_t)(p - in);
    size_t repl_len = strlen(repl);
    const char *tail = p + strlen(needle);
    size_t tail_len = strlen(tail);
    if (head + repl_len + tail_len + 1u > cap) return 0;
    memcpy(out, in, head);
    memcpy(out + head, repl, repl_len);
    memcpy(out + head + repl_len, tail, tail_len + 1u);
    return 1;
}

static int expect_ok(const char *json, tp_canonical_event_t *out)
{
    char reason[TP_CANONICAL_MAX_REASON] = {0};
    int rc = tp_canonical_decode(json, strlen(json), out, reason, sizeof(reason));
    if (rc != TP_OK) {
        fprintf(stderr, "expected OK, got %s: %s\n", tp_canonical_status_name(rc), reason);
        return 0;
    }
    return 1;
}

static int expect_fail(const char *json, int expected)
{
    tp_canonical_event_t out;
    char reason[TP_CANONICAL_MAX_REASON] = {0};
    int rc = tp_canonical_decode(json, strlen(json), &out, reason, sizeof(reason));
    if (rc == TP_OK) {
        fprintf(stderr, "expected rejection, got OK\n");
        return 0;
    }
    if (expected != TP_ERR && rc != expected) {
        fprintf(stderr, "expected status %d, got %s: %s\n", expected,
                tp_canonical_status_name(rc), reason);
        return 0;
    }
    return 1;
}

static int payload_has(const tp_event_t *event, const char *needle)
{
    char buf[TP_MAX_PAYLOAD + 1];
    if (tp_decode_payload_text(event, buf, sizeof(buf)) != TP_OK) return 0;
    return strstr(buf, needle) != NULL;
}

/* One valid, complete event. */
static int test_full_event(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    CHECK(BUILD_DEFAULT(json));

    tp_canonical_event_t out;
    CHECK(expect_ok(json, &out));
    const tp_event_t *e = &out.event;

    CHECK(e->version == TP_EVENT_VERSION);
    CHECK((e->flags & TP_FLAG_INGEST) != 0);
    CHECK((e->flags & TP_FLAG_EVENT_ID_HASHED) != 0);
    CHECK((e->flags & TP_FLAG_MONOTONIC_RECEIVER_CLOCK) == 0);
    CHECK((e->flags & TP_FLAG_PID_UNKNOWN) == 0);
    CHECK(e->event_id != 0);
    CHECK(e->sequence == 0); /* assigned by the ring, not the producer */
    CHECK(out.producer_sequence == 42);
    CHECK(e->type == TP_TYPE_PROCESS_START);
    CHECK(e->severity == TP_SEV_INFO);
    CHECK(e->timestamp.wall_ns == 1792756800000000000ull);
    CHECK(e->timestamp.monotonic_ns == 1000000000ull);
    CHECK(e->pid == 4242);
    CHECK(e->ppid == 1000);
    CHECK(strcmp(e->process_name, "curl") == 0);
    CHECK(strcmp(e->executable_path, "curl") == 0);
    CHECK(strcmp(e->source_probe, "replay") == 0);
    CHECK(strcmp(e->source_module, "ingress") == 0);
    CHECK(strcmp(e->host_id, "demo-guest") == 0);
    CHECK(payload_has(e, "canonical.sequence=42"));
    CHECK(payload_has(e, "canonical.event_id=demo-producer-000042"));
    CHECK(payload_has(e, "component=demo"));
    CHECK(payload_has(e, "attempt=3"));
    CHECK(!payload_has(e, "canonical.pid=unknown"));
    CHECK(tp_validate_event(e) == TP_OK);
    return 0;
}

/* Minimum acceptable event: process/network null, empty attributes. */
static int test_minimal_event(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    CHECK(build(json, sizeof(json), "7", D_TS, "null", "proc_adapter", "demo.marker", "info",
                "guest-02", "null", "null", "{}", "1"));

    tp_canonical_event_t out;
    CHECK(expect_ok(json, &out));
    const tp_event_t *e = &out.event;
    CHECK(e->event_id == 7);
    CHECK((e->flags & TP_FLAG_EVENT_ID_HASHED) == 0);
    CHECK((e->flags & TP_FLAG_MONOTONIC_RECEIVER_CLOCK) != 0);
    CHECK((e->flags & TP_FLAG_PID_UNKNOWN) != 0);
    CHECK(e->timestamp.monotonic_ns != 0);
    CHECK(e->pid == 0);
    CHECK(strcmp(e->process_name, "unknown") == 0);
    CHECK(strcmp(e->executable_path, "unknown") == 0);
    CHECK(payload_has(e, "canonical.pid=unknown"));
    CHECK(payload_has(e, "canonical.monotonic=receiver_clock"));
    CHECK(payload_has(e, "canonical.sequence=1"));
    CHECK(e->type == TP_TYPE_CUSTOM);
    return 0;
}

static int test_event_id_forms(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    tp_canonical_event_t out;

    CHECK(build(json, sizeof(json), "42", D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.event_id == 42);
    CHECK((out.event.flags & TP_FLAG_EVENT_ID_HASHED) == 0);

    CHECK(build(json, sizeof(json), "0x2a", D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.event_id == 42);
    CHECK((out.event.flags & TP_FLAG_EVENT_ID_HASHED) == 0);

    CHECK(build(json, sizeof(json), "uuid-style-id", D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.event_id != 0);
    CHECK((out.event.flags & TP_FLAG_EVENT_ID_HASHED) != 0);
    return 0;
}

static int test_severity_mapping(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    tp_canonical_event_t out;

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, "low", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.severity == TP_SEV_INFO);
    CHECK((out.event.flags & TP_FLAG_SEVERITY_UNMAPPED) != 0);
    CHECK(payload_has(&out.event, "canonical.severity=low"));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, "medium", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.severity == TP_SEV_NORMAL);
    CHECK((out.event.flags & TP_FLAG_SEVERITY_UNMAPPED) == 0);

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, "high", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.severity == TP_SEV_HIGH);

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, "unknown", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.severity == TP_SEV_INFO);
    CHECK((out.event.flags & TP_FLAG_SEVERITY_UNMAPPED) != 0);
    CHECK((out.event.flags & TP_FLAG_SEVERITY_UNKNOWN) != 0);
    CHECK(payload_has(&out.event, "canonical.severity=unknown"));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, "critical", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));
    return 0;
}

static int test_timestamps(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    tp_canonical_event_t out;

    /* Numeric UTC offset is converted to UTC. */
    CHECK(build(json, sizeof(json), D_EID, "2026-10-23T14:00:00+02:00", D_MONO, D_SRC,
                D_TYPE, D_SEV, D_HOST, D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.timestamp.wall_ns == 1792756800000000000ull);

    /* Fractional seconds become nanoseconds. */
    CHECK(build(json, sizeof(json), D_EID, "2026-10-23T12:00:00.123456789Z", D_MONO, D_SRC,
                D_TYPE, D_SEV, D_HOST, D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_ok(json, &out));
    CHECK(out.event.timestamp.wall_ns == 1792756800123456789ull);

    /* Impossible date / time. */
    CHECK(build(json, sizeof(json), D_EID, "2026-13-40T99:99:99Z", D_MONO, D_SRC,
                D_TYPE, D_SEV, D_HOST, D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    /* Missing timezone designator. */
    CHECK(build(json, sizeof(json), D_EID, "2026-10-23T12:00:00", D_MONO, D_SRC,
                D_TYPE, D_SEV, D_HOST, D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    /* Wrong JSON type for the timestamp. */
    char replaced[TP_CANONICAL_MAX_FRAME];
    CHECK(BUILD_DEFAULT(json));
    CHECK(replace_once(json, "\"timestamp_utc\":\"2026-10-23T12:00:00Z\"",
                       "\"timestamp_utc\":123", replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_CORRUPT));
    return 0;
}

static int test_schema_and_event_types(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    char replaced[TP_CANONICAL_MAX_FRAME];

    CHECK(BUILD_DEFAULT(json));
    CHECK(replace_once(json, "\"schema_version\":\"1\"", "\"schema_version\":\"999\"",
                       replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, "file.open", D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, "auth.failure", "high", D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    {
        tp_canonical_event_t out;
        CHECK(expect_ok(json, &out));
        CHECK(out.event.type == TP_TYPE_SECURITY);
        CHECK(out.event.severity == TP_SEV_HIGH);
    }
    return 0;
}

static int test_required_fields(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    char replaced[TP_CANONICAL_MAX_FRAME];

    CHECK(BUILD_DEFAULT(json));

    CHECK(replace_once(json, "\"event_type\":\"process.start\",", "", replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_INVAL));

    CHECK(replace_once(json, ",\"sequence\":42", "", replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_INVAL));

    CHECK(replace_once(json, "\"attributes\":{\"component\":\"demo\",\"attempt\":3},",
                       "\"attributes\":null,", replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_INVAL));

    /* process/network must be explicitly present (contract: object or null). */
    CHECK(replace_once(json, "\"process\":{\"pid\":4242,\"ppid\":1000,\"executable\":\"/usr/bin/curl\"},",
                       "", replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), "", D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, "", D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, "",
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));
    return 0;
}

static int test_bad_json(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    char replaced[TP_CANONICAL_MAX_FRAME];

    CHECK(expect_fail("{\"schema_version\":\"1\"", TP_ERR_CORRUPT));
    CHECK(expect_fail("[]", TP_ERR_CORRUPT));
    CHECK(expect_fail("", TP_ERR_CORRUPT));
    CHECK(expect_fail("{\"schema_version\":\"1\",\"schema_version\":\"1\"}", TP_ERR_CORRUPT));

    CHECK(BUILD_DEFAULT(json));
    CHECK(replace_once(json, "\"sequence\":42", "\"extra\":1,\"sequence\":42",
                       replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_CORRUPT)); /* unknown field */

    CHECK(replace_once(json, "\"severity\":\"info\"",
                       "\"severity\":\"info\",\"severity\":\"info\"",
                       replaced, sizeof(replaced)));
    CHECK(expect_fail(replaced, TP_ERR_CORRUPT)); /* duplicate key */

    CHECK(BUILD_DEFAULT(json));
    char trailing[TP_CANONICAL_MAX_FRAME + 8];
    strcpy(trailing, json);
    strcat(trailing, " x");
    CHECK(expect_fail(trailing, TP_ERR_CORRUPT)); /* trailing bytes */

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, "a\\nb", D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_CORRUPT)); /* escaped control character */
    return 0;
}

static int test_limits(void)
{
    char json[TP_CANONICAL_MAX_FRAME + 64];
    char long_src[48];
    char long_value[320];
    char attrs[400];
    char big[TP_CANONICAL_MAX_FRAME + 64];

    /* Frame larger than the transport cap is refused before parsing. */
    memset(big, ' ', sizeof(big));
    tp_canonical_event_t out;
    char reason[TP_CANONICAL_MAX_REASON];
    CHECK(tp_canonical_decode(big, sizeof(big), &out, reason, sizeof(reason)) == TP_ERR_FULL);

    /* source maps into a 32-byte field: 40 bytes must be rejected, not cut. */
    memset(long_src, 'a', sizeof(long_src));
    long_src[40] = '\0';
    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, long_src, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_FULL));

    /* Attribute values are limited to 255 bytes. */
    memset(long_value, 'v', sizeof(long_value));
    long_value[300] = '\0';
    snprintf(attrs, sizeof(attrs), "{\"k\":\"%s\"}", long_value);
    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, attrs, "1"));
    CHECK(expect_fail(json, TP_ERR_FULL));

    /* Nested attributes are not part of the flat contract. */
    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                D_PROC, D_NET, "{\"nested\":{\"a\":1}}", "1"));
    CHECK(expect_fail(json, TP_ERR_CORRUPT));

    /* Negative and string PIDs are impossible values. */
    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                "{\"pid\":-1,\"ppid\":0,\"executable\":\"x\"}", D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, D_TYPE, D_SEV, D_HOST,
                "{\"pid\":\"42\"}", D_NET, D_ATTR, "1"));
    CHECK(expect_fail(json, TP_ERR_INVAL));

    CHECK(tp_canonical_decode(NULL, 0, &out, reason, sizeof(reason)) == TP_ERR_INVAL);
    return 0;
}

static int test_network_and_attributes(void)
{
    char json[TP_CANONICAL_MAX_FRAME];
    CHECK(build(json, sizeof(json), D_EID, D_TS, D_MONO, D_SRC, "network.connection", "info",
                D_HOST, D_PROC,
                "{\"protocol\":\"tcp\",\"local\":\"10.0.0.2:51000\",\"remote\":\"10.0.0.5:443\",\"status\":\"established\"}",
                "{\"framework\":\"demo\",\"ok\":true}", "9"));

    tp_canonical_event_t out;
    CHECK(expect_ok(json, &out));
    CHECK(out.event.type == TP_TYPE_SOCKET_CONNECT);
    CHECK(out.producer_sequence == 9);
    CHECK(payload_has(&out.event, "network.protocol=tcp"));
    CHECK(payload_has(&out.event, "network.local=10.0.0.2:51000"));
    CHECK(payload_has(&out.event, "network.remote=10.0.0.5:443"));
    CHECK(payload_has(&out.event, "network.status=established"));
    CHECK(payload_has(&out.event, "framework=demo"));
    CHECK(payload_has(&out.event, "ok=true"));
    return 0;
}

int tp_test_canonical(void)
{
    /* Frozen storage ABI: adding a field to tp_event_t breaks --verify/--replay
     * for existing files. Ingress must not change this size. */
    if (sizeof(tp_event_t) != 3120u) {
        fprintf(stderr, "tp_event_t size changed: %zu\n", sizeof(tp_event_t));
        return 1;
    }
    if (test_full_event()) return 1;
    if (test_minimal_event()) return 1;
    if (test_event_id_forms()) return 1;
    if (test_severity_mapping()) return 1;
    if (test_timestamps()) return 1;
    if (test_schema_and_event_types()) return 1;
    if (test_required_fields()) return 1;
    if (test_bad_json()) return 1;
    if (test_limits()) return 1;
    if (test_network_and_attributes()) return 1;
    return 0;
}

#ifdef TP_TEST_STANDALONE
int main(void) { return tp_test_canonical(); }
#endif
