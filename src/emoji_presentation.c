#include "emoji_presentation.h"

#include <stddef.h>

typedef struct
{
        uint32_t first;
        uint32_t last;
} XtpEmojiRange;

#include "emoji_ranges.h"

static bool
InRanges(uint32_t codepoint, const XtpEmojiRange *ranges, size_t count)
{
        size_t first = 0;
        size_t last = count;

        while (first < last) {
                size_t middle = first + (last - first) / 2U;

                if (codepoint < ranges[middle].first) {
                        last = middle;
                } else if (codepoint > ranges[middle].last) {
                        first = middle + 1U;
                } else {
                        return true;
                }
        }
        return false;
}

static bool
KeycapBase(uint32_t codepoint)
{
        return codepoint == '#' || codepoint == '*' || (codepoint >= '0' && codepoint <= '9');
}

const char *
XtpEmojiUnicodeVersion(void)
{
        return XTP_EMOJI_UNICODE_VERSION;
}

bool
XtpEmojiHasProperty(uint32_t codepoint)
{
        return InRanges(codepoint, xtp_emoji_ranges,
                        sizeof(xtp_emoji_ranges) / sizeof(xtp_emoji_ranges[0]));
}

bool
XtpEmojiHasDefaultPresentation(uint32_t codepoint)
{
        return InRanges(codepoint, xtp_emoji_presentation_ranges,
                        sizeof(xtp_emoji_presentation_ranges) /
                            sizeof(xtp_emoji_presentation_ranges[0]));
}

XtpEmojiStyle
XtpEmojiResolveStyle(uint32_t base, uint32_t selector, XtpEmojiPolicy policy)
{
        if (!XtpEmojiHasProperty(base))
                return XTP_EMOJI_STYLE_NONE;
        if (selector == 0xfe0eU)
                return XTP_EMOJI_STYLE_TEXT;
        if (selector == 0xfe0fU)
                return XTP_EMOJI_STYLE_EMOJI;
        if (KeycapBase(base))
                return XTP_EMOJI_STYLE_TEXT;
        switch (policy) {
        case XTP_EMOJI_POLICY_UNICODE:
                return XtpEmojiHasDefaultPresentation(base) ? XTP_EMOJI_STYLE_EMOJI
                                                            : XTP_EMOJI_STYLE_TEXT;
        case XTP_EMOJI_POLICY_TEXT:
                return XTP_EMOJI_STYLE_TEXT;
        case XTP_EMOJI_POLICY_EMOJI:
                return XTP_EMOJI_STYLE_EMOJI;
        }
        return XTP_EMOJI_STYLE_TEXT;
}
