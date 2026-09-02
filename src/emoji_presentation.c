#include "emoji_presentation.h"

#include "utf8.h"

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

static bool
RegionalIndicator(uint32_t codepoint)
{
        return codepoint >= 0x1f1e6U && codepoint <= 0x1f1ffU;
}

static bool
EmojiModifier(uint32_t codepoint)
{
        return codepoint >= 0x1f3fbU && codepoint <= 0x1f3ffU;
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

XtpEmojiClusterStyle
XtpEmojiResolveClusterStyle(const char *text, size_t length, XtpEmojiPolicy policy)
{
        XtpEmojiClusterStyle result = {0, XTP_EMOJI_STYLE_NONE, false};
        uint32_t selector = 0;
        bool keycap = false;
        bool saw_tag = false;
        bool terminated_tag = false;
        bool trailing_zwj = false;
        unsigned int regional_indicators = 0;
        size_t offset = 0;

        while (offset < length) {
                uint32_t codepoint = 0;
                size_t consumed = 0;

                if (!XtpUtf8Decode(text + offset, length - offset, &codepoint, &consumed))
                        break;
                if (offset == 0)
                        result.base = codepoint;
                else if (selector == 0 && (codepoint == 0xfe0eU || codepoint == 0xfe0fU))
                        selector = codepoint;
                if (codepoint == 0x200dU) {
                        trailing_zwj = true;
                } else if (trailing_zwj && codepoint != 0xfe0eU && codepoint != 0xfe0fU) {
                        result.requires_composition = true;
                        trailing_zwj = false;
                }
                if (codepoint == 0x20e3U)
                        keycap = true;
                if (offset != 0 && EmojiModifier(codepoint))
                        result.requires_composition = true;
                if (RegionalIndicator(codepoint))
                        ++regional_indicators;
                if (codepoint >= 0xe0020U && codepoint <= 0xe007eU)
                        saw_tag = true;
                if (codepoint == 0xe007fU)
                        terminated_tag = true;
                offset += consumed;
        }
        result.style = XtpEmojiResolveStyle(result.base, selector, policy);
        if (KeycapBase(result.base) && keycap) {
                result.requires_composition = true;
                if (selector != 0xfe0eU)
                        result.style = XTP_EMOJI_STYLE_EMOJI;
        }
        if (regional_indicators >= 2U)
                result.requires_composition = true;
        if (result.base == 0x1f3f4U && saw_tag && terminated_tag)
                result.requires_composition = true;
        return result;
}
