#include "terminal.h"

#include "diagnostics.h"

#include <stdint.h>

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
