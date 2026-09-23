#ifndef XTERM_PLUS_TITLE_ENCODING_H
#define XTERM_PLUS_TITLE_ENCODING_H

#include <stdbool.h>

/* utf8Title values, numbered as in xterm; default stays set only in a UTF-8 locale. */
typedef enum
{
        XTP_UTF8_TITLE_FALSE = 0,
        XTP_UTF8_TITLE_TRUE = 1,
        XTP_UTF8_TITLE_ALWAYS = 2,
        XTP_UTF8_TITLE_DEFAULT = 3,
} XtpUtf8Title;

/* Parses a resource value; returns false for an unrecognized word. */
bool XtpUtf8TitleParse(const char *text, XtpUtf8Title *value);
/* xterm's set_utf8_feature for its medium locale setting. */
XtpUtf8Title XtpUtf8TitleResolve(XtpUtf8Title value, bool utf8_locale);
/* The label xterm's ChangeGroup hands to Xt and to the EWMH property; the caller frees it. */
char *XtpTitleEncode(const char *value, bool utf8_title, bool utf8_locale);

#endif
