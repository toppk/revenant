#ifndef XTERM_PLUS_CHAR_CLASS_H
#define XTERM_PLUS_CHAR_CLASS_H

#include <stdint.h>

typedef struct _XtpCharClassTable XtpCharClassTable;

int XtpCharClassParse(const char *specification, XtpCharClassTable **table);
int XtpCharClassOf(const XtpCharClassTable *table, uint32_t codepoint);
void XtpCharClassFree(XtpCharClassTable *table);

#endif
