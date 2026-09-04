#include "vt_widgetP.h"

#include "diagnostics.h"
#include "url_match.h"
#include "utf8.h"

#include <X11/Xatom.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define XTP_URL_SCAN_MAX_BYTES 65536U
#define XTP_URL_SCAN_MAX_CELLS 4096U

static Boolean
OwnsSelection(const Vt100Rec *vt, Atom selection)
{
        Cardinal index;

        for (index = 0; index < vt->vt.owned_selection_count; ++index) {
                if (vt->vt.owned_selections[index] == selection)
                        return True;
        }
        return False;
}

static SelectionSource
ResolveSelectionSource(Vt100Rec *vt, const char *name)
{
        SelectionSource source = {XTP_SELECTION_SOURCE_ATOM, None, -1};

        if (name == NULL || *name == '\0')
                return source;
        if (strncmp(name, "CUT_BUFFER", 10) == 0) {
                if (name[10] >= '0' && name[10] <= '7' && name[11] == '\0') {
                        source.kind = XTP_SELECTION_SOURCE_CUT_BUFFER;
                        source.cut_buffer = name[10] - '0';
                } else {
                        XtpLog(XTP_LOG_WARNING, "selection", "invalid cut-buffer name=%s", name);
                }
                return source;
        }
        if (strcmp(name, "SELECT") == 0)
                name = vt->vt.select_to_clipboard ? "CLIPBOARD" : "PRIMARY";
        if (strcmp(name, "PRIMARY") == 0)
                source.atom = XA_PRIMARY;
        else if (strcmp(name, "SECONDARY") == 0)
                source.atom = XA_SECONDARY;
        else
                source.atom = XInternAtom(XtDisplay((Widget)vt), name, False);
        return source;
}

static uint32_t
DecodeUtf8(const uint8_t *bytes, size_t length, size_t *consumed)
{
        uint32_t codepoint;

        *consumed = 1;
        if (!XtpUtf8Decode((const char *)bytes, length, &codepoint, consumed))
                return '?';
        return codepoint;
}

static uint8_t *
Utf8ToLatin1(const uint8_t *bytes, size_t length, size_t *result_length)
{
        uint8_t *result = malloc(length + 1U);
        size_t input = 0;
        size_t output = 0;

        if (result == NULL)
                return NULL;
        while (input < length) {
                size_t consumed;
                uint32_t codepoint = DecodeUtf8(bytes + input, length - input, &consumed);

                result[output++] = codepoint <= UINT8_MAX ? (uint8_t)codepoint : (uint8_t)'?';
                input += consumed;
        }
        result[output] = '\0';
        *result_length = output;
        return result;
}

static uint8_t *
Latin1ToUtf8(const uint8_t *bytes, size_t length, size_t *result_length)
{
        uint8_t *result;
        size_t input;
        size_t output = 0;

        if (length > (SIZE_MAX - 1U) / 2U)
                return NULL;
        result = malloc(length * 2U + 1U);
        if (result == NULL)
                return NULL;
        for (input = 0; input < length; ++input) {
                if (bytes[input] < 0x80) {
                        result[output++] = bytes[input];
                } else {
                        result[output++] = (uint8_t)(0xc0U | (bytes[input] >> 6));
                        result[output++] = (uint8_t)(0x80U | (bytes[input] & 0x3fU));
                }
        }
        result[output] = '\0';
        *result_length = output;
        return result;
}

static Boolean
ConvertSelection(Widget widget, Atom *selection, Atom *target, Atom *type_return,
                 XtPointer *value_return, unsigned long *length_return, int *format_return)
{
        Vt100Rec *vt = VtAsRecord(widget);
        Display *display = XtDisplay(widget);
        Atom targets = XInternAtom(display, "TARGETS", False);
        Atom timestamp = XInternAtom(display, "TIMESTAMP", False);
        Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
        Atom text = XInternAtom(display, "TEXT", False);

        if (!OwnsSelection(vt, *selection) || vt->vt.selection_text == NULL)
                return False;
        if (*target == targets) {
                Atom *available = (Atom *)XtMalloc(5U * sizeof(*available));

                available[0] = targets;
                available[1] = timestamp;
                available[2] = utf8;
                available[3] = text;
                available[4] = XA_STRING;
                *type_return = XA_ATOM;
                *value_return = available;
                *length_return = 5;
                *format_return = 32;
                return True;
        }
        if (*target == timestamp) {
                Time *value = (Time *)XtMalloc(sizeof(*value));

                *value = vt->vt.selection_time;
                *type_return = XA_INTEGER;
                *value_return = value;
                *length_return = 1;
                *format_return = 32;
                return True;
        }
        if (*target == XA_STRING) {
                size_t converted_length;
                uint8_t *converted = Utf8ToLatin1(vt->vt.selection_text,
                                                  vt->vt.selection_text_length, &converted_length);
                uint8_t *value;

                if (converted == NULL)
                        return False;
                value = (uint8_t *)XtMalloc(converted_length + 1U);
                memcpy(value, converted, converted_length + 1U);
                free(converted);
                *type_return = XA_STRING;
                *value_return = value;
                *length_return = (unsigned long)converted_length;
                *format_return = 8;
                return True;
        }
        if (*target == utf8 || *target == text) {
                uint8_t *value = (uint8_t *)XtMalloc(vt->vt.selection_text_length + 1U);

                if (vt->vt.selection_text_length != 0)
                        memcpy(value, vt->vt.selection_text, vt->vt.selection_text_length);
                value[vt->vt.selection_text_length] = '\0';
                *type_return = utf8;
                *value_return = value;
                *length_return = (unsigned long)vt->vt.selection_text_length;
                *format_return = 8;
                return True;
        }
        return False;
}

