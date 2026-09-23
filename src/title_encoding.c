#include "title_encoding.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wctype.h>

#define BAD_ASCII '?'

bool
XtpUtf8TitleParse(const char *text, XtpUtf8Title *value)
{
        static const char *const truths[] = {"true", "on", "yes", "1"};
        static const char *const falsehoods[] = {"false", "off", "no", "0"};

        if (text == NULL)
                return false;
        for (size_t index = 0; index < sizeof(truths) / sizeof(truths[0]); ++index) {
                if (strcasecmp(text, truths[index]) == 0) {
                        *value = XTP_UTF8_TITLE_TRUE;
                        return true;
                }
                if (strcasecmp(text, falsehoods[index]) == 0) {
                        *value = XTP_UTF8_TITLE_FALSE;
                        return true;
                }
        }
        if (strcasecmp(text, "always") == 0) {
                *value = XTP_UTF8_TITLE_ALWAYS;
                return true;
        }
        if (strcasecmp(text, "default") == 0) {
                *value = XTP_UTF8_TITLE_DEFAULT;
                return true;
        }
        return false;
}

XtpUtf8Title
XtpUtf8TitleResolve(XtpUtf8Title value, bool utf8_locale)
{
        if (value == XTP_UTF8_TITLE_DEFAULT && !utf8_locale)
                return XTP_UTF8_TITLE_FALSE;
        return value;
}

/* xterm's convertFromUTF8, including its count of every following continuation byte. */
static const unsigned char *
DecodeUtf8(const unsigned char *text, unsigned int *code)
{
        int want = 0;
        int have = 1;
        unsigned int mask;
        int shift = 0;

        if ((*text & 0x80U) == 0)
                want = 1;
        else if ((*text & 0xe0U) == 0xc0U)
                want = 2;
        else if ((*text & 0xf0U) == 0xe0U)
                want = 3;
        else if ((*text & 0xf8U) == 0xf0U)
                want = 4;
        else if ((*text & 0xfcU) == 0xf8U)
                want = 5;
        else if ((*text & 0xfeU) == 0xfcU)
                want = 6;
        *code = BAD_ASCII;
        if (want == 0)
                return NULL;
        while (text[have] != '\0' && (text[have] & 0xc0U) == 0x80U)
                ++have;
        if (want != have)
                return NULL;
        mask = want == 1 ? *text : (unsigned int)*text & (0x7fU >> want);
        *code = 0;
        for (int index = 1; index < want; ++index) {
                *code |= (unsigned int)(text[want - index] & 0x3fU) << shift;
                shift += 6;
        }
        *code |= mask << shift;
        return text + want;
}

static bool
ValidUtf8(const unsigned char *text)
{
        while (*text != '\0') {
                unsigned int code;

                text = DecodeUtf8(text, &code);
                if (text == NULL || code == 0)
                        return false;
        }
        return true;
}

/* xterm's NonLatin1 without allowC1Printable: C0 and C1 controls except LF and HT. */
static unsigned int
OnlyLatin1(unsigned int code)
{
        bool control =
            code != '\n' && code != '\t' && (code < 0x20U || (code >= 0x7fU && code <= 0x9fU));

        return control ? BAD_ASCII : code;
}

char *
XtpTitleEncode(const char *value, bool utf8_title, bool utf8_locale)
{
        size_t length = strlen(value);
        unsigned char *result = malloc(length + 1U);
        bool is_utf8 = ValidUtf8((const unsigned char *)value);
        bool high = false;
        unsigned char *converted;
        unsigned char *out;

        if (result == NULL)
                return NULL;
        memcpy(result, value, length + 1U);
        if (utf8_title && is_utf8) {
                const unsigned char *next = result;
                size_t used = 0;
                bool latin1 = true;
                unsigned char *narrow = malloc(length + 1U);

                if (narrow == NULL) {
                        free(result);
                        return NULL;
                }
                while (*next != '\0') {
                        unsigned int code;

                        next = DecodeUtf8(next, &code);
                        if (code > 255U)
                                latin1 = false;
                        else if (!((code >= 32U && code <= 126U) || code >= 160U))
                                code = OnlyLatin1(code);
                        narrow[used++] = (unsigned char)code;
                }
                narrow[used] = '\0';
                if (latin1) {
                        free(result);
                        result = narrow;
                        is_utf8 = false;
                } else {
                        free(narrow);
                        for (unsigned char *cursor = result; *cursor != '\0';) {
                                unsigned int code;
                                unsigned char *skip = (unsigned char *)DecodeUtf8(cursor, &code);

                                if (iswcntrl((wint_t)code))
                                        memset(cursor, BAD_ASCII, (size_t)(skip - cursor));
                                cursor = skip;
                        }
                }
        } else {
                /* As in xterm, a valid UTF-8 label stays marked UTF-8 and is not widened. */
                for (unsigned char *cursor = result; *cursor != '\0'; ++cursor)
                        *cursor = (unsigned char)OnlyLatin1(*cursor);
        }
        if (!utf8_locale || is_utf8)
                return (char *)result;
        for (const unsigned char *cursor = result; *cursor != '\0'; ++cursor)
                high = high || *cursor > 127U;
        if (!high)
                return (char *)result;
        /* In a UTF-8 locale Xt expects UTF-8, so the Latin-1 label is widened again. */
        converted = malloc(2U * strlen((const char *)result) + 1U);
        if (converted == NULL) {
                free(result);
                return NULL;
        }
        out = converted;
        for (const unsigned char *cursor = result; *cursor != '\0'; ++cursor) {
                if (*cursor < 0x80U) {
                        *out++ = *cursor;
                } else {
                        *out++ = (unsigned char)(0xc0U | (*cursor >> 6));
                        *out++ = (unsigned char)(0x80U | (*cursor & 0x3fU));
                }
        }
        *out = '\0';
        free(result);
        return (char *)converted;
}
