#ifndef XTERM_PLUS_URGENCY_H
#define XTERM_PLUS_URGENCY_H

#include <X11/Xlib.h>

#include <stdbool.h>

/* Sets or clears only XUrgencyHint in WM_HINTS; every other field is kept. Returns false when
 * the window has no WM_HINTS or the server call fails. */
bool XtpUrgencyApply(Display *display, Window window, bool urgent);

#endif
