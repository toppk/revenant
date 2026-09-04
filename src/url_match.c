#include "url_match.h"

#include <ctype.h>
#include <string.h>

static bool
AsciiPrefix(const uint8_t *text, size_t length, const char *prefix)
{
        size_t prefix_length = strlen(prefix);
        size_t index;

        if (length < prefix_length)
                return false;
        for (index = 0; index < prefix_length; ++index) {
                if (tolower((unsigned char)text[index]) != tolower((unsigned char)prefix[index]))
                        return false;
        }
        return true;
}

static bool
UrlTerminator(uint8_t byte)
{
        return byte <= 0x20U || byte == 0x7fU || byte == '<' || byte == '>' || byte == '"' ||
               byte == '\'' || byte == '`';
}

static bool
SentencePunctuation(uint8_t byte)
{
        return byte == '.' || byte == ',' || byte == ';' || byte == ':' || byte == '!' ||
               byte == '?';
}

static size_t
TrimCandidate(const uint8_t *text, size_t start, size_t end)
{
        size_t parentheses[2] = {0};
        size_t brackets[2] = {0};
        size_t braces[2] = {0};
        size_t index;
        bool trimmed = true;

        for (index = start; index < end; ++index) {
                if (text[index] == '(')
                        ++parentheses[0];
                else if (text[index] == ')')
                        ++parentheses[1];
                else if (text[index] == '[')
                        ++brackets[0];
                else if (text[index] == ']')
                        ++brackets[1];
                else if (text[index] == '{')
                        ++braces[0];
                else if (text[index] == '}')
                        ++braces[1];
        }
        while (end > start && trimmed) {
                uint8_t last = text[end - 1U];

                trimmed = false;
                if (SentencePunctuation(last)) {
                        --end;
                        trimmed = true;
                } else if (last == ')' && parentheses[1] > parentheses[0]) {
                        --end;
                        --parentheses[1];
                        trimmed = true;
                } else if (last == ']' && brackets[1] > brackets[0]) {
                        --end;
                        --brackets[1];
                        trimmed = true;
                } else if (last == '}' && braces[1] > braces[0]) {
                        --end;
                        --braces[1];
                        trimmed = true;
                }
        }
        return end;
}

static bool
HostPresent(const uint8_t *text, size_t start, size_t end, size_t scheme_length)
{
        size_t index = start + scheme_length;

        if (index >= end)
                return false;
        for (; index < end && text[index] != '/' && text[index] != '?' && text[index] != '#';
             ++index) {
                uint8_t byte = text[index];

                if (isalnum(byte) || byte >= 0x80U || byte == '[')
                        return true;
        }
        return false;
}

bool
XtpUrlMatchAt(const uint8_t *text, size_t length, size_t pointer_start, size_t pointer_end,
              XtpUrlMatch *match)
{
        size_t start;

        if (text == NULL || match == NULL || pointer_start >= pointer_end || pointer_end > length)
                return false;
        for (start = 0; start < length; ++start) {
                size_t scheme_length;
                size_t end;

                if (AsciiPrefix(text + start, length - start, "https://"))
                        scheme_length = 8U;
                else if (AsciiPrefix(text + start, length - start, "http://"))
                        scheme_length = 7U;
                else
                        continue;
                end = start + scheme_length;
                while (end < length && !UrlTerminator(text[end]) &&
                       end - start < XTP_URL_MAX_LENGTH)
                        ++end;
                if (end < length && end - start == XTP_URL_MAX_LENGTH && !UrlTerminator(text[end]))
                        continue;
                end = TrimCandidate(text, start, end);
                if (!HostPresent(text, start, end, scheme_length))
                        continue;
                if (pointer_start < end && pointer_end > start) {
                        match->start = start;
                        match->end = end;
                        return true;
                }
                if (end > start)
                        start = end - 1U;
        }
        return false;
}
