#ifndef XTERM_PLUS_PIPE_COMMAND_H
#define XTERM_PLUS_PIPE_COMMAND_H

#include <X11/Intrinsic.h>

#include <stddef.h>

typedef struct XtpPipeCommand XtpPipeCommand;

/* Runs `command` through /bin/sh in `directory` (NULL keeps the current one) and feeds `text`
 * to its stdin from the Xt event loop without blocking; takes ownership of `text`. The job
 * reaps its child and frees itself when both the write and the child are finished. */
typedef void (*XtpPipeCommandDoneFn)(XtpPipeCommand *job, void *closure);
XtpPipeCommand *XtpPipeCommandStart(XtAppContext context, const char *command,
                                    const char *directory, char *text, size_t length,
                                    XtpPipeCommandDoneFn done, void *closure);
/* Teardown: stops watching, closes the pipe, and terminates and reaps the helper's process group.
 */
void XtpPipeCommandAbandon(XtpPipeCommand *job);
/* Link cell for the caller's list; `done` runs before the job frees itself. */
XtpPipeCommand **XtpPipeCommandLink(XtpPipeCommand *job);

#endif