static void
LoseSelection(Widget widget, Atom *selection)
{
        Vt100Rec *vt = VtAsRecord(widget);
        Cardinal index;
        char *name;

        if (vt->vt.disowning_selections)
                return;
        for (index = 0; index < vt->vt.owned_selection_count; ++index) {
                if (vt->vt.owned_selections[index] == *selection) {
                        memmove(&vt->vt.owned_selections[index],
                                &vt->vt.owned_selections[index + 1U],
                                (vt->vt.owned_selection_count - index - 1U) * sizeof(Atom));
                        --vt->vt.owned_selection_count;
                        break;
                }
        }
        name = XGetAtomName(XtDisplay(widget), *selection);
        XtpLog(XTP_LOG_INFO, "selection", "lost %s ownership remaining=%u",
               name != NULL ? name : "(unknown)", (unsigned int)vt->vt.owned_selection_count);
        if (name != NULL)
                XFree(name);
        if (vt->vt.owned_selection_count != 0)
                return;
        free(vt->vt.selection_text);
        vt->vt.selection_text = NULL;
        vt->vt.selection_text_length = 0;
        if (vt->vt.terminal != NULL)
                XtpTerminalSelectionClear(vt->vt.terminal);
        XtpVtUpdate(widget);
}

static void
DisownSelections(Vt100Rec *vt, Time time)
{
        Atom *owned = vt->vt.owned_selections;
        Cardinal count = vt->vt.owned_selection_count;
        Cardinal index;

        vt->vt.owned_selections = NULL;
        vt->vt.owned_selection_count = 0;
        vt->vt.disowning_selections = True;
        for (index = 0; index < count; ++index)
                XtDisownSelection((Widget)vt, owned[index], time);
        vt->vt.disowning_selections = False;
        free(owned);
}

static Boolean
SelectionCell(Vt100Rec *vt, int x, int y, uint16_t *column, uint16_t *row)
{
        int grid_x = x - VtTerminalX(vt);
        int grid_y = y - (int)vt->vt.internal_border;
        unsigned int cell_width = XtpVtCellWidth((Widget)vt);
        unsigned int cell_height = XtpVtCellHeight((Widget)vt);

        if (grid_x < 0 || grid_y < 0 || cell_width == 0 || cell_height == 0)
                return False;
        *column = (uint16_t)((unsigned int)grid_x / cell_width);
        *row = (uint16_t)((unsigned int)grid_y / cell_height);
        return *column < (unsigned int)vt->vt.columns && *row < (unsigned int)vt->vt.rows;
}

