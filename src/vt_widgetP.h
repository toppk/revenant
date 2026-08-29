#ifndef XTERM_PLUS_VT_WIDGETP_H
#define XTERM_PLUS_VT_WIDGETP_H

#include "vt_widget.h"

#include "terminal.h"
#include "emoji_presentation.h"
#include "glyph_cairo.h"
#include "x11_opacity.h"

#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/CompositeP.h>
#include <X11/Xft/Xft.h>

#define XTP_FONT_SLOTS 8
#define XTP_COLOR_CACHE_SIZE 512
#define XTP_GLYPH_INK_CACHE_SIZE 256

#define XtNfont1 "font1"
#define XtNfont2 "font2"
#define XtNfont3 "font3"
#define XtNfont4 "font4"
#define XtNfont5 "font5"
#define XtNfont6 "font6"
#define XtNfont7 "font7"

#define XtCFont1 "Font1"
#define XtCFont2 "Font2"
#define XtCFont3 "Font3"
#define XtCFont4 "Font4"
#define XtCFont5 "Font5"
#define XtCFont6 "Font6"
#define XtCFont7 "Font7"

typedef struct
{
        Boolean used;
        Boolean owned;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        Pixel pixel;
        Pixel allocation_pixel;
        XftColor xft;
} ColorCacheEntry;

typedef struct
{
        XftFont *font;
        uint32_t codepoint;
        uint8_t width;
        Boolean color_glyphs;
        Boolean has_ink;
} GlyphInkCacheEntry;

#define XTP_RECENT_KEY_ACTIONS 16
#define XTP_SCROLL_RENDER_DELAY_MS 8
#define XTP_SELECTION_AUTOSCROLL_MS 15

typedef enum
{
        XTP_SELECTION_SOURCE_ATOM,
        XTP_SELECTION_SOURCE_CUT_BUFFER,
} SelectionSourceKind;

typedef struct
{
        SelectionSourceKind kind;
        Atom atom;
        int cut_buffer;
} SelectionSource;

typedef enum
{
        XTP_CURSOR_BLINK_DEFAULT_FALSE,
        XTP_CURSOR_BLINK_DEFAULT_TRUE,
        XTP_CURSOR_BLINK_ALWAYS,
        XTP_CURSOR_BLINK_NEVER,
} CursorBlinkPolicy;

typedef enum
{
        XTP_LOCAL_ACTION_FONT_LARGER,
        XTP_LOCAL_ACTION_FONT_SMALLER,
        XTP_LOCAL_ACTION_PASTE,
        XTP_LOCAL_ACTION_SCROLL_BACK,
        XTP_LOCAL_ACTION_SCROLL_FORWARD,
} LocalKeyAction;

typedef struct
{
        Boolean used;
        Boolean duplicate_logged;
        unsigned long serial;
        Time time;
        unsigned int keycode;
        unsigned int state;
        LocalKeyAction action;
} KeyActionIdentity;

typedef struct
{
        Pixel foreground;
        Pixel background;
        Pixel opaque_background;
        char text[64];
        size_t text_length;
        uint8_t width;
        Boolean bold;
        Boolean underline;
        Boolean strikethrough;
        Boolean overline;
} VisualCell;

