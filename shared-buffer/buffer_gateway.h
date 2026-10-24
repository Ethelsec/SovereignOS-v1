#ifndef BUFFER_GATEWAY_H
#define BUFFER_GATEWAY_H

#include <stdbool.h>
#include <stddef.h>

int gateway_init(const char *audit_log_path);
bool gateway_check_rate_limit(void);
bool gateway_validate_frame(const void *data, size_t len);
int gateway_forward_guest_to_sel4(const void *data, size_t len);
int gateway_forward_sel4_to_guest(const void *data, size_t len);
void gateway_stats(void);
void gateway_audit(const char *event, const char *details);

#endif