static Boolean
SameUri(const uint8_t *left, size_t left_length, const uint8_t *right, size_t right_length)
{
        return left_length == right_length &&
               (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static Boolean
SameHyperlinkTarget(const VtHyperlinkTarget *left, const VtHyperlinkTarget *right)
{
        return SameUri(left->uri, left->length, right->uri, right->length) &&
               left->inferred == right->inferred &&
               (!left->inferred ||
                (left->first_cell == right->first_cell && left->last_cell == right->last_cell));
}

static void
ClearHyperlinkTarget(VtHyperlinkTarget *target)
{
        free(target->uri);
        memset(target, 0, sizeof(*target));
}

static size_t
HyperlinkCellText(const VisualCell *cell, const char **text)
{
        if (cell->width == 0)
                return 0;
        if (cell->text_length == 0) {
                *text = " ";
                return 1;
        }
        *text = cell->text;
        return cell->text_length;
}

static Boolean
InferredHyperlinkAt(Vt100Rec *vt, uint16_t column, uint16_t row, VtHyperlinkTarget *target)
{
        uint8_t *text;
        size_t first_row;
        size_t last_row;
        size_t cell_count;
        size_t capacity = 0;
        size_t clicked_cell;
        size_t pointer_start = SIZE_MAX;
        size_t pointer_end = SIZE_MAX;
        size_t length = 0;
        size_t current_row;
        XtpUrlMatch match;
        Boolean found = False;

        if (!vt->vt.frame_valid || vt->vt.frame_cells == NULL || column >= vt->vt.frame_columns ||
            row >= vt->vt.frame_rows)
                return False;
        first_row = row;
        while (first_row > 0 &&
               vt->vt.frame_cells[(first_row - 1U) * vt->vt.frame_columns].row_wrapped)
                --first_row;
        last_row = row;
        while (last_row + 1U < vt->vt.frame_rows &&
               vt->vt.frame_cells[last_row * vt->vt.frame_columns].row_wrapped)
                ++last_row;
        cell_count = (last_row - first_row + 1U) * vt->vt.frame_columns;
        if (cell_count == 0 || cell_count > XTP_URL_SCAN_MAX_CELLS)
                return False;
        clicked_cell = (size_t)row * vt->vt.frame_columns + column;
        if (vt->vt.frame_cells[clicked_cell].width == 0 && column != 0)
                --clicked_cell;
        for (current_row = first_row; current_row <= last_row; ++current_row) {
                size_t current_column;

                for (current_column = 0; current_column < vt->vt.frame_columns; ++current_column) {
                        const char *cell_text;
                        size_t frame_index = current_row * vt->vt.frame_columns + current_column;
                        size_t cell_length =
                            HyperlinkCellText(&vt->vt.frame_cells[frame_index], &cell_text);

                        if (cell_length > XTP_URL_SCAN_MAX_BYTES - capacity)
                                return False;
                        capacity += cell_length;
                }
        }
        if (capacity == 0)
                return False;
        text = malloc(capacity);
        if (text == NULL)
                return False;
        for (current_row = first_row; current_row <= last_row; ++current_row) {
                size_t current_column;

                for (current_column = 0; current_column < vt->vt.frame_columns; ++current_column) {
                        size_t frame_index = current_row * vt->vt.frame_columns + current_column;
                        const VisualCell *cell = &vt->vt.frame_cells[frame_index];
                        const char *cell_text;
                        size_t cell_length = HyperlinkCellText(cell, &cell_text);

                        if (cell_length == 0)
                                continue;
                        if (frame_index == clicked_cell)
                                pointer_start = length;
                        memcpy(text + length, cell_text, cell_length);
                        length += cell_length;
                        if (frame_index == clicked_cell)
                                pointer_end = length;
                }
        }
        if (pointer_start == SIZE_MAX ||
            !XtpUrlMatchAt(text, length, pointer_start, pointer_end, &match) ||
            (match.end == length &&
             vt->vt.frame_cells[last_row * vt->vt.frame_columns].row_wrapped))
                goto done;
        target->uri = malloc(match.end - match.start + 1U);
        if (target->uri == NULL)
                goto done;
        target->length = match.end - match.start;
        memcpy(target->uri, text + match.start, target->length);
        target->uri[target->length] = '\0';
        target->inferred = True;
        length = 0;
        for (current_row = first_row; current_row <= last_row; ++current_row) {
                size_t current_column;

                for (current_column = 0; current_column < vt->vt.frame_columns; ++current_column) {
                        const char *cell_text;
                        size_t frame_index = current_row * vt->vt.frame_columns + current_column;
                        size_t cell_length =
                            HyperlinkCellText(&vt->vt.frame_cells[frame_index], &cell_text);

                        if (match.start >= length && match.start < length + cell_length)
                                target->first_cell = frame_index;
                        if (match.end - 1U >= length && match.end - 1U < length + cell_length)
                                target->last_cell = frame_index;
                        length += cell_length;
                }
        }
        found = True;

done:
        free(text);
        return found;
}

static Boolean
HyperlinkAtPointer(Vt100Rec *vt, int x, int y, VtHyperlinkTarget *target)
{
        uint16_t column;
        uint16_t row;

        memset(target, 0, sizeof(*target));
        if (vt->vt.terminal == NULL || !SelectionCell(vt, x, y, &column, &row))
                return False;
        if (XtpTerminalHyperlinkAt(vt->vt.terminal, column, row, &target->uri, &target->length) ==
                0 &&
            target->length != 0)
                return True;
        ClearHyperlinkTarget(target);
        return InferredHyperlinkAt(vt, column, row, target);
}

Boolean
VtHyperlinkTargetMatchesCell(Vt100Rec *vt, const XtpRenderCell *cell)
{
        uint8_t *uri = NULL;
        size_t length = 0;
        Boolean matches = False;

        if (vt->vt.hovered_hyperlink.uri == NULL)
                return False;
        if (vt->vt.hovered_hyperlink.inferred) {
                size_t index = (size_t)cell->row * vt->vt.frame_columns + cell->column;

                return index >= vt->vt.hovered_hyperlink.first_cell &&
                       index <= vt->vt.hovered_hyperlink.last_cell;
        }
        if (!cell->hyperlink || vt->vt.terminal == NULL)
                return False;
        if (XtpTerminalHyperlinkAt(vt->vt.terminal, cell->column, cell->row, &uri, &length) == 0)
                matches = SameUri(uri, length, vt->vt.hovered_hyperlink.uri,
                                  vt->vt.hovered_hyperlink.length);
        free(uri);
        return matches;
}

static void
SetHoveredHyperlink(Vt100Rec *vt, int x, int y, unsigned int state)
{
        VtHyperlinkTarget target = {0};
        Boolean found = False;

        if ((state & ShiftMask) != 0)
                found = HyperlinkAtPointer(vt, x, y, &target);
        if (!found)
                ClearHyperlinkTarget(&target);
        if (SameHyperlinkTarget(&target, &vt->vt.hovered_hyperlink)) {
                ClearHyperlinkTarget(&target);
                return;
        }
        ClearHyperlinkTarget(&vt->vt.hovered_hyperlink);
        vt->vt.hovered_hyperlink = target;
        if (target.uri != NULL)
                XtpLogBytePreview(XTP_LOG_DEBUG, "hyperlink",
                                  target.inferred ? "hover inferred" : "hover", target.uri,
                                  target.length);
        else
                XtpLog(XTP_LOG_DEBUG, "hyperlink", "hover cleared");
        if (XtIsRealized((Widget)vt) && vt->vt.terminal != NULL) {
                if (VtRenderTerminal(vt, True) != 0)
                        XtpLog(XTP_LOG_ERROR, "hyperlink", "hover repaint failed");
                XFlush(XtDisplay((Widget)vt));
        }
}

static Boolean
IsShiftKey(const XKeyEvent *event)
{
        KeySym key = XLookupKeysym((XKeyEvent *)event, 0);

        return key == XK_Shift_L || key == XK_Shift_R;
}

void
VtHyperlinkEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        Vt100Rec *vt = closure;

        (void)widget;
        (void)continue_dispatch;
        if (event->type == MotionNotify) {
                SetHoveredHyperlink(vt, event->xmotion.x, event->xmotion.y, event->xmotion.state);
        } else if ((event->type == KeyPress || event->type == KeyRelease) &&
                   IsShiftKey(&event->xkey) && XtIsRealized((Widget)vt)) {
                Window root;
                Window child;
                int root_x;
                int root_y;
                int x;
                int y;
                unsigned int state;

                if (XQueryPointer(XtDisplay((Widget)vt), XtWindow((Widget)vt), &root, &child,
                                  &root_x, &root_y, &x, &y, &state)) {
                        if (event->type == KeyPress)
                                state |= ShiftMask;
                        else
                                state &= ~ShiftMask;
                        SetHoveredHyperlink(vt, x, y, state);
                }
        } else if (event->type == LeaveNotify) {
                SetHoveredHyperlink(vt, 0, 0, 0);
        }
}

static Boolean
HttpUri(const uint8_t *uri, size_t length)
{
        static const char http[] = "http://";
        static const char https[] = "https://";

        return (length >= sizeof(http) - 1U &&
                strncasecmp((const char *)uri, http, sizeof(http) - 1U) == 0) ||
               (length >= sizeof(https) - 1U &&
                strncasecmp((const char *)uri, https, sizeof(https) - 1U) == 0);
}

static int
OpenHttpUri(const uint8_t *uri, size_t length)
{
        pid_t child;
        int status;

        if (!HttpUri(uri, length) || memchr(uri, '\0', length) != NULL)
                return 1;
        child = fork();
        if (child < 0)
                return -1;
        if (child == 0) {
                pid_t opener = fork();

                if (opener < 0)
                        _exit(127);
                if (opener != 0)
                        _exit(0);
                (void)setsid();
                execlp("xdg-open", "xdg-open", (const char *)uri, (char *)NULL);
                _exit(127);
        }
        while (waitpid(child, &status, 0) < 0) {
                if (errno != EINTR)
                        return -1;
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

void
VtHyperlinkStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        VtHyperlinkTarget target = {0};

        if (event == NULL || event->type != ButtonPress || event->xbutton.button != Button1)
                return;
        if (HyperlinkAtPointer(vt, event->xbutton.x, event->xbutton.y, &target)) {
                ClearHyperlinkTarget(&vt->vt.pressed_hyperlink);
                vt->vt.pressed_hyperlink = target;
                SetHoveredHyperlink(vt, event->xbutton.x, event->xbutton.y,
                                    event->xbutton.state | ShiftMask);
                XtpLogBytePreview(XTP_LOG_DEBUG, "hyperlink", "press", target.uri, target.length);
                return;
        }
        ClearHyperlinkTarget(&target);
        VtSelectStartAction(widget, event, params, num_params);
}

static Boolean
FinishHyperlinkPress(Vt100Rec *vt, const XButtonEvent *event)
{
        VtHyperlinkTarget target = {0};
        Boolean matches;
        int opened;

        if (vt->vt.pressed_hyperlink.uri == NULL || event->button != Button1)
                return False;
        matches = (event->state & ShiftMask) != 0 &&
                  HyperlinkAtPointer(vt, event->x, event->y, &target) &&
                  SameHyperlinkTarget(&target, &vt->vt.pressed_hyperlink);
        if (matches) {
                opened = OpenHttpUri(target.uri, target.length);
                if (opened == 0) {
                        XtpLogBytePreview(XTP_LOG_INFO, "hyperlink", "opened", target.uri,
                                          target.length);
                } else if (opened > 0) {
                        XtpLogBytePreview(XTP_LOG_INFO, "hyperlink", "blocked", target.uri,
                                          target.length);
                } else {
                        XtpLog(XTP_LOG_ERROR, "hyperlink", "cannot launch xdg-open");
                        XBell(XtDisplay((Widget)vt), 0);
                }
        }
        ClearHyperlinkTarget(&target);
        ClearHyperlinkTarget(&vt->vt.pressed_hyperlink);
        SetHoveredHyperlink(vt, event->x, event->y, event->state);
        return True;
}

static Boolean
SelectionCellClamped(Vt100Rec *vt, int x, int y, uint16_t *column, uint16_t *row)
{
        int grid_x = x - VtTerminalX(vt);
        int grid_y = y - (int)vt->vt.internal_border;
        unsigned int cell_width = XtpVtCellWidth((Widget)vt);
        unsigned int cell_height = XtpVtCellHeight((Widget)vt);

        if (cell_width == 0 || cell_height == 0 || vt->vt.columns <= 0 || vt->vt.rows <= 0)
                return False;
        if (grid_x < 0)
                *column = 0;
        else if ((unsigned int)grid_x / cell_width >= (unsigned int)vt->vt.columns)
                *column = (uint16_t)(vt->vt.columns - 1);
        else
                *column = (uint16_t)((unsigned int)grid_x / cell_width);
        if (grid_y < 0)
                *row = 0;
        else if ((unsigned int)grid_y / cell_height >= (unsigned int)vt->vt.rows)
                *row = (uint16_t)(vt->vt.rows - 1);
        else
                *row = (uint16_t)((unsigned int)grid_y / cell_height);
        return True;
}

static void
StopSelectionAutoscroll(Vt100Rec *vt)
{
        if (vt->vt.selection_autoscroll_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.selection_autoscroll_timer);
                vt->vt.selection_autoscroll_timer = (XtIntervalId)0;
        }
}

static void ScheduleSelectionAutoscroll(Vt100Rec *vt);

static XtpSelectionAutoscroll
SelectionPointerAutoscroll(const Vt100Rec *vt)
{
        if (vt->vt.selection_pointer_y <= 1)
                return XTP_SELECTION_AUTOSCROLL_UP;
        if (vt->vt.selection_pointer_y > (int)vt->core.height - 1)
                return XTP_SELECTION_AUTOSCROLL_DOWN;
        return XTP_SELECTION_AUTOSCROLL_NONE;
}

static void
SelectionAutoscrollTick(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;
        XtpSelectionAutoscroll direction = XTP_SELECTION_AUTOSCROLL_NONE;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        uint16_t column;
        uint16_t row;
        int result;

        (void)timer;
        vt->vt.selection_autoscroll_timer = (XtIntervalId)0;
        if (!vt->vt.selection_dragging || vt->vt.terminal == NULL ||
            !SelectionCellClamped(vt, vt->vt.selection_pointer_x, vt->vt.selection_pointer_y,
                                  &column, &row))
                return;
        if (vt->vt.selection_extending) {
                direction = SelectionPointerAutoscroll(vt);
        } else if (XtpTerminalSelectionGetAutoscroll(vt->vt.terminal, &direction) != 0) {
                return;
        }
        if (direction == XTP_SELECTION_AUTOSCROLL_NONE)
                return;
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &before) != 0)
                return;
        row = direction == XTP_SELECTION_AUTOSCROLL_UP ? 0 : (uint16_t)(vt->vt.rows - 1);
        if (vt->vt.selection_extending) {
                result = XtpTerminalScrollBy(vt->vt.terminal,
                                             direction == XTP_SELECTION_AUTOSCROLL_UP ? -1 : 1);
                if (result == 0)
                        result = XtpTerminalSelectionExtendActive(
                            vt->vt.terminal, column, row, vt->vt.selection_rectangle != False);
        } else {
                result = XtpTerminalSelectionAutoscrollTick(
                    vt->vt.terminal, column, row, vt->vt.selection_pointer_x,
                    vt->vt.selection_pointer_y, (uint32_t)vt->vt.columns,
                    XtpVtCellWidth((Widget)vt), (uint32_t)VtTerminalX(vt), vt->core.height,
                    vt->vt.selection_rectangle != False);
        }
        if (result < 0) {
                XtpLog(XTP_LOG_ERROR, "selection", "autoscroll tick failed");
                XBell(XtDisplay((Widget)vt), 0);
                return;
        }
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &after) != 0)
                return;
        if (after.offset == before.offset)
                return;
        VtUpdateScrollbar(vt);
        XtpVtUpdate((Widget)vt);
        ScheduleSelectionAutoscroll(vt);
}

