#ifndef XTERM_PLUS_SEARCH_MATCH_H
#define XTERM_PLUS_SEARCH_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One codepoint of searchable text and the screen cell it came from. */
typedef struct
{
        uint32_t codepoint;
        uint64_t row;
        uint16_t column;
        uint8_t width;
        bool cluster_start;
        bool cluster_end;
} XtpSearchUnit;

typedef struct
{
        uint64_t row;
        uint16_t column;
} XtpSearchPosition;

typedef struct
{
        uint32_t *codepoints;
        size_t *failure;
        size_t length;
} XtpSearchQuery;

typedef void *(*XtpSearchRealloc)(void *pointer, size_t size);

/* NULL restores realloc; the self-test injects allocation failures. */
void XtpSearchSetAllocator(XtpSearchRealloc allocator);
void *XtpSearchReallocate(void *pointer, size_t size);
/* False on invalid UTF-8 or allocation failure; an empty query has length 0. */
bool XtpSearchQueryInit(XtpSearchQuery *query, const char *utf8, size_t length);
void XtpSearchQueryFree(XtpSearchQuery *query);

typedef bool (*XtpSearchFound)(size_t first, size_t last, void *closure);

/* Every occurrence, overlaps included, on whole clusters; stops when found returns false. */
void XtpSearchFindAll(const XtpSearchQuery *query, const XtpSearchUnit *units, size_t count,
                      XtpSearchFound found, void *closure);
/* Whether the units spell exactly the query as whole clusters. */
bool XtpSearchUnitsEqual(const XtpSearchQuery *query, const XtpSearchUnit *units, size_t count);
int XtpSearchPositionCompare(XtpSearchPosition left, XtpSearchPosition right);
/* Nearest position after or before `from` in a descending array, wrapping; count > 0. */
size_t XtpSearchPick(const XtpSearchPosition *positions, size_t count, XtpSearchPosition from,
                     bool forward, bool *wrapped);

#endif
