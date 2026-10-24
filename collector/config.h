#pragma once

#include "types.h"

void tp_config_defaults(tp_config_t *cfg);
int tp_config_load_file(tp_config_t *cfg, const char *path);
int tp_config_apply_env(tp_config_t *cfg);
void tp_config_print(const tp_config_t *cfg);
