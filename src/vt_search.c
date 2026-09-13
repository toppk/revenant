#include "vt_widgetP.h"

#include "diagnostics.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Label.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rows scanned per event-loop turn, so PTY reads continue during a deep search. */
#define XTP_SEARCH_STEP_ROWS 2048U
/* Retry delay while the alternate screen suspends a scan. */
#define XTP_SEARCH_RETRY_MS 250UL

static void SearchStep(XtPointer closure, XtIntervalId *timer);

static Widget
TopLevel(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        while (XtParent(widget) != NULL && !XtIsShell(widget))
                widget = XtParent(widget);
        return widget;
}

static void
SearchRepaint(Vt100Rec *vt)
{
        if (!XtIsRealized((Widget)vt) || vt->vt.terminal == NULL || VtDeferSynchronizedRedraw(vt))
                return;
        VtInvalidateFrame(vt);
        XtpVtRedraw((Widget)vt);
}

static void
CancelSearchTimer(Vt100Rec *vt)
{
        if (vt->vt.search_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.search_timer);
                vt->vt.search_timer = (XtIntervalId)0;
        }
}

static void
ScheduleSearchStep(Vt100Rec *vt, unsigned long delay)
{
        if (vt->vt.search_timer == (XtIntervalId)0)
                vt->vt.search_timer = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt),
                                                      delay, SearchStep, vt);
}

static const char *
SearchStateName(XtpSearchState state)
{
        switch (state) {
        case XTP_SEARCH_IDLE:
                return "idle";
        case XTP_SEARCH_RUNNING:
                return "running";
        case XTP_SEARCH_COMPLETE:
                return "complete";
        case XTP_SEARCH_UNAVAILABLE:
                return "unavailable";
        case XTP_SEARCH_ERROR:
                return "error";
        }
        return "?";
}

/* The overlay sits over the bottom-left corner of the terminal. */
static void
PlaceOverlay(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;
        Dimension width = 0;
        Dimension height = 0;
        Dimension border = 0;
        Window child;
        int root_x = 0;
        int root_y = 0;
        int y;

        if (vt->vt.search_overlay == NULL || !XtIsRealized(widget))
                return;
        XtVaGetValues(vt->vt.search_overlay, XtNwidth, &width, XtNheight, &height, XtNborderWidth,
                      &border, NULL);
        y = (int)vt->core.height - (int)height - 2 * (int)border - (int)vt->vt.internal_border;
        if (y < 0)
                y = 0;
        (void)XTranslateCoordinates(XtDisplay(widget), XtWindow(widget),
                                    RootWindowOfScreen(XtScreen(widget)),
                                    (int)vt->vt.internal_border, y, &root_x, &root_y, &child);
        XtVaSetValues(vt->vt.search_overlay, XtNx, (Position)root_x, XtNy, (Position)root_y, NULL);
        XtpLog(XTP_LOG_DEBUG, "search", "overlay placed x=%d y=%d width=%u height=%u", root_x,
               root_y, (unsigned int)width, (unsigned int)height);
}

static void
UpdateOverlay(Vt100Rec *vt)
{
        char text[XTP_SEARCH_QUERY_CAPACITY + 96U];
        char count[64];
        XtpSearchState state = XtpTerminalSearchState(vt->vt.search);
        bool truncated = false;
        size_t matches = XtpTerminalSearchMatches(vt->vt.search, &truncated);
        const char *status = count;

        if (vt->vt.search_unavailable || state == XTP_SEARCH_UNAVAILABLE)
                status = "not available on this screen";
        else if (state == XTP_SEARCH_ERROR)
                status = "search failed";
        else if (vt->vt.search_query_length == 0)
                status = "type to search";
        else
                (void)snprintf(count, sizeof(count), "%zu%s match%s%s", matches,
                               truncated ? "+" : "", matches == 1U ? "" : "es",
                               state == XTP_SEARCH_RUNNING ? ", searching" : "");
        (void)snprintf(text, sizeof(text), "Search: %.*s  [%s]", (int)vt->vt.search_query_length,
                       vt->vt.search_query, status);
        if (vt->vt.search_label != NULL)
                XtVaSetValues(vt->vt.search_label, XtNlabel, text, NULL);
        PlaceOverlay(vt);
        XtpLog(XTP_LOG_INFO, "search",
               "status query-bytes=%zu matches=%zu truncated=%s state=%s unavailable=%s",
               vt->vt.search_query_length, matches, truncated ? "true" : "false",
               SearchStateName(state), vt->vt.search_unavailable ? "true" : "false");
}

