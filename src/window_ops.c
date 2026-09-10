#include "window_ops.h"

#include "ops_list.h"

static const XtpOpsEntry entries[XTP_WINDOW_OP_COUNT] = {
    {"GetSelection", 0}, {"SetSelection", 0}, {"GetIconTitle", 20},
    {"GetWinTitle", 21}, {"PushTitle", 22},   {"PopTitle", 23},
};

const char *
XtpWindowOpName(XtpWindowOp op)
{
        return op < XTP_WINDOW_OP_COUNT ? entries[op].name : "?";
}

void
XtpWindowOpsParse(const char *list, XtpWindowOps *ops)
{
        ops->ignored_entries = XtpOpsListParse(list, entries, XTP_WINDOW_OP_COUNT, ops->disallowed);
}

bool
XtpWindowOpAllowed(bool allow_window_ops, const XtpWindowOps *ops, XtpWindowOp op)
{
        if (allow_window_ops)
                return true;
        return op < XTP_WINDOW_OP_COUNT && !ops->disallowed[op];
}