static void
ScheduleSelectionAutoscroll(Vt100Rec *vt)
{
        XtpSelectionAutoscroll direction = XTP_SELECTION_AUTOSCROLL_NONE;

        if (!vt->vt.selection_dragging || vt->vt.terminal == NULL) {
                StopSelectionAutoscroll(vt);
                return;
        }
        if (vt->vt.selection_extending) {
                direction = SelectionPointerAutoscroll(vt);
        } else if (XtpTerminalSelectionGetAutoscroll(vt->vt.terminal, &direction) != 0) {
                StopSelectionAutoscroll(vt);
                return;
        }
        if (direction == XTP_SELECTION_AUTOSCROLL_NONE) {
                StopSelectionAutoscroll(vt);
                return;
        }
        if (vt->vt.selection_autoscroll_timer == (XtIntervalId)0)
                vt->vt.selection_autoscroll_timer =
                    XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt),
                                    XTP_SELECTION_AUTOSCROLL_MS, SelectionAutoscrollTick, vt);
}

static XtpSelectionUnit
EvalSelectUnit(Vt100Rec *vt, Time button_down_time, unsigned int button, XtpSelectionUnit fallback,
               Boolean *repeat)
{
        Time delta;
        static const XtpSelectionUnit units[] = {
            XTP_SELECTION_CELL,
            XTP_SELECTION_WORD,
            XTP_SELECTION_LINE,
        };

        if (button != vt->vt.last_button || vt->vt.last_button_up_time == 0) {
                delta = (Time)vt->vt.multi_click_time + 1U;
        } else if (button_down_time > vt->vt.last_button_up_time) {
                delta = button_down_time - vt->vt.last_button_up_time;
        } else {
                delta = ((Time)~0U - vt->vt.last_button_up_time) + button_down_time;
        }
        if (delta > (Time)vt->vt.multi_click_time) {
                vt->vt.number_of_clicks = 1;
                *repeat = False;
                return fallback;
        }
        *repeat = True;
        fallback = units[vt->vt.number_of_clicks % XtNumber(units)];
        ++vt->vt.number_of_clicks;
        return fallback;
}

