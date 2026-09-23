#ifndef XTERM_PLUS_PTY_PROCESS_H
#define XTERM_PLUS_PTY_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* TERM and XTGETTCAP TN name used when termName is unset or empty. */
#define XTP_TERM_NAME_DEFAULT "xterm-256color"

typedef struct XtpPty XtpPty;

/* shell_path NULL runs argv through PATH; otherwise argv[0] is the shell's name and a failed
 * exec reports itself on the terminal as xterm does. */
XtpPty *XtpPtySpawn(const char *shell_path, char *const argv[], const char *term_name,
                    uint16_t columns, uint16_t rows, uint32_t cell_width, uint32_t cell_height);
/* xterm's shell: $SHELL if an absolute executable file, else the password-file shell when
 * /etc/shells lists it, else /bin/sh. The caller frees it. */
char *XtpPtyResolveShell(void);
/* The shell's argv[0]: its basename, with a leading dash for a login shell. */
char *XtpPtyShellName(const char *shell_path, bool login);
void XtpPtyFree(XtpPty *pty);
int XtpPtyFd(const XtpPty *pty);
pid_t XtpPtyPid(const XtpPty *pty);
ssize_t XtpPtyRead(XtpPty *pty, void *buffer, size_t length);
int XtpPtyQueue(XtpPty *pty, const void *buffer, size_t length);
int XtpPtyFlush(XtpPty *pty);
size_t XtpPtyPending(const XtpPty *pty);
/* 1 once the child is reaped (it is then forgotten), 0 while it runs, -1 on error. */
int XtpPtyReap(XtpPty *pty, int *status);
/* Drops bytes queued for writing to the child; returns the byte count. */
size_t XtpPtyDiscard(XtpPty *pty);
int XtpPtyResize(XtpPty *pty, uint16_t columns, uint16_t rows, uint32_t cell_width,
                 uint32_t cell_height);

#endif
