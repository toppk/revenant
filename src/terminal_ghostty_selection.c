#include "terminal_ghosttyP.h"

#include <stdlib.h>
#include <string.h>

static GhosttyResult
SelectionRef(XtpTerminal *terminal, uint16_t column, uint16_t row, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_VIEWPORT,
            {.coordinate = {column, row}},
        };

        return ghostty_terminal_grid_ref(terminal->handle, point, ref);
}

static XtpSelectionResult
InstallGestureSelection(XtpTerminal *terminal, GhosttyResult result, GhosttySelection *selection)
{
        if (result == GHOSTTY_NO_VALUE)
                return XTP_SELECTION_UNCHANGED;
        if (result != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, selection) !=
                GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        return XTP_SELECTION_CHANGED;
}

static GhosttyResult
GridRefCharacterClass(XtpTerminal *terminal, const GhosttyGridRef *ref, int *character_class)
{
        GhosttyCell cell;
        uint32_t codepoint = 0;

        if (ghostty_grid_ref_cell(ref, &cell) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        *character_class = XtpCharClassOf(terminal->char_classes, codepoint);
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
ScreenSelectionRef(XtpTerminal *terminal, GhosttyPointCoordinate coordinate, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = coordinate},
        };

        return ghostty_terminal_grid_ref(terminal->handle, point, ref);
}

static bool
GridRefRowWrapped(const GhosttyGridRef *ref)
{
        GhosttyRow row;
        bool wrapped = false;

        return ghostty_grid_ref_row(ref, &row) == GHOSTTY_SUCCESS &&
               ghostty_row_get(row, GHOSTTY_ROW_DATA_WRAP, &wrapped) == GHOSTTY_SUCCESS && wrapped;
}

static bool
GridRefDrawn(const GhosttyGridRef *ref)
{
        GhosttyCell cell;
        GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
        uint32_t codepoint = 0;

        if (ghostty_grid_ref_cell(ref, &cell) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_WIDE, &wide) != GHOSTTY_SUCCESS)
                return false;
        return codepoint != 0 || wide != GHOSTTY_CELL_WIDE_NARROW;
}

static int
LastDrawnColumn(XtpTerminal *terminal, uint32_t screen_row, uint16_t columns)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate point;
        int column;

        point.y = screen_row;
        for (column = (int)columns - 1; column >= 0; --column) {
                point.x = (uint16_t)column;
                if (ScreenSelectionRef(terminal, point, &ref) != GHOSTTY_SUCCESS)
                        return -1;
                if (GridRefDrawn(&ref))
                        return column;
        }
        return -1;
}

