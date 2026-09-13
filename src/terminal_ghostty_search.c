#include "search_match.h"
#include "terminal_ghosttyP.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
        GhosttyTrackedGridRef start;
        GhosttyTrackedGridRef end;
} SearchMatch;

typedef struct
{
        size_t first;
        size_t last;
} PendingMatch;

/* Positions are tracked references, so scrolling, eviction and reflow move or kill them. */
struct XtpTerminalSearch
{
        XtpTerminal *terminal;
        XtpSearchQuery query;
        XtpSearchState state;
        /* Bottom row of the next logical line to scan, going up. */
        GhosttyTrackedGridRef cursor;
        /* Descending by start: lines are scanned newest first. */
        SearchMatch *matches;
        size_t match_count;
        size_t match_capacity;
        bool truncated;
        XtpSearchUnit *units;
        size_t unit_count;
        size_t unit_capacity;
        PendingMatch *pending;
        size_t pending_count;
        size_t pending_capacity;
        bool pending_failed;
        uint32_t *cluster;
        size_t cluster_capacity;
        uint64_t rows_scanned;
};

static bool
SearchCellRef(XtpTerminal *terminal, uint64_t row, uint16_t column, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = {column, (uint32_t)row}},
        };

        return row <= UINT32_MAX &&
               ghostty_terminal_grid_ref(terminal->handle, point, ref) == GHOSTTY_SUCCESS;
}

static bool
SearchRowWrapped(XtpTerminal *terminal, uint64_t row)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyRow data;
        bool wrapped = false;

        return SearchCellRef(terminal, row, 0, &ref) &&
               ghostty_grid_ref_row(&ref, &data) == GHOSTTY_SUCCESS &&
               ghostty_row_get(data, GHOSTTY_ROW_DATA_WRAP, &wrapped) == GHOSTTY_SUCCESS && wrapped;
}

static bool
TrackPoint(XtpTerminal *terminal, uint64_t row, uint16_t column, GhosttyTrackedGridRef *out)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = {column, (uint32_t)row}},
        };

        *out = NULL;
        return row <= UINT32_MAX &&
               ghostty_terminal_grid_ref_track(terminal->handle, point, out) == GHOSTTY_SUCCESS;
}

static bool
TrackedPoint(GhosttyTrackedGridRef ref, GhosttyPointCoordinate *point)
{
        return ref != NULL && ghostty_tracked_grid_ref_has_value(ref) &&
               ghostty_tracked_grid_ref_point(ref, GHOSTTY_POINT_TAG_SCREEN, point) ==
                   GHOSTTY_SUCCESS;
}

static void
FreeTracked(GhosttyTrackedGridRef *ref)
{
        if (*ref != NULL)
                ghostty_tracked_grid_ref_free(*ref);
        *ref = NULL;
}

static bool
Grow(void **items, size_t *capacity, size_t needed, size_t item_size)
{
        size_t grown = *capacity == 0 ? 64U : *capacity;
        void *resized;

        if (needed <= *capacity)
                return true;
        while (grown < needed)
                grown *= 2U;
        resized = XtpSearchReallocate(*items, grown * item_size);
        if (resized == NULL)
                return false;
        *items = resized;
        *capacity = grown;
        return true;
}

static bool
AppendUnit(XtpTerminalSearch *search, uint32_t codepoint, uint64_t row, uint16_t column,
           uint8_t width, bool cluster_start, bool cluster_end)
{
        XtpSearchUnit *unit;

        if (!Grow((void **)&search->units, &search->unit_capacity, search->unit_count + 1U,
                  sizeof(*search->units)))
                return false;
        unit = &search->units[search->unit_count++];
        unit->codepoint = codepoint;
        unit->row = row;
        unit->column = column;
        unit->width = width;
        unit->cluster_start = cluster_start;
        unit->cluster_end = cluster_end;
        return true;
}

