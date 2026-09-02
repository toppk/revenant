#include "terminal.h"

#include "char_class.h"
#include "diagnostics.h"

#include <stdlib.h>
#include <string.h>

struct XtpTerminal
{
        uint16_t columns;
        uint16_t rows;
        uint32_t cell_width;
        uint32_t cell_height;
        size_t scrollback_lines;
        bool modes[XTP_TERMINAL_MODE_COUNT];
        XtpCharClassTable *char_classes;
        XtpTerminalEffects effects;
};

XtpTerminal *
XtpTerminalNewWithGraphemeWidth(uint16_t columns, uint16_t rows, uint32_t cell_width,
                                uint32_t cell_height, bool unicode_width)
{
        XtpTerminal *terminal = calloc(1, sizeof(*terminal));

        if (terminal != NULL) {
                terminal->columns = columns;
                terminal->rows = rows;
                terminal->cell_width = cell_width;
                terminal->cell_height = cell_height;
        }
        XtpLog(XTP_LOG_INFO, "terminal",
               "creating backend=stub grid=%ux%u cell=%ux%u graphemeWidth=%s", columns, rows,
               cell_width, cell_height, unicode_width ? "unicode" : "legacy");
        return terminal;
}

void
XtpTerminalFree(XtpTerminal *terminal)
{
        XtpLog(XTP_LOG_INFO, "terminal", "destroying backend=stub");
        if (terminal != NULL)
                XtpCharClassFree(terminal->char_classes);
        free(terminal);
}

void
XtpTerminalFeed(XtpTerminal *terminal, const uint8_t *bytes, size_t length)
{
        (void)terminal;
        (void)bytes;
        (void)length;
        XtpLog(XTP_LOG_DEBUG, "terminal", "stub ignored feed bytes=%zu", length);
}

int
XtpTerminalResize(XtpTerminal *terminal, uint16_t columns, uint16_t rows, uint32_t cell_width,
                  uint32_t cell_height)
{
        if (terminal == NULL)
                return -1;

        terminal->columns = columns;
        terminal->rows = rows;
        terminal->cell_width = cell_width;
        terminal->cell_height = cell_height;
        XtpLog(XTP_LOG_INFO, "terminal", "stub resize grid=%ux%u cell=%ux%u", columns, rows,
               cell_width, cell_height);
        return 0;
}

int
XtpTerminalRender(XtpTerminal *terminal, const XtpRenderer *renderer, void *closure,
                  bool force_full)
{
        XtpRenderFrame frame;

        if (terminal == NULL || renderer == NULL)
                return -1;
        frame.columns = terminal->columns;
        frame.rows = terminal->rows;
        frame.full_repaint = force_full;
        frame.reverse_colors = false;
        frame.cursor_visible = false;
        frame.cursor_column = 0;
        frame.cursor_row = 0;
        frame.cursor_shape = XTP_CURSOR_SHAPE_BLOCK;
        frame.cursor_blink_requested = false;
        if (renderer->begin != NULL)
                renderer->begin(&frame, closure);
        if (renderer->end != NULL)
                renderer->end(&frame, closure);
        XtpLog(XTP_LOG_DEBUG, "render", "stub frame grid=%ux%u", frame.columns, frame.rows);
        return 0;
}

int
XtpTerminalEncodeKey(XtpTerminal *terminal, const XtpKeyEvent *event, char *buffer, size_t capacity,
                     size_t *written)
{
        (void)terminal;
        if (event == NULL || written == NULL)
                return -1;
        *written = event->utf8_length;
        if (event->utf8_length > capacity)
                return -1;
        if (event->utf8_length != 0)
                memcpy(buffer, event->utf8, event->utf8_length);
        XtpLog(XTP_LOG_DEBUG, "input", "stub key=%d modifiers=0x%x text-bytes=%zu output-bytes=%zu",
               (int)event->key, event->modifiers, event->utf8_length, *written);
        return 0;
}

