#include "terminal_ui.h"
#include <stdio.h>

void tp_terminal_restore(void)
{
    printf("\033[0m\033[?25h");
    fflush(stdout);
}
