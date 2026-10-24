#ifndef TP_SEL4_ADAPTER_H
#define TP_SEL4_ADAPTER_H

#include "types.h"

int tp_is_sel4_env(void);
int tp_send_event_separated(void *transport_ctx, const tp_event_t *event);
int tp_recv_event_separated(void *transport_ctx, tp_event_t *event);
void *tp_alloc_bounded(size_t size);

#endif
