#ifndef XTERM_PLUS_OPS_LIST_H
#define XTERM_PLUS_OPS_LIST_H

#include <stdbool.h>
#include <stddef.h>

/* One entry of an xterm disallowed-ops table; number 0 means no numeric alias. */
typedef struct
{
        const char *name;
        unsigned int number;
} XtpOpsEntry;

/* xterm's set_flags_from_list: comma/space separated, case-insensitive names with
 * shell-style wildcards, numeric aliases, and "~pattern" re-allowing matches.
 * Returns the number of entries that matched nothing. */
unsigned int XtpOpsListParse(const char *list, const XtpOpsEntry *entries, size_t count,
                             bool *disallowed);

#endif
