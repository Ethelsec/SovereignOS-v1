#pragma once

/*
 * Canonical event decoder (God's Eye, Member A: receiver side).
 *
 * Boundary: ReceivedFrame -> ValidatedEvent (gods_eye_sunday_plan/02_ARCHITECTURE.md §6).
 *
 * Input is one newline-delimited JSON object following
 * gods_eye_sunday_plan/03_EVENT_CONTRACT.md. Output is a tp_event_t that has
 * been mapped and validated *before* it enters the collector ring buffer.
 *
 * The collector's internal schema cannot represent every canonical field
 * faithfully. Mapping decisions and their lossy parts are documented in
 * INGEST.md; the decoder never truncates a field silently. If a canonical
 * value cannot be represented, the frame is rejected or the loss is recorded
 * as explicit provenance in the payload (see flags below).
 */

#include "types.h"
#include <stddef.h>
#include <stdint.h>

#define TP_CANONICAL_SCHEMA_VERSION "1"
#define TP_CANONICAL_MAX_FRAME 8192u
#define TP_CANONICAL_MAX_REASON 160u

/* tp_event_t.flags bits are defined in types.h (TP_FLAG_*). */

typedef struct {
    tp_event_t event;
    uint64_t producer_sequence;
} tp_canonical_event_t;

/*
 * Decode and validate one canonical JSON frame.
 *
 * Returns TP_OK on success. On failure returns TP_ERR_CORRUPT for malformed or
 * unsupported JSON, TP_ERR_FULL for an over-long field, or TP_ERR_INVAL when a
 * required value is missing/invalid. `reason` receives a short diagnostic that
 * never contains raw untrusted payload data.
 *
 * Returns TP_ERR_INVAL for NULL arguments.
 */
int tp_canonical_decode(const char *frame, size_t len, tp_canonical_event_t *out,
                        char *reason, size_t reason_len);

/* Human-readable status label for the codes returned above. */
const char *tp_canonical_status_name(int status);
