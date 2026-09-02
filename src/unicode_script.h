#ifndef XTERM_PLUS_UNICODE_SCRIPT_H
#define XTERM_PLUS_UNICODE_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utf8.h"

const char *XtpHanUnicodeVersion(void);
bool XtpUnicodeScriptHan(uint32_t codepoint);
bool XtpUnicodeClusterRequiresInk(const char *text, size_t length);
bool XtpUnicodeSequenceControl(uint32_t codepoint);

#endif
