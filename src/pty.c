#include "pty_process.h"

#include "diagnostics.h"

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define XTP_PTY_FLUSH_LIMIT (64U * 1024U)

struct XtpPty
{
        int master;
        pid_t child;
        uint8_t *output;
        size_t output_offset;
        size_t output_length;
        size_t output_capacity;
};

static struct winsize
WindowSize(uint16_t columns, uint16_t rows, uint32_t cell_width, uint32_t cell_height)
{
        struct winsize size = {0};

        size.ws_col = columns;
        size.ws_row = rows;
        size.ws_xpixel = (unsigned short)(columns * cell_width);
        size.ws_ypixel = (unsigned short)(rows * cell_height);
        return size;
}

XtpPty *
XtpPtySpawn(char *const argv[], uint16_t columns, uint16_t rows, uint32_t cell_width,
            uint32_t cell_height)
{
        XtpPty *pty;
        struct winsize size = WindowSize(columns, rows, cell_width, cell_height);
        int flags;

        if (argv == NULL || argv[0] == NULL)
                return NULL;

        pty = calloc(1, sizeof(*pty));
        if (pty == NULL)
                return NULL;
        pty->master = -1;
        pty->child = forkpty(&pty->master, NULL, NULL, &size);
        if (pty->child < 0) {
                free(pty);
                return NULL;
        }
        if (pty->child == 0) {
                (void)setenv("TERM", "xterm-256color", 1);
                execvp(argv[0], argv);
                _exit(127);
        }

        flags = fcntl(pty->master, F_GETFL);
        if (flags >= 0)
                (void)fcntl(pty->master, F_SETFL, flags | O_NONBLOCK);
        (void)fcntl(pty->master, F_SETFD, FD_CLOEXEC);
        XtpLog(XTP_LOG_INFO, "pty", "spawned pid=%ld master-fd=%d command=%s size=%ux%u cell=%ux%u",
               (long)pty->child, pty->master, argv[0], columns, rows, cell_width, cell_height);
        return pty;
}

void
XtpPtyFree(XtpPty *pty)
{
        if (pty == NULL)
                return;
        XtpLog(XTP_LOG_INFO, "pty", "closing pid=%ld master-fd=%d", (long)pty->child, pty->master);
        if (pty->master >= 0)
                close(pty->master);
        if (pty->child > 0) {
                if (waitpid(pty->child, NULL, WNOHANG) == 0) {
                        (void)kill(pty->child, SIGHUP);
                        (void)waitpid(pty->child, NULL, WNOHANG);
                }
        }
        if (pty->output_length != 0)
                XtpLog(XTP_LOG_WARNING, "pty", "closing with queued output bytes=%zu",
                       pty->output_length);
        free(pty->output);
        free(pty);
}

int
XtpPtyFd(const XtpPty *pty)
{
        return pty != NULL ? pty->master : -1;
}

pid_t
XtpPtyPid(const XtpPty *pty)
{
        return pty != NULL ? pty->child : (pid_t)-1;
}

ssize_t
XtpPtyRead(XtpPty *pty, void *buffer, size_t length)
{
        if (pty == NULL || pty->master < 0) {
                errno = EBADF;
                return -1;
        }
        return read(pty->master, buffer, length);
}

int
XtpPtyQueue(XtpPty *pty, const void *buffer, size_t length)
{
        size_t required;
        size_t capacity;
        uint8_t *grown;

        if (pty == NULL || (buffer == NULL && length != 0)) {
                errno = EINVAL;
                return -1;
        }
        if (length > SIZE_MAX - pty->output_length) {
                errno = ENOMEM;
                return -1;
        }
        if (length == 0)
                return 0;
        required = pty->output_length + length;
        if (pty->output_capacity - pty->output_offset - pty->output_length < length &&
            pty->output_offset != 0) {
                memmove(pty->output, pty->output + pty->output_offset, pty->output_length);
                pty->output_offset = 0;
        }
        if (pty->output_capacity - pty->output_length < length) {
                capacity = pty->output_capacity != 0 ? pty->output_capacity : 4096U;
                while (capacity < required) {
                        if (capacity > SIZE_MAX / 2U) {
                                capacity = required;
                                break;
                        }
                        capacity *= 2U;
                }
                grown = realloc(pty->output, capacity);
                if (grown == NULL)
                        return -1;
                pty->output = grown;
                pty->output_capacity = capacity;
        }
        memcpy(pty->output + pty->output_offset + pty->output_length, buffer, length);
        pty->output_length += length;
        return 0;
}

int
XtpPtyFlush(XtpPty *pty)
{
        size_t flushed = 0;

        if (pty == NULL || pty->master < 0) {
                errno = EBADF;
                return -1;
        }
        while (pty->output_length != 0 && flushed < XTP_PTY_FLUSH_LIMIT) {
                size_t available = XTP_PTY_FLUSH_LIMIT - flushed;
                size_t requested = pty->output_length < available ? pty->output_length : available;
                ssize_t amount = write(pty->master, pty->output + pty->output_offset, requested);

                if (amount > 0) {
                        pty->output_offset += (size_t)amount;
                        pty->output_length -= (size_t)amount;
                        flushed += (size_t)amount;
                } else if (amount < 0 && errno == EINTR) {
                        continue;
                } else if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        return 1;
                } else {
                        return -1;
                }
        }
        if (pty->output_length == 0) {
                pty->output_offset = 0;
                return 0;
        }
        return 1;
}

size_t
XtpPtyPending(const XtpPty *pty)
{
        return pty != NULL ? pty->output_length : 0;
}

int
XtpPtyResize(XtpPty *pty, uint16_t columns, uint16_t rows, uint32_t cell_width,
             uint32_t cell_height)
{
        struct winsize size = WindowSize(columns, rows, cell_width, cell_height);

        if (pty == NULL || pty->master < 0)
                return -1;
        XtpLog(XTP_LOG_INFO, "pty", "resize pid=%ld grid=%ux%u cell=%ux%u pixels=%ux%u",
               (long)pty->child, columns, rows, cell_width, cell_height, columns * cell_width,
               rows * cell_height);
        return ioctl(pty->master, TIOCSWINSZ, &size);
}
