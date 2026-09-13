#include "search_match.h"

#include "utf8.h"

#include <stdlib.h>
#include <string.h>

static XtpSearchRealloc search_realloc = realloc;

void
XtpSearchSetAllocator(XtpSearchRealloc allocator)
{
        search_realloc = allocator != NULL ? allocator : realloc;
}

void *
XtpSearchReallocate(void *pointer, size_t size)
{
        return search_realloc(pointer, size);
}

bool
XtpSearchQueryInit(XtpSearchQuery *query, const char *utf8, size_t length)
{
        size_t offset = 0;
        size_t count = 0;
        size_t index;
        size_t prefix = 0;

        memset(query, 0, sizeof(*query));
        if (length == 0)
                return true;
        if (utf8 == NULL)
                return false;
        query->codepoints = XtpSearchReallocate(NULL, length * sizeof(*query->codepoints));
        query->failure = XtpSearchReallocate(NULL, length * sizeof(*query->failure));
        if (query->codepoints == NULL || query->failure == NULL) {
                XtpSearchQueryFree(query);
                return false;
        }
        while (offset < length) {
                uint32_t codepoint;
                size_t consumed;

                if (!XtpUtf8Decode(utf8 + offset, length - offset, &codepoint, &consumed) ||
                    consumed == 0) {
                        XtpSearchQueryFree(query);
                        return false;
                }
                query->codepoints[count++] = codepoint;
                offset += consumed;
        }
        query->length = count;
        /* Knuth-Morris-Pratt: the longest proper prefix that is also a suffix. */
        query->failure[0] = 0;
        for (index = 1; index < count; ++index) {
                while (prefix > 0 && query->codepoints[index] != query->codepoints[prefix])
                        prefix = query->failure[prefix - 1U];
                if (query->codepoints[index] == query->codepoints[prefix])
                        ++prefix;
                query->failure[index] = prefix;
        }
        return true;
}

void
XtpSearchQueryFree(XtpSearchQuery *query)
{
        free(query->codepoints);
        free(query->failure);
        memset(query, 0, sizeof(*query));
}

void
XtpSearchFindAll(const XtpSearchQuery *query, const XtpSearchUnit *units, size_t count,
                 XtpSearchFound found, void *closure)
{
        size_t matched = 0;
        size_t index;

        if (query->length == 0 || count < query->length)
                return;
        for (index = 0; index < count; ++index) {
                while (matched > 0 && units[index].codepoint != query->codepoints[matched])
                        matched = query->failure[matched - 1U];
                if (units[index].codepoint == query->codepoints[matched])
                        ++matched;
                if (matched == query->length) {
                        size_t first = index + 1U - query->length;

                        if (units[first].cluster_start && units[index].cluster_end &&
                            !found(first, index, closure))
                                return;
                        matched = query->failure[matched - 1U];
                }
        }
}

bool
XtpSearchUnitsEqual(const XtpSearchQuery *query, const XtpSearchUnit *units, size_t count)
{
        size_t index;

        if (count == 0 || count != query->length || !units[0].cluster_start ||
            !units[count - 1U].cluster_end)
                return false;
        for (index = 0; index < count; ++index)
                if (units[index].codepoint != query->codepoints[index])
                        return false;
        return true;
}

int
XtpSearchPositionCompare(XtpSearchPosition left, XtpSearchPosition right)
{
        if (left.row != right.row)
                return left.row < right.row ? -1 : 1;
        if (left.column != right.column)
                return left.column < right.column ? -1 : 1;
        return 0;
}

size_t
XtpSearchPick(const XtpSearchPosition *positions, size_t count, XtpSearchPosition from,
              bool forward, bool *wrapped)
{
        size_t low = 0;
        size_t high = count;

        *wrapped = false;
        while (low < high) {
                size_t middle = low + (high - low) / 2U;
                int order = XtpSearchPositionCompare(positions[middle], from);

                if (forward ? order > 0 : order >= 0)
                        low = middle + 1U;
                else
                        high = middle;
        }
        /* After `from` is the prefix [0, low); before it is the suffix [low, count). */
        if (forward) {
                if (low == 0) {
                        *wrapped = true;
                        return count - 1U;
                }
                return low - 1U;
        }
        if (low == count) {
                *wrapped = true;
                return 0;
        }
        return low;
}
