#include "window_ops.h"

#include <ctype.h>
#include <string.h>

static const char *const names[XTP_WINDOW_OP_COUNT] = {
    "GetSelection",
    "SetSelection",
};

const char *
XtpWindowOpName(XtpWindowOp op)
{
        return op < XTP_WINDOW_OP_COUNT ? names[op] : "?";
}

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

/* xterm grammar: comma/space separated patterns; "~pattern" re-allows matches. */
void
XtpWindowOpsParse(const char *list, XtpWindowOps *ops)
{
        const char *cursor = list != NULL ? list : "";

        memset(ops, 0, sizeof(*ops));
        for (;;) {
                const char *start;
                size_t length;
                bool negate = false;
                bool matched = false;
                XtpWindowOp op;

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
                for (op = 0; op < XTP_WINDOW_OP_COUNT; ++op) {
                        if (PatternMatches(start, length, names[op])) {
                                ops->disallowed[op] = !negate;
                                matched = true;
                        }
                }
                if (!matched)
                        ++ops->ignored_entries;
        }
}

bool
XtpWindowOpAllowed(bool allow_window_ops, const XtpWindowOps *ops, XtpWindowOp op)
{
        if (allow_window_ops)
                return true;
        return op < XTP_WINDOW_OP_COUNT && !ops->disallowed[op];
}