int
XtpTerminalEncodeFocus(XtpTerminal *terminal, bool focused, char *buffer, size_t capacity,
                       size_t *written)
{
        (void)focused;
        (void)buffer;
        (void)capacity;
        if (terminal == NULL || written == NULL)
                return -1;
        *written = 0;
        return 0;
}

int
XtpTerminalEncodeMouse(XtpTerminal *terminal, const XtpMouseEvent *event, char *buffer,
                       size_t capacity, size_t *written)
{
        (void)event;
        (void)buffer;
        (void)capacity;
        if (terminal == NULL || written == NULL)
                return -1;
        *written = 0;
        return 0;
}

int
XtpTerminalSetScrollbackLines(XtpTerminal *terminal, size_t lines)
{
        if (terminal == NULL)
                return -1;
        terminal->scrollback_lines = lines;
        return 0;
}

int
XtpTerminalSetCursorBlinkDefault(XtpTerminal *terminal, bool blinking)
{
        (void)blinking;
        return terminal != NULL ? 0 : -1;
}

int
XtpTerminalSetCursorBlinkRequestsEnabled(XtpTerminal *terminal, bool enabled)
{
        (void)enabled;
        return terminal != NULL ? 0 : -1;
}

int
XtpTerminalSetDefaultColors(XtpTerminal *terminal, XtpRgbColor foreground, XtpRgbColor background,
                            XtpRgbColor cursor)
{
        (void)foreground;
        (void)background;
        (void)cursor;
        return terminal != NULL ? 0 : -1;
}

int
XtpTerminalSetAnsiPalette(XtpTerminal *terminal, const XtpRgbColor palette[XTP_ANSI_PALETTE_SIZE])
{
        return terminal != NULL && palette != NULL ? 0 : -1;
}

int
XtpTerminalSetCharClass(XtpTerminal *terminal, const char *specification)
{
        XtpCharClassTable *table = NULL;

        if (terminal == NULL)
                return -1;
        if (XtpCharClassParse(specification, &table) != 0) {
                XtpLog(XTP_LOG_WARNING, "selection", "invalid charClass specification=%s",
                       specification);
                return -1;
        }
        XtpCharClassFree(terminal->char_classes);
        terminal->char_classes = table;
        XtpLog(XTP_LOG_INFO, "selection", "stub charClass specification=%s",
               specification != NULL ? specification : "(default)");
        return 0;
}

int
XtpTerminalGetScrollbar(XtpTerminal *terminal, XtpTerminalScrollbar *scrollbar)
{
        if (terminal == NULL || scrollbar == NULL)
                return -1;
        scrollbar->total = terminal->rows;
        scrollbar->offset = 0;
        scrollbar->length = terminal->rows;
        return 0;
}

int
XtpTerminalScrollBy(XtpTerminal *terminal, intptr_t rows)
{
        (void)rows;
        return terminal != NULL ? 0 : -1;
}

int
XtpTerminalScrollTo(XtpTerminal *terminal, uint64_t row)
{
        (void)row;
        return terminal != NULL ? 0 : -1;
}

int
XtpTerminalScrollToBottom(XtpTerminal *terminal)
{
        return terminal != NULL ? 0 : -1;
}

XtpSelectionResult
XtpTerminalSelectionStart(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                          double surface_y, uint64_t time_ns, XtpSelectionUnit unit, bool repeat)
{
        (void)column;
        (void)row;
        (void)surface_x;
        (void)surface_y;
        (void)time_ns;
        (void)unit;
        (void)repeat;
        return terminal != NULL ? XTP_SELECTION_UNCHANGED : XTP_SELECTION_ERROR;
}

XtpSelectionResult
XtpTerminalSelectionExtend(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                           double surface_y, uint32_t columns, uint32_t cell_width,
                           uint32_t padding_left, uint32_t screen_height, bool rectangle)
{
        (void)column;
        (void)row;
        (void)surface_x;
        (void)surface_y;
        (void)columns;
        (void)cell_width;
        (void)padding_left;
        (void)screen_height;
        (void)rectangle;
        return terminal != NULL ? XTP_SELECTION_UNCHANGED : XTP_SELECTION_ERROR;
}

