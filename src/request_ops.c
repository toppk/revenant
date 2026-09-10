#include "request_ops.h"

#include "ops_list.h"

static const XtpOpsEntry font_entries[] = {{"SetFont", 0}, {"GetFont", 0}};

void
XtpFontOpsParse(const char *list, XtpFontOps *ops)
{
        ops->ignored_entries =
            XtpOpsListParse(list, font_entries, XTP_FONT_OP_COUNT, ops->disallowed);
}

bool
XtpFontOpAllowed(bool allow, const XtpFontOps *ops, XtpFontOp op)
{
        return allow || (op < XTP_FONT_OP_COUNT && !ops->disallowed[op]);
}

static const XtpOpsEntry tcap_entries[] = {{"SetTcap", 0}, {"GetTcap", 0}};

void
XtpTcapOpsParse(const char *list, XtpTcapOps *ops)
{
        ops->ignored_entries =
            XtpOpsListParse(list, tcap_entries, XTP_TCAP_OP_COUNT, ops->disallowed);
}

bool
XtpTcapOpAllowed(bool allow, const XtpTcapOps *ops, XtpTcapOp op)
{
        return allow || (op < XTP_TCAP_OP_COUNT && !ops->disallowed[op]);
}
