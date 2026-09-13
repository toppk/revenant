#ifndef XTERM_PLUS_TERMINAL_GHOSTTYP_H
#define XTERM_PLUS_TERMINAL_GHOSTTYP_H

#include "char_class.h"
#include "cursor_blink.h"
#include "terminal.h"

/* osc.zig Parser.MAX_BUF: the fixed capture used for OSC 7 and most other OSCs. */
#define XTP_GHOSTTY_OSC_CAPTURE_LIMIT 2048U

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
        bool allow_mouse_ops;
        bool allow_tcap_ops;
        XtpTcapOps tcap_ops;
        XtpColorOps color_ops;
        /* OSC 7 in flight through the observer; counts payload bytes so a
         * report the core drops can still be reported to the application. */
        bool pwd_report_active;
        size_t pwd_report_start;
        size_t pwd_report_bytes;
        unsigned int pwd_reports_delivered;
        unsigned int pwd_reports_before_report;
        /* OSC 133 prompt marks as tracked references, one per row, in screen-row order; the core's
         * row retention bounds the count and each dies with its row. */
        GhosttyTrackedGridRef *prompt_marks;
        size_t prompt_mark_count;
        size_t prompt_mark_capacity;
        bool prompt_mark_first_item;
        bool prompt_mark_pending;
        /* Prompt of the command whose OSC 133 D arrived last; NULL until a D is seen. */
        GhosttyTrackedGridRef completed_prompt;
        bool command_end_seen;
        /* An OSC 133 in flight whose first item may be D; offsets are feed-relative and the byte
         * counts carry across feeds like the OSC 7 accounting. */
        bool command_end_active;
        bool command_end_first_item;
        bool command_end_first_done;
        size_t command_end_start;
        size_t command_end_bytes;
        size_t command_end_item_start;
        size_t command_end_item_bytes;
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
        char *answerback;
        size_t answerback_length;
};

/* Primary-screen rows including scrollback; 0 on the alternate screen. */
uint64_t XtpGhosttyScreenRows(XtpTerminal *terminal);

#endif