/* Rows top..bottom as units; empty cells read as spaces, spacer cells are skipped. */
static bool
BuildUnits(XtpTerminalSearch *search, uint64_t top, uint64_t bottom, bool trim)
{
        uint16_t columns = 0;
        uint64_t row;

        search->unit_count = 0;
        if (ghostty_terminal_get(search->terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0)
                return false;
        for (row = top; row <= bottom; ++row) {
                uint16_t column;

                for (column = 0; column < columns; ++column) {
                        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
                        GhosttyCell cell;
                        GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
                        GhosttyResult result;
                        size_t length = 0;
                        size_t index;
                        uint8_t width;

                        if (!SearchCellRef(search->terminal, row, column, &ref) ||
                            ghostty_grid_ref_cell(&ref, &cell) != GHOSTTY_SUCCESS ||
                            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_WIDE, &wide) !=
                                GHOSTTY_SUCCESS)
                                return false;
                        if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL ||
                            wide == GHOSTTY_CELL_WIDE_SPACER_HEAD)
                                continue;
                        result = ghostty_grid_ref_graphemes(&ref, search->cluster,
                                                            search->cluster_capacity, &length);
                        if (result == GHOSTTY_OUT_OF_SPACE) {
                                if (!Grow((void **)&search->cluster, &search->cluster_capacity,
                                          length, sizeof(*search->cluster)))
                                        return false;
                                result = ghostty_grid_ref_graphemes(
                                    &ref, search->cluster, search->cluster_capacity, &length);
                        }
                        if (result != GHOSTTY_SUCCESS)
                                return false;
                        width = wide == GHOSTTY_CELL_WIDE_WIDE ? 2U : 1U;
                        if (length == 0) {
                                if (!AppendUnit(search, 0x20U, row, column, 1U, true, true))
                                        return false;
                                continue;
                        }
                        for (index = 0; index < length; ++index)
                                if (!AppendUnit(search, search->cluster[index], row, column, width,
                                                index == 0, index + 1U == length))
                                        return false;
                }
        }
        while (trim && search->unit_count != 0 &&
               search->units[search->unit_count - 1U].codepoint == 0x20U &&
               search->units[search->unit_count - 1U].row == bottom)
                --search->unit_count;
        return true;
}

static bool
CollectMatch(size_t first, size_t last, void *closure)
{
        XtpTerminalSearch *search = closure;

        if (!Grow((void **)&search->pending, &search->pending_capacity, search->pending_count + 1U,
                  sizeof(*search->pending))) {
                search->pending_failed = true;
                return false;
        }
        search->pending[search->pending_count].first = first;
        search->pending[search->pending_count].last = last;
        ++search->pending_count;
        return true;
}

static bool
AddMatch(XtpTerminalSearch *search, const PendingMatch *pending)
{
        const XtpSearchUnit *start = &search->units[pending->first];
        const XtpSearchUnit *end = &search->units[pending->last];
        SearchMatch match = {NULL, NULL};

        if (!Grow((void **)&search->matches, &search->match_capacity, search->match_count + 1U,
                  sizeof(*search->matches)))
                return false;
        if (!TrackPoint(search->terminal, start->row, start->column, &match.start) ||
            !TrackPoint(search->terminal, end->row, (uint16_t)(end->column + end->width - 1U),
                        &match.end)) {
                FreeTracked(&match.start);
                FreeTracked(&match.end);
                return false;
        }
        search->matches[search->match_count++] = match;
        return true;
}

static void
RemoveMatch(XtpTerminalSearch *search, size_t index)
{
        FreeTracked(&search->matches[index].start);
        FreeTracked(&search->matches[index].end);
        memmove(&search->matches[index], &search->matches[index + 1U],
                (search->match_count - index - 1U) * sizeof(*search->matches));
        --search->match_count;
}

