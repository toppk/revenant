#include "ops_list.h"

#include <ctype.h>
#include <string.h>

/* xterm's x_wildstrcmp: case-insensitive with shell-style "*" and "?". */
static bool
PatternMatches(const char *pattern, size_t length, const char *name)
{
        size_t index = 0;

        while (index < length) {
                char c = pattern[index];

                if (c == '*') {
                        size_t skip = index + 1U;

                        while (skip < length && pattern[skip] == '*')
                                ++skip;
                        if (skip == length)
                                return true;
                        for (;;) {
                                if (PatternMatches(pattern + skip, length - skip, name))
                                        return true;
                                if (*name == '\0')
                                        return false;
                                ++name;
                        }
                }
                if (*name == '\0')
                        return false;
                if (c != '?' && tolower((unsigned char)c) != tolower((unsigned char)*name))
                        return false;
                ++index;
                ++name;
        }
        return *name == '\0';
}

static bool
NumberMatches(const char *entry, size_t length, unsigned int number)
{
        unsigned int value = 0;
        size_t index;

        if (number == 0 || length == 0 || length > 5)
                return false;
        for (index = 0; index < length; ++index) {
                if (!isdigit((unsigned char)entry[index]))
                        return false;
                value = value * 10U + (unsigned int)(entry[index] - '0');
        }
        return value == number;
}

unsigned int
XtpOpsListParse(const char *list, const XtpOpsEntry *entries, size_t count, bool *disallowed)
{
        const char *cursor = list != NULL ? list : "";
        unsigned int ignored = 0;

        memset(disallowed, 0, count * sizeof(*disallowed));
        for (;;) {
                const char *start;
                size_t length;
                size_t op;
                bool negate = false;
                bool matched = false;

                while (*cursor == ',' || isspace((unsigned char)*cursor))
                        ++cursor;
                if (*cursor == '\0')
                        break;
                if (*cursor == '~') {
                        negate = true;
                        ++cursor;
                }
                start = cursor;
                while (*cursor != '\0' && *cursor != ',' && !isspace((unsigned char)*cursor))
                        ++cursor;
                length = (size_t)(cursor - start);
                for (op = 0; op < count; ++op) {
                        if (PatternMatches(start, length, entries[op].name) ||
                            NumberMatches(start, length, entries[op].number)) {
                                disallowed[op] = !negate;
                                matched = true;
                        }
                }
                if (!matched)
                        ++ignored;
        }
        return ignored;
}