static GhosttyResult
UndrawnSuffixSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyPointCoordinate point;
        GhosttyPointCoordinate first;
        GhosttyPointCoordinate last;
        uint16_t columns = 0;
        int last_drawn;

        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0)
                return GHOSTTY_INVALID_VALUE;
        last_drawn = LastDrawnColumn(terminal, point.y, columns);
        if ((int)point.x <= last_drawn)
                return GHOSTTY_NO_VALUE;
        first.x = (uint16_t)(last_drawn + 1);
        first.y = point.y;
        last.x = columns - 1U;
        last.y = point.y;
        if (ScreenSelectionRef(terminal, first, &selection->start) != GHOSTTY_SUCCESS ||
            ScreenSelectionRef(terminal, last, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static bool
PointBefore(GhosttyPointCoordinate left, GhosttyPointCoordinate right)
{
        return left.y < right.y || (left.y == right.y && left.x < right.x);
}

static bool
PointInUndrawnSuffix(XtpTerminal *terminal, GhosttyPointCoordinate point, uint16_t columns)
{
        return (int)point.x > LastDrawnColumn(terminal, point.y, columns);
}

static bool
AdvanceWordPoint(XtpTerminal *terminal, GhosttyPointCoordinate *point, uint16_t columns,
                 int direction)
{
        GhosttyGridRef edge = GHOSTTY_INIT_SIZED(GhosttyGridRef);

        if (direction < 0) {
                if (point->x != 0) {
                        --point->x;
                        return true;
                }
                if (point->y == 0) {
                        return false;
                }
                {
                        GhosttyPointCoordinate previous = {columns - 1U, point->y - 1U};

                        if (ScreenSelectionRef(terminal, previous, &edge) != GHOSTTY_SUCCESS ||
                            !GridRefRowWrapped(&edge))
                                return false;
                }
                --point->y;
                point->x = columns - 1U;
                return true;
        }
        if ((uint16_t)(point->x + 1U) < columns) {
                ++point->x;
                return true;
        }
        if (ScreenSelectionRef(terminal, *point, &edge) != GHOSTTY_SUCCESS ||
            !GridRefRowWrapped(&edge))
                return false;
        ++point->y;
        point->x = 0;
        return true;
}

static GhosttyResult
CharacterClassSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyPointCoordinate origin;
        GhosttyPointCoordinate first;
        GhosttyPointCoordinate last;
        GhosttyPointCoordinate probe_point;
        GhosttyGridRef probe = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        uint16_t columns = 0;
        int wanted_class;
        int probe_class;

        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &origin) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0 ||
            GridRefCharacterClass(terminal, &target, &wanted_class) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointInUndrawnSuffix(terminal, origin, columns))
                return GHOSTTY_NO_VALUE;
        first = origin;
        probe_point = first;
        while (AdvanceWordPoint(terminal, &probe_point, columns, -1)) {
                if (PointInUndrawnSuffix(terminal, probe_point, columns) ||
                    ScreenSelectionRef(terminal, probe_point, &probe) != GHOSTTY_SUCCESS ||
                    GridRefCharacterClass(terminal, &probe, &probe_class) != GHOSTTY_SUCCESS ||
                    probe_class != wanted_class)
                        break;
                first = probe_point;
        }
        last = origin;
        probe_point = last;
        while (AdvanceWordPoint(terminal, &probe_point, columns, 1)) {
                if (PointInUndrawnSuffix(terminal, probe_point, columns) ||
                    ScreenSelectionRef(terminal, probe_point, &probe) != GHOSTTY_SUCCESS ||
                    GridRefCharacterClass(terminal, &probe, &probe_class) != GHOSTTY_SUCCESS ||
                    probe_class != wanted_class)
                        break;
                last = probe_point;
        }
        if (ScreenSelectionRef(terminal, first, &selection->start) != GHOSTTY_SUCCESS ||
            ScreenSelectionRef(terminal, last, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
CharacterClassDragSelection(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                            GhosttySelection *selection)
{
        GhosttySelection anchor_word = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection target_word = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;
        GhosttyResult anchor_result;
        GhosttyResult target_result;

        anchor_result = CharacterClassSelection(terminal, anchor, &anchor_word);
        if (anchor_result == GHOSTTY_NO_VALUE)
                return GHOSTTY_NO_VALUE;
        target_result = CharacterClassSelection(terminal, target, &target_word);
        if (target_result == GHOSTTY_NO_VALUE)
                target_result = UndrawnSuffixSelection(terminal, target, &target_word);
        if (anchor_result != GHOSTTY_SUCCESS || target_result != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point)) {
                selection->start = target_word.start;
                selection->end = anchor_word.end;
        } else {
                selection->start = anchor_word.start;
                selection->end = target_word.end;
        }
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
ExpandCellSelectionForUndrawn(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                              GhosttySelection *selection)
{
        GhosttySelection undrawn = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection ordered = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;
        GhosttyResult result = UndrawnSuffixSelection(terminal, target, &undrawn);

        if (result == GHOSTTY_NO_VALUE)
                return GHOSTTY_SUCCESS;
        if (result != GHOSTTY_SUCCESS ||
            ghostty_terminal_selection_ordered(terminal->handle, selection,
                                               GHOSTTY_SELECTION_ORDER_FORWARD,
                                               &ordered) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point))
                ordered.start = undrawn.start;
        else
                ordered.end = undrawn.end;
        *selection = ordered;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
LineSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyTerminalSelectLineOptions options =
            GHOSTTY_INIT_SIZED(GhosttyTerminalSelectLineOptions);
        GhosttyPointCoordinate point;
        GhosttyGridRef drawn = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        uint16_t columns = 0;
        int last_drawn;
        GhosttyResult result;

        options.ref = target;
        result = ghostty_terminal_select_line(terminal->handle, &options, selection);
        if (result != GHOSTTY_NO_VALUE)
                return result;
        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0)
                return GHOSTTY_INVALID_VALUE;
        last_drawn = LastDrawnColumn(terminal, point.y, columns);
        if (last_drawn >= 0) {
                point.x = (uint16_t)last_drawn;
                if (ScreenSelectionRef(terminal, point, &drawn) != GHOSTTY_SUCCESS)
                        return GHOSTTY_INVALID_VALUE;
                options.ref = drawn;
                result = ghostty_terminal_select_line(terminal->handle, &options, selection);
                if (result != GHOSTTY_NO_VALUE)
                        return result;
        }
        point.x = 0;
        if (ScreenSelectionRef(terminal, point, &selection->start) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        point.x = columns - 1U;
        if (ScreenSelectionRef(terminal, point, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
LineDragSelection(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                  GhosttySelection *selection)
{
        GhosttySelection anchor_line = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection target_line = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;

        if (LineSelection(terminal, anchor, &anchor_line) != GHOSTTY_SUCCESS ||
            LineSelection(terminal, target, &target_line) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point)) {
                selection->start = target_line.start;
                selection->end = anchor_line.end;
        } else {
                selection->start = anchor_line.start;
                selection->end = target_line.end;
        }
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static XtpSelectionResult
InstallDragSelection(XtpTerminal *terminal, GhosttyResult result, GhosttyGridRef target,
                     bool rectangle, GhosttySelection *selection)
{
        GhosttyGridRef anchor = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelectionGestureBehavior behavior = GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;

        if (ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_BEHAVIOR,
                                          &behavior) == GHOSTTY_SUCCESS &&
            ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_ANCHOR,
                                          &anchor) == GHOSTTY_SUCCESS) {
                if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD)
                        result = CharacterClassDragSelection(terminal, anchor, target, selection);
                else if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE)
                        result = LineDragSelection(terminal, anchor, target, selection);
                else if (!rectangle && result == GHOSTTY_SUCCESS)
                        result = ExpandCellSelectionForUndrawn(terminal, anchor, target, selection);
        }
        return InstallGestureSelection(terminal, result, selection);
}