static Boolean
SpanContains(const XtpSemanticSpan *span, uint64_t row, uint16_t column)
{
        if (row < span->start_row || row > span->end_row)
                return False;
        if (row == span->start_row && column < span->start_column)
                return False;
        return row != span->end_row || column <= span->end_column;
}

/* The match that starts at the marked cell; navigation drops it if its text changed. */
static Boolean
ActiveMatchSpan(Vt100Rec *vt, XtpSemanticSpan *span)
{
        uint64_t row;
        uint16_t column;
        bool wrapped = false;

        if (vt->vt.search_active_mark == NULL ||
            XtpTerminalMarkPosition(vt->vt.search_active_mark, &row, &column) != 0)
                return False;
        if (XtpTerminalSearchNavigate(vt->vt.search, column == UINT16_MAX ? row + 1U : row,
                                      column == UINT16_MAX ? 0 : (uint16_t)(column + 1U), false,
                                      span, &wrapped) != 0)
                return False;
        return span->start_row == row && span->start_column == column;
}

static void
ClearActive(Vt100Rec *vt)
{
        XtpTerminalMarkFree(vt->vt.search_active_mark);
        vt->vt.search_active_mark = NULL;
}

static void
ScrollToSpan(Vt100Rec *vt, const XtpSemanticSpan *span)
{
        XtpTerminalScrollbar bar;

        if (XtpTerminalGetScrollbar(vt->vt.terminal, &bar) != 0 || bar.length == 0)
                return;
        if (span->start_row >= bar.offset && span->end_row < bar.offset + bar.length)
                return;
        (void)VtScrollViewportToRow(
            vt, span->start_row > bar.length / 2U ? span->start_row - bar.length / 2U : 0U);
}

static void
SetActive(Vt100Rec *vt, const XtpSemanticSpan *span, const char *reason, bool wrapped)
{
        ClearActive(vt);
        vt->vt.search_active_mark =
            XtpTerminalMarkCell(vt->vt.terminal, span->start_row, span->start_column);
        XtpLog(XTP_LOG_INFO, "search", "active start=%llu,%u end=%llu,%u reason=%s wrapped=%s",
               (unsigned long long)span->start_row, (unsigned int)span->start_column,
               (unsigned long long)span->end_row, (unsigned int)span->end_column, reason,
               wrapped ? "true" : "false");
        ScrollToSpan(vt, span);
}

/* The nearest match above the bottom of the viewport the search started from. */
static void
PickActive(Vt100Rec *vt)
{
        XtpTerminalScrollbar bar;
        XtpSemanticSpan span;
        uint64_t from_row;
        uint64_t row;
        uint16_t column;
        bool wrapped = false;

        if (vt->vt.search_active_mark != NULL || vt->vt.terminal == NULL ||
            XtpTerminalGetScrollbar(vt->vt.terminal, &bar) != 0)
                return;
        from_row = bar.total;
        if (!vt->vt.search_viewport_at_bottom && vt->vt.search_viewport_mark != NULL &&
            XtpTerminalMarkPosition(vt->vt.search_viewport_mark, &row, &column) == 0)
                from_row = row + bar.length;
        if (XtpTerminalSearchNavigate(vt->vt.search, from_row, 0, false, &span, &wrapped) != 0)
                return;
        /* Scanning runs newest first, so only a finished scan may wrap past the start. */
        if (wrapped && XtpTerminalSearchState(vt->vt.search) == XTP_SEARCH_RUNNING)
                return;
        SetActive(vt, &span, "nearest", wrapped);
}

static void
ApplyQuery(Vt100Rec *vt)
{
        ClearActive(vt);
        CancelSearchTimer(vt);
        vt->vt.search_unavailable = False;
        if (XtpTerminalSearchSetQuery(vt->vt.search, vt->vt.search_query,
                                      vt->vt.search_query_length) != 0) {
                vt->vt.search_unavailable =
                    XtpTerminalSearchState(vt->vt.search) != XTP_SEARCH_ERROR;
                if (vt->vt.search_unavailable)
                        ScheduleSearchStep(vt, XTP_SEARCH_RETRY_MS);
        } else {
                PickActive(vt);
                if (XtpTerminalSearchState(vt->vt.search) == XTP_SEARCH_RUNNING)
                        ScheduleSearchStep(vt, 0);
        }
        UpdateOverlay(vt);
        SearchRepaint(vt);
}

