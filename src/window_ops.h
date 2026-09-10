#ifndef XTERM_PLUS_WINDOW_OPS_H
#define XTERM_PLUS_WINDOW_OPS_H

#include <stdbool.h>

/* The subset of xterm's disallowedWindowOps names that the terminal consults. */
typedef enum
{
        XTP_WINDOW_OP_GET_SELECTION,
        XTP_WINDOW_OP_SET_SELECTION,
        XTP_WINDOW_OP_GET_ICON_TITLE,
        XTP_WINDOW_OP_GET_WIN_TITLE,
        XTP_WINDOW_OP_PUSH_TITLE,
        XTP_WINDOW_OP_POP_TITLE,
        XTP_WINDOW_OP_COUNT,
} XtpWindowOp;

typedef struct
{
        bool disallowed[XTP_WINDOW_OP_COUNT];
        unsigned int ignored_entries;
} XtpWindowOps;

void XtpWindowOpsParse(const char *list, XtpWindowOps *ops);
bool XtpWindowOpAllowed(bool allow_window_ops, const XtpWindowOps *ops, XtpWindowOp op);
const char *XtpWindowOpName(XtpWindowOp op);

#endif