static XtpMouseButton
MouseButton(unsigned int button)
{
        switch (button) {
        case Button1:
                return XTP_MOUSE_BUTTON_LEFT;
        case Button2:
                return XTP_MOUSE_BUTTON_MIDDLE;
        case Button3:
                return XTP_MOUSE_BUTTON_RIGHT;
        case Button4:
                return XTP_MOUSE_BUTTON_FOUR;
        case Button5:
                return XTP_MOUSE_BUTTON_FIVE;
        default:
                if (button >= 6 && button <= 11)
                        return (XtpMouseButton)(XTP_MOUSE_BUTTON_SIX + button - 6U);
                return XTP_MOUSE_BUTTON_NONE;
        }
}

static unsigned int
ReportedMouseButtonMask(unsigned int button)
{
        return button < sizeof(unsigned int) * CHAR_BIT ? 1U << button : 0;
}

static XtpMouseButton
MotionMouseButton(const Vt100Rec *vt, unsigned int state)
{
        unsigned int buttons = vt->vt.reported_mouse_buttons;

        if ((state & Button1Mask) != 0 || (buttons & ReportedMouseButtonMask(Button1)) != 0)
                return XTP_MOUSE_BUTTON_LEFT;
        if ((state & Button3Mask) != 0 || (buttons & ReportedMouseButtonMask(Button3)) != 0)
                return XTP_MOUSE_BUTTON_RIGHT;
        if ((state & Button2Mask) != 0 || (buttons & ReportedMouseButtonMask(Button2)) != 0)
                return XTP_MOUSE_BUTTON_MIDDLE;
        return XTP_MOUSE_BUTTON_NONE;
}

static Boolean
ApplicationMouseTracking(const Vt100Rec *vt, unsigned int state)
{
        return vt->vt.terminal != NULL && (state & ShiftMask) == 0 &&
               XtpTerminalMouseTracking(vt->vt.terminal);
}

static Boolean
SendMouseInput(Vt100Rec *vt, XtpMouseAction action, XtpMouseButton button, unsigned int state,
               int x, int y, Boolean any_button_pressed)
{
        unsigned int cell_width = XtpVtCellWidth((Widget)vt);
        unsigned int cell_height = XtpVtCellHeight((Widget)vt);
        unsigned int grid_width = (unsigned int)vt->vt.columns * cell_width;
        unsigned int grid_height = (unsigned int)vt->vt.rows * cell_height;
        unsigned int padding_left = (unsigned int)VtTerminalX(vt);
        unsigned int padding_top = (unsigned int)vt->vt.internal_border;
        unsigned int occupied_width = padding_left + grid_width;
        unsigned int occupied_height = padding_top + grid_height;
        char encoded[128];
        size_t written = 0;
        XtpMouseEvent event = {
            .action = action,
            .button = button,
            .modifiers = VtModifiersFromState(state),
            .x = (float)x,
            .y = (float)y,
            .screen_width = vt->core.width,
            .screen_height = vt->core.height,
            .cell_width = cell_width,
            .cell_height = cell_height,
            .padding_top = padding_top,
            .padding_bottom =
                vt->core.height > occupied_height ? vt->core.height - occupied_height : 0,
            .padding_left = padding_left,
            .padding_right = vt->core.width > occupied_width ? vt->core.width - occupied_width : 0,
            .any_button_pressed = any_button_pressed != False,
        };

        if (XtpTerminalEncodeMouse(vt->vt.terminal, &event, encoded, sizeof(encoded), &written) !=
            0) {
                XtpLog(XTP_LOG_ERROR, "input", "mouse encoding failed action=%d button=%d", action,
                       button);
                XBell(XtDisplay((Widget)vt), 0);
                return False;
        }
        if (written != 0) {
                XtpEncodedInput input = {(const uint8_t *)encoded, written};

                XtCallCallbacks((Widget)vt, XtNinputCallback, &input);
        }
        return True;
}

