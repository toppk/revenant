#include "unicode_script.h"

#include <stddef.h>

typedef struct
{
        uint32_t first;
        uint32_t last;
} XtpUnicodeRange;

#include "han_ranges.h"

const char *
XtpHanUnicodeVersion(void)
{
        return XTP_HAN_UNICODE_VERSION;
}

bool
XtpUnicodeScriptHan(uint32_t codepoint)
{
        size_t first = 0;
        size_t last = sizeof(xtp_han_ranges) / sizeof(xtp_han_ranges[0]);

        while (first < last) {
                size_t middle = first + (last - first) / 2U;

                if (codepoint < xtp_han_ranges[middle].first) {
                        last = middle;
                } else if (codepoint > xtp_han_ranges[middle].last) {
                        first = middle + 1U;
                } else {
                        return true;
                }
        }
        return false;
}
