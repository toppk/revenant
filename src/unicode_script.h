#ifndef XTERM_PLUS_UNICODE_SCRIPT_H
#define XTERM_PLUS_UNICODE_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const char *XtpHanUnicodeVersion(void);
bool XtpUnicodeScriptHan(uint32_t codepoint);
bool XtpUtf8Decode(const char *text, size_t length, uint32_t *codepoint, size_t *consumed);
bool XtpUnicodeClusterRequiresInk(const char *text, size_t length);
bool XtpUnicodeSequenceControl(uint32_t codepoint);

#endif