static void
ResetSearch(XtpTerminalSearch *search)
{
        while (search->match_count != 0)
                RemoveMatch(search, search->match_count - 1U);
        FreeTracked(&search->cursor);
        XtpSearchQueryFree(&search->query);
        search->state = XTP_SEARCH_IDLE;
        search->truncated = false;
        search->rows_scanned = 0;
}

XtpTerminalSearch *
XtpTerminalSearchNew(XtpTerminal *terminal)
{
        XtpTerminalSearch *search;

        if (terminal == NULL)
                return NULL;
        search = calloc(1, sizeof(*search));
        if (search == NULL)
                return NULL;
        search->terminal = terminal;
        search->state = XTP_SEARCH_IDLE;
        return search;
}

void
XtpTerminalSearchFree(XtpTerminalSearch *search)
{
        if (search == NULL)
                return;
        ResetSearch(search);
        free(search->matches);
        free(search->units);
        free(search->pending);
        free(search->cluster);
        free(search);
}

static void
FinishScan(XtpTerminalSearch *search, XtpSearchState state)
{
        FreeTracked(&search->cursor);
        search->state = state;
}

/* Scans the logical line ending at the cursor row, then moves the cursor above it. */
static size_t
ScanLine(XtpTerminalSearch *search, uint64_t *top_out)
{
        GhosttyPointCoordinate point;
        uint64_t top;
        uint64_t bottom;
        size_t rows;
        size_t index;

        *top_out = 0;
        /* A dead cursor means its row, and every row above it, was evicted. */
        if (!TrackedPoint(search->cursor, &point)) {
                FinishScan(search, XTP_SEARCH_COMPLETE);
                return 0;
        }
        bottom = point.y;
        top = bottom;
        while (top > 0 && bottom - top + 1U < XTP_SEARCH_LINE_ROW_LIMIT &&
               SearchRowWrapped(search->terminal, top - 1U))
                --top;
        *top_out = top;
        rows = (size_t)(bottom - top + 1U);
        search->rows_scanned += rows;
        search->pending_count = 0;
        search->pending_failed = false;
        if (!BuildUnits(search, top, bottom, true)) {
                FinishScan(search, XTP_SEARCH_ERROR);
                return rows;
        }
        XtpSearchFindAll(&search->query, search->units, search->unit_count, CollectMatch, search);
        if (search->pending_failed) {
                FinishScan(search, XTP_SEARCH_ERROR);
                return rows;
        }
        for (index = search->pending_count; index > 0; --index) {
                if (search->match_count == XTP_SEARCH_MATCH_LIMIT) {
                        search->truncated = true;
                        break;
                }
                if (!AddMatch(search, &search->pending[index - 1U])) {
                        FinishScan(search, XTP_SEARCH_ERROR);
                        return rows;
                }
        }
        if (search->truncated || top == 0) {
                FinishScan(search, XTP_SEARCH_COMPLETE);
                return rows;
        }
        {
                GhosttyPoint above = {
                    GHOSTTY_POINT_TAG_SCREEN,
                    {.coordinate = {0, (uint32_t)(top - 1U)}},
                };

                if (ghostty_tracked_grid_ref_set(search->cursor, search->terminal->handle, above) !=
                    GHOSTTY_SUCCESS)
                        FinishScan(search, XTP_SEARCH_ERROR);
        }
        return rows;
}

int
XtpTerminalSearchSetQuery(XtpTerminalSearch *search, const char *utf8, size_t length)
{
        XtpSearchQuery query;
        uint64_t rows;
        uint64_t history_rows;
        uint16_t active_rows = 0;
        uint64_t top = 0;

        if (search == NULL)
                return -1;
        ResetSearch(search);
        if (!XtpSearchQueryInit(&query, utf8, length))
                return -1;
        rows = XtpGhosttyScreenRows(search->terminal);
        if (rows == 0 || ghostty_terminal_get(search->terminal->handle, GHOSTTY_TERMINAL_DATA_ROWS,
                                              &active_rows) != GHOSTTY_SUCCESS) {
                XtpSearchQueryFree(&query);
                return -1;
        }
        search->query = query;
        if (query.length == 0) {
                search->state = XTP_SEARCH_COMPLETE;
                return 0;
        }
        if (!TrackPoint(search->terminal, rows - 1U, 0, &search->cursor)) {
                ResetSearch(search);
                return -1;
        }
        search->state = XTP_SEARCH_RUNNING;
        /* Only active rows can still be written, so they are read now; history rows are fixed. */
        history_rows = rows > active_rows ? rows - active_rows : 0;
        do
                (void)ScanLine(search, &top);
        while (search->state == XTP_SEARCH_RUNNING && top > history_rows);
        return search->state == XTP_SEARCH_ERROR ? -1 : 0;
}

