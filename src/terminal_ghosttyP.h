#ifndef XTERM_PLUS_TERMINAL_GHOSTTYP_H
#define XTERM_PLUS_TERMINAL_GHOSTTYP_H

#include "char_class.h"
#include "cursor_blink.h"
#include "terminal.h"

#include <ghostty/vt.h>

/* One OSC 4/5 query occurrence: replies for an index are consumed in order. */
#define XTP_PALETTE_QUERY_LIMIT 512U
typedef struct
{
        unsigned int index;
        bool denied;
        bool consumed;
} XtpPaletteQuery;

struct XtpTerminal
{
        GhosttyTerminal handle;
        uint16_t geometry_columns;
        uint16_t geometry_rows;
        uint32_t geometry_cell_width;
        uint32_t geometry_cell_height;
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
        bool bold_colors;
        bool allow_color_ops;
        XtpColorOps color_ops;
        bool color_list_active;
        bool color_list_skipping;
        bool color_list_any_denied;
        bool color_list_deny_unknown_index;
        unsigned int color_list_selector;
        unsigned int color_list_items;
        uint32_t color_list_drop_selectors;
        XtpPaletteQuery color_list_queries[XTP_PALETTE_QUERY_LIMIT];
        unsigned int color_list_query_count;
        bool color_list_queries_overflow;
        bool color_list_capturing;
        size_t color_list_capture_start;
        unsigned int color_list_index_value;
        bool color_list_index_digits;
        bool color_list_index_invalid;
        bool colors_initialized;
        GhosttyColorRgb last_foreground;
        GhosttyColorRgb last_background;
        GhosttyColorRgb last_cursor;
        bool scheme_initialized;
        bool scheme_light;
        XtpCursorBlinkObserver cursor_blink;
        XtpCharClassTable *char_classes;
        XtpTerminalEffects effects;
};

#endif
