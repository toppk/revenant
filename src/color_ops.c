#include "color_ops.h"

#include "ops_list.h"

static const XtpOpsEntry entries[XTP_COLOR_OP_COUNT] = {
    {"SetColor", 0},
    {"GetColor", 0},
    {"GetAnsiColor", 0},
};

const char *
XtpColorOpName(XtpColorOp op)
{
        return op < XTP_COLOR_OP_COUNT ? entries[op].name : "?";
}

void
XtpColorOpsParse(const char *list, XtpColorOps *ops)
{
        ops->ignored_entries = XtpOpsListParse(list, entries, XTP_COLOR_OP_COUNT, ops->disallowed);
}

bool
XtpColorOpAllowed(bool allow_color_ops, const XtpColorOps *ops, XtpColorOp op)
{
        if (allow_color_ops)
                return true;
        return op < XTP_COLOR_OP_COUNT && !ops->disallowed[op];
}