XtpSelectionResult
XtpTerminalSelectionStart(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                          double surface_y, uint64_t time_ns, XtpSelectionUnit unit, bool repeat)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelectionGestureBehavior behavior;
        GhosttySelectionGestureBehaviors behaviors;
        GhosttySurfacePosition position = {surface_x, surface_y};
        const uint64_t repeat_interval_ns = UINT64_MAX;
        const double repeat_distance = 1.0e100;
        GhosttyResult result;

        if (terminal == NULL || unit > XTP_SELECTION_LINE)
                return XTP_SELECTION_ERROR;
        behavior = unit == XTP_SELECTION_WORD   ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD
                   : unit == XTP_SELECTION_LINE ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE
                                                : GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        behaviors.single_click = behavior;
        behaviors.double_click = behavior;
        behaviors.triple_click = behavior;
        if (!repeat)
                ghostty_selection_gesture_reset(terminal->selection_gesture, terminal->handle);
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, NULL) !=
                GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF,
                                                &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS,
                                                &time_ns) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(
                terminal->selection_press, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS,
                &repeat_interval_ns) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE,
                                                &repeat_distance) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_BEHAVIORS,
                                                &behaviors) != GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_press, &selection);
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD) {
                GhosttyGridRef anchor = GHOSTTY_INIT_SIZED(GhosttyGridRef);

                if (ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                                  GHOSTTY_SELECTION_GESTURE_DATA_ANCHOR,
                                                  &anchor) == GHOSTTY_SUCCESS)
                        result = CharacterClassDragSelection(terminal, anchor, ref, &selection);
                else
                        result = CharacterClassSelection(terminal, ref, &selection);
                return InstallGestureSelection(terminal, result, &selection);
        }
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE) {
                result = LineSelection(terminal, ref, &selection);
                return InstallGestureSelection(terminal, result, &selection);
        }
        return InstallGestureSelection(terminal, result, &selection);
}

XtpSelectionResult
XtpTerminalSelectionExtend(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                           double surface_y, uint32_t columns, uint32_t cell_width,
                           uint32_t padding_left, uint32_t screen_height, bool rectangle)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySurfacePosition position = {surface_x, surface_y};
        GhosttySelectionGestureGeometry geometry = {
            columns,
            cell_width,
            padding_left,
            screen_height,
        };
        GhosttyResult result;

        if (terminal == NULL || SelectionRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF,
                                                &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY,
                                                &geometry) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_RECTANGLE,
                                                &rectangle) != GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_drag, &selection);
        return InstallDragSelection(terminal, result, ref, rectangle, &selection);
}

int
XtpTerminalSelectionGetAutoscroll(XtpTerminal *terminal, XtpSelectionAutoscroll *direction)
{
        GhosttySelectionGestureAutoscroll ghostty_direction;

        if (terminal == NULL || direction == NULL ||
            ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_AUTOSCROLL,
                                          &ghostty_direction) != GHOSTTY_SUCCESS)
                return -1;
        if (ghostty_direction == GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_UP)
                *direction = XTP_SELECTION_AUTOSCROLL_UP;
        else if (ghostty_direction == GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_DOWN)
                *direction = XTP_SELECTION_AUTOSCROLL_DOWN;
        else
                *direction = XTP_SELECTION_AUTOSCROLL_NONE;
        return 0;
}

