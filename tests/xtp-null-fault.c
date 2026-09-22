/* Preload for xvfb-pty-closed-stdio.sh: faults /dev/null opens, logs X, socket and PTY calls. */

#define _GNU_SOURCE
#undef _FILE_OFFSET_BITS

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct termios;
struct winsize;

typedef int (*OpenFunction)(const char *, int, ...);

static int log_fd = -1;
static int null_opens;
static char fault[16];

static void (*Next(const char *symbol))(void)
{
        void *address = dlsym(RTLD_NEXT, symbol);
        void (*function)(void) = NULL;

        if (address != NULL)
                memcpy(&function, &address, sizeof(function));
        return function;
}

__attribute__((constructor)) static void
OpenLog(void)
{
        const char *program = getenv("XTP_NULL_FAULT_PROGRAM");
        const char *path = getenv("XTP_NULL_FAULT_LOG");
        const char *mode = getenv("XTP_NULL_FAULT");
        OpenFunction real_open = (OpenFunction)Next("open");
        int fd;

        if (program == NULL || strcmp(program, program_invocation_short_name) != 0)
                return;
        if (mode != NULL)
                (void)snprintf(fault, sizeof(fault), "%s", mode);
        /* The terminal's child must start without the fault. */
        (void)unsetenv("LD_PRELOAD");
        if (path == NULL || real_open == NULL)
                return;
        fd = real_open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (fd < 0)
                return;
        /* A standard descriptor may be closed; the log must not take its number. */
        log_fd = fcntl(fd, F_DUPFD_CLOEXEC, 100);
        close(fd);
}

static void
Record(const char *format, ...)
{
        char line[256];
        va_list arguments;
        int length;

        if (log_fd < 0)
                return;
        va_start(arguments, format);
        length = vsnprintf(line, sizeof(line), format, arguments);
        va_end(arguments);
        if (length > 0)
                (void)write(log_fd, line,
                            (size_t)length < sizeof(line) ? (size_t)length : sizeof(line) - 1);
}

static int
NeedsMode(int flags)
{
        return (flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE;
}

static int
OpenWithFault(const char *symbol, const char *path, int flags, mode_t mode)
{
        OpenFunction real_open = (OpenFunction)Next(symbol);
        int fd;
        int moved;

        if (path == NULL || strcmp(path, "/dev/null") != 0 || fault[0] == '\0')
                return real_open(path, flags, mode);
        null_opens++;
        if (strcmp(fault, "fail") == 0) {
                Record("open /dev/null -> EACCES\n");
                errno = EACCES;
                return -1;
        }
        if (strcmp(fault, "eintr") == 0 && null_opens == 1) {
                Record("open /dev/null -> EINTR\n");
                errno = EINTR;
                return -1;
        }
        fd = real_open(path, flags, mode);
        if (strcmp(fault, "elsewhere") == 0 && null_opens == 1 && fd >= 0) {
                moved = fcntl(fd, F_DUPFD, 20);
                close(fd);
                fd = moved;
        }
        Record("open /dev/null -> %d\n", fd);
        return fd;
}

int
open(const char *path, int flags, ...)
{
        va_list arguments;
        mode_t mode = 0;

        if (NeedsMode(flags)) {
                va_start(arguments, flags);
                mode = (mode_t)va_arg(arguments, int);
                va_end(arguments);
        }
        return OpenWithFault("open", path, flags, mode);
}

int
open64(const char *path, int flags, ...)
{
        va_list arguments;
        mode_t mode = 0;

        if (NeedsMode(flags)) {
                va_start(arguments, flags);
                mode = (mode_t)va_arg(arguments, int);
                va_end(arguments);
        }
        return OpenWithFault("open64", path, flags, mode);
}

void *
XOpenDisplay(const char *name)
{
        void *(*real)(const char *) = (void *(*)(const char *))Next("XOpenDisplay");

        Record("XOpenDisplay\n");
        return real(name);
}

int
socket(int domain, int type, int protocol)
{
        int (*real)(int, int, int) = (int (*)(int, int, int))Next("socket");

        Record("socket\n");
        return real(domain, type, protocol);
}

pid_t
forkpty(int *master, char *name, const struct termios *termios, const struct winsize *size)
{
        pid_t (*real)(int *, char *, const struct termios *, const struct winsize *) = (pid_t (*)(
            int *, char *, const struct termios *, const struct winsize *))Next("forkpty");

        Record("forkpty\n");
        return real(master, name, termios, size);
}
