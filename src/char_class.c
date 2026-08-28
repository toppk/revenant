#include "char_class.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct
{
        uint32_t low;
        uint32_t high;
        int value;
} CharClassRange;

struct _XtpCharClassTable
{
        CharClassRange *ranges;
        size_t count;
};

static const char *
SkipSpace(const char *cursor)
{
        while (*cursor != '\0' && isspace((unsigned char)*cursor))
                ++cursor;
        return cursor;
}

static int
ParseNumber(const char **cursor, unsigned long *value)
{
        char *end;

        *cursor = SkipSpace(*cursor);
        if (!isdigit((unsigned char)**cursor))
                return -1;
        errno = 0;
        *value = strtoul(*cursor, &end, 0);
        if (errno != 0 || end == *cursor)
                return -1;
        *cursor = end;
        return 0;
}

int
XtpCharClassParse(const char *specification, XtpCharClassTable **table)
{
        XtpCharClassTable *result = NULL;
        const char *cursor;

        if (table == NULL)
                return -1;
        if (specification == NULL) {
                *table = NULL;
                return 0;
        }
        cursor = SkipSpace(specification);
        if (*cursor == '\0')
                return -1;
        result = calloc(1, sizeof(*result));
        if (result == NULL)
                return -1;
        for (;;) {
                CharClassRange range;
                CharClassRange *grown;
                unsigned long value;

                if (ParseNumber(&cursor, &value) != 0 || value > 0x10ffffUL)
                        goto invalid;
                range.low = (uint32_t)value;
                range.high = range.low;
                range.value = -1;
                cursor = SkipSpace(cursor);
                if (*cursor == '-') {
                        ++cursor;
                        if (ParseNumber(&cursor, &value) != 0 || value > 0x10ffffUL)
                                goto invalid;
                        range.high = (uint32_t)value;
                }
                cursor = SkipSpace(cursor);
                if (*cursor == ':') {
                        ++cursor;
                        if (ParseNumber(&cursor, &value) != 0 || value > INT_MAX)
                                goto invalid;
                        range.value = (int)value;
                }
                if (range.high < range.low || result->count == SIZE_MAX / sizeof(*result->ranges))
                        goto invalid;
                grown = realloc(result->ranges, (result->count + 1U) * sizeof(*result->ranges));
                if (grown == NULL)
                        goto invalid;
                result->ranges = grown;
                result->ranges[result->count++] = range;
                cursor = SkipSpace(cursor);
                if (*cursor == '\0')
                        break;
                if (*cursor != ',')
                        goto invalid;
                cursor = SkipSpace(cursor + 1);
                if (*cursor == '\0')
                        goto invalid;
        }
        *table = result;
        return 0;

invalid:
        XtpCharClassFree(result);
        return -1;
}

static int
DefaultCharacterClass(uint32_t codepoint)
{
        int result = (int)codepoint;

        if (codepoint == 0)
                result = 32;
        if (codepoint >= 1 && codepoint <= 31)
                result = 1;
        if (codepoint == '\t')
                result = 32;
        if ((codepoint >= '0' && codepoint <= '9') || (codepoint >= 'A' && codepoint <= 'Z') ||
            codepoint == '_' || (codepoint >= 'a' && codepoint <= 'z'))
                result = 48;
        if (codepoint >= 127 && codepoint <= 159)
                result = 1;
        if (codepoint >= 160 && codepoint <= 191)
                result = (int)codepoint;
        if (codepoint >= 192 && codepoint <= 255)
                result = 48;
        if (codepoint == 215 || codepoint == 247)
                result = (int)codepoint;
        if (codepoint >= 0x0100 && codepoint <= 0xffdf)
                result = 48;
        if (codepoint == 0x037e || codepoint == 0x0387 ||
            (codepoint >= 0x055a && codepoint <= 0x055f) || codepoint == 0x0589 ||
            (codepoint >= 0x0700 && codepoint <= 0x070d) ||
            (codepoint >= 0x104a && codepoint <= 0x104f) || codepoint == 0x10fb ||
            (codepoint >= 0x1361 && codepoint <= 0x1368) ||
            (codepoint >= 0x166d && codepoint <= 0x166e) ||
            (codepoint >= 0x17d4 && codepoint <= 0x17dc) ||
            (codepoint >= 0x1800 && codepoint <= 0x180a))
                result = (int)codepoint;
        if (codepoint >= 0x2000 && codepoint <= 0x200a)
                result = 32;
        if (codepoint >= 0x200b && codepoint <= 0x200f)
                result = 1;
        if (codepoint >= 0x2010 && codepoint <= 0x27ff)
                result = (int)codepoint;
        if ((codepoint >= 0x202a && codepoint <= 0x202e) ||
            (codepoint >= 0x2060 && codepoint <= 0x206f))
                result = 1;
        if (codepoint >= 0x2070 && codepoint <= 0x207f)
                result = 0x2070;
        if (codepoint >= 0x2080 && codepoint <= 0x208f)
                result = 0x2080;
        if (codepoint == 0x3000)
                result = 32;
        if (codepoint >= 0x3001 && codepoint <= 0x3020)
                result = (int)codepoint;
        if (codepoint >= 0x3040 && codepoint <= 0x309f)
                result = 0x3040;
        if (codepoint >= 0x30a0 && codepoint <= 0x30ff)
                result = 0x30a0;
        if (codepoint >= 0x3300 && codepoint <= 0x9fff)
                result = 0x4e00;
        if (codepoint >= 0xac00 && codepoint <= 0xd7a3)
                result = 0xac00;
        if (codepoint >= 0xf900 && codepoint <= 0xfaff)
                result = 0x4e00;
        if (codepoint >= 0xfe30 && codepoint <= 0xfe6b)
                result = (int)codepoint;
        if (codepoint == 0xfeff || (codepoint >= 0xfff9 && codepoint <= 0xfffb))
                result = 1;
        if ((codepoint >= 0xff00 && codepoint <= 0xff0f) ||
            (codepoint >= 0xff1a && codepoint <= 0xff20) ||
            (codepoint >= 0xff3b && codepoint <= 0xff40) ||
            (codepoint >= 0xff5b && codepoint <= 0xff64))
                result = (int)codepoint;
        return result;
}

int
XtpCharClassOf(const XtpCharClassTable *table, uint32_t codepoint)
{
        int result = DefaultCharacterClass(codepoint);
        size_t index;

        if (table == NULL)
                return result;
        for (index = 0; index < table->count; ++index) {
                const CharClassRange *range = &table->ranges[index];

                if (codepoint >= range->low && codepoint <= range->high)
                        result = range->value < 0 ? (int)codepoint : range->value;
        }
        return result;
}

void
XtpCharClassFree(XtpCharClassTable *table)
{
        if (table != NULL) {
                free(table->ranges);
                free(table);
        }
}
