#pragma once
#include "types.h"
#include <signal.h>

int tp_platform_install_signals(sigset_t *set);
int tp_platform_wait_signal(const sigset_t *set);
int tp_platform_make_directory_for_file(const char *path);
