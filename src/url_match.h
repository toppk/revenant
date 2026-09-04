#ifndef XTERM_PLUS_URL_MATCH_H
#define XTERM_PLUS_URL_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XTP_URL_MAX_LENGTH 4096U

typedef struct
{
        size_t start;
        size_t end;
} XtpUrlMatch;

bool XtpUrlMatchAt(const uint8_t *text, size_t length, size_t pointer_start, size_t pointer_end,
                   XtpUrlMatch *match);

#endif
