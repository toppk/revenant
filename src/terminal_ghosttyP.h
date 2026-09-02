#ifndef XTERM_PLUS_TERMINAL_GHOSTTYP_H
#define XTERM_PLUS_TERMINAL_GHOSTTYP_H

#include "char_class.h"
#include "cursor_blink.h"
#include "terminal.h"

#include <ghostty/vt.h>

struct XtpTerminal
{
        GhosttyTerminal handle;
        GhosttyRenderState render_state;
        GhosttyRenderStateRowIterator rows;
        GhosttyRenderStateRowCells cells;
        GhosttyKeyEncoder key_encoder;
        GhosttyKeyEvent key_event;
        GhosttyMouseEncoder mouse_encoder;
        GhosttyMouseEvent mouse_event;
        GhosttySelectionGesture selection_gesture;
        GhosttySelectionGestureEvent selection_press;
        GhosttySelectionGestureEvent selection_drag;
        GhosttySelectionGestureEvent selection_autoscroll;
        GhosttySelectionGestureEvent selection_release;
        GhosttyTrackedGridRef selection_extend_start;
        GhosttyTrackedGridRef selection_extend_end;
        GhosttySelectionGestureBehavior selection_extend_behavior;
        bool selection_extend_left;
        bool selection_extend_rectangle;
        bool reverse_colors_initialized;
        bool reverse_colors;
        XtpCursorBlinkObserver cursor_blink;
        XtpCharClassTable *char_classes;
        XtpTerminalEffects effects;
};

#endif