typedef struct
{
        Pixel foreground;
        Pixel cursor_color;
        XFontStruct *initial_font;
        String font_names[XTP_FONT_SLOTS];
        String render_font_name;
        String face_name;
        String face_name_doublesize;
        String face_name_emoji;
        String emoji_presentation_name;
        XtpEmojiPolicy emoji_presentation;
        Boolean color_glyphs;
        String char_class;
        String background_opacity_name;
        String face_size_names[XTP_FONT_SLOTS];
        Dimension internal_border;
        int columns;
        int rows;
        int save_lines;
        int multi_click_time;
        String cursor_blink_name;
        CursorBlinkPolicy cursor_blink_policy;
        int cursor_on_time;
        int cursor_off_time;
        Boolean scroll_bar;
        Boolean right_scroll_bar;
        Boolean scroll_key;
        Boolean scroll_tty_output;
        Boolean select_to_clipboard;
        Boolean reverse_video;
        Dimension scroll_bar_border;
        Boolean always_highlight;
        XtCallbackList font_changed_callback;
        XtCallbackList size_changed_callback;
        XtCallbackList popup_menu_callback;
        XtCallbackList paste_callback;
        XtCallbackList input_callback;

        GC gc;
        Widget scrollbar;
        XFontStruct *fonts[XTP_FONT_SLOTS];
        Boolean owned[XTP_FONT_SLOTS];
        Boolean use_xft;
        Boolean alpha_visual;
        uint16_t background_alpha;
        XtpX11AlphaFormat alpha_format;
        Pixel opaque_background_pixel;
        XftFont *xft_fonts[XTP_FONT_SLOTS];
        XftFont *xft_bold_fonts[XTP_FONT_SLOTS];
        XftFont *xft_wide_fonts[XTP_FONT_SLOTS];
        XftFont *xft_wide_bold_fonts[XTP_FONT_SLOTS];
        XftFont *xft_emoji_fonts[XTP_FONT_SLOTS];
        XftFont *xft_emoji_bold_fonts[XTP_FONT_SLOTS];
        double xft_sizes[XTP_FONT_SLOTS];
        XftDraw *xft_draw;
        XtpCairo *cairo_draw;
        int current_font;
        XtpTerminal *terminal;
        uint8_t *selection_text;
        size_t selection_text_length;
        Time selection_time;
        Atom *owned_selections;
        Cardinal owned_selection_count;
        Boolean disowning_selections;
        Boolean selection_dragging;
        Boolean selection_extending;
        XtIntervalId selection_autoscroll_timer;
        int selection_pointer_x;
        int selection_pointer_y;
        Boolean selection_rectangle;
        uint8_t *hovered_hyperlink;
        size_t hovered_hyperlink_length;
        uint8_t *pressed_hyperlink;
        size_t pressed_hyperlink_length;
        unsigned int reported_mouse_buttons;
        Time last_button_up_time;
        unsigned int last_button;
        unsigned int number_of_clicks;
        XtpSelectionUnit select_unit;
        Boolean focused;
        Boolean render_cursor_visible;
        Boolean render_reverse_colors;
        unsigned int render_cursor_column;
        unsigned int render_cursor_row;
        Boolean cursor_cell_seen;
        char cursor_text[64];
        size_t cursor_text_length;
        uint8_t cursor_width;
        Pixel cursor_fill;
        Pixel cursor_text_color;
        Boolean cursor_bold;
        VisualCell *frame_cells;
        VisualCell *pending_cells;
        size_t frame_capacity;
        unsigned int frame_columns;
        unsigned int frame_rows;
        Boolean frame_valid;
        Boolean capture_full_frame;
        Boolean damage_clip_active;
        XRectangle damage_clip;
        Boolean last_cursor_visible;
        unsigned int last_cursor_column;
        unsigned int last_cursor_row;
        XtpCursorShape last_cursor_shape;
        Boolean cursor_protocol_visible;
        Boolean cursor_blink_requested;
        Boolean cursor_blinking;
        Boolean cursor_blink_on;
        XtIntervalId cursor_blink_timer;
        XtIntervalId viewport_update_timer;
        unsigned int viewport_updates_coalesced;
        Boolean suppress_grid_resize;
        XIM input_method;
        XIC input_context;
        Window input_window;
        uint8_t pressed_keycodes[32];
        uint8_t filtered_keycodes[32];
        Boolean detectable_autorepeat;
        KeyActionIdentity recent_key_actions[XTP_RECENT_KEY_ACTIONS];
        unsigned int next_key_action;
        ColorCacheEntry colors[XTP_COLOR_CACHE_SIZE];
        size_t color_count;
        GlyphInkCacheEntry glyph_ink_cache[XTP_GLYPH_INK_CACHE_SIZE];
        size_t next_glyph_ink_cache;
} Vt100Part;

typedef struct _Vt100Rec
{
        CorePart core;
        CompositePart composite;
        Vt100Part vt;
} Vt100Rec;

typedef struct
{
        int unused;
} Vt100ClassPart;

typedef struct _Vt100ClassRec
{
        CoreClassPart core_class;
        CompositeClassPart composite_class;
        Vt100ClassPart vt_class;
} Vt100ClassRec;

Vt100Rec *VtAsRecord(Widget widget);
unsigned int VtSlotWidth(const Vt100Rec *vt, int slot);
unsigned int VtSlotHeight(const Vt100Rec *vt, int slot);
int VtSlotAscent(const Vt100Rec *vt, int slot);
Boolean VtEffectiveCursorBlink(CursorBlinkPolicy policy, Boolean requested);
Dimension VtScrollbarTotalWidth(Vt100Rec *vt);
void VtUpdateScrollbar(Vt100Rec *vt);
Boolean VtScrollViewportBy(Vt100Rec *vt, intptr_t rows);
Boolean VtAcceptLocalKeyAction(Vt100Rec *vt, XEvent *event, LocalKeyAction action);

int VtTerminalX(Vt100Rec *vt);
void VtEraseLastCursor(Vt100Rec *vt);
void VtDrawCursor(Vt100Rec *vt, Boolean visible, unsigned int column, unsigned int row,
                  XtpCursorShape shape);
void VtStopCursorBlink(Vt100Rec *vt);
void VtScheduleCursorBlink(Vt100Rec *vt);
void VtRestartCursorBlink(Vt100Rec *vt);
int VtRenderTerminal(Vt100Rec *vt, Boolean force_full);
void VtRepaintCached(Vt100Rec *vt, const XRectangle *damage);
void VtPlaceholder(Vt100Rec *vt);
Pixel VtOpaquePixel(const Vt100Rec *vt, Pixel pixel);
uint16_t VtPixelAlpha(const Vt100Rec *vt, Pixel pixel);
void VtRedisplay(Widget widget, XEvent *event, Region region);

void VtInitializeInput(Vt100Rec *vt);
void VtDestroyInput(Vt100Rec *vt);
Boolean VtHyperlinkUriEqualsCell(Vt100Rec *vt, const XtpRenderCell *cell);
void VtHyperlinkEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch);
void VtScrollBackAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtScrollForwardAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtSelectStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtSelectExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtSelectEndAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtStartExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtInsertSelectionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtMousePressAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtMouseMotionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
void VtHyperlinkStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);

#endif
