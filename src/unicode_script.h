#ifndef XTERM_PLUS_UNICODE_SCRIPT_H
#define XTERM_PLUS_UNICODE_SCRIPT_H

#include <stdbool.h>
#include <stdint.h>

const char *XtpHanUnicodeVersion(void);
bool XtpUnicodeScriptHan(uint32_t codepoint);

#endif
