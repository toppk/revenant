#include "terminal.h"

#include "diagnostics.h"

#include <stdint.h>

const char *
XtpPointerShapeName(XtpPointerShape shape)
{
        static const char *const names[XTP_POINTER_SHAPE_COUNT] = {
            [XTP_POINTER_SHAPE_DEFAULT] = "default",
            [XTP_POINTER_SHAPE_CONTEXT_MENU] = "context-menu",
            [XTP_POINTER_SHAPE_HELP] = "help",
            [XTP_POINTER_SHAPE_POINTER] = "pointer",
            [XTP_POINTER_SHAPE_PROGRESS] = "progress",
            [XTP_POINTER_SHAPE_WAIT] = "wait",
            [XTP_POINTER_SHAPE_CELL] = "cell",
            [XTP_POINTER_SHAPE_CROSSHAIR] = "crosshair",
            [XTP_POINTER_SHAPE_TEXT] = "text",
            [XTP_POINTER_SHAPE_VERTICAL_TEXT] = "vertical-text",
            [XTP_POINTER_SHAPE_ALIAS] = "alias",
            [XTP_POINTER_SHAPE_COPY] = "copy",
            [XTP_POINTER_SHAPE_MOVE] = "move",
            [XTP_POINTER_SHAPE_NO_DROP] = "no-drop",
            [XTP_POINTER_SHAPE_NOT_ALLOWED] = "not-allowed",
            [XTP_POINTER_SHAPE_GRAB] = "grab",
            [XTP_POINTER_SHAPE_GRABBING] = "grabbing",
            [XTP_POINTER_SHAPE_ALL_SCROLL] = "all-scroll",
            [XTP_POINTER_SHAPE_COL_RESIZE] = "col-resize",
            [XTP_POINTER_SHAPE_ROW_RESIZE] = "row-resize",
            [XTP_POINTER_SHAPE_N_RESIZE] = "n-resize",
            [XTP_POINTER_SHAPE_E_RESIZE] = "e-resize",
            [XTP_POINTER_SHAPE_S_RESIZE] = "s-resize",
            [XTP_POINTER_SHAPE_W_RESIZE] = "w-resize",
            [XTP_POINTER_SHAPE_NE_RESIZE] = "ne-resize",
            [XTP_POINTER_SHAPE_NW_RESIZE] = "nw-resize",
            [XTP_POINTER_SHAPE_SE_RESIZE] = "se-resize",
            [XTP_POINTER_SHAPE_SW_RESIZE] = "sw-resize",
            [XTP_POINTER_SHAPE_EW_RESIZE] = "ew-resize",
            [XTP_POINTER_SHAPE_NS_RESIZE] = "ns-resize",
            [XTP_POINTER_SHAPE_NESW_RESIZE] = "nesw-resize",
            [XTP_POINTER_SHAPE_NWSE_RESIZE] = "nwse-resize",
            [XTP_POINTER_SHAPE_ZOOM_IN] = "zoom-in",
            [XTP_POINTER_SHAPE_ZOOM_OUT] = "zoom-out",
        };

        if ((unsigned int)shape >= XTP_POINTER_SHAPE_COUNT)
                return names[XTP_POINTER_SHAPE_TEXT];
        return names[shape];
}

static uint64_t
ViewportEnd(const XtpTerminalScrollbar *state)
{
        uint64_t end;

        if (state->length > UINT64_MAX - state->offset)
                end = UINT64_MAX;
        else
                end = state->offset + state->length;
        return end < state->total ? end : state->total;
}

int
XtpTerminalFeedOutput(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                      bool scroll_tty_output)
{
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        uint64_t rows_below = 0;
        uint64_t end;
        uint64_t desired;
        bool anchored;

        if (terminal == NULL || (bytes == NULL && length != 0))
                return -1;
        if (scroll_tty_output) {
                if (XtpTerminalScrollToBottom(terminal) != 0)
                        XtpLog(XTP_LOG_WARNING, "scrollback",
                               "cannot scroll to bottom before tty output");
                XtpTerminalFeed(terminal, bytes, length);
                return 0;
        }

        anchored = XtpTerminalGetScrollbar(terminal, &before) == 0;
        if (anchored)
                rows_below = before.total - ViewportEnd(&before);
        XtpTerminalFeed(terminal, bytes, length);
        if (!anchored || XtpTerminalGetScrollbar(terminal, &after) != 0)
                return anchored ? -1 : 0;

        end = after.total > rows_below ? after.total - rows_below : 0;
        desired = end > after.length ? end - after.length : 0;
        if (desired != after.offset && XtpTerminalScrollTo(terminal, desired) != 0)
                return -1;
        XtpLog(XTP_LOG_DEBUG, "scrollback",
               "tty output anchor rows-below=%llu offset=%llu->%llu total=%llu->%llu",
               (unsigned long long)rows_below, (unsigned long long)before.offset,
               (unsigned long long)desired, (unsigned long long)before.total,
               (unsigned long long)after.total);
        return 0;
}

int
XtpTerminalFeedOutputPinned(XtpTerminal *terminal, const uint8_t *bytes, size_t length)
{
        XtpTerminalScrollbar state;
        XtpTerminalCellMark *mark = NULL;
        uint64_t row = 0;
        uint16_t column = 0;

        if (terminal == NULL || (bytes == NULL && length != 0))
                return -1;
        if (XtpTerminalGetScrollbar(terminal, &state) == 0)
                mark = XtpTerminalMarkCell(terminal, state.offset, 0);
        if (mark == NULL)
                return XtpTerminalFeedOutput(terminal, bytes, length, false);
        XtpTerminalFeed(terminal, bytes, length);
        /* An evicted top row leaves the oldest retained row in view. */
        if (XtpTerminalMarkPosition(mark, &row, &column) != 0)
                row = 0;
        XtpTerminalMarkFree(mark);
        return XtpTerminalScrollTo(terminal, row);
}

const char *
XtpClipboardTargetName(XtpClipboardTarget target)
{
        switch (target) {
        case XTP_CLIPBOARD_TARGET_CLIPBOARD:
                return "CLIPBOARD";
        case XTP_CLIPBOARD_TARGET_SELECT:
                return "SELECT";
        case XTP_CLIPBOARD_TARGET_PRIMARY:
                return "PRIMARY";
        }
        return "?";
}

XtpCursorShape
XtpTerminalStartupCursorShape(bool underline, bool bar)
{
        if (underline)
                return XTP_CURSOR_SHAPE_UNDERLINE;
        return bar ? XTP_CURSOR_SHAPE_BAR : XTP_CURSOR_SHAPE_BLOCK;
}
