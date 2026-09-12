#include "urgency.h"

#include <X11/Xutil.h>

bool
XtpUrgencyApply(Display *display, Window window, bool urgent)
{
        XWMHints *hints;
        bool result;

        if (display == NULL || window == None)
                return false;
        hints = XGetWMHints(display, window);
        if (hints == NULL)
                return false;
        if (urgent)
                hints->flags |= XUrgencyHint;
        else
                hints->flags &= ~XUrgencyHint;
        result = XSetWMHints(display, window, hints) != 0;
        XFree(hints);
        return result;
}