XtpSearchState
XtpTerminalSearchStep(XtpTerminalSearch *search, size_t row_budget)
{
        size_t visited = 0;

        if (search == NULL)
                return XTP_SEARCH_IDLE;
        if (search->state != XTP_SEARCH_RUNNING)
                return XtpTerminalSearchState(search);
        if (XtpGhosttyScreenRows(search->terminal) == 0)
                return XTP_SEARCH_UNAVAILABLE;
        while (visited < row_budget && search->state == XTP_SEARCH_RUNNING) {
                uint64_t top;

                visited += ScanLine(search, &top);
        }
        return XtpTerminalSearchState(search);
}

XtpSearchState
XtpTerminalSearchState(XtpTerminalSearch *search)
{
        if (search == NULL)
                return XTP_SEARCH_IDLE;
        if (search->state != XTP_SEARCH_IDLE && XtpGhosttyScreenRows(search->terminal) == 0)
                return XTP_SEARCH_UNAVAILABLE;
        return search->state;
}

void
XtpTerminalSearchCancel(XtpTerminalSearch *search)
{
        if (search != NULL)
                ResetSearch(search);
}

size_t
XtpTerminalSearchMatches(XtpTerminalSearch *search, bool *truncated)
{
        size_t read;
        size_t write = 0;

        if (truncated != NULL)
                *truncated = search != NULL && search->truncated;
        if (search == NULL)
                return 0;
        for (read = 0; read < search->match_count; ++read) {
                SearchMatch *match = &search->matches[read];

                if (match->start == NULL || match->end == NULL ||
                    !ghostty_tracked_grid_ref_has_value(match->start) ||
                    !ghostty_tracked_grid_ref_has_value(match->end)) {
                        FreeTracked(&match->start);
                        FreeTracked(&match->end);
                        continue;
                }
                search->matches[write++] = *match;
        }
        search->match_count = write;
        return write;
}

uint64_t
XtpTerminalSearchRowsScanned(XtpTerminalSearch *search)
{
        return search != NULL ? search->rows_scanned : 0;
}

/* The text between the match's tracked ends must still spell the query across wrapped rows. */
static bool
MatchStillSpells(XtpTerminalSearch *search, size_t index, GhosttyPointCoordinate *start,
                 GhosttyPointCoordinate *end)
{
        const SearchMatch *match = &search->matches[index];
        size_t first = 0;
        size_t last;
        uint64_t row;

        if (!TrackedPoint(match->start, start) || !TrackedPoint(match->end, end) ||
            end->y < start->y || (end->y == start->y && end->x < start->x) ||
            end->y - start->y >= XTP_SEARCH_LINE_ROW_LIMIT)
                return false;
        for (row = start->y; row < end->y; ++row)
                if (!SearchRowWrapped(search->terminal, row))
                        return false;
        if (!BuildUnits(search, start->y, end->y, false))
                return false;
        while (first < search->unit_count &&
               !(search->units[first].row == start->y && search->units[first].column == start->x &&
                 search->units[first].cluster_start))
                ++first;
        for (last = first; last < search->unit_count; ++last)
                if (search->units[last].row == end->y && search->units[last].cluster_end &&
                    search->units[last].column + search->units[last].width - 1U == end->x)
                        break;
        return last < search->unit_count &&
               XtpSearchUnitsEqual(&search->query, search->units + first, last - first + 1U);
}

