#include "platform.h"
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>

int tp_platform_install_signals(sigset_t *set)
{
    sigemptyset(set);
    sigaddset(set, SIGINT);
    sigaddset(set, SIGTERM);
    return pthread_sigmask(SIG_BLOCK, set, NULL) == 0 ? TP_OK : TP_ERR;
}

int tp_platform_wait_signal(const sigset_t *set)
{
    int fd = signalfd(-1, set, SFD_CLOEXEC);
    if (fd < 0) return TP_ERR_IO;
    struct signalfd_siginfo si;
    ssize_t n = read(fd, &si, sizeof(si));
    close(fd);
    return n == sizeof(si) ? (int)si.ssi_signo : TP_ERR_IO;
}

int tp_platform_make_directory_for_file(const char *path)
{
    char copy[TP_MAX_PATH];
    snprintf(copy, sizeof(copy), "%s", path);
    char *dir = dirname(copy);
    if (!dir || strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) return TP_OK;
    return mkdir(dir, 0755) == 0 || errno == EEXIST ? TP_OK : TP_ERR_IO;
}