static Boolean
ReportMouseButton(Vt100Rec *vt, XButtonEvent *event, XtpMouseAction action)
{
        unsigned int mask = ReportedMouseButtonMask(event->button);
        XtpMouseButton button = MouseButton(event->button);
        Boolean pressed;

        if (button == XTP_MOUSE_BUTTON_NONE)
                return False;
        if (action == XTP_MOUSE_ACTION_PRESS) {
                if (!ApplicationMouseTracking(vt, event->state))
                        return False;
                vt->vt.reported_mouse_buttons |= mask;
        } else if ((vt->vt.reported_mouse_buttons & mask) != 0) {
                vt->vt.reported_mouse_buttons &= ~mask;
        } else {
                return False;
        }
        pressed = (event->state & (Button1Mask | Button2Mask | Button3Mask)) != 0;
        if (action == XTP_MOUSE_ACTION_PRESS && event->button <= Button3)
                pressed = True;
        if (action == XTP_MOUSE_ACTION_RELEASE && event->button <= Button3) {
                static const unsigned int masks[] = {0, Button1Mask, Button2Mask, Button3Mask};

                pressed = (event->state & ~masks[event->button] &
                           (Button1Mask | Button2Mask | Button3Mask)) != 0;
        }
        return SendMouseInput(vt, action, button, event->state, event->x, event->y, pressed);
}

static Boolean
ReportMouseMotion(Vt100Rec *vt, XMotionEvent *event)
{
        XtpMouseButton button;
        Boolean pressed;

        if (vt->vt.reported_mouse_buttons == 0 && !ApplicationMouseTracking(vt, event->state))
                return False;
        button = MotionMouseButton(vt, event->state);
        pressed = button != XTP_MOUSE_BUTTON_NONE;
        return SendMouseInput(vt, XTP_MOUSE_ACTION_MOTION, button, event->state, event->x, event->y,
                              pressed);
}

void
VtSelectStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        uint16_t column;
        uint16_t row;
        Boolean repeat;
        int result;

        (void)params;
        (void)num_params;
        if (event == NULL || event->type != ButtonPress)
                return;
        if (ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS))
                return;
        if (vt->vt.terminal == NULL ||
            !SelectionCell(vt, event->xbutton.x, event->xbutton.y, &column, &row))
                return;
        vt->vt.select_unit = EvalSelectUnit(vt, event->xbutton.time, event->xbutton.button,
                                            XTP_SELECTION_CELL, &repeat);
        result = XtpTerminalSelectionStart(
            vt->vt.terminal, column, row, event->xbutton.x, event->xbutton.y,
            (uint64_t)event->xbutton.time * 1000000U, vt->vt.select_unit, repeat != False);
        if (result < 0) {
                XBell(XtDisplay(widget), 0);
                return;
        }
        vt->vt.selection_dragging = True;
        vt->vt.selection_extending = False;
        vt->vt.selection_pointer_x = event->xbutton.x;
        vt->vt.selection_pointer_y = event->xbutton.y;
        vt->vt.selection_rectangle = (event->xbutton.state & Mod1Mask) != 0;
        XtpVtUpdate(widget);
        XtpLog(XTP_LOG_DEBUG, "selection", "start column=%u row=%u immediate=%s", column, row,
               result > 0 ? "true" : "false");
}

void
VtSelectExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        uint16_t column;
        uint16_t row;
        int result;

        (void)params;
        (void)num_params;
        if (event == NULL || event->type != MotionNotify)
                return;
        if (!vt->vt.selection_dragging || vt->vt.terminal == NULL ||
            !SelectionCellClamped(vt, event->xmotion.x, event->xmotion.y, &column, &row))
                return;
        vt->vt.selection_pointer_x = event->xmotion.x;
        vt->vt.selection_pointer_y = event->xmotion.y;
        vt->vt.selection_rectangle = (event->xmotion.state & Mod1Mask) != 0;
        if (vt->vt.selection_extending) {
                result = XtpTerminalSelectionExtendActive(vt->vt.terminal, column, row,
                                                          (event->xmotion.state & Mod1Mask) != 0);
        } else {
                result = XtpTerminalSelectionExtend(
                    vt->vt.terminal, column, row, event->xmotion.x, event->xmotion.y,
                    (uint32_t)vt->vt.columns, XtpVtCellWidth(widget), (uint32_t)VtTerminalX(vt),
                    vt->core.height, (event->xmotion.state & Mod1Mask) != 0);
        }
        if (result < 0) {
                StopSelectionAutoscroll(vt);
                XBell(XtDisplay(widget), 0);
                return;
        }
        if (result > 0)
                XtpVtUpdate(widget);
        ScheduleSelectionAutoscroll(vt);
}

void
VtStartExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        uint16_t column;
        uint16_t row;
        Boolean repeat;
        int result;

        (void)params;
        (void)num_params;
        if (event == NULL || event->type != ButtonPress)
                return;
        if (ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS))
                return;
        if (vt->vt.terminal == NULL ||
            !SelectionCell(vt, event->xbutton.x, event->xbutton.y, &column, &row))
                return;
        vt->vt.select_unit = EvalSelectUnit(vt, event->xbutton.time, event->xbutton.button,
                                            vt->vt.select_unit, &repeat);
        result = XtpTerminalSelectionExtendStart(vt->vt.terminal, column, row, vt->vt.select_unit);
        if (result < 0) {
                XBell(XtDisplay(widget), 0);
                return;
        }
        if (result > 0) {
                vt->vt.selection_dragging = True;
                vt->vt.selection_extending = True;
                vt->vt.selection_pointer_x = event->xbutton.x;
                vt->vt.selection_pointer_y = event->xbutton.y;
                vt->vt.selection_rectangle = False;
                XtpVtUpdate(widget);
        }
}

