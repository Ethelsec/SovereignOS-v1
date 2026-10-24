#ifndef GUEST_SEL4_BUFFER_H
#define GUEST_SEL4_BUFFER_H

#include <stddef.h>

void gs_buffer_init(void);
int gs_buffer_push_guest(const void *data, size_t len);
int gs_buffer_push_sel4(const void *data, size_t len);
int gs_buffer_pop_guest(void *out, size_t *out_len);
int gs_buffer_pop_sel4(void *out, size_t *out_len);
void gs_buffer_stats(void);

#endif