XtpSelectionResult
XtpTerminalSelectionAutoscrollTick(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                   double surface_x, double surface_y, uint32_t columns,
                                   uint32_t cell_width, uint32_t padding_left,
                                   uint32_t screen_height, bool rectangle)
{
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate viewport = {column, row};
        GhosttySurfacePosition position = {surface_x, surface_y};
        GhosttySelectionGestureGeometry geometry = {
            columns,
            cell_width,
            padding_left,
            screen_height,
        };
        GhosttyResult result;

        if (terminal == NULL ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_VIEWPORT,
                                                &viewport) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY,
                                                &geometry) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_RECTANGLE,
                                                &rectangle) != GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_autoscroll, &selection);
        if (result == GHOSTTY_NO_VALUE)
                return XTP_SELECTION_UNCHANGED;
        if (result != GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        return InstallDragSelection(terminal, result, target, rectangle, &selection);
}

void
XtpTerminalSelectionEnd(XtpTerminal *terminal, uint16_t column, uint16_t row, bool valid)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);

        if (terminal == NULL)
                return;
        if (valid && SelectionRef(terminal, column, row, &ref) == GHOSTTY_SUCCESS)
                (void)ghostty_selection_gesture_event_set(
                    terminal->selection_release, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref);
        else
                (void)ghostty_selection_gesture_event_set(
                    terminal->selection_release, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, NULL);
        (void)ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                              terminal->selection_release, NULL);
}

static uint64_t
SelectionCoordinate(GhosttyPointCoordinate point, uint16_t columns)
{
        return (uint64_t)point.y * columns + point.x;
}

static GhosttyResult
SelectionForBehavior(XtpTerminal *terminal, GhosttyGridRef target,
                     GhosttySelectionGestureBehavior behavior, GhosttySelection *selection)
{
        GhosttyResult result;

        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD) {
                result = CharacterClassSelection(terminal, target, selection);
                if (result == GHOSTTY_NO_VALUE)
                        result = UndrawnSuffixSelection(terminal, target, selection);
                return result;
        }
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE) {
                return LineSelection(terminal, target, selection);
        }
        result = UndrawnSuffixSelection(terminal, target, selection);
        if (result == GHOSTTY_NO_VALUE) {
                selection->start = target;
                selection->end = target;
                selection->rectangle = false;
                return GHOSTTY_SUCCESS;
        }
        return result;
}

static GhosttyResult
TrackSelectionPoint(XtpTerminal *terminal, GhosttyPointCoordinate coordinate,
                    GhosttyTrackedGridRef *tracked)
{
        GhosttyPoint point;

        point.tag = GHOSTTY_POINT_TAG_SCREEN;
        point.value.coordinate = coordinate;
        return ghostty_terminal_grid_ref_track(terminal->handle, point, tracked);
}

XtpSelectionResult
XtpTerminalSelectionExtendStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                XtpSelectionUnit unit)
{
        GhosttySelection current = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection ordered = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate start_point;
        GhosttyPointCoordinate end_point;
        GhosttyPointCoordinate target_point;
        uint16_t columns = 0;
        uint64_t start_distance;
        uint64_t end_distance;
        uint64_t target_coordinate;
        uint64_t endpoint_coordinate;
        GhosttySelectionGestureBehavior behavior;

        if (terminal == NULL || unit > XTP_SELECTION_LINE ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_SELECTION, &current) !=
                GHOSTTY_SUCCESS ||
            ghostty_terminal_selection_ordered(terminal->handle, &current,
                                               GHOSTTY_SELECTION_ORDER_FORWARD,
                                               &ordered) != GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &ordered.start,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &start_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &ordered.end,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &end_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        behavior = unit == XTP_SELECTION_WORD   ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD
                   : unit == XTP_SELECTION_LINE ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE
                                                : GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        if (current.rectangle)
                behavior = GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        target_coordinate = SelectionCoordinate(target_point, columns);
        endpoint_coordinate = SelectionCoordinate(start_point, columns);
        start_distance = target_coordinate > endpoint_coordinate
                             ? target_coordinate - endpoint_coordinate
                             : endpoint_coordinate - target_coordinate;
        endpoint_coordinate = SelectionCoordinate(end_point, columns);
        end_distance = target_coordinate > endpoint_coordinate
                           ? target_coordinate - endpoint_coordinate
                           : endpoint_coordinate - target_coordinate;
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        terminal->selection_extend_start = NULL;
        terminal->selection_extend_end = NULL;
        if (TrackSelectionPoint(terminal, start_point, &terminal->selection_extend_start) !=
                GHOSTTY_SUCCESS ||
            TrackSelectionPoint(terminal, end_point, &terminal->selection_extend_end) !=
                GHOSTTY_SUCCESS) {
                XtpTerminalSelectionExtendEnd(terminal);
                return XTP_SELECTION_ERROR;
        }
        terminal->selection_extend_left =
            start_distance < end_distance ||
            target_coordinate < SelectionCoordinate(start_point, columns);
        terminal->selection_extend_rectangle = current.rectangle;
        terminal->selection_extend_behavior = behavior;
        return XtpTerminalSelectionExtendActive(terminal, column, row, current.rectangle);
}

