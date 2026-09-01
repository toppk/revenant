#ifndef XTERM_PLUS_FONT_CHAIN_H
#define XTERM_PLUS_FONT_CHAIN_H

#include <stddef.h>

#define XTP_FONT_CHAIN_CAPACITY 2U

typedef struct
{
        char *entries[XTP_FONT_CHAIN_CAPACITY];
        size_t count;
        size_t discarded;
} XtpFontChain;

/* Parse xterm's characterized two-entry, prefix-aware Xft list grammar. */
int XtpFontChainParse(const char *configured, XtpFontChain *chain);
/* Parse only entries explicitly directed to Xft from an X11-default resource. */
int XtpFontChainParseXftEntries(const char *configured, XtpFontChain *chain);
void XtpFontChainClear(XtpFontChain *chain);

#endif