static void
StoreCutBuffer(Vt100Rec *vt, int cut_buffer)
{
        size_t converted_length;
        uint8_t *converted =
            Utf8ToLatin1(vt->vt.selection_text, vt->vt.selection_text_length, &converted_length);
        unsigned long request_words = XMaxRequestSize(XtDisplay((Widget)vt));
        unsigned long request_limit = request_words > 8U ? request_words * 4U - 32U : 0U;
        int stored_length;

        if (converted == NULL) {
                XtpLog(XTP_LOG_ERROR, "selection", "cannot encode CUT_BUFFER%d", cut_buffer);
                return;
        }
        if (converted_length > request_limit || converted_length > INT_MAX) {
                XtpLog(XTP_LOG_WARNING, "selection",
                       "CUT_BUFFER%d bytes=%zu exceeds X request limit=%lu; not stored", cut_buffer,
                       converted_length, request_limit);
                free(converted);
                return;
        }
        stored_length = (int)converted_length;
        XStoreBuffer(XtDisplay((Widget)vt), (const char *)converted, stored_length, cut_buffer);
        XtpLog(XTP_LOG_INFO, "selection", "CUT_BUFFER%d bytes=%d stored=true", cut_buffer,
               stored_length);
        free(converted);
}

static void
PublishSelection(Vt100Rec *vt, String *params, Cardinal num_params)
{
        Cardinal index;

        vt->vt.owned_selections = calloc(num_params, sizeof(*vt->vt.owned_selections));
        if (num_params != 0 && vt->vt.owned_selections == NULL) {
                XtpLog(XTP_LOG_ERROR, "selection", "cannot allocate selection owner list");
                return;
        }
        for (index = 0; index < num_params; ++index) {
                SelectionSource source = ResolveSelectionSource(vt, params[index]);
                Boolean duplicate = False;
                Cardinal owned;

                if (source.kind == XTP_SELECTION_SOURCE_CUT_BUFFER) {
                        StoreCutBuffer(vt, source.cut_buffer);
                        continue;
                }
                if (source.atom == None) {
                        XtpLog(XTP_LOG_WARNING, "selection", "ignored empty selection name");
                        continue;
                }
                for (owned = 0; owned < vt->vt.owned_selection_count; ++owned) {
                        if (vt->vt.owned_selections[owned] == source.atom) {
                                duplicate = True;
                                break;
                        }
                }
                if (duplicate)
                        continue;
                {
                        Boolean owned_now =
                            XtOwnSelection((Widget)vt, source.atom, vt->vt.selection_time,
                                           ConvertSelection, LoseSelection, NULL);
                        char *atom_name = XGetAtomName(XtDisplay((Widget)vt), source.atom);

                        XtpLog(owned_now ? XTP_LOG_INFO : XTP_LOG_WARNING, "selection",
                               "publish source=%s selection=%s bytes=%zu owned=%s", params[index],
                               atom_name != NULL ? atom_name : "(unknown)",
                               vt->vt.selection_text_length, owned_now ? "true" : "false");
                        if (atom_name != NULL)
                                XFree(atom_name);
                        if (owned_now)
                                vt->vt.owned_selections[vt->vt.owned_selection_count++] =
                                    source.atom;
                }
        }
        if (num_params != 0 && vt->vt.owned_selection_count == 0) {
                XtpTerminalSelectionClear(vt->vt.terminal);
                XtpVtUpdate((Widget)vt);
        }
}

void
VtSelectEndAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        uint16_t column = 0;
        uint16_t row = 0;
        Boolean valid;
        uint8_t *text = NULL;
        size_t length = 0;

        if (event == NULL || event->type != ButtonRelease)
                return;
        if (FinishHyperlinkPress(vt, &event->xbutton))
                return;
        if (!vt->vt.selection_dragging &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE))
                return;
        if (!vt->vt.selection_dragging || vt->vt.terminal == NULL)
                return;
        StopSelectionAutoscroll(vt);
        valid = SelectionCell(vt, event->xbutton.x, event->xbutton.y, &column, &row);
        if (vt->vt.selection_extending) {
                if (valid)
                        (void)XtpTerminalSelectionExtendActive(
                            vt->vt.terminal, column, row, (event->xbutton.state & Mod1Mask) != 0);
                XtpTerminalSelectionExtendEnd(vt->vt.terminal);
        } else {
                XtpTerminalSelectionEnd(vt->vt.terminal, column, row, valid != False);
        }
        vt->vt.selection_dragging = False;
        vt->vt.selection_extending = False;
        vt->vt.last_button_up_time = event->xbutton.time;
        vt->vt.last_button = event->xbutton.button;
        if (XtpTerminalSelectionText(vt->vt.terminal, &text, &length) == 0) {
                DisownSelections(vt, event->xbutton.time);
                free(vt->vt.selection_text);
                vt->vt.selection_text = text;
                vt->vt.selection_text_length = length;
                vt->vt.selection_time = event->xbutton.time;
                PublishSelection(vt, params, *num_params);
        } else {
                DisownSelections(vt, event->xbutton.time);
                free(vt->vt.selection_text);
                vt->vt.selection_text = NULL;
                vt->vt.selection_text_length = 0;
        }
}

typedef struct
{
        Time time;
        Cardinal source_count;
        Cardinal source_index;
        Boolean tried_string;
        SelectionSource sources[];
} PasteRequest;

static void
DeliverPaste(Widget widget, const void *bytes, size_t length)
{
        XtpPaste paste;

        paste.bytes = bytes;
        paste.length = length;
        XtCallCallbacks(widget, XtNpasteCallback, &paste);
        XtpLog(XTP_LOG_INFO, "selection", "paste received bytes=%zu", paste.length);
}

