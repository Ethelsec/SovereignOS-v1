#pragma once

#include "types.h"

int tp_cli_parse(tp_config_t *cfg, int argc, char **argv);
void tp_cli_print_help(const char *argv0);
void tp_cli_print_version(void);
