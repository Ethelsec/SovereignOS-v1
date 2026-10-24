#include "protobuf_serializer.h"
#include "parser.h"
#include "utilities.h"
#include <stdio.h>
#include <string.h>

static void escape_json(const char *in, char *out, size_t len)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < len; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c >= 32) {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

int tp_otlp_json_serialize(const tp_event_t *event, char *out, size_t out_len)
{
    if (!event || !out || out_len == 0) return TP_ERR_INVAL;
    char payload[TP_MAX_PAYLOAD + 1], ep[TP_MAX_PAYLOAD * 2], attr[TP_MAX_TEXT * 2], analysis[TP_MAX_TEXT * 2];
    tp_decode_payload_text(event, payload, sizeof(payload));
    escape_json(payload, ep, sizeof(ep));
    escape_json(event->resource_attributes, attr, sizeof(attr));
    escape_json(event->analysis, analysis, sizeof(analysis));
    int n = snprintf(out, out_len,
        "{\"resourceLogs\":[{\"resource\":{\"attributes\":\"%s\"},\"scopeLogs\":[{\"logRecords\":[{"
        "\"timeUnixNano\":\"%llu\",\"severityText\":\"%s\",\"body\":\"%s\",\"traceId\":\"%s\","
        "\"spanId\":\"%s\",\"attributes\":{\"event.name\":\"%s\",\"pid\":%d,\"tid\":%d,"
        "\"correlation_id\":\"%llu\",\"analysis\":\"%s\"}}]}]}]}\n",
        attr, (unsigned long long)event->timestamp.wall_ns, tp_severity_name(event->severity), ep,
        event->trace_id, event->span_id, tp_event_type_name(event->type), event->pid, event->tid,
        (unsigned long long)event->correlation_id, analysis);
    return (n > 0 && (size_t)n < out_len) ? n : TP_ERR;
}

int tp_binary_serialize(const tp_event_t *event, uint8_t *out, size_t out_len, size_t *written)
{
    if (!event || !out || out_len < sizeof(*event)) return TP_ERR_INVAL;
    memcpy(out, event, sizeof(*event));
    if (written) *written = sizeof(*event);
    return TP_OK;
}

int tp_binary_deserialize(const uint8_t *buf, size_t len, tp_event_t *event)
{
    if (!buf || !event || len < sizeof(*event)) return TP_ERR_INVAL;
    memcpy(event, buf, sizeof(*event));
    return TP_OK;
}
