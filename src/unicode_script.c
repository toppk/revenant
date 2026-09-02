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

static bool
CodepointIsBlank(uint32_t codepoint)
{
        return codepoint <= 0x20U || (codepoint >= 0x7fU && codepoint <= 0xa0U) ||
               codepoint == 0x00adU || codepoint == 0x034fU || codepoint == 0x061cU ||
               codepoint == 0x1680U || (codepoint >= 0x115fU && codepoint <= 0x1160U) ||
               (codepoint >= 0x17b4U && codepoint <= 0x17b5U) ||
               (codepoint >= 0x180bU && codepoint <= 0x180fU) ||
               (codepoint >= 0x2000U && codepoint <= 0x200fU) ||
               (codepoint >= 0x2028U && codepoint <= 0x202fU) ||
               (codepoint >= 0x205fU && codepoint <= 0x206fU) || codepoint == 0x3000U ||
               codepoint == 0x3164U || (codepoint >= 0xfe00U && codepoint <= 0xfe0fU) ||
               codepoint == 0xfeffU || codepoint == 0xffa0U ||
               (codepoint >= 0xfff0U && codepoint <= 0xfff8U) ||
               (codepoint >= 0x1bca0U && codepoint <= 0x1bca3U) ||
               (codepoint >= 0x1d173U && codepoint <= 0x1d17aU) ||
               (codepoint >= 0xe0000U && codepoint <= 0xe0fffU);
}

bool
XtpUnicodeClusterRequiresInk(const char *text, size_t length)
{
        size_t offset = 0;

        while (offset < length) {
                uint32_t codepoint;
                size_t consumed;

                if (!XtpUtf8Decode(text + offset, length - offset, &codepoint, &consumed))
                        return true;
                if (!CodepointIsBlank(codepoint))
                        return true;
                offset += consumed;
        }
        return false;
}

bool
XtpUnicodeSequenceControl(uint32_t codepoint)
{
        return codepoint == 0x00adU || codepoint == 0x034fU || codepoint == 0x061cU ||
               (codepoint >= 0x115fU && codepoint <= 0x1160U) ||
               (codepoint >= 0x17b4U && codepoint <= 0x17b5U) ||
               (codepoint >= 0x180bU && codepoint <= 0x180fU) ||
               (codepoint >= 0x200bU && codepoint <= 0x200fU) ||
               (codepoint >= 0x202aU && codepoint <= 0x202eU) ||
               (codepoint >= 0x2060U && codepoint <= 0x206fU) || codepoint == 0x3164U ||
               (codepoint >= 0xfe00U && codepoint <= 0xfe0fU) || codepoint == 0xfeffU ||
               codepoint == 0xffa0U || (codepoint >= 0xfff0U && codepoint <= 0xfff8U) ||
               (codepoint >= 0x1bca0U && codepoint <= 0x1bca3U) ||
               (codepoint >= 0x1d173U && codepoint <= 0x1d17aU) ||
               (codepoint >= 0xe0000U && codepoint <= 0xe007fU) ||
               (codepoint >= 0xe0100U && codepoint <= 0xe01efU);
}
