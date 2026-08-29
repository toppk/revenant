#ifndef XTERM_PLUS_EMOJI_PRESENTATION_H
#define XTERM_PLUS_EMOJI_PRESENTATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
        XTP_EMOJI_POLICY_UNICODE,
        XTP_EMOJI_POLICY_TEXT,
        XTP_EMOJI_POLICY_EMOJI,
} XtpEmojiPolicy;

typedef enum
{
        XTP_EMOJI_STYLE_NONE,
        XTP_EMOJI_STYLE_TEXT,
        XTP_EMOJI_STYLE_EMOJI,
} XtpEmojiStyle;

const char *XtpEmojiUnicodeVersion(void);
bool XtpEmojiHasProperty(uint32_t codepoint);
bool XtpEmojiHasDefaultPresentation(uint32_t codepoint);
XtpEmojiStyle XtpEmojiResolveStyle(uint32_t base, uint32_t selector, XtpEmojiPolicy policy);

#endif
