#ifndef XTERM_PLUS_PTY_PROCESS_H
#define XTERM_PLUS_PTY_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct XtpPty XtpPty;

XtpPty *XtpPtySpawn(char *const argv[], uint16_t columns, uint16_t rows, uint32_t cell_width,
                    uint32_t cell_height);
void XtpPtyFree(XtpPty *pty);
int XtpPtyFd(const XtpPty *pty);
pid_t XtpPtyPid(const XtpPty *pty);
ssize_t XtpPtyRead(XtpPty *pty, void *buffer, size_t length);
int XtpPtyQueue(XtpPty *pty, const void *buffer, size_t length);
int XtpPtyFlush(XtpPty *pty);
size_t XtpPtyPending(const XtpPty *pty);
int XtpPtyResize(XtpPty *pty, uint16_t columns, uint16_t rows, uint32_t cell_width,
                 uint32_t cell_height);

#endif