int
XtpTerminalSelectionGetAutoscroll(XtpTerminal *terminal, XtpSelectionAutoscroll *direction)
{
        if (terminal == NULL || direction == NULL)
                return -1;
        *direction = XTP_SELECTION_AUTOSCROLL_NONE;
        return 0;
}

XtpSelectionResult
XtpTerminalSelectionAutoscrollTick(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                   double surface_x, double surface_y, uint32_t columns,
                                   uint32_t cell_width, uint32_t padding_left,
                                   uint32_t screen_height, bool rectangle)
{
        (void)column;
        (void)row;
        (void)surface_x;
        (void)surface_y;
        (void)columns;
        (void)cell_width;
        (void)padding_left;
        (void)screen_height;
        (void)rectangle;
        return terminal != NULL ? XTP_SELECTION_UNCHANGED : XTP_SELECTION_ERROR;
}

void
XtpTerminalSelectionEnd(XtpTerminal *terminal, uint16_t column, uint16_t row, bool valid)
{
        (void)terminal;
        (void)column;
        (void)row;
        (void)valid;
}

XtpSelectionResult
XtpTerminalSelectionExtendStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                XtpSelectionUnit unit)
{
        (void)column;
        (void)row;
        (void)unit;
        return terminal != NULL ? XTP_SELECTION_UNCHANGED : XTP_SELECTION_ERROR;
}

XtpSelectionResult
XtpTerminalSelectionExtendActive(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                 bool rectangle)
{
        (void)column;
        (void)row;
        (void)rectangle;
        return terminal != NULL ? XTP_SELECTION_UNCHANGED : XTP_SELECTION_ERROR;
}

void
XtpTerminalSelectionExtendEnd(XtpTerminal *terminal)
{
        (void)terminal;
}

void
XtpTerminalSelectionClear(XtpTerminal *terminal)
{
        (void)terminal;
}

int
XtpTerminalSelectionText(XtpTerminal *terminal, uint8_t **bytes, size_t *length)
{
        if (terminal == NULL || bytes == NULL || length == NULL)
                return -1;
        *bytes = NULL;
        *length = 0;
        return -1;
}

int
XtpTerminalHyperlinkAt(XtpTerminal *terminal, uint16_t column, uint16_t row, uint8_t **uri,
                       size_t *length)
{
        (void)column;
        (void)row;
        if (terminal == NULL || uri == NULL || length == NULL)
                return -1;
        *uri = NULL;
        *length = 0;
        return 0;
}

int
XtpTerminalEncodePaste(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                       uint8_t **encoded, size_t *encoded_length)
{
        uint8_t *copy;

        if (terminal == NULL || (bytes == NULL && length != 0) || encoded == NULL ||
            encoded_length == NULL)
                return -1;
        copy = malloc(length != 0 ? length : 1U);
        if (copy == NULL)
                return -1;
        if (length != 0)
                memcpy(copy, bytes, length);
        *encoded = copy;
        *encoded_length = length;
        return 0;
}

bool
XtpTerminalMouseTracking(XtpTerminal *terminal)
{
        (void)terminal;
        return false;
}

int
XtpTerminalGetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool *enabled)
{
        if (terminal == NULL || enabled == NULL || mode >= XTP_TERMINAL_MODE_COUNT)
                return -1;
        *enabled = terminal->modes[mode];
        return 0;
}

int
XtpTerminalSetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool enabled)
{
        if (terminal == NULL || mode >= XTP_TERMINAL_MODE_COUNT)
                return -1;
        terminal->modes[mode] = enabled;
        return 0;
}

void
XtpTerminalSetEffects(XtpTerminal *terminal, const XtpTerminalEffects *effects)
{
        if (terminal == NULL)
                return;
        if (effects == NULL)
                memset(&terminal->effects, 0, sizeof(terminal->effects));
        else
                terminal->effects = *effects;
        XtpLog(XTP_LOG_INFO, "terminal", "stub effects configured");
}

const char *
XtpTerminalBackend(void)
{
        return "stub";
}

bool
XtpTerminalBackendIsStub(void)
{
        return true;
}