static void
SearchStep(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;
        XtpSearchState state;
        Boolean had_active;

        (void)timer;
        vt->vt.search_timer = (XtIntervalId)0;
        if (!vt->vt.search_active || vt->vt.search == NULL)
                return;
        /* A query the alternate screen refused is retried quietly until it is accepted. */
        if (vt->vt.search_unavailable) {
                if (XtpTerminalSearchSetQuery(vt->vt.search, vt->vt.search_query,
                                              vt->vt.search_query_length) != 0) {
                        if (XtpTerminalSearchState(vt->vt.search) != XTP_SEARCH_ERROR) {
                                ScheduleSearchStep(vt, XTP_SEARCH_RETRY_MS);
                                return;
                        }
                        vt->vt.search_unavailable = False;
                        UpdateOverlay(vt);
                        return;
                }
                vt->vt.search_unavailable = False;
                PickActive(vt);
                if (XtpTerminalSearchState(vt->vt.search) == XTP_SEARCH_RUNNING)
                        ScheduleSearchStep(vt, 0);
                UpdateOverlay(vt);
                SearchRepaint(vt);
                return;
        }
        state = XtpTerminalSearchStep(vt->vt.search, XTP_SEARCH_STEP_ROWS);
        had_active = vt->vt.search_active_mark != NULL;
        PickActive(vt);
        UpdateOverlay(vt);
        if (state == XTP_SEARCH_RUNNING)
                ScheduleSearchStep(vt, 0);
        else if (state == XTP_SEARCH_UNAVAILABLE)
                ScheduleSearchStep(vt, XTP_SEARCH_RETRY_MS);
        if (state != XTP_SEARCH_RUNNING || had_active != (vt->vt.search_active_mark != NULL))
                SearchRepaint(vt);
}

static void
ShellMoved(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        (void)widget;
        (void)continue_dispatch;
        if (event->type == ConfigureNotify)
                PlaceOverlay(closure);
}

static Boolean
CreateOverlay(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;
        Screen *screen = XtScreen(widget);

        if (vt->vt.search_overlay != NULL)
                return True;
        /* Override-redirect keeps focus on the terminal, which owns every key while searching. */
        vt->vt.search_overlay = XtVaCreatePopupShell(
            "searchOverlay", overrideShellWidgetClass, widget, XtNvisual,
            DefaultVisualOfScreen(screen), XtNdepth, DefaultDepthOfScreen(screen), XtNcolormap,
            DefaultColormapOfScreen(screen), XtNallowShellResize, True, XtNgeometry, NULL, NULL);
        if (vt->vt.search_overlay == NULL)
                return False;
        vt->vt.search_label =
            XtVaCreateManagedWidget("label", labelWidgetClass, vt->vt.search_overlay, NULL);
        return vt->vt.search_label != NULL;
}

static void
OpenSearch(Vt100Rec *vt)
{
        XtpTerminalScrollbar bar;

        if (vt->vt.search_active) {
                XtpLog(XTP_LOG_DEBUG, "search", "already open");
                return;
        }
        if (vt->vt.terminal == NULL || !XtIsRealized((Widget)vt))
                return;
        if (vt->vt.search == NULL)
                vt->vt.search = XtpTerminalSearchNew(vt->vt.terminal);
        if (vt->vt.search == NULL || !CreateOverlay(vt)) {
                XtpLog(XTP_LOG_WARNING, "search", "search is not available with backend=%s",
                       XtpTerminalBackend());
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        vt->vt.search_viewport_at_bottom = True;
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &bar) == 0 &&
            bar.offset + bar.length < bar.total) {
                vt->vt.search_viewport_at_bottom = False;
                vt->vt.search_viewport_mark = XtpTerminalMarkCell(vt->vt.terminal, bar.offset, 0);
        }
        vt->vt.search_active = True;
        vt->vt.search_query_length = 0;
        XtAddEventHandler(TopLevel(vt), StructureNotifyMask, False, ShellMoved, vt);
        XtPopup(vt->vt.search_overlay, XtGrabNone);
        ApplyQuery(vt);
        XtpLog(XTP_LOG_INFO, "search", "opened overlay=0x%lx viewport-at-bottom=%s",
               XtWindow(vt->vt.search_overlay),
               vt->vt.search_viewport_at_bottom ? "true" : "false");
}

