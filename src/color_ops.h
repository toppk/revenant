#ifndef XTERM_PLUS_COLOR_OPS_H
#define XTERM_PLUS_COLOR_OPS_H

#include <stdbool.h>

/* xterm's tblColorOps: SetColor gates OSC 10-19 sets and 110-119 resets,
 * GetColor their queries, and GetAnsiColor OSC 4/5 queries. */
typedef enum
{
        XTP_COLOR_OP_SET_COLOR,
        XTP_COLOR_OP_GET_COLOR,
        XTP_COLOR_OP_GET_ANSI_COLOR,
        XTP_COLOR_OP_COUNT,
} XtpColorOp;

typedef struct
{
        bool disallowed[XTP_COLOR_OP_COUNT];
        unsigned int ignored_entries;
} XtpColorOps;

#define XTP_COLOR_OPS_DEFAULT_DISALLOWED "SetColor,GetColor,GetAnsiColor"

void XtpColorOpsParse(const char *list, XtpColorOps *ops);
bool XtpColorOpAllowed(bool allow_color_ops, const XtpColorOps *ops, XtpColorOp op);
const char *XtpColorOpName(XtpColorOp op);

#endif