static void
FinishPaste(Widget widget, PasteRequest *request, const uint8_t *bytes, size_t length,
            Boolean latin1)
{
        uint8_t *converted = NULL;

        if (latin1) {
                size_t converted_length;

                converted = Latin1ToUtf8(bytes, length, &converted_length);
                if (converted == NULL) {
                        XBell(XtDisplay(widget), 0);
                        free(request);
                        return;
                }
                bytes = converted;
                length = converted_length;
        }
        DeliverPaste(widget, bytes, length);
        free(converted);
        free(request);
}

static void RequestNextPasteSource(Widget widget, PasteRequest *request);

static void
SelectionReceived(Widget widget, XtPointer closure, Atom *selection, Atom *type, XtPointer value,
                  unsigned long *length, int *format)
{
        PasteRequest *request = closure;

        (void)selection;
        if (*type != XT_CONVERT_FAIL && value != NULL && *format == 8) {
                Boolean latin1 = *type == XA_STRING;

                if (latin1) {
                        uint8_t *converted;
                        size_t converted_length;

                        converted = Latin1ToUtf8(value, (size_t)*length, &converted_length);
                        if (converted != NULL)
                                DeliverPaste(widget, converted, converted_length);
                        else
                                XBell(XtDisplay(widget), 0);
                        free(converted);
                } else {
                        DeliverPaste(widget, value, (size_t)*length);
                }
                XtFree(value);
                free(request);
                return;
        }
        if (value != NULL)
                XtFree(value);
        if (!request->tried_string) {
                request->tried_string = True;
                XtGetSelectionValue(widget, request->sources[request->source_index].atom, XA_STRING,
                                    SelectionReceived, request, request->time);
                return;
        }
        ++request->source_index;
        request->tried_string = False;
        RequestNextPasteSource(widget, request);
}

static void
RequestNextPasteSource(Widget widget, PasteRequest *request)
{
        Atom utf8 = XInternAtom(XtDisplay(widget), "UTF8_STRING", False);

        while (request->source_index < request->source_count) {
                SelectionSource *source = &request->sources[request->source_index];

                if (source->kind == XTP_SELECTION_SOURCE_ATOM) {
                        XtGetSelectionValue(widget, source->atom, utf8, SelectionReceived, request,
                                            request->time);
                        return;
                }
                {
                        int length_return = 0;
                        char *buffer =
                            XFetchBuffer(XtDisplay(widget), &length_return, source->cut_buffer);

                        if (buffer != NULL) {
                                FinishPaste(widget, request, (const uint8_t *)buffer,
                                            length_return > 0 ? (size_t)length_return : 0U, True);
                                XFree(buffer);
                                return;
                        }
                }
                ++request->source_index;
        }
        XBell(XtDisplay(widget), 0);
        free(request);
}

static void
RequestNamedPaste(Widget widget, Time time, String *params, Cardinal num_params)
{
        static String defaults[] = {"SELECT", "CUT_BUFFER0"};
        Vt100Rec *vt = VtAsRecord(widget);
        PasteRequest *request;
        Cardinal index;

        if (num_params == 0) {
                params = defaults;
                num_params = XtNumber(defaults);
        }
        request = calloc(1, sizeof(*request) + num_params * sizeof(request->sources[0]));

        if (request == NULL) {
                XBell(XtDisplay(widget), 0);
                return;
        }
        request->time = time;
        for (index = 0; index < num_params; ++index) {
                SelectionSource source = ResolveSelectionSource(vt, params[index]);

                if (source.kind == XTP_SELECTION_SOURCE_ATOM && source.atom == None)
                        continue;
                request->sources[request->source_count++] = source;
        }
        RequestNextPasteSource(widget, request);
}

void
VtInsertSelectionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        Time time = XtLastTimestampProcessed(XtDisplay(widget));

        if (event != NULL && event->type == ButtonRelease &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE))
                return;
        if (event != NULL && event->type == KeyPress &&
            !VtAcceptLocalKeyAction(vt, event, XTP_LOCAL_ACTION_PASTE))
                return;
        if (event != NULL && (event->type == ButtonPress || event->type == ButtonRelease))
                time = event->xbutton.time;
        else if (event != NULL && event->type == KeyPress)
                time = event->xkey.time;
        RequestNamedPaste(widget, time, params, *num_params);
}

void
VtMousePressAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);

        (void)params;
        (void)num_params;
        if (event != NULL && event->type == ButtonPress)
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS);
}

void
VtMouseMotionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);

        (void)params;
        (void)num_params;
        if (event != NULL && event->type == MotionNotify && !vt->vt.selection_dragging)
                (void)ReportMouseMotion(vt, &event->xmotion);
}

static intptr_t
ScrollActionRows(Vt100Rec *vt, String *params, Cardinal num_params)
{
        long count = 1;

        if (num_params != 0) {
                char *end = NULL;
                long parsed = strtol(params[0], &end, 10);

                if (end != params[0] && *end == '\0' && parsed > 0)
                        count = parsed;
        }
        if (num_params > 1 && strcmp(params[1], "page") == 0)
                count *= vt->vt.rows;
        else if (num_params > 1 && strcmp(params[1], "halfpage") == 0)
                count *= vt->vt.rows > 1 ? vt->vt.rows / 2 : 1;
        return (intptr_t)count;
}

void
VtScrollBackAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        intptr_t rows = ScrollActionRows(vt, params, *num_params);

        if (event != NULL && event->type == KeyPress &&
            !VtAcceptLocalKeyAction(vt, event, XTP_LOCAL_ACTION_SCROLL_BACK))
                return;
        if (event != NULL && event->type == ButtonPress &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS)) {
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE);
                return;
        }
        (void)VtScrollViewportBy(vt, -rows);
}

void
VtScrollForwardAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        intptr_t rows = ScrollActionRows(vt, params, *num_params);

        if (event != NULL && event->type == KeyPress &&
            !VtAcceptLocalKeyAction(vt, event, XTP_LOCAL_ACTION_SCROLL_FORWARD))
                return;
        if (event != NULL && event->type == ButtonPress &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS)) {
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE);
                return;
        }
        (void)VtScrollViewportBy(vt, rows);
}