static void
CloseSearch(Vt100Rec *vt, Boolean restore_viewport, const char *reason)
{
        uint64_t row = 0;
        uint16_t column = 0;

        if (!vt->vt.search_active)
                return;
        CancelSearchTimer(vt);
        vt->vt.search_active = False;
        ClearActive(vt);
        XtpTerminalSearchCancel(vt->vt.search);
        if (vt->vt.search_overlay != NULL)
                XtPopdown(vt->vt.search_overlay);
        XtRemoveEventHandler(TopLevel(vt), StructureNotifyMask, False, ShellMoved, vt);
        if (restore_viewport && vt->vt.terminal != NULL) {
                if (vt->vt.search_viewport_at_bottom)
                        (void)VtScrollViewportToEnd(vt);
                else if (XtpTerminalMarkPosition(vt->vt.search_viewport_mark, &row, &column) == 0)
                        (void)VtScrollViewportToRow(vt, row);
                else
                        (void)VtScrollViewportToRow(vt, 0);
        }
        XtpTerminalMarkFree(vt->vt.search_viewport_mark);
        vt->vt.search_viewport_mark = NULL;
        XtpLog(XTP_LOG_INFO, "search", "closed reason=%s restore-viewport=%s", reason,
               restore_viewport ? "true" : "false");
        SearchRepaint(vt);
}

