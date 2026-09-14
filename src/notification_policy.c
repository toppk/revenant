#include "notification_policy.h"

#include "utf8.h"

#include <stdlib.h>
#include <string.h>

XtpNotifyGateResult
XtpNotifyGateCheck(XtpNotifyGate *gate, uint64_t now_ms, bool *first_denial)
{
        *first_denial = false;
        if (gate->failing && now_ms < gate->backoff_until) {
                *first_denial = !gate->backoff_reported;
                gate->backoff_reported = true;
                return XTP_NOTIFY_GATE_BACKOFF;
        }
        if (gate->attempt_count == XTP_NOTIFY_RATE_BURST &&
            now_ms - gate->attempts[gate->oldest] < XTP_NOTIFY_RATE_WINDOW_MS) {
                *first_denial = !gate->limit_reported;
                gate->limit_reported = true;
                return XTP_NOTIFY_GATE_RATE_LIMITED;
        }
        if (gate->attempt_count < XTP_NOTIFY_RATE_BURST) {
                gate->attempts[(gate->oldest + gate->attempt_count) % XTP_NOTIFY_RATE_BURST] =
                    now_ms;
                ++gate->attempt_count;
        } else {
                gate->attempts[gate->oldest] = now_ms;
                gate->oldest = (gate->oldest + 1U) % XTP_NOTIFY_RATE_BURST;
        }
        gate->limit_reported = false;
        gate->backoff_reported = false;
        return XTP_NOTIFY_GATE_ALLOW;
}

bool
XtpNotifyGateFailed(XtpNotifyGate *gate, uint64_t now_ms)
{
        bool first = !gate->failing;

        gate->failing = true;
        gate->backoff_until = now_ms + XTP_NOTIFY_FAILURE_BACKOFF_MS;
        gate->backoff_reported = false;
        return first;
}

bool
XtpNotifyGateSucceeded(XtpNotifyGate *gate)
{
        bool was_failing = gate->failing;

        gate->failing = false;
        gate->backoff_until = 0;
        return was_failing;
}

static size_t
EncodeUtf8(uint32_t codepoint, char *out)
{
        if (codepoint < 0x80U) {
                out[0] = (char)codepoint;
                return 1;
        }
        if (codepoint < 0x800U) {
                out[0] = (char)(0xc0U | (codepoint >> 6));
                out[1] = (char)(0x80U | (codepoint & 0x3fU));
                return 2;
        }
        if (codepoint < 0x10000U) {
                out[0] = (char)(0xe0U | (codepoint >> 12));
                out[1] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
                out[2] = (char)(0x80U | (codepoint & 0x3fU));
                return 3;
        }
        out[0] = (char)(0xf0U | (codepoint >> 18));
        out[1] = (char)(0x80U | ((codepoint >> 12) & 0x3fU));
        out[2] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
        out[3] = (char)(0x80U | (codepoint & 0x3fU));
        return 4;
}

bool
XtpNotifyTextPrepare(const uint8_t *bytes, size_t length, size_t limit, bool multiline,
                     XtpNotifyText *text)
{
        size_t offset = 0;

        memset(text, 0, sizeof(*text));
        text->text = malloc(limit + 1U);
        if (text->text == NULL)
                return false;
        while (offset < length) {
                uint32_t codepoint;
                size_t consumed;
                char encoded[4];
                size_t size;

                if (!XtpUtf8Decode((const char *)bytes + offset, length - offset, &codepoint,
                                   &consumed)) {
                        codepoint = 0xfffdU;
                        consumed = 1;
                        text->replaced = true;
                } else if (codepoint == '\n' || codepoint == '\t') {
                        /* A title is one line: newline and tab read as a space. */
                        if (!multiline)
                                codepoint = ' ';
                } else if (codepoint < 0x20U || codepoint == 0x7fU ||
                           (codepoint >= 0x80U && codepoint <= 0x9fU)) {
                        codepoint = 0xfffdU;
                        text->replaced = true;
                }
                size = EncodeUtf8(codepoint, encoded);
                if (text->length + size > limit) {
                        text->truncated = true;
                        break;
                }
                memcpy(text->text + text->length, encoded, size);
                text->length += size;
                offset += consumed;
        }
        text->text[text->length] = '\0';
        return true;
}

void
XtpNotifyTextFree(XtpNotifyText *text)
{
        free(text->text);
        memset(text, 0, sizeof(*text));
}

static const char *
MarkupEntity(char byte)
{
        switch (byte) {
        case '&':
                return "&amp;";
        case '<':
                return "&lt;";
        case '>':
                return "&gt;";
        case '"':
                return "&quot;";
        case '\'':
                return "&apos;";
        default:
                return NULL;
        }
}

char *
XtpNotifyEscapeMarkup(const char *text, size_t length)
{
        size_t size = 0;
        size_t index;
        char *escaped;
        char *out;

        for (index = 0; index < length; ++index) {
                const char *entity = MarkupEntity(text[index]);

                size += entity != NULL ? strlen(entity) : 1U;
        }
        escaped = malloc(size + 1U);
        if (escaped == NULL)
                return NULL;
        out = escaped;
        for (index = 0; index < length; ++index) {
                const char *entity = MarkupEntity(text[index]);

                if (entity != NULL) {
                        memcpy(out, entity, strlen(entity));
                        out += strlen(entity);
                } else {
                        *out++ = text[index];
                }
        }
        *out = '\0';
        return escaped;
}
