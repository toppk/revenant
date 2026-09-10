#ifndef XTERM_PLUS_REQUEST_OPS_H
#define XTERM_PLUS_REQUEST_OPS_H

#include <stdbool.h>

/* Font policy is prepared for a future OSC 50 callback; Tcap currently
 * enforces GetTcap replies only. Recognizing a name does not implement it. */
typedef enum
{
        XTP_FONT_OP_SET,
        XTP_FONT_OP_GET,
        XTP_FONT_OP_COUNT
} XtpFontOp;

typedef struct
{
        bool disallowed[XTP_FONT_OP_COUNT];
        unsigned int ignored_entries;
} XtpFontOps;

#define XTP_FONT_OPS_DEFAULT_DISALLOWED "SetFont,GetFont"

void XtpFontOpsParse(const char *list, XtpFontOps *ops);
bool XtpFontOpAllowed(bool allow, const XtpFontOps *ops, XtpFontOp op);

typedef enum
{
        XTP_TCAP_OP_SET,
        XTP_TCAP_OP_GET,
        XTP_TCAP_OP_COUNT
} XtpTcapOp;

typedef struct
{
        bool disallowed[XTP_TCAP_OP_COUNT];
        unsigned int ignored_entries;
} XtpTcapOps;

#define XTP_TCAP_OPS_DEFAULT_DISALLOWED "SetTcap,GetTcap"

void XtpTcapOpsParse(const char *list, XtpTcapOps *ops);
bool XtpTcapOpAllowed(bool allow, const XtpTcapOps *ops, XtpTcapOp op);

#endif