int
XtpTerminalSearchNavigate(XtpTerminalSearch *search, uint64_t row, uint16_t column, bool forward,
                          XtpSemanticSpan *match, bool *wrapped)
{
        XtpSearchPosition from = {row, column};

        if (wrapped != NULL)
                *wrapped = false;
        if (search == NULL || match == NULL || XtpGhosttyScreenRows(search->terminal) == 0)
                return -1;
        for (;;) {
                size_t count = XtpTerminalSearchMatches(search, NULL);
                XtpSearchPosition *positions;
                GhosttyPointCoordinate start;
                GhosttyPointCoordinate end;
                bool wrapped_here = false;
                size_t index;
                size_t pick;

                if (count == 0)
                        return -1;
                positions = malloc(count * sizeof(*positions));
                if (positions == NULL)
                        return -1;
                for (index = 0; index < count; ++index) {
                        if (!TrackedPoint(search->matches[index].start, &start))
                                break;
                        positions[index].row = start.y;
                        positions[index].column = start.x;
                }
                if (index != count) {
                        free(positions);
                        RemoveMatch(search, index);
                        continue;
                }
                pick = XtpSearchPick(positions, count, from, forward, &wrapped_here);
                free(positions);
                if (!MatchStillSpells(search, pick, &start, &end)) {
                        RemoveMatch(search, pick);
                        continue;
                }
                match->start_row = start.y;
                match->start_column = start.x;
                match->end_row = end.y;
                match->end_column = end.x;
                if (wrapped != NULL)
                        *wrapped = wrapped_here;
                return 0;
        }
}

size_t
XtpTerminalSearchVisible(XtpTerminalSearch *search, uint64_t first_row, uint64_t last_row,
                         XtpSemanticSpan *spans, size_t capacity)
{
        size_t index;
        size_t written = 0;

        if (search == NULL || spans == NULL || capacity == 0 ||
            XtpGhosttyScreenRows(search->terminal) == 0)
                return 0;
        /* Stored newest first, so walking backward yields ascending positions. */
        for (index = search->match_count; index > 0 && written < capacity; --index) {
                const SearchMatch *match = &search->matches[index - 1U];
                GhosttyPointCoordinate start;
                GhosttyPointCoordinate end;

                if (!TrackedPoint(match->start, &start) || !TrackedPoint(match->end, &end))
                        continue;
                if (start.y > last_row)
                        break;
                if (end.y < first_row)
                        continue;
                spans[written].start_row = start.y;
                spans[written].start_column = start.x;
                spans[written].end_row = end.y;
                spans[written].end_column = end.x;
                ++written;
        }
        return written;
}

struct XtpTerminalCellMark
{
        GhosttyTrackedGridRef ref;
};

XtpTerminalCellMark *
XtpTerminalMarkCell(XtpTerminal *terminal, uint64_t row, uint16_t column)
{
        XtpTerminalCellMark *mark;

        if (terminal == NULL || XtpGhosttyScreenRows(terminal) == 0)
                return NULL;
        mark = calloc(1, sizeof(*mark));
        if (mark == NULL)
                return NULL;
        if (!TrackPoint(terminal, row, column, &mark->ref)) {
                free(mark);
                return NULL;
        }
        return mark;
}

int
XtpTerminalMarkPosition(const XtpTerminalCellMark *mark, uint64_t *row, uint16_t *column)
{
        GhosttyPointCoordinate point;

        if (mark == NULL || row == NULL || column == NULL || !TrackedPoint(mark->ref, &point))
                return -1;
        *row = point.y;
        *column = point.x;
        return 0;
}

void
XtpTerminalMarkFree(XtpTerminalCellMark *mark)
{
        if (mark == NULL)
                return;
        FreeTracked(&mark->ref);
        free(mark);
}
