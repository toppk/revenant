#ifndef XTERM_PLUS_UTF8_H
#define XTERM_PLUS_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool XtpUtf8Decode(const char *text, size_t length, uint32_t *codepoint, size_t *consumed);

#endif