XtpSelectionResult
XtpTerminalSelectionExtendActive(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                 bool rectangle)
{
        GhosttyGridRef original_start = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyGridRef original_end = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection target_selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate start_point;
        GhosttyPointCoordinate end_point;
        GhosttyPointCoordinate target_point;
        uint16_t columns = 0;
        uint64_t target_coordinate;
        uint64_t start_coordinate;
        uint64_t end_coordinate;
        uint64_t unit_offset;

        if (terminal == NULL || terminal->selection_extend_start == NULL ||
            terminal->selection_extend_end == NULL ||
            ghostty_tracked_grid_ref_snapshot(terminal->selection_extend_start, &original_start) !=
                GHOSTTY_SUCCESS ||
            ghostty_tracked_grid_ref_snapshot(terminal->selection_extend_end, &original_end) !=
                GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &original_start,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &start_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &original_end,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &end_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            SelectionForBehavior(terminal, target, terminal->selection_extend_behavior,
                                 &target_selection) != GHOSTTY_SUCCESS)
                return XTP_SELECTION_ERROR;
        start_coordinate = SelectionCoordinate(start_point, columns);
        end_coordinate = SelectionCoordinate(end_point, columns);
        target_coordinate = SelectionCoordinate(target_point, columns);
        unit_offset = terminal->selection_extend_behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL
                          ? 0U
                          : 1U;
        if (terminal->selection_extend_left && target_coordinate + unit_offset > end_coordinate)
                terminal->selection_extend_left = false;
        else if (!terminal->selection_extend_left && target_coordinate < start_coordinate)
                terminal->selection_extend_left = true;

        if (terminal->selection_extend_left) {
                selection.start = original_end;
                selection.end = target_selection.start;
        } else {
                selection.start = original_start;
                selection.end = target_selection.end;
        }
        selection.rectangle = terminal->selection_extend_rectangle && rectangle;
        return ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, &selection) ==
                       GHOSTTY_SUCCESS
                   ? XTP_SELECTION_CHANGED
                   : XTP_SELECTION_ERROR;
}

void
XtpTerminalSelectionExtendEnd(XtpTerminal *terminal)
{
        if (terminal == NULL)
                return;
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        terminal->selection_extend_start = NULL;
        terminal->selection_extend_end = NULL;
}

void
XtpTerminalSelectionClear(XtpTerminal *terminal)
{
        if (terminal == NULL)
                return;
        XtpTerminalSelectionExtendEnd(terminal);
        ghostty_selection_gesture_reset(terminal->selection_gesture, terminal->handle);
        (void)ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
}

int
XtpTerminalSelectionText(XtpTerminal *terminal, uint8_t **bytes, size_t *length)
{
        GhosttyTerminalSelectionFormatOptions options =
            GHOSTTY_INIT_SIZED(GhosttyTerminalSelectionFormatOptions);
        uint8_t *ghostty_bytes = NULL;
        uint8_t *copy;
        size_t ghostty_length = 0;

        if (terminal == NULL || bytes == NULL || length == NULL)
                return -1;
        *bytes = NULL;
        *length = 0;
        options.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
        options.unwrap = true;
        options.trim = true;
        if (ghostty_terminal_selection_format_alloc(terminal->handle, NULL, options, &ghostty_bytes,
                                                    &ghostty_length) != GHOSTTY_SUCCESS)
                return -1;
        copy = malloc(ghostty_length != 0 ? ghostty_length : 1U);
        if (copy == NULL) {
                ghostty_free(NULL, ghostty_bytes, ghostty_length);
                return -1;
        }
        if (ghostty_length != 0)
                memcpy(copy, ghostty_bytes, ghostty_length);
        ghostty_free(NULL, ghostty_bytes, ghostty_length);
        *bytes = copy;
        *length = ghostty_length;
        return 0;
}