static void
CopyActive(Vt100Rec *vt, Time time)
{
        XtpSemanticSpan span;
        char *text = NULL;
        size_t length = 0;

        if (!ActiveMatchSpan(vt, &span) ||
            XtpTerminalSpanText(vt->vt.terminal, &span, &text, &length) != 0) {
                XtpLog(XTP_LOG_INFO, "search", "copy skipped; no active match");
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        if (!VtPublishSearchMatch(vt, (const uint8_t *)text, length, time)) {
                XtpLog(XTP_LOG_WARNING, "search", "copy failed bytes=%zu; search stays open",
                       length);
                free(text);
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        XtpLog(XTP_LOG_INFO, "search", "copied match bytes=%zu selection=PRIMARY", length);
        free(text);
        CloseSearch(vt, False, "copied");
}

static void
MoveActive(Vt100Rec *vt, bool forward)
{
        XtpSemanticSpan span;
        bool wrapped = false;

        if (!ActiveMatchSpan(vt, &span)) {
                ClearActive(vt);
                PickActive(vt);
        } else if (XtpTerminalSearchNavigate(vt->vt.search, span.start_row, span.start_column,
                                             forward, &span, &wrapped) == 0) {
                /* Wrapping waits for the scan, which may still find matches in between. */
                if (wrapped && XtpTerminalSearchState(vt->vt.search) == XTP_SEARCH_RUNNING) {
                        XtpLog(XTP_LOG_INFO, "search", "navigation waits for the scan");
                        XBell(XtDisplay((Widget)vt), 0);
                } else {
                        SetActive(vt, &span, forward ? "next" : "previous", wrapped);
                }
        } else {
                XBell(XtDisplay((Widget)vt), 0);
        }
        UpdateOverlay(vt);
        SearchRepaint(vt);
}

static void
RemoveLastCodepoint(Vt100Rec *vt)
{
        size_t length = vt->vt.search_query_length;

        if (length == 0) {
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        do
                --length;
        while (length > 0 && ((unsigned char)vt->vt.search_query[length] & 0xC0U) == 0x80U);
        vt->vt.search_query_length = length;
        ApplyQuery(vt);
}

void
VtStartSearchAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);

        (void)params;
        (void)num_params;
        if (event != NULL && event->type == KeyPress &&
            !VtAcceptLocalKeyAction(vt, event, XTP_LOCAL_ACTION_START_SEARCH))
                return;
        OpenSearch(vt);
}

void
VtSearchKey(Vt100Rec *vt, KeySym keysym, unsigned int state, const char *text, size_t length,
            Time time)
{
        if (!vt->vt.search_active)
                return;
        switch (keysym) {
        case XK_Escape:
                CloseSearch(vt, True, "escape");
                return;
        case XK_Return:
        case XK_KP_Enter:
                CopyActive(vt, time);
                return;
        case XK_BackSpace:
                RemoveLastCodepoint(vt);
                return;
        case XK_Up:
        case XK_KP_Up:
                MoveActive(vt, false);
                return;
        case XK_Down:
        case XK_KP_Down:
                MoveActive(vt, true);
                return;
        case XK_F3:
                MoveActive(vt, (state & ShiftMask) == 0);
                return;
        default:
                break;
        }
        if ((state & (ControlMask | Mod1Mask)) != 0 || length == 0 ||
            (unsigned char)text[0] < 0x20U || (unsigned char)text[0] == 0x7fU) {
                XtpLog(XTP_LOG_DEBUG, "search", "ignored key keysym=0x%lx state=0x%x", keysym,
                       state);
                return;
        }
        if (vt->vt.search_query_length + length > sizeof(vt->vt.search_query)) {
                XtpLog(XTP_LOG_INFO, "search", "query full; refused bytes=%zu", length);
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        memcpy(vt->vt.search_query + vt->vt.search_query_length, text, length);
        vt->vt.search_query_length += length;
        ApplyQuery(vt);
}

void
VtSearchPrepareFrame(Vt100Rec *vt)
{
        XtpTerminalScrollbar bar;

        vt->vt.search_visible_count = 0;
        vt->vt.search_have_span = False;
        if (!vt->vt.search_active || vt->vt.search == NULL || vt->vt.terminal == NULL ||
            vt->vt.rows < 1 || XtpTerminalGetScrollbar(vt->vt.terminal, &bar) != 0)
                return;
        if (vt->vt.search_visible == NULL)
                vt->vt.search_visible = malloc(XTP_SEARCH_MATCH_LIMIT * sizeof(XtpSemanticSpan));
        if (vt->vt.search_visible == NULL)
                return;
        vt->vt.search_frame_top = bar.offset;
        vt->vt.search_visible_count = XtpTerminalSearchVisible(
            vt->vt.search, bar.offset, bar.offset + (uint64_t)vt->vt.rows - 1U,
            vt->vt.search_visible, XTP_SEARCH_MATCH_LIMIT);
        vt->vt.search_have_span = ActiveMatchSpan(vt, &vt->vt.search_span);
}

unsigned int
VtSearchCellHighlight(const Vt100Rec *vt, uint16_t row, uint16_t column)
{
        uint64_t screen_row = vt->vt.search_frame_top + row;
        size_t low = 0;
        size_t high = vt->vt.search_visible_count;

        if (vt->vt.search_have_span && SpanContains(&vt->vt.search_span, screen_row, column))
                return 2U;
        /* Starts and ends both ascend, so the first span ending at or after the cell decides. */
        while (low < high) {
                size_t middle = low + (high - low) / 2U;
                const XtpSemanticSpan *span = &vt->vt.search_visible[middle];

                if (span->end_row < screen_row ||
                    (span->end_row == screen_row && span->end_column < column))
                        low = middle + 1U;
                else
                        high = middle;
        }
        return low < vt->vt.search_visible_count &&
                       SpanContains(&vt->vt.search_visible[low], screen_row, column)
                   ? 1U
                   : 0U;
}

void
VtSearchResized(Vt100Rec *vt)
{
        if (vt->vt.search_active)
                PlaceOverlay(vt);
}

void
VtSearchTerminalChanged(Vt100Rec *vt)
{
        CloseSearch(vt, False, "terminal-changed");
        XtpTerminalSearchFree(vt->vt.search);
        vt->vt.search = NULL;
}

void
VtSearchDestroy(Vt100Rec *vt)
{
        CancelSearchTimer(vt);
        if (vt->vt.search_active) {
                XtRemoveEventHandler(TopLevel(vt), StructureNotifyMask, False, ShellMoved, vt);
                XtpLog(XTP_LOG_INFO, "search", "closed reason=destroy restore-viewport=false");
        }
        vt->vt.search_active = False;
        ClearActive(vt);
        XtpTerminalMarkFree(vt->vt.search_viewport_mark);
        vt->vt.search_viewport_mark = NULL;
        XtpTerminalSearchFree(vt->vt.search);
        vt->vt.search = NULL;
        free(vt->vt.search_visible);
        vt->vt.search_visible = NULL;
        vt->vt.search_visible_count = 0;
}

Boolean
XtpVtSearchActive(Widget widget)
{
        return VtAsRecord(widget)->vt.search_active;
}
