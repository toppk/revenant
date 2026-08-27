#include "vt_widget.h"

#include "diagnostics.h"
#include "terminal.h"

#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/CompositeP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xmu/Converters.h>
#include <X11/keysym.h>

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define XTP_FONT_SLOTS 8
#define XTP_COLOR_CACHE_SIZE 512

#ifndef XtNfont1
#define XtNfont1 "font1"
#define XtNfont2 "font2"
#define XtNfont3 "font3"
#define XtNfont4 "font4"
#define XtNfont5 "font5"
#define XtNfont6 "font6"
#define XtNfont7 "font7"
#endif

#ifndef XtCFont1
#define XtCFont1 "Font1"
#define XtCFont2 "Font2"
#define XtCFont3 "Font3"
#define XtCFont4 "Font4"
#define XtCFont5 "Font5"
#define XtCFont6 "Font6"
#define XtCFont7 "Font7"
#endif

typedef struct
{
        Boolean used;
        Boolean owned;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        Pixel pixel;
        XftColor xft;
} ColorCacheEntry;

#define XTP_RECENT_KEY_ACTIONS 16
#define XTP_SCROLL_RENDER_DELAY_MS 8
#define XTP_SELECTION_AUTOSCROLL_MS 15

typedef enum
{
        XTP_SELECTION_SOURCE_ATOM,
        XTP_SELECTION_SOURCE_CUT_BUFFER,
} XtpSelectionSourceKind;

typedef struct
{
        XtpSelectionSourceKind kind;
        Atom atom;
        int cut_buffer;
} XtpSelectionSource;

typedef enum
{
        XTP_CURSOR_BLINK_DEFAULT_FALSE,
        XTP_CURSOR_BLINK_DEFAULT_TRUE,
        XTP_CURSOR_BLINK_ALWAYS,
        XTP_CURSOR_BLINK_NEVER,
} XtpCursorBlinkPolicy;

typedef enum
{
        XTP_KEY_ACTION_FONT_LARGER,
        XTP_KEY_ACTION_FONT_SMALLER,
        XTP_KEY_ACTION_PASTE,
        XTP_KEY_ACTION_SCROLL_BACK,
        XTP_KEY_ACTION_SCROLL_FORWARD,
} XtpLocalKeyAction;

typedef struct
{
        Boolean used;
        Boolean duplicate_logged;
        unsigned long serial;
        Time time;
        unsigned int keycode;
        unsigned int state;
        XtpLocalKeyAction action;
} KeyActionIdentity;

typedef struct
{
        Pixel foreground;
        Pixel background;
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
        String char_class;
        String face_size_names[XTP_FONT_SLOTS];
        Dimension internal_border;
        int columns;
        int rows;
        int save_lines;
        int multi_click_time;
        String cursor_blink_name;
        XtpCursorBlinkPolicy cursor_blink_policy;
        int cursor_on_time;
        int cursor_off_time;
        Boolean scroll_bar;
        Boolean right_scroll_bar;
        Boolean scroll_key;
        Boolean scroll_tty_output;
        Boolean select_to_clipboard;
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
        XftFont *xft_fonts[XTP_FONT_SLOTS];
        XftFont *xft_bold_fonts[XTP_FONT_SLOTS];
        double xft_sizes[XTP_FONT_SLOTS];
        XftDraw *xft_draw;
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
        unsigned int render_cursor_column;
        unsigned int render_cursor_row;
        Boolean cursor_cell_seen;
        char cursor_text[64];
        size_t cursor_text_length;
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
        KeyActionIdentity recent_key_actions[XTP_RECENT_KEY_ACTIONS];
        unsigned int next_key_action;
        ColorCacheEntry colors[XTP_COLOR_CACHE_SIZE];
        size_t color_count;
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

static void Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args);
static void Destroy(Widget widget);
static void ResizeWidget(Widget widget);
static void Redisplay(Widget widget, XEvent *event, Region region);
static Boolean SetValues(Widget current, Widget request, Widget new_widget, ArgList args,
                         Cardinal *num_args);
static void LargerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SmallerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SetRenderFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SetSelectAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void PopupMenuAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void ScrollBackAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void ScrollForwardAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SelectStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SelectExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SelectEndAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void StartExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void InsertSelectionAction(Widget widget, XEvent *event, String *params,
                                  Cardinal *num_params);
static void MousePressAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void MouseMotionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void HyperlinkStartAction(Widget widget, XEvent *event, String *params,
                                 Cardinal *num_params);
static void HyperlinkEvent(Widget widget, XtPointer closure, XEvent *event,
                           Boolean *continue_dispatch);
static Boolean HyperlinkUriEqualsCell(Vt100Rec *vt, const XtpRenderCell *cell);
static void ClassInitialize(void);

static XtActionsRec actions[] = {
    {"larger-vt-font", LargerFontAction},      {"smaller-vt-font", SmallerFontAction},
    {"set-render-font", SetRenderFontAction},  {"set-select", SetSelectAction},
    {"popup-menu", PopupMenuAction},           {"scroll-back", ScrollBackAction},
    {"scroll-forw", ScrollForwardAction},      {"select-start", SelectStartAction},
    {"select-extend", SelectExtendAction},     {"select-end", SelectEndAction},
    {"start-extend", StartExtendAction},       {"insert-selection", InsertSelectionAction},
    {"mouse-press", MousePressAction},         {"mouse-motion", MouseMotionAction},
    {"hyperlink-start", HyperlinkStartAction},
};

/*
 * Implemented subset of patch-410 VTInitTranslations(). Keep this table and
 * docs/compatibility/default-bindings.md in sync with the tracked xterm source.
 * Do not bind an unsupported action: Xt would accept the gesture but fail it at
 * runtime, hiding a real compatibility gap.
 */
static char translations[] = "Shift~Ctrl <KeyPress> KP_Add: larger-vt-font()\n"
                             "Shift Ctrl <KeyPress> KP_Add: smaller-vt-font()\n"
                             "Shift <KeyPress> KP_Subtract: smaller-vt-font()\n"
                             "Shift <KeyPress> Insert: insert-selection(SELECT, CUT_BUFFER0)\n"
                             "Shift <KeyPress> Prior: scroll-back(1,halfpage)\n"
                             "Shift <KeyPress> Next: scroll-forw(1,halfpage)\n"
                             "!Ctrl <Btn1Down>: popup-menu(mainMenu)\n"
                             "!Lock Ctrl <Btn1Down>: popup-menu(mainMenu)\n"
                             "!Lock Ctrl @Num_Lock <Btn1Down>: popup-menu(mainMenu)\n"
                             "! @Num_Lock Ctrl <Btn1Down>: popup-menu(mainMenu)\n"
                             "!Ctrl <Btn2Down>: popup-menu(vtMenu)\n"
                             "!Lock Ctrl <Btn2Down>: popup-menu(vtMenu)\n"
                             "!Lock Ctrl @Num_Lock <Btn2Down>: popup-menu(vtMenu)\n"
                             "! @Num_Lock Ctrl <Btn2Down>: popup-menu(vtMenu)\n"
                             "!Ctrl <Btn3Down>: popup-menu(fontMenu)\n"
                             "!Lock Ctrl <Btn3Down>: popup-menu(fontMenu)\n"
                             "!Lock Ctrl @Num_Lock <Btn3Down>: popup-menu(fontMenu)\n"
                             "! @Num_Lock Ctrl <Btn3Down>: popup-menu(fontMenu)\n"
                             "Shift ~Ctrl ~Meta <Btn1Down>: hyperlink-start()\n"
                             "~Meta <Btn1Down>: select-start()\n"
                             "Meta <Btn1Down>: select-start(block)\n"
                             "~Ctrl ~Meta <Btn2Down>: mouse-press()\n"
                             "~Ctrl ~Meta <Btn2Up>: insert-selection(SELECT, CUT_BUFFER0)\n"
                             "~Ctrl ~Meta <Btn3Down>: start-extend()\n"
                             "<BtnUp>: select-end(SELECT, CUT_BUFFER0)\n"
                             "Ctrl <Btn4Down>: scroll-back(1,halfpage,m)\n"
                             "Lock Ctrl <Btn4Down>: scroll-back(1,halfpage,m)\n"
                             "Lock @Num_Lock Ctrl <Btn4Down>: scroll-back(1,halfpage,m)\n"
                             "@Num_Lock Ctrl <Btn4Down>: scroll-back(1,halfpage,m)\n"
                             "<Btn4Down>: scroll-back(5,line,m)\n"
                             "Ctrl <Btn5Down>: scroll-forw(1,halfpage,m)\n"
                             "Lock Ctrl <Btn5Down>: scroll-forw(1,halfpage,m)\n"
                             "Lock @Num_Lock Ctrl <Btn5Down>: scroll-forw(1,halfpage,m)\n"
                             "@Num_Lock Ctrl <Btn5Down>: scroll-forw(1,halfpage,m)\n"
                             "<Btn5Down>: scroll-forw(5,line,m)\n"
                             "<BtnMotion>: mouse-motion() select-extend()\n"
                             "<BtnDown>: mouse-press()\n";

#define OFFSET(field) XtOffsetOf(Vt100Rec, vt.field)

static XtResource resources[] = {
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel), OFFSET(foreground), XtRString,
     XtDefaultForeground},
    {"cursorColor", "CursorColor", XtRPixel, sizeof(Pixel), OFFSET(cursor_color), XtRString,
     XtDefaultForeground},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *), OFFSET(initial_font), XtRString,
     (XtPointer) "fixed"},
    {XtNfont1, XtCFont1, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[1]),
     XtRString, (XtPointer) "nil2"},
    {XtNfont2, XtCFont2, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[2]),
     XtRString, (XtPointer) "5x7"},
    {XtNfont3, XtCFont3, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[3]),
     XtRString, (XtPointer) "6x10"},
    {XtNfont4, XtCFont4, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[4]),
     XtRString, (XtPointer) "7x13"},
    {XtNfont5, XtCFont5, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[5]),
     XtRString, (XtPointer) "9x15"},
    {XtNfont6, XtCFont6, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[6]),
     XtRString, (XtPointer) "10x20"},
    {XtNfont7, XtCFont7, XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.font_names[7]),
     XtRString, (XtPointer) "12x24"},
    {"renderFont", "RenderFont", XtRString, sizeof(String), OFFSET(render_font_name), XtRString,
     (XtPointer) "default"},
    {"faceName", "FaceName", XtRString, sizeof(String), OFFSET(face_name), XtRString, NULL},
    {"faceNameDoublesize", "FaceNameDoublesize", XtRString, sizeof(String),
     OFFSET(face_name_doublesize), XtRString, NULL},
    {"faceSize", "FaceSize", XtRString, sizeof(String), XtOffsetOf(Vt100Rec, vt.face_size_names[0]),
     XtRString, (XtPointer) "8.0"},
    {"faceSize1", "FaceSize1", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[1]), XtRString, (XtPointer) "0.0"},
    {"faceSize2", "FaceSize2", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[2]), XtRString, (XtPointer) "0.0"},
    {"faceSize3", "FaceSize3", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[3]), XtRString, (XtPointer) "0.0"},
    {"faceSize4", "FaceSize4", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[4]), XtRString, (XtPointer) "0.0"},
    {"faceSize5", "FaceSize5", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[5]), XtRString, (XtPointer) "0.0"},
    {"faceSize6", "FaceSize6", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[6]), XtRString, (XtPointer) "0.0"},
    {"faceSize7", "FaceSize7", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.face_size_names[7]), XtRString, (XtPointer) "0.0"},
    {"internalBorder", XtCBorderWidth, XtRDimension, sizeof(Dimension), OFFSET(internal_border),
     XtRImmediate, (XtPointer)2},
    {"columns", "Columns", XtRInt, sizeof(int), OFFSET(columns), XtRImmediate, (XtPointer)80},
    {"rows", "Rows", XtRInt, sizeof(int), OFFSET(rows), XtRImmediate, (XtPointer)24},
    {"saveLines", "SaveLines", XtRInt, sizeof(int), OFFSET(save_lines), XtRImmediate,
     (XtPointer)1024},
    {"charClass", "CharClass", XtRString, sizeof(String), OFFSET(char_class), XtRImmediate, NULL},
    {"multiClickTime", "MultiClickTime", XtRInt, sizeof(int), OFFSET(multi_click_time),
     XtRImmediate, (XtPointer)250},
    {"cursorBlink", "CursorBlink", XtRString, sizeof(String), OFFSET(cursor_blink_name), XtRString,
     (XtPointer) "false"},
    {"cursorOnTime", "CursorOnTime", XtRInt, sizeof(int), OFFSET(cursor_on_time), XtRImmediate,
     (XtPointer)600},
    {"cursorOffTime", "CursorOffTime", XtRInt, sizeof(int), OFFSET(cursor_off_time), XtRImmediate,
     (XtPointer)300},
    {"scrollBar", "ScrollBar", XtRBoolean, sizeof(Boolean), OFFSET(scroll_bar), XtRImmediate,
     (XtPointer)False},
    {"rightScrollBar", "RightScrollBar", XtRBoolean, sizeof(Boolean), OFFSET(right_scroll_bar),
     XtRImmediate, (XtPointer)False},
    {"scrollKey", "ScrollCond", XtRBoolean, sizeof(Boolean), OFFSET(scroll_key), XtRImmediate,
     (XtPointer)False},
    {"scrollTtyOutput", "ScrollCond", XtRBoolean, sizeof(Boolean), OFFSET(scroll_tty_output),
     XtRImmediate, (XtPointer)True},
    {"selectToClipboard", "SelectToClipboard", XtRBoolean, sizeof(Boolean),
     OFFSET(select_to_clipboard), XtRImmediate, (XtPointer)False},
    {"scrollBarBorder", "ScrollBarBorder", XtRDimension, sizeof(Dimension),
     OFFSET(scroll_bar_border), XtRImmediate, (XtPointer)1},
    {"alwaysHighlight", "AlwaysHighlight", XtRBoolean, sizeof(Boolean), OFFSET(always_highlight),
     XtRImmediate, (XtPointer)False},
    {XtNfontChangedCallback, XtCFontChangedCallback, XtRCallback, sizeof(XtCallbackList),
     OFFSET(font_changed_callback), XtRCallback, NULL},
    {XtNsizeChangedCallback, XtCSizeChangedCallback, XtRCallback, sizeof(XtCallbackList),
     OFFSET(size_changed_callback), XtRCallback, NULL},
    {XtNpopupMenuCallback, XtCPopupMenuCallback, XtRCallback, sizeof(XtCallbackList),
     OFFSET(popup_menu_callback), XtRCallback, NULL},
    {XtNpasteCallback, XtCPasteCallback, XtRCallback, sizeof(XtCallbackList),
     OFFSET(paste_callback), XtRCallback, NULL},
    {XtNinputCallback, XtCInputCallback, XtRCallback, sizeof(XtCallbackList),
     OFFSET(input_callback), XtRCallback, NULL},
};

#undef OFFSET

Vt100ClassRec vt100ClassRec = {
    {
        (WidgetClass)&compositeClassRec,
        "VT100",
        sizeof(Vt100Rec),
        ClassInitialize,
        NULL,
        False,
        Initialize,
        NULL,
        XtInheritRealize,
        actions,
        XtNumber(actions),
        resources,
        XtNumber(resources),
        NULLQUARK,
        True,
        XtExposeCompressMultiple,
        True,
        False,
        Destroy,
        ResizeWidget,
        Redisplay,
        SetValues,
        NULL,
        XtInheritSetValuesAlmost,
        NULL,
        NULL,
        XtVersion,
        NULL,
        translations,
        XtInheritQueryGeometry,
        XtInheritDisplayAccelerator,
        NULL,
    },
    {
        XtInheritGeometryManager,
        XtInheritChangeManaged,
        XtInheritInsertChild,
        XtInheritDeleteChild,
        NULL,
    },
    {0},
};

WidgetClass vt100WidgetClass = (WidgetClass)&vt100ClassRec;

static void
ClassInitialize(void)
{
        XtRegisterGrabAction(PopupMenuAction, True, ButtonPressMask | ButtonReleaseMask,
                             GrabModeAsync, GrabModeAsync);
}

static Vt100Rec *
AsVt(Widget widget)
{
        return (Vt100Rec *)widget;
}

static unsigned int
FontWidth(const XFontStruct *font)
{
        int width = font->max_bounds.width;

        return width > 0 ? (unsigned int)width : 1U;
}

static unsigned int
FontHeight(const XFontStruct *font)
{
        int height = font->ascent + font->descent;

        return height > 0 ? (unsigned int)height : 1U;
}

static unsigned int
XftFontWidth(const XftFont *font)
{
        return font != NULL && font->max_advance_width > 0 ? (unsigned int)font->max_advance_width
                                                           : 1U;
}

static unsigned int
XftFontHeight(const XftFont *font)
{
        int height = font != NULL ? font->ascent + font->descent : 0;

        return height > 0 ? (unsigned int)height : 1U;
}

static unsigned int
SlotWidth(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft)
                return XftFontWidth(vt->vt.xft_fonts[slot]);
        return FontWidth(vt->vt.fonts[slot]);
}

static unsigned int
SlotHeight(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft)
                return XftFontHeight(vt->vt.xft_fonts[slot]);
        return FontHeight(vt->vt.fonts[slot]);
}

static int
SlotAscent(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft)
                return vt->vt.xft_fonts[slot]->ascent;
        return vt->vt.fonts[slot]->ascent;
}

static Boolean
Nonempty(const char *value)
{
        return value != NULL && *value != '\0';
}

static Boolean
ResourceBoolean(const char *value, Boolean default_value)
{
        if (!Nonempty(value) || strcasecmp(value, "default") == 0)
                return default_value;
        if (strcasecmp(value, "true") == 0 || strcasecmp(value, "on") == 0 ||
            strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0)
                return True;
        if (strcasecmp(value, "false") == 0 || strcasecmp(value, "off") == 0 ||
            strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0 ||
            strcasecmp(value, "defaultOff") == 0)
                return False;
        return default_value;
}

static XtpCursorBlinkPolicy
ParseCursorBlinkPolicy(const char *value)
{
        if (value != NULL && strcasecmp(value, "true") == 0)
                return XTP_CURSOR_BLINK_DEFAULT_TRUE;
        if (value != NULL && strcasecmp(value, "always") == 0)
                return XTP_CURSOR_BLINK_ALWAYS;
        if (value != NULL && strcasecmp(value, "never") == 0)
                return XTP_CURSOR_BLINK_NEVER;
        if (value != NULL && strcasecmp(value, "false") != 0)
                XtpLog(
                    XTP_LOG_ERROR, "config",
                    "invalid cursorBlink=%s; using false (expected false, true, always, or never)",
                    value);
        return XTP_CURSOR_BLINK_DEFAULT_FALSE;
}

static Boolean
CursorBlinkDefault(XtpCursorBlinkPolicy policy)
{
        return policy == XTP_CURSOR_BLINK_DEFAULT_TRUE || policy == XTP_CURSOR_BLINK_ALWAYS;
}

static Boolean
EffectiveCursorBlink(XtpCursorBlinkPolicy policy, Boolean requested)
{
        if (policy == XTP_CURSOR_BLINK_ALWAYS)
                return True;
        if (policy == XTP_CURSOR_BLINK_NEVER)
                return False;
        return requested;
}

static double
PositiveNumber(const char *value, double fallback)
{
        char *end = NULL;
        double number;

        if (!Nonempty(value))
                return fallback;
        number = strtod(value, &end);
        return end != value && number > 0.0 ? number : fallback;
}

static char *
PrimaryFaceName(const char *configured)
{
        char *copy;
        char *item;
        char *state = NULL;

        if (!Nonempty(configured))
                return NULL;
        copy = strdup(configured);
        if (copy == NULL)
                return NULL;
        for (item = strtok_r(copy, ",", &state); item != NULL; item = strtok_r(NULL, ",", &state)) {
                char *end;
                char *result;

                while (isspace((unsigned char)*item))
                        ++item;
                end = item + strlen(item);
                while (end > item && isspace((unsigned char)end[-1]))
                        *--end = '\0';
                if (strncmp(item, "x:", 2) == 0 || strncmp(item, "x11:", 4) == 0)
                        continue;
                if (strncmp(item, "xft:", 4) == 0)
                        item += 4;
                result = strdup(item);
                free(copy);
                return result;
        }
        free(copy);
        return NULL;
}

static Dimension
ScrollbarTotalWidth(Vt100Rec *vt)
{
        Dimension width;
        Dimension border;

        if (vt->vt.scrollbar == NULL)
                return 0;
        XtVaGetValues(vt->vt.scrollbar, XtNwidth, &width, XtNborderWidth, &border, NULL);
        return width + border;
}

static void
LayoutScrollbar(Vt100Rec *vt)
{
        Dimension width;
        Dimension border;
        Position x;

        if (vt->vt.scrollbar == NULL || !vt->vt.scroll_bar)
                return;

        XtVaGetValues(vt->vt.scrollbar, XtNwidth, &width, XtNborderWidth, &border, NULL);
        x = vt->vt.right_scroll_bar ? (Position)(vt->core.width - width - border)
                                    : -(Position)border;
        XtConfigureWidget(vt->vt.scrollbar, x, -(Position)border, width, vt->core.height, border);
}

static void
UpdateScrollbar(Vt100Rec *vt)
{
        XtpTerminalScrollbar state;
        float top;
        float shown;

        if (vt->vt.scrollbar == NULL || vt->vt.terminal == NULL ||
            XtpTerminalGetScrollbar(vt->vt.terminal, &state) != 0)
                return;
        if (state.total == 0) {
                top = 0.0F;
                shown = 1.0F;
        } else {
                top = (float)((double)state.offset / (double)state.total);
                shown = (float)((double)state.length / (double)state.total);
                if (top < 0.0F)
                        top = 0.0F;
                if (top > 1.0F)
                        top = 1.0F;
                if (shown < 0.0F)
                        shown = 0.0F;
                if (shown > 1.0F)
                        shown = 1.0F;
        }
        XawScrollbarSetThumb(vt->vt.scrollbar, top, shown);
}

static Boolean
ViewportStateEqual(const XtpTerminalScrollbar *left, const XtpTerminalScrollbar *right)
{
        return left->offset == right->offset && left->length == right->length &&
               left->total == right->total;
}

static void
FlushViewportUpdate(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;
        unsigned int updates = vt->vt.viewport_updates_coalesced;

        (void)timer;
        vt->vt.viewport_update_timer = (XtIntervalId)0;
        vt->vt.viewport_updates_coalesced = 0;
        XtpLog(XTP_LOG_DEBUG, "scrollback", "rendering coalesced viewport updates=%u", updates);
        XtpVtUpdate((Widget)vt);
}

static void
ScheduleViewportUpdate(Vt100Rec *vt)
{
        ++vt->vt.viewport_updates_coalesced;
        if (vt->vt.viewport_update_timer == (XtIntervalId)0)
                vt->vt.viewport_update_timer =
                    XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt),
                                    XTP_SCROLL_RENDER_DELAY_MS, FlushViewportUpdate, vt);
}

static Boolean
ScrollViewportBy(Vt100Rec *vt, intptr_t rows)
{
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar state;
        Boolean had_before;

        if (vt->vt.terminal == NULL || rows == 0)
                return False;
        had_before = XtpTerminalGetScrollbar(vt->vt.terminal, &before) == 0;
        if (XtpTerminalScrollBy(vt->vt.terminal, rows) != 0)
                return False;
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &state) == 0) {
                XtpLog(XTP_LOG_DEBUG, "scrollback",
                       "viewport delta=%ld offset=%llu length=%llu total=%llu", (long)rows,
                       (unsigned long long)state.offset, (unsigned long long)state.length,
                       (unsigned long long)state.total);
                if (had_before && ViewportStateEqual(&state, &before)) {
                        XtpLog(XTP_LOG_DEBUG, "scrollback",
                               "viewport unchanged; render suppressed");
                        return False;
                }
        }
        ScheduleViewportUpdate(vt);
        return True;
}

static Boolean
ScrollViewportTo(Vt100Rec *vt, uint64_t row)
{
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar state;
        Boolean had_before;

        if (vt->vt.terminal == NULL)
                return False;
        had_before = XtpTerminalGetScrollbar(vt->vt.terminal, &before) == 0;
        if (XtpTerminalScrollTo(vt->vt.terminal, row) != 0)
                return False;
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &state) == 0) {
                XtpLog(XTP_LOG_DEBUG, "scrollback",
                       "viewport row=%llu offset=%llu length=%llu total=%llu",
                       (unsigned long long)row, (unsigned long long)state.offset,
                       (unsigned long long)state.length, (unsigned long long)state.total);
                if (had_before && ViewportStateEqual(&state, &before)) {
                        XtpLog(XTP_LOG_DEBUG, "scrollback",
                               "viewport unchanged; render suppressed");
                        return False;
                }
        }
        ScheduleViewportUpdate(vt);
        return True;
}

static Boolean
ScrollViewportToBottom(Vt100Rec *vt)
{
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar state;
        Boolean had_before;

        if (vt->vt.terminal == NULL)
                return False;
        had_before = XtpTerminalGetScrollbar(vt->vt.terminal, &before) == 0;
        if (XtpTerminalScrollToBottom(vt->vt.terminal) != 0)
                return False;
        if (XtpTerminalGetScrollbar(vt->vt.terminal, &state) == 0) {
                XtpLog(XTP_LOG_DEBUG, "scrollback",
                       "viewport bottom offset=%llu length=%llu total=%llu",
                       (unsigned long long)state.offset, (unsigned long long)state.length,
                       (unsigned long long)state.total);
                if (had_before && ViewportStateEqual(&state, &before))
                        return False;
        }
        return True;
}

static void
ScrollbarScroll(Widget scrollbar, XtPointer closure, XtPointer call_data)
{
        Vt100Rec *vt = closure;
        intptr_t pixels = (intptr_t)call_data;
        intptr_t height = (intptr_t)XtpVtCellHeight((Widget)vt);
        intptr_t rows = height != 0 ? pixels / height : 0;

        (void)scrollbar;
        if (rows == 0 && pixels != 0)
                rows = pixels < 0 ? -1 : 1;
        (void)ScrollViewportBy(vt, rows);
}

static void
ScrollbarJump(Widget scrollbar, XtPointer closure, XtPointer call_data)
{
        Vt100Rec *vt = closure;
        const float *fraction = call_data;
        XtpTerminalScrollbar state;
        double position;
        uint64_t limit;
        uint64_t target;

        (void)scrollbar;
        if (fraction == NULL || vt->vt.terminal == NULL ||
            XtpTerminalGetScrollbar(vt->vt.terminal, &state) != 0)
                return;
        position = *fraction;
        if (position < 0.0)
                position = 0.0;
        if (position > 1.0)
                position = 1.0;
        limit = state.total > state.length ? state.total - state.length : 0;
        target = (uint64_t)(position * (double)state.total);
        (void)ScrollViewportTo(vt, target > limit ? limit : target);
}

static void
EnsureScrollbar(Vt100Rec *vt)
{
        Arg args[3];

        if (vt->vt.scrollbar != NULL)
                return;
        XtSetArg(args[0], XtNorientation, XtorientVertical);
        XtSetArg(args[1], XtNborderWidth, vt->vt.scroll_bar_border);
        XtSetArg(args[2], XtNheight, vt->core.height);
        vt->vt.scrollbar =
            XtCreateWidget("scrollbar", scrollbarWidgetClass, (Widget)vt, args, XtNumber(args));
        XtAddCallback(vt->vt.scrollbar, XtNscrollProc, ScrollbarScroll, vt);
        XtAddCallback(vt->vt.scrollbar, XtNjumpProc, ScrollbarJump, vt);
        UpdateScrollbar(vt);
}

static void
ReleaseGc(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        if (vt->vt.gc != NULL) {
                XFreeGC(XtDisplay(widget), vt->vt.gc);
                vt->vt.gc = NULL;
        }
}

static void
CreateGc(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);
        XFontStruct *font = vt->vt.fonts[vt->vt.current_font];
        XGCValues values;
        XtGCMask mask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;

        if (font == NULL)
                font = vt->vt.initial_font;
        values.foreground = vt->vt.foreground;
        values.background = vt->core.background_pixel;
        values.font = font->fid;
        values.graphics_exposures = False;
        vt->vt.gc =
            XCreateGC(XtDisplay(widget), RootWindowOfScreen(XtScreen(widget)), mask, &values);
}

static XFontStruct *
LoadSlot(Vt100Rec *vt, int slot)
{
        if (slot < 0 || slot >= XTP_FONT_SLOTS)
                return NULL;
        if (vt->vt.fonts[slot] != NULL)
                return vt->vt.fonts[slot];
        if (vt->vt.font_names[slot] == NULL)
                return NULL;

        vt->vt.fonts[slot] = XLoadQueryFont(XtDisplay((Widget)vt), vt->vt.font_names[slot]);
        if (vt->vt.fonts[slot] != NULL) {
                vt->vt.owned[slot] = True;
                XtpLog(XTP_LOG_DEBUG, "font", "loaded slot=%d request=%s cell=%ux%u", slot,
                       vt->vt.font_names[slot], FontWidth(vt->vt.fonts[slot]),
                       FontHeight(vt->vt.fonts[slot]));
        } else {
                XtpLog(XTP_LOG_WARNING, "font", "failed slot=%d request=%s", slot,
                       vt->vt.font_names[slot]);
        }
        return vt->vt.fonts[slot];
}

static XftFont *
OpenXftFont(Vt100Rec *vt, const char *face, double size, Boolean bold)
{
        FcPattern *pattern;
        FcPattern *match;
        FcResult result;
        XftFont *font;

        pattern = FcNameParse((const FcChar8 *)face);
        if (pattern == NULL)
                return NULL;
        FcPatternDel(pattern, FC_SIZE);
        FcPatternDel(pattern, FC_PIXEL_SIZE);
        FcPatternAddDouble(pattern, FC_SIZE, size);
        if (bold) {
                FcPatternDel(pattern, FC_WEIGHT);
                FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
        }
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(XtDisplay((Widget)vt), XScreenNumberOfScreen(XtScreen((Widget)vt)),
                             pattern);
        match = FcFontMatch(NULL, pattern, &result);
        FcPatternDestroy(pattern);
        font = match != NULL ? XftFontOpenPattern(XtDisplay((Widget)vt), match) : NULL;
        return font;
}

static XftFont *
LoadXftSlot(Vt100Rec *vt, int slot, const char *face, double size)
{
        if (slot < 0 || slot >= XTP_FONT_SLOTS)
                return NULL;
        if (vt->vt.xft_fonts[slot] != NULL)
                return vt->vt.xft_fonts[slot];
        vt->vt.xft_fonts[slot] = OpenXftFont(vt, face, size, False);
        if (vt->vt.xft_fonts[slot] != NULL) {
                vt->vt.xft_bold_fonts[slot] = OpenXftFont(vt, face, size, True);
                vt->vt.xft_sizes[slot] = size;
                XtpLog(XTP_LOG_INFO, "font",
                       "loaded Xft slot=%d face=%s points=%.2f cell=%ux%u ascent=%d bold=%s", slot,
                       face, size, XftFontWidth(vt->vt.xft_fonts[slot]),
                       XftFontHeight(vt->vt.xft_fonts[slot]), vt->vt.xft_fonts[slot]->ascent,
                       vt->vt.xft_bold_fonts[slot] != NULL ? "yes" : "fallback");
        } else {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft slot=%d face=%s points=%.2f", slot,
                       face, size);
        }
        return vt->vt.xft_fonts[slot];
}

static void
InitializeXft(Vt100Rec *vt)
{
        char *face = PrimaryFaceName(vt->vt.face_name);
        Boolean requested = ResourceBoolean(vt->vt.render_font_name, Nonempty(vt->vt.face_name));
        double base_size = PositiveNumber(vt->vt.face_size_names[0], 8.0);
        unsigned long base_area;
        int slot;

        vt->vt.use_xft = False;
        vt->vt.xft_draw = NULL;
        memset(vt->vt.xft_fonts, 0, sizeof(vt->vt.xft_fonts));
        memset(vt->vt.xft_bold_fonts, 0, sizeof(vt->vt.xft_bold_fonts));
        memset(vt->vt.xft_sizes, 0, sizeof(vt->vt.xft_sizes));
        if (!Nonempty(face)) {
                if (requested)
                        XtpLog(XTP_LOG_WARNING, "font",
                               "renderFont=%s ignored because faceName is empty",
                               vt->vt.render_font_name != NULL ? vt->vt.render_font_name
                                                               : "(null)");
                goto done;
        }

        base_area = (unsigned long)FontWidth(vt->vt.initial_font) * FontHeight(vt->vt.initial_font);
        if (base_area == 0)
                base_area = 1;
        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                double size = PositiveNumber(vt->vt.face_size_names[slot], 0.0);

                if (size <= 0.0) {
                        XFontStruct *bitmap =
                            vt->vt.fonts[slot] != NULL ? vt->vt.fonts[slot] : vt->vt.initial_font;
                        unsigned long area = (unsigned long)FontWidth(bitmap) * FontHeight(bitmap);

                        size = base_size * sqrt((double)area / (double)base_area);
                }
                (void)LoadXftSlot(vt, slot, face, size);
        }
        if (requested && vt->vt.xft_fonts[0] != NULL)
                vt->vt.use_xft = True;

done:
        XtpLog(XTP_LOG_INFO, "font", "renderer=%s renderFont=%s faceName=%s faceSize=%.2f",
               vt->vt.use_xft ? "xft" : "xlib-bitmap",
               vt->vt.render_font_name != NULL ? vt->vt.render_font_name : "(null)",
               vt->vt.face_name != NULL ? vt->vt.face_name : "(null)", base_size);
        free(face);
}

static void
LogInitialFont(Vt100Rec *vt)
{
        unsigned long atom = None;
        char *name = NULL;
        XColor foreground = {0};
        XColor background = {0};
        int slot;

        if (XGetFontProperty(vt->vt.initial_font, XA_FONT, &atom) && atom != None)
                name = XGetAtomName(XtDisplay((Widget)vt), (Atom)atom);
        foreground.pixel = vt->vt.foreground;
        background.pixel = vt->core.background_pixel;
        XQueryColor(XtDisplay((Widget)vt), DefaultColormapOfScreen(XtScreen((Widget)vt)),
                    &foreground);
        XQueryColor(XtDisplay((Widget)vt), DefaultColormapOfScreen(XtScreen((Widget)vt)),
                    &background);
        XtpLog(XTP_LOG_INFO, "config",
               "VT100 resolved renderer=%s font=%s faceName=%s faceSize=%.2f cell=%ux%u "
               "foreground=#%02x%02x%02x background=#%02x%02x%02x cursorColor=%lu",
               vt->vt.use_xft ? "xft" : "xlib-bitmap", name != NULL ? name : "(unknown)",
               vt->vt.face_name != NULL ? vt->vt.face_name : "(unset)", vt->vt.xft_sizes[0],
               SlotWidth(vt, 0), SlotHeight(vt, 0), foreground.red >> 8, foreground.green >> 8,
               foreground.blue >> 8, background.red >> 8, background.green >> 8,
               background.blue >> 8, vt->vt.cursor_color);
        XtpLog(XTP_LOG_INFO, "config",
               "VT100 resolved grid=%dx%d internalBorder=%u saveLines=%d scrollBar=%s "
               "rightScrollBar=%s alwaysHighlight=%s selectToClipboard=%s",
               vt->vt.columns, vt->vt.rows, vt->vt.internal_border, vt->vt.save_lines,
               vt->vt.scroll_bar ? "true" : "false", vt->vt.right_scroll_bar ? "true" : "false",
               vt->vt.always_highlight ? "true" : "false",
               vt->vt.select_to_clipboard ? "true" : "false");
        for (slot = 1; slot < XTP_FONT_SLOTS; ++slot) {
                XtpLog(XTP_LOG_INFO, "config", "VT100 resolved font%d=%s", slot,
                       vt->vt.font_names[slot] != NULL ? vt->vt.font_names[slot] : "(null)");
        }
        if (name != NULL)
                XFree(name);
}

static void
Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        Vt100Rec *vt = AsVt(new_widget);

        (void)request;
        (void)args;
        (void)num_args;

        if (vt->vt.columns < 1)
                vt->vt.columns = 80;
        if (vt->vt.rows < 1)
                vt->vt.rows = 24;
        if (vt->vt.save_lines < 0)
                vt->vt.save_lines = 0;
        if (vt->vt.multi_click_time < 0)
                vt->vt.multi_click_time = 0;
        if (vt->vt.cursor_on_time < 0)
                vt->vt.cursor_on_time = 0;
        if (vt->vt.cursor_off_time < 0)
                vt->vt.cursor_off_time = 0;
        vt->vt.cursor_blink_policy = ParseCursorBlinkPolicy(vt->vt.cursor_blink_name);

        XtpLog(XTP_LOG_INFO, "config",
               "VT100 compiled defaults grid=80x24 font=fixed internalBorder=2 saveLines=1024 "
               "scrollBar=false rightScrollBar=false");

        memset(vt->vt.fonts, 0, sizeof(vt->vt.fonts));
        memset(vt->vt.owned, 0, sizeof(vt->vt.owned));
        vt->vt.fonts[0] = vt->vt.initial_font;
        vt->vt.use_xft = False;
        vt->vt.xft_draw = NULL;
        memset(vt->vt.xft_fonts, 0, sizeof(vt->vt.xft_fonts));
        memset(vt->vt.xft_bold_fonts, 0, sizeof(vt->vt.xft_bold_fonts));
        memset(vt->vt.xft_sizes, 0, sizeof(vt->vt.xft_sizes));
        vt->vt.current_font = 0;
        vt->vt.gc = NULL;
        vt->vt.terminal = NULL;
        vt->vt.selection_text = NULL;
        vt->vt.selection_text_length = 0;
        vt->vt.selection_time = CurrentTime;
        vt->vt.owned_selections = NULL;
        vt->vt.owned_selection_count = 0;
        vt->vt.disowning_selections = False;
        vt->vt.selection_dragging = False;
        vt->vt.selection_extending = False;
        vt->vt.selection_autoscroll_timer = (XtIntervalId)0;
        vt->vt.selection_pointer_x = 0;
        vt->vt.selection_pointer_y = 0;
        vt->vt.selection_rectangle = False;
        vt->vt.hovered_hyperlink = NULL;
        vt->vt.hovered_hyperlink_length = 0;
        vt->vt.pressed_hyperlink = NULL;
        vt->vt.pressed_hyperlink_length = 0;
        vt->vt.reported_mouse_buttons = 0;
        vt->vt.last_button_up_time = 0;
        vt->vt.last_button = 0;
        vt->vt.number_of_clicks = 0;
        vt->vt.select_unit = XTP_SELECTION_CELL;
        vt->vt.focused = False;
        vt->vt.render_cursor_visible = False;
        vt->vt.render_cursor_column = 0;
        vt->vt.render_cursor_row = 0;
        vt->vt.cursor_cell_seen = False;
        vt->vt.cursor_text_length = 0;
        vt->vt.cursor_fill = vt->vt.foreground;
        vt->vt.cursor_text_color = vt->core.background_pixel;
        vt->vt.cursor_bold = False;
        vt->vt.frame_cells = NULL;
        vt->vt.pending_cells = NULL;
        vt->vt.frame_capacity = 0;
        vt->vt.frame_columns = 0;
        vt->vt.frame_rows = 0;
        vt->vt.frame_valid = False;
        vt->vt.capture_full_frame = False;
        vt->vt.damage_clip_active = False;
        memset(&vt->vt.damage_clip, 0, sizeof(vt->vt.damage_clip));
        vt->vt.last_cursor_visible = False;
        vt->vt.last_cursor_column = 0;
        vt->vt.last_cursor_row = 0;
        vt->vt.last_cursor_shape = XTP_CURSOR_SHAPE_BLOCK;
        vt->vt.cursor_protocol_visible = False;
        vt->vt.cursor_blink_requested = False;
        vt->vt.cursor_blinking = False;
        vt->vt.cursor_blink_on = True;
        vt->vt.cursor_blink_timer = (XtIntervalId)0;
        vt->vt.viewport_update_timer = (XtIntervalId)0;
        vt->vt.viewport_updates_coalesced = 0;
        vt->vt.suppress_grid_resize = False;
        memset(vt->vt.recent_key_actions, 0, sizeof(vt->vt.recent_key_actions));
        vt->vt.next_key_action = 0;
        vt->vt.color_count = 0;
        memset(vt->vt.colors, 0, sizeof(vt->vt.colors));
        {
                int slot;

                for (slot = 1; slot < XTP_FONT_SLOTS; ++slot)
                        (void)LoadSlot(vt, slot);
                XtpLog(XTP_LOG_DEBUG, "font", "preloaded configured bitmap slots");
        }
        InitializeXft(vt);
        LogInitialFont(vt);
        vt->vt.scrollbar = NULL;

        if (vt->core.width == 0)
                vt->core.width = XtpVtNaturalWidth(new_widget);
        if (vt->core.height == 0)
                vt->core.height = XtpVtNaturalHeight(new_widget);

        CreateGc(new_widget);
        XtAddEventHandler(new_widget,
                          PointerMotionMask | KeyPressMask | KeyReleaseMask | LeaveWindowMask,
                          False, HyperlinkEvent, vt);
}

static void
Destroy(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);
        int slot;
        size_t color;

        if (vt->vt.viewport_update_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.viewport_update_timer);
        if (vt->vt.selection_autoscroll_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.selection_autoscroll_timer);
        if (vt->vt.cursor_blink_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.cursor_blink_timer);
        ReleaseGc(widget);
        if (vt->vt.xft_draw != NULL)
                XftDrawDestroy(vt->vt.xft_draw);
        free(vt->vt.frame_cells);
        free(vt->vt.pending_cells);
        free(vt->vt.selection_text);
        free(vt->vt.owned_selections);
        free(vt->vt.hovered_hyperlink);
        free(vt->vt.pressed_hyperlink);
        for (color = 0; color < vt->vt.color_count; ++color) {
                if (vt->vt.colors[color].used && vt->vt.colors[color].owned) {
                        Pixel pixel = vt->vt.colors[color].pixel;
                        XFreeColors(XtDisplay(widget), DefaultColormapOfScreen(XtScreen(widget)),
                                    &pixel, 1, 0);
                }
        }
        for (slot = 1; slot < XTP_FONT_SLOTS; ++slot) {
                if (vt->vt.owned[slot] && vt->vt.fonts[slot] != NULL)
                        XFreeFont(XtDisplay(widget), vt->vt.fonts[slot]);
        }
        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                if (vt->vt.xft_fonts[slot] != NULL)
                        XftFontClose(XtDisplay(widget), vt->vt.xft_fonts[slot]);
                if (vt->vt.xft_bold_fonts[slot] != NULL)
                        XftFontClose(XtDisplay(widget), vt->vt.xft_bold_fonts[slot]);
        }
}

static void
ResizeWidget(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);
        Dimension scrollbar = vt->vt.scroll_bar ? ScrollbarTotalWidth(vt) : 0;
        unsigned int cell_width = XtpVtCellWidth(widget);
        unsigned int cell_height = XtpVtCellHeight(widget);
        unsigned int horizontal = 2U * vt->vt.internal_border + scrollbar;
        unsigned int vertical = 2U * vt->vt.internal_border;
        unsigned int columns =
            vt->core.width > horizontal ? (vt->core.width - horizontal) / cell_width : 1U;
        unsigned int rows =
            vt->core.height > vertical ? (vt->core.height - vertical) / cell_height : 1U;

        LayoutScrollbar(vt);
        XtpLog(XTP_LOG_DEBUG, "resize",
               "VT100 pixels=%ux%u usable=%ux%u candidate-grid=%ux%u current-grid=%dx%d "
               "realized=%s suppressed=%s",
               vt->core.width, vt->core.height,
               vt->core.width > horizontal ? vt->core.width - horizontal : 0,
               vt->core.height > vertical ? vt->core.height - vertical : 0, columns, rows,
               vt->vt.columns, vt->vt.rows, XtIsRealized(widget) ? "true" : "false",
               vt->vt.suppress_grid_resize ? "true" : "false");
        if (!XtIsRealized(widget) || vt->vt.suppress_grid_resize)
                return;
        if (columns < 1U)
                columns = 1U;
        if (rows < 1U)
                rows = 1U;
        if (columns != (unsigned int)vt->vt.columns || rows != (unsigned int)vt->vt.rows) {
                XtpSizeChanged changed;

                XtpLog(XTP_LOG_INFO, "resize", "VT100 grid changed %ux%u -> %ux%u",
                       (unsigned int)vt->vt.columns, (unsigned int)vt->vt.rows, columns, rows);
                vt->vt.frame_valid = False;
                vt->vt.last_cursor_visible = False;
                vt->vt.columns = (int)columns;
                vt->vt.rows = (int)rows;
                changed.columns = columns;
                changed.rows = rows;
                changed.cell_width = cell_width;
                changed.cell_height = cell_height;
                XtCallCallbacks(widget, XtNsizeChangedCallback, &changed);
        }
}

static Pixel
RgbPixel(Vt100Rec *vt, uint8_t red, uint8_t green, uint8_t blue)
{
        size_t index;
        XColor color;

        for (index = 0; index < vt->vt.color_count; ++index) {
                ColorCacheEntry *entry = &vt->vt.colors[index];

                if (entry->used && entry->red == red && entry->green == green &&
                    entry->blue == blue)
                        return entry->pixel;
        }

        color.red = (unsigned short)(red * 257U);
        color.green = (unsigned short)(green * 257U);
        color.blue = (unsigned short)(blue * 257U);
        color.flags = DoRed | DoGreen | DoBlue;
        if (!XAllocColor(XtDisplay((Widget)vt), DefaultColormapOfScreen(XtScreen((Widget)vt)),
                         &color))
                return vt->vt.foreground;

        if (vt->vt.color_count < XTP_COLOR_CACHE_SIZE) {
                ColorCacheEntry *entry = &vt->vt.colors[vt->vt.color_count++];

                entry->used = True;
                entry->owned = True;
                entry->red = red;
                entry->green = green;
                entry->blue = blue;
                entry->pixel = color.pixel;
                entry->xft.pixel = color.pixel;
                entry->xft.color.red = color.red;
                entry->xft.color.green = color.green;
                entry->xft.color.blue = color.blue;
                entry->xft.color.alpha = 0xffffU;
        }
        return color.pixel;
}

static Pixel
RenderColor(Vt100Rec *vt, XtpColor color, Boolean foreground)
{
        Pixel pixel = foreground ? vt->vt.foreground : vt->core.background_pixel;

        switch (color.kind) {
        case XTP_COLOR_DEFAULT:
                break;
        case XTP_COLOR_PALETTE:
        case XTP_COLOR_RGB:
                pixel = RgbPixel(vt, color.red, color.green, color.blue);
                break;
        }
        return pixel;
}

static int
TerminalX(Vt100Rec *vt)
{
        int x = (int)vt->vt.internal_border;

        if (vt->vt.scroll_bar && !vt->vt.right_scroll_bar)
                x += (int)ScrollbarTotalWidth(vt);
        return x;
}

static void
ResetVisualCells(Vt100Rec *vt, VisualCell *cells, size_t count)
{
        size_t index;

        memset(cells, 0, count * sizeof(*cells));
        for (index = 0; index < count; ++index) {
                cells[index].foreground = vt->vt.foreground;
                cells[index].background = vt->core.background_pixel;
                cells[index].width = 1;
        }
}

static Boolean
EnsureFrameStorage(Vt100Rec *vt, unsigned int columns, unsigned int rows)
{
        size_t count = (size_t)columns * rows;

        if (columns == vt->vt.frame_columns && rows == vt->vt.frame_rows &&
            count <= vt->vt.frame_capacity && vt->vt.frame_cells != NULL &&
            vt->vt.pending_cells != NULL)
                return True;

        free(vt->vt.frame_cells);
        free(vt->vt.pending_cells);
        vt->vt.frame_cells = calloc(count, sizeof(*vt->vt.frame_cells));
        vt->vt.pending_cells = calloc(count, sizeof(*vt->vt.pending_cells));
        if (vt->vt.frame_cells == NULL || vt->vt.pending_cells == NULL) {
                free(vt->vt.frame_cells);
                free(vt->vt.pending_cells);
                vt->vt.frame_cells = NULL;
                vt->vt.pending_cells = NULL;
                vt->vt.frame_capacity = 0;
                vt->vt.frame_columns = 0;
                vt->vt.frame_rows = 0;
                vt->vt.frame_valid = False;
                return False;
        }
        vt->vt.frame_capacity = count;
        vt->vt.frame_columns = columns;
        vt->vt.frame_rows = rows;
        vt->vt.frame_valid = False;
        ResetVisualCells(vt, vt->vt.frame_cells, count);
        ResetVisualCells(vt, vt->vt.pending_cells, count);
        return True;
}

static VisualCell
MakeVisualCell(Vt100Rec *vt, const XtpRenderCell *cell)
{
        VisualCell visual = {0};
        size_t index;
        Boolean drawable = cell->utf8_length < sizeof(visual.text);

        visual.foreground = RenderColor(vt, cell->foreground, True);
        visual.background = RenderColor(vt, cell->background, False);
        visual.width = cell->width;
        if (cell->inverse) {
                Pixel temporary = visual.foreground;

                visual.foreground = visual.background;
                visual.background = temporary;
        }
        if (cell->selected) {
                Pixel temporary = visual.foreground;

                visual.foreground = visual.background;
                visual.background = temporary;
        }
        if (drawable) {
                for (index = 0; index < cell->utf8_length; ++index) {
                        unsigned char byte = (unsigned char)cell->utf8[index];

                        if (byte < 0x20U || (!vt->vt.use_xft && byte >= 0x7fU)) {
                                drawable = False;
                                break;
                        }
                        visual.text[index] = (char)byte;
                }
        }
        if (!cell->invisible) {
                if (!drawable && cell->utf8_length != 0) {
                        visual.text[0] = '?';
                        visual.text_length = 1;
                } else {
                        visual.text_length = cell->utf8_length;
                }
        }
        visual.bold = cell->bold;
        visual.underline = cell->underline != 0 || HyperlinkUriEqualsCell(vt, cell);
        visual.strikethrough = cell->strikethrough;
        visual.overline = cell->overline;
        return visual;
}

static Boolean
EnsureXftDraw(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        if (!vt->vt.use_xft)
                return False;
        if (vt->vt.xft_draw != NULL)
                return True;
        if (!XtIsRealized(widget))
                return False;
        vt->vt.xft_draw = XftDrawCreate(XtDisplay(widget), XtWindow(widget),
                                        DefaultVisualOfScreen(XtScreen(widget)),
                                        DefaultColormapOfScreen(XtScreen(widget)));
        if (vt->vt.xft_draw == NULL) {
                XtpLog(XTP_LOG_ERROR, "font", "cannot create Xft draw context");
                return False;
        }
        return True;
}

static XftColor
CachedXftColor(Vt100Rec *vt, Pixel pixel)
{
        size_t index;
        XColor xcolor = {0};
        XftColor color = {0};

        for (index = 0; index < vt->vt.color_count; ++index) {
                ColorCacheEntry *entry = &vt->vt.colors[index];

                if (entry->used && entry->pixel == pixel)
                        return entry->xft;
        }
        xcolor.pixel = pixel;
        XQueryColor(XtDisplay((Widget)vt), DefaultColormapOfScreen(XtScreen((Widget)vt)), &xcolor);
        color.pixel = pixel;
        color.color.red = xcolor.red;
        color.color.green = xcolor.green;
        color.color.blue = xcolor.blue;
        color.color.alpha = 0xffffU;
        if (vt->vt.color_count < XTP_COLOR_CACHE_SIZE) {
                ColorCacheEntry *entry = &vt->vt.colors[vt->vt.color_count++];

                entry->used = True;
                entry->owned = False;
                entry->red = (uint8_t)(xcolor.red >> 8);
                entry->green = (uint8_t)(xcolor.green >> 8);
                entry->blue = (uint8_t)(xcolor.blue >> 8);
                entry->pixel = pixel;
                entry->xft = color;
        }
        return color;
}

static Boolean
IntersectRectangles(const XRectangle *left, const XRectangle *right, XRectangle *result)
{
        int x1 = left->x > right->x ? left->x : right->x;
        int y1 = left->y > right->y ? left->y : right->y;
        int x2 = left->x + (int)left->width < right->x + (int)right->width
                     ? left->x + (int)left->width
                     : right->x + (int)right->width;
        int y2 = left->y + (int)left->height < right->y + (int)right->height
                     ? left->y + (int)left->height
                     : right->y + (int)right->height;

        if (x2 <= x1 || y2 <= y1)
                return False;
        result->x = (short)x1;
        result->y = (short)y1;
        result->width = (unsigned short)(x2 - x1);
        result->height = (unsigned short)(y2 - y1);
        return True;
}

static Boolean
SetTextClip(Vt100Rec *vt, const XRectangle *requested)
{
        Widget widget = (Widget)vt;
        XRectangle clipped;
        const XRectangle *clip = requested;

        if (vt->vt.damage_clip_active) {
                if (!IntersectRectangles(requested, &vt->vt.damage_clip, &clipped))
                        return False;
                clip = &clipped;
        }
        XSetClipRectangles(XtDisplay(widget), vt->vt.gc, 0, 0, (XRectangle *)clip, 1, Unsorted);
        if (vt->vt.use_xft && EnsureXftDraw(vt))
                (void)XftDrawSetClipRectangles(vt->vt.xft_draw, 0, 0, clip, 1);
        return True;
}

static void
ClearTextClip(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        XSetClipMask(XtDisplay(widget), vt->vt.gc, None);
        if (vt->vt.use_xft && vt->vt.xft_draw != NULL)
                (void)XftDrawSetClip(vt->vt.xft_draw, NULL);
}

static void
DrawTextClipped(Vt100Rec *vt, Pixel pixel, int x, int baseline, const char *text, size_t length,
                Boolean bold, const XRectangle *clip)
{
        Widget widget = (Widget)vt;

        if (length == 0)
                return;
        if (clip != NULL && !SetTextClip(vt, clip))
                return;
        if (vt->vt.use_xft && EnsureXftDraw(vt)) {
                XftColor color = CachedXftColor(vt, pixel);
                XftFont *font = bold ? vt->vt.xft_bold_fonts[vt->vt.current_font] : NULL;

                if (font == NULL)
                        font = vt->vt.xft_fonts[vt->vt.current_font];
                XftDrawStringUtf8(vt->vt.xft_draw, &color, font, x, baseline, (const FcChar8 *)text,
                                  (int)length);
        } else {
                XSetForeground(XtDisplay(widget), vt->vt.gc, pixel);
                XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, baseline, text,
                            (int)length);
                if (bold)
                        XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x + 1, baseline,
                                    text, (int)length);
        }
        if (clip != NULL)
                ClearTextClip(vt);
}

static void
DrawText(Vt100Rec *vt, Pixel pixel, int x, int baseline, const char *text, size_t length,
         Boolean bold)
{
        DrawTextClipped(vt, pixel, x, baseline, text, length, bold, NULL);
}

static void
PaintVisualRun(Vt100Rec *vt, const VisualCell *style, const XRectangle *area, int x, int baseline,
               const char *xft_text, size_t xft_length, const char *bitmap_text,
               size_t bitmap_length)
{
        Widget widget = (Widget)vt;

        if (!SetTextClip(vt, area))
                return;
        if (vt->vt.use_xft && EnsureXftDraw(vt)) {
                XftColor background = CachedXftColor(vt, style->background);
                XftColor foreground = CachedXftColor(vt, style->foreground);
                XftFont *font = style->bold ? vt->vt.xft_bold_fonts[vt->vt.current_font] : NULL;

                if (font == NULL)
                        font = vt->vt.xft_fonts[vt->vt.current_font];
                XftDrawRect(vt->vt.xft_draw, &background, area->x, area->y, area->width,
                            area->height);
                if (xft_length != 0)
                        XftDrawStringUtf8(vt->vt.xft_draw, &foreground, font, x, baseline,
                                          (const FcChar8 *)xft_text, (int)xft_length);
        } else {
                XSetForeground(XtDisplay(widget), vt->vt.gc, style->foreground);
                XSetBackground(XtDisplay(widget), vt->vt.gc, style->background);
                XDrawImageString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, baseline,
                                 bitmap_text, (int)bitmap_length);
                if (style->bold)
                        XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x + 1, baseline,
                                    bitmap_text, (int)bitmap_length);
        }
        ClearTextClip(vt);
}

static void
DrawDecorations(Vt100Rec *vt, const VisualCell *cell, const XRectangle *area)
{
        Widget widget = (Widget)vt;
        int right = area->x + (int)area->width - 1;

        if (!SetTextClip(vt, area))
                return;
        XSetForeground(XtDisplay(widget), vt->vt.gc, cell->foreground);
        if (cell->underline)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x,
                          area->y + (int)area->height - 1, right, area->y + (int)area->height - 1);
        if (cell->strikethrough)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x,
                          area->y + (int)area->height / 2, right, area->y + (int)area->height / 2);
        if (cell->overline)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x, area->y, right,
                          area->y);
        ClearTextClip(vt);
}

static void
DrawVisualCell(Vt100Rec *vt, const VisualCell *cell, unsigned int column, unsigned int row)
{
        unsigned int cell_width = SlotWidth(vt, vt->vt.current_font);
        unsigned int height = SlotHeight(vt, vt->vt.current_font);
        unsigned int columns = cell->width != 0 ? cell->width : 1U;
        XRectangle area;
        int x = TerminalX(vt) + (int)column * (int)cell_width;
        int y = (int)vt->vt.internal_border + (int)row * (int)height;

        if (cell->width == 0)
                return;
        area.x = (short)x;
        area.y = (short)y;
        area.width = (unsigned short)(columns * cell_width);
        area.height = (unsigned short)height;
        {
                char image[2] = {' ', ' '};

                if (cell->text_length != 0)
                        image[0] = cell->text[0];
                PaintVisualRun(vt, cell, &area, x, y + SlotAscent(vt, vt->vt.current_font),
                               cell->text, cell->text_length, image, columns);
        }
        DrawDecorations(vt, cell, &area);
}

static Boolean
SameVisualStyle(const VisualCell *left, const VisualCell *right)
{
        return left->foreground == right->foreground && left->background == right->background &&
               left->bold == right->bold && left->underline == right->underline &&
               left->strikethrough == right->strikethrough && left->overline == right->overline;
}

static void
DrawVisualRowRange(Vt100Rec *vt, unsigned int row, unsigned int first_column,
                   unsigned int end_column)
{
        enum
        {
                RUN_CAPACITY = 4096
        };
        unsigned int width = SlotWidth(vt, vt->vt.current_font);
        unsigned int height = SlotHeight(vt, vt->vt.current_font);
        unsigned int column = first_column;

        if (end_column > vt->vt.frame_columns)
                end_column = vt->vt.frame_columns;
        while (column < end_column) {
                const VisualCell *first =
                    &vt->vt.frame_cells[(size_t)row * vt->vt.frame_columns + column];

                if (first->width == 0) {
                        ++column;
                } else if (first->width == 1 && first->text_length <= 1) {
                        char run[RUN_CAPACITY];
                        unsigned int start = column;
                        size_t length = 0;
                        size_t visible = 0;

                        while (column < end_column && length < sizeof(run)) {
                                const VisualCell *cell =
                                    &vt->vt
                                         .frame_cells[(size_t)row * vt->vt.frame_columns + column];

                                if (cell->width != 1 || cell->text_length > 1 ||
                                    !SameVisualStyle(cell, first))
                                        break;
                                run[length] = cell->text_length == 1 ? cell->text[0] : ' ';
                                if (cell->text_length == 1)
                                        visible = length + 1U;
                                ++length;
                                ++column;
                        }
                        {
                                XRectangle area;
                                int x = TerminalX(vt) + (int)start * (int)width;
                                int y = (int)vt->vt.internal_border + (int)row * (int)height;

                                area.x = (short)x;
                                area.y = (short)y;
                                area.width = (unsigned short)(length * width);
                                area.height = (unsigned short)height;
                                PaintVisualRun(vt, first, &area, x,
                                               y + SlotAscent(vt, vt->vt.current_font), run,
                                               visible, run, length);
                                DrawDecorations(vt, first, &area);
                        }
                } else {
                        DrawVisualCell(vt, first, column, row);
                        column += first->width;
                }
        }
}

static void
DrawVisualRow(Vt100Rec *vt, unsigned int row)
{
        DrawVisualRowRange(vt, row, 0, vt->vt.frame_columns);
}

static void
SetCursorCell(Vt100Rec *vt, const VisualCell *cell)
{
        vt->vt.cursor_cell_seen = True;
        vt->vt.cursor_text_length = cell->text_length;
        if (cell->text_length != 0)
                memcpy(vt->vt.cursor_text, cell->text, cell->text_length);
        vt->vt.cursor_fill =
            vt->vt.cursor_color != cell->background ? vt->vt.cursor_color : cell->foreground;
        vt->vt.cursor_text_color = cell->background;
        vt->vt.cursor_bold = cell->bold;
}

static void
EraseLastCursor(Vt100Rec *vt)
{
        size_t index;

        if (!vt->vt.frame_valid || !vt->vt.last_cursor_visible ||
            vt->vt.last_cursor_column >= vt->vt.frame_columns ||
            vt->vt.last_cursor_row >= vt->vt.frame_rows)
                return;
        index = (size_t)vt->vt.last_cursor_row * vt->vt.frame_columns + vt->vt.last_cursor_column;
        DrawVisualCell(vt, &vt->vt.frame_cells[index], vt->vt.last_cursor_column,
                       vt->vt.last_cursor_row);
}

static void
DrawCursor(Vt100Rec *vt, Boolean visible, unsigned int column, unsigned int row,
           XtpCursorShape shape)
{
        Widget widget = (Widget)vt;

        if (visible && !vt->vt.cursor_cell_seen && column < vt->vt.frame_columns &&
            row < vt->vt.frame_rows && vt->vt.frame_valid) {
                size_t index = (size_t)row * vt->vt.frame_columns + column;

                SetCursorCell(vt, &vt->vt.frame_cells[index]);
        }

        if (visible && column < vt->vt.frame_columns && row < vt->vt.frame_rows) {
                unsigned int width = SlotWidth(vt, vt->vt.current_font);
                unsigned int height = SlotHeight(vt, vt->vt.current_font);
                XRectangle area;
                int x = TerminalX(vt) + (int)column * (int)width;
                int y = (int)vt->vt.internal_border + (int)row * (int)height;

                area.x = (short)x;
                area.y = (short)y;
                area.width = (unsigned short)width;
                area.height = (unsigned short)height;
                if (!SetTextClip(vt, &area))
                        return;
                if (shape == XTP_CURSOR_SHAPE_BLOCK &&
                    (vt->vt.focused || vt->vt.always_highlight) && vt->vt.cursor_cell_seen) {
                        XSetForeground(XtDisplay(widget), vt->vt.gc, vt->vt.cursor_fill);
                        XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y, width,
                                       height);
                        if (vt->vt.cursor_text_length != 0) {
                                DrawText(vt, vt->vt.cursor_text_color, x,
                                         y + SlotAscent(vt, vt->vt.current_font),
                                         vt->vt.cursor_text, vt->vt.cursor_text_length,
                                         vt->vt.cursor_bold);
                        }
                } else if (shape == XTP_CURSOR_SHAPE_UNDERLINE || shape == XTP_CURSOR_SHAPE_BAR) {
                        unsigned int dimension =
                            shape == XTP_CURSOR_SHAPE_UNDERLINE ? height : width;
                        unsigned int thickness = dimension > 1 ? (dimension - 1U) / 8U : dimension;
                        Pixel cursor =
                            vt->vt.cursor_cell_seen ? vt->vt.cursor_fill : vt->vt.cursor_color;

                        if (thickness < 2U && dimension >= 2U)
                                thickness = 2U;
                        if (thickness > dimension)
                                thickness = dimension;
                        XSetForeground(XtDisplay(widget), vt->vt.gc, cursor);
                        if (shape == XTP_CURSOR_SHAPE_UNDERLINE)
                                XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x,
                                               y + (int)height - (int)thickness, width, thickness);
                        else
                                XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y,
                                               thickness, height);
                } else {
                        Pixel outline =
                            vt->vt.cursor_cell_seen ? vt->vt.cursor_fill : vt->vt.cursor_color;

                        XSetForeground(XtDisplay(widget), vt->vt.gc, outline);
                        XDrawRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y,
                                       width > 0 ? width - 1 : 0, height > 0 ? height - 1 : 0);
                }
                ClearTextClip(vt);
        }
        vt->vt.last_cursor_visible = visible;
        vt->vt.last_cursor_column = column;
        vt->vt.last_cursor_row = row;
        vt->vt.last_cursor_shape = shape;
}

static void
StopCursorBlink(Vt100Rec *vt)
{
        if (vt->vt.cursor_blink_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.cursor_blink_timer);
                vt->vt.cursor_blink_timer = (XtIntervalId)0;
        }
}

static void ScheduleCursorBlink(Vt100Rec *vt);

static void
CursorBlinkTick(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;
        Boolean active;

        (void)timer;
        vt->vt.cursor_blink_timer = (XtIntervalId)0;
        if (!XtIsRealized((Widget)vt) || !vt->vt.cursor_protocol_visible ||
            !vt->vt.cursor_blinking || !vt->vt.frame_valid)
                return;

        active = vt->vt.focused || vt->vt.always_highlight;
        if (!active) {
                if (!vt->vt.last_cursor_visible) {
                        vt->vt.cursor_cell_seen = False;
                        vt->vt.cursor_text_length = 0;
                        DrawCursor(vt, True, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                                   vt->vt.last_cursor_shape);
                        XFlush(XtDisplay((Widget)vt));
                }
                vt->vt.cursor_blink_on = True;
        } else if (vt->vt.cursor_blink_on) {
                EraseLastCursor(vt);
                DrawCursor(vt, False, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                           vt->vt.last_cursor_shape);
                vt->vt.cursor_blink_on = False;
                XFlush(XtDisplay((Widget)vt));
        } else {
                vt->vt.cursor_cell_seen = False;
                vt->vt.cursor_text_length = 0;
                DrawCursor(vt, True, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                           vt->vt.last_cursor_shape);
                vt->vt.cursor_blink_on = True;
                XFlush(XtDisplay((Widget)vt));
        }
        XtpLog(XTP_LOG_DEBUG, "render", "cursor blink phase=%s column=%u row=%u",
               vt->vt.cursor_blink_on ? "on" : "off", vt->vt.last_cursor_column,
               vt->vt.last_cursor_row);
        ScheduleCursorBlink(vt);
}

static void
ScheduleCursorBlink(Vt100Rec *vt)
{
        unsigned long delay;

        if (vt->vt.cursor_blink_timer != (XtIntervalId)0 || !vt->vt.cursor_protocol_visible ||
            !vt->vt.cursor_blinking || !XtIsRealized((Widget)vt))
                return;
        delay = (unsigned long)(vt->vt.cursor_blink_on ? vt->vt.cursor_on_time
                                                       : vt->vt.cursor_off_time);
        if (delay == 0)
                delay = 1;
        vt->vt.cursor_blink_timer =
            XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt), delay, CursorBlinkTick, vt);
}

static void
RestartCursorBlink(Vt100Rec *vt)
{
        StopCursorBlink(vt);
        vt->vt.cursor_blink_on = vt->vt.last_cursor_visible || !vt->vt.cursor_protocol_visible;
        ScheduleCursorBlink(vt);
}

static Boolean
SameVisualCell(const VisualCell *left, const VisualCell *right)
{
        return left->foreground == right->foreground && left->background == right->background &&
               left->text_length == right->text_length && left->width == right->width &&
               left->bold == right->bold && left->underline == right->underline &&
               left->strikethrough == right->strikethrough && left->overline == right->overline &&
               memcmp(left->text, right->text, left->text_length) == 0;
}

static void
RenderBegin(const XtpRenderFrame *frame, void *closure)
{
        Vt100Rec *vt = closure;

        vt->vt.render_cursor_visible = frame->cursor_visible;
        vt->vt.render_cursor_column = frame->cursor_column;
        vt->vt.render_cursor_row = frame->cursor_row;
        vt->vt.cursor_cell_seen = False;
        vt->vt.cursor_text_length = 0;
        vt->vt.capture_full_frame =
            frame->full_repaint && EnsureFrameStorage(vt, frame->columns, frame->rows);
        if (vt->vt.capture_full_frame) {
                ResetVisualCells(vt, vt->vt.pending_cells, vt->vt.frame_capacity);
        } else {
                (void)EnsureFrameStorage(vt, frame->columns, frame->rows);
        }
}

static void
RenderCell(const XtpRenderCell *cell, void *closure)
{
        Vt100Rec *vt = closure;
        VisualCell visual = MakeVisualCell(vt, cell);
        size_t index = (size_t)cell->row * vt->vt.frame_columns + cell->column;

        if (vt->vt.render_cursor_visible && cell->column == vt->vt.render_cursor_column &&
            cell->row == vt->vt.render_cursor_row)
                SetCursorCell(vt, &visual);

        if (index < vt->vt.frame_capacity) {
                if (vt->vt.capture_full_frame) {
                        vt->vt.pending_cells[index] = visual;
                } else {
                        Boolean changed = !vt->vt.frame_valid ||
                                          !SameVisualCell(&vt->vt.frame_cells[index], &visual);
                        Boolean covered_by_cursor = vt->vt.last_cursor_visible &&
                                                    cell->column == vt->vt.last_cursor_column &&
                                                    cell->row == vt->vt.last_cursor_row;

                        vt->vt.frame_cells[index] = visual;
                        if (changed && !covered_by_cursor)
                                DrawVisualCell(vt, &visual, cell->column, cell->row);
                        vt->vt.frame_valid = True;
                }
        } else {
                DrawVisualCell(vt, &visual, cell->column, cell->row);
        }
}

static void
RenderEnd(const XtpRenderFrame *frame, void *closure)
{
        Vt100Rec *vt = closure;
        Boolean effective_blinking =
            EffectiveCursorBlink(vt->vt.cursor_blink_policy, frame->cursor_blinking);
        Boolean cursor_changed =
            vt->vt.cursor_protocol_visible != frame->cursor_visible ||
            (frame->cursor_visible && (vt->vt.last_cursor_column != frame->cursor_column ||
                                       vt->vt.last_cursor_row != frame->cursor_row ||
                                       vt->vt.last_cursor_shape != frame->cursor_shape));
        Boolean blinking_changed = vt->vt.cursor_blinking != effective_blinking;
        Boolean refresh_cursor = vt->vt.capture_full_frame || cursor_changed || blinking_changed ||
                                 vt->vt.cursor_cell_seen;

        if (!vt->vt.capture_full_frame)
                XtpLog(XTP_LOG_DEBUG, "render",
                       "cursor transition old=%s@%u,%u/%d blink=%s new=%s@%u,%u/%d "
                       "blink-requested=%s blink-effective=%s changed=%s cell=%s",
                       vt->vt.cursor_protocol_visible ? "visible" : "hidden",
                       vt->vt.last_cursor_column, vt->vt.last_cursor_row, vt->vt.last_cursor_shape,
                       vt->vt.cursor_blinking ? "true" : "false",
                       frame->cursor_visible ? "visible" : "hidden", frame->cursor_column,
                       frame->cursor_row, frame->cursor_shape,
                       frame->cursor_blinking ? "true" : "false",
                       effective_blinking ? "true" : "false",
                       cursor_changed || blinking_changed ? "true" : "false",
                       vt->vt.cursor_cell_seen ? "dirty" : "clean");

        if (cursor_changed || blinking_changed) {
                StopCursorBlink(vt);
                vt->vt.cursor_blink_on = True;
        }

        if (vt->vt.capture_full_frame) {
                unsigned int row;
                {
                        VisualCell *temporary = vt->vt.frame_cells;

                        vt->vt.frame_cells = vt->vt.pending_cells;
                        vt->vt.pending_cells = temporary;
                }
                vt->vt.frame_valid = True;
                for (row = 0; row < vt->vt.frame_rows; ++row)
                        DrawVisualRow(vt, row);
        }

        /*
         * Restore the old cursor cell after partial updates have refreshed the
         * frame cache. Erasing it in RenderBegin briefly paints the stale
         * pre-keypress cell before Xft draws the new glyph, which is visible as
         * a flash while typing. A full-frame repaint overwrites the old cursor
         * as it walks the rows, so it does not need a separate erase.
         */
        if (!vt->vt.capture_full_frame &&
            (cursor_changed || blinking_changed || vt->vt.cursor_cell_seen))
                EraseLastCursor(vt);

        vt->vt.cursor_protocol_visible = frame->cursor_visible;
        vt->vt.cursor_blink_requested = frame->cursor_blinking;
        vt->vt.cursor_blinking = effective_blinking;
        if (refresh_cursor) {
                Boolean draw_cursor =
                    frame->cursor_visible && (!effective_blinking || vt->vt.cursor_blink_on);

                DrawCursor(vt, draw_cursor, frame->cursor_column, frame->cursor_row,
                           frame->cursor_shape);
                if (cursor_changed && !vt->vt.capture_full_frame && !vt->vt.cursor_cell_seen)
                        XtpLog(XTP_LOG_DEBUG, "render", "cursor-only repaint column=%u row=%u",
                               frame->cursor_column, frame->cursor_row);
        }
        if (frame->cursor_visible && effective_blinking)
                ScheduleCursorBlink(vt);
        else
                StopCursorBlink(vt);
        UpdateScrollbar(vt);
}

static int
RenderTerminal(Vt100Rec *vt, Boolean force_full)
{
        static const XtpRenderer renderer = {
            RenderBegin,
            RenderCell,
            RenderEnd,
        };

        if (vt->vt.terminal == NULL)
                return -1;
        return XtpTerminalRender(vt->vt.terminal, &renderer, vt, force_full != False);
}

static void
RepaintCached(Vt100Rec *vt, const XRectangle *damage)
{
        unsigned int cell_width = SlotWidth(vt, vt->vt.current_font);
        unsigned int cell_height = SlotHeight(vt, vt->vt.current_font);
        XRectangle grid;
        XRectangle clipped;
        unsigned int first_row;
        unsigned int end_row;
        unsigned int row;

        if (!vt->vt.frame_valid || vt->vt.frame_columns == 0 || vt->vt.frame_rows == 0)
                return;
        grid.x = (short)TerminalX(vt);
        grid.y = (short)vt->vt.internal_border;
        grid.width = (unsigned short)(vt->vt.frame_columns * cell_width);
        grid.height = (unsigned short)(vt->vt.frame_rows * cell_height);
        if (damage != NULL) {
                if (!IntersectRectangles(&grid, damage, &clipped))
                        return;
        } else {
                clipped = grid;
        }

        first_row = (unsigned int)(clipped.y - grid.y) / cell_height;
        end_row =
            ((unsigned int)(clipped.y - grid.y) + clipped.height + cell_height - 1U) / cell_height;
        if (end_row > vt->vt.frame_rows)
                end_row = vt->vt.frame_rows;
        vt->vt.damage_clip_active = True;
        vt->vt.damage_clip = clipped;
        for (row = first_row; row < end_row; ++row) {
                unsigned int first_column = (unsigned int)(clipped.x - grid.x) / cell_width;
                unsigned int end_column =
                    ((unsigned int)(clipped.x - grid.x) + clipped.width + cell_width - 1U) /
                    cell_width;
                VisualCell *cells = vt->vt.frame_cells + (size_t)row * vt->vt.frame_columns;

                if (end_column > vt->vt.frame_columns)
                        end_column = vt->vt.frame_columns;
                if (first_column > 0 && cells[first_column].width == 0)
                        --first_column;
                if (end_column != 0 && cells[end_column - 1U].width > 1U &&
                    end_column - 1U + cells[end_column - 1U].width > end_column)
                        end_column = end_column - 1U + cells[end_column - 1U].width;
                if (end_column > vt->vt.frame_columns)
                        end_column = vt->vt.frame_columns;
                DrawVisualRowRange(vt, row, first_column, end_column);
        }
        if (vt->vt.last_cursor_visible) {
                XRectangle cursor;
                XRectangle intersection;

                cursor.x = (short)(grid.x + (int)vt->vt.last_cursor_column * (int)cell_width);
                cursor.y = (short)(grid.y + (int)vt->vt.last_cursor_row * (int)cell_height);
                cursor.width = (unsigned short)cell_width;
                cursor.height = (unsigned short)cell_height;
                if (IntersectRectangles(&cursor, &clipped, &intersection)) {
                        unsigned int column = vt->vt.last_cursor_column;
                        unsigned int cursor_row = vt->vt.last_cursor_row;

                        vt->vt.cursor_cell_seen = False;
                        vt->vt.cursor_text_length = 0;
                        DrawCursor(vt, True, column, cursor_row, vt->vt.last_cursor_shape);
                }
        }
        vt->vt.damage_clip_active = False;
}

static void
Placeholder(Vt100Rec *vt)
{
        static const char *const lines[] = {
            "xterm+",
            "UI-only build: configure with -Dlibghostty=enabled",
        };
        int x = TerminalX(vt);
        int y = (int)vt->vt.internal_border + SlotAscent(vt, vt->vt.current_font);
        size_t line;

        XSetForeground(XtDisplay((Widget)vt), vt->vt.gc, vt->core.background_pixel);
        XFillRectangle(XtDisplay((Widget)vt), XtWindow((Widget)vt), vt->vt.gc, 0, 0, vt->core.width,
                       vt->core.height);
        for (line = 0; line < XtNumber(lines); ++line) {
                size_t length = strlen(lines[line]);

                DrawText(vt, vt->vt.foreground, x, y, lines[line], length, False);
                y += (int)SlotHeight(vt, vt->vt.current_font);
        }
}

static void
Redisplay(Widget widget, XEvent *event, Region region)
{
        Vt100Rec *vt = AsVt(widget);
        XRectangle damage;
        Boolean have_damage = False;

        XtpLog(XTP_LOG_DEBUG, "render", "expose pixels=%ux%u grid=%dx%d font-slot=%d cell=%ux%u",
               vt->core.width, vt->core.height, vt->vt.columns, vt->vt.rows, vt->vt.current_font,
               XtpVtCellWidth(widget), XtpVtCellHeight(widget));
        if (!vt->vt.frame_valid) {
                if (RenderTerminal(vt, True) != 0)
                        Placeholder(vt);
                return;
        }
        if (region != NULL) {
                XClipBox(region, &damage);
                have_damage = damage.width != 0 && damage.height != 0;
        } else if (event != NULL && event->type == Expose) {
                damage.x = (short)event->xexpose.x;
                damage.y = (short)event->xexpose.y;
                damage.width = (unsigned short)event->xexpose.width;
                damage.height = (unsigned short)event->xexpose.height;
                have_damage = damage.width != 0 && damage.height != 0;
        }
        if (have_damage)
                RepaintCached(vt, &damage);
        else
                RepaintCached(vt, NULL);
}

static Boolean
SetValues(Widget current, Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        Vt100Rec *old_vt = AsVt(current);
        Vt100Rec *new_vt = AsVt(new_widget);
        Boolean changed = False;

        (void)request;
        (void)args;
        (void)num_args;

        if (new_vt->vt.save_lines < 0)
                new_vt->vt.save_lines = 0;
        if (new_vt->vt.multi_click_time < 0)
                new_vt->vt.multi_click_time = 0;
        if (new_vt->vt.cursor_on_time < 0)
                new_vt->vt.cursor_on_time = 0;
        if (new_vt->vt.cursor_off_time < 0)
                new_vt->vt.cursor_off_time = 0;
        if (old_vt->vt.save_lines != new_vt->vt.save_lines && new_vt->vt.terminal != NULL &&
            XtpTerminalSetScrollbackLines(new_vt->vt.terminal, (size_t)new_vt->vt.save_lines) != 0)
                XtpLog(XTP_LOG_ERROR, "scrollback", "cannot set history limit=%d",
                       new_vt->vt.save_lines);
        if ((old_vt->vt.char_class == NULL) != (new_vt->vt.char_class == NULL) ||
            (old_vt->vt.char_class != NULL && new_vt->vt.char_class != NULL &&
             strcmp(old_vt->vt.char_class, new_vt->vt.char_class) != 0)) {
                if (new_vt->vt.terminal != NULL &&
                    XtpTerminalSetCharClass(new_vt->vt.terminal, new_vt->vt.char_class) != 0)
                        XtpLog(XTP_LOG_ERROR, "selection", "cannot apply charClass=%s",
                               new_vt->vt.char_class != NULL ? new_vt->vt.char_class : "(default)");
        }

        new_vt->vt.cursor_blink_policy = ParseCursorBlinkPolicy(new_vt->vt.cursor_blink_name);
        if (old_vt->vt.cursor_blink_policy != new_vt->vt.cursor_blink_policy) {
                Boolean effective = EffectiveCursorBlink(new_vt->vt.cursor_blink_policy,
                                                         new_vt->vt.cursor_blink_requested);

                if (new_vt->vt.terminal != NULL &&
                    XtpTerminalSetCursorBlinkDefault(
                        new_vt->vt.terminal, CursorBlinkDefault(new_vt->vt.cursor_blink_policy)) !=
                        0)
                        XtpLog(XTP_LOG_ERROR, "render", "cannot apply cursorBlink=%s",
                               new_vt->vt.cursor_blink_name);
                StopCursorBlink(new_vt);
                new_vt->vt.cursor_blink_on = True;
                new_vt->vt.cursor_blinking = effective;
                XtpLog(XTP_LOG_INFO, "config", "VT100 cursorBlink=%s effective=%s",
                       new_vt->vt.cursor_blink_name, effective ? "true" : "false");
                changed = True;
        }

        if (old_vt->vt.foreground != new_vt->vt.foreground ||
            old_vt->core.background_pixel != new_vt->core.background_pixel) {
                XtpLog(XTP_LOG_INFO, "config",
                       "VT100 colors changed foreground=%lu->%lu background=%lu->%lu",
                       old_vt->vt.foreground, new_vt->vt.foreground, old_vt->core.background_pixel,
                       new_vt->core.background_pixel);
                ReleaseGc(new_widget);
                CreateGc(new_widget);
                changed = True;
        }
        if (old_vt->vt.cursor_color != new_vt->vt.cursor_color ||
            old_vt->vt.always_highlight != new_vt->vt.always_highlight) {
                XtpLog(XTP_LOG_INFO, "config",
                       "VT100 cursor changed color=%lu->%lu alwaysHighlight=%s->%s",
                       old_vt->vt.cursor_color, new_vt->vt.cursor_color,
                       old_vt->vt.always_highlight ? "true" : "false",
                       new_vt->vt.always_highlight ? "true" : "false");
                changed = True;
        }
        if (old_vt->vt.cursor_on_time != new_vt->vt.cursor_on_time ||
            old_vt->vt.cursor_off_time != new_vt->vt.cursor_off_time) {
                XtpLog(XTP_LOG_INFO, "config", "VT100 cursor timing on=%dms off=%dms",
                       new_vt->vt.cursor_on_time, new_vt->vt.cursor_off_time);
                RestartCursorBlink(new_vt);
        }
        if (old_vt->vt.scroll_bar != new_vt->vt.scroll_bar) {
                XtpLog(XTP_LOG_INFO, "scrollbar", "resource visibility %s -> %s",
                       old_vt->vt.scroll_bar ? "shown" : "hidden",
                       new_vt->vt.scroll_bar ? "shown" : "hidden");
                EnsureScrollbar(new_vt);
                new_vt->vt.suppress_grid_resize = True;
                if (new_vt->vt.scroll_bar)
                        XtManageChild(new_vt->vt.scrollbar);
                else
                        XtUnmanageChild(new_vt->vt.scrollbar);
                new_vt->vt.suppress_grid_resize = False;
                changed = True;
        }
        if (old_vt->vt.right_scroll_bar != new_vt->vt.right_scroll_bar) {
                XtpLog(XTP_LOG_INFO, "scrollbar", "resource side %s -> %s",
                       old_vt->vt.right_scroll_bar ? "right" : "left",
                       new_vt->vt.right_scroll_bar ? "right" : "left");
                changed = True;
        }
        if (old_vt->vt.select_to_clipboard != new_vt->vt.select_to_clipboard)
                XtpLog(XTP_LOG_INFO, "selection", "selectToClipboard=%s",
                       new_vt->vt.select_to_clipboard ? "true" : "false");
        if (changed)
                LayoutScrollbar(new_vt);
        if (changed) {
                new_vt->vt.frame_valid = False;
                new_vt->vt.last_cursor_visible = False;
        }
        return changed;
}

Boolean
XtpVtSelectFont(Widget widget, int slot)
{
        Vt100Rec *vt = AsVt(widget);
        XtpFontChanged changed;

        if (slot < 0 || slot >= XTP_FONT_SLOTS)
                return False;
        if (vt->vt.use_xft) {
                if (vt->vt.xft_fonts[slot] == NULL)
                        return False;
        } else if (LoadSlot(vt, slot) == NULL) {
                return False;
        }
        if (slot == vt->vt.current_font)
                return True;

        XtpLog(XTP_LOG_INFO, "font", "select slot=%d -> %d old-cell=%ux%u new-cell=%ux%u",
               vt->vt.current_font, slot, XtpVtCellWidth(widget), XtpVtCellHeight(widget),
               SlotWidth(vt, slot), SlotHeight(vt, slot));
        vt->vt.current_font = slot;
        vt->vt.frame_valid = False;
        vt->vt.last_cursor_visible = False;
        ReleaseGc(widget);
        CreateGc(widget);

        changed.slot = slot;
        changed.cell_width = XtpVtCellWidth(widget);
        changed.cell_height = XtpVtCellHeight(widget);
        XtCallCallbacks(widget, XtNfontChangedCallback, &changed);

        XtpVtRedraw(widget);
        return True;
}

static void
RelativeFont(Widget widget, int direction)
{
        Vt100Rec *vt = AsVt(widget);
        double current_size = vt->vt.use_xft ? vt->vt.xft_sizes[vt->vt.current_font]
                                             : (double)SlotWidth(vt, vt->vt.current_font) *
                                                   SlotHeight(vt, vt->vt.current_font);
        double best_delta = DBL_MAX;
        int best = -1;
        int slot;

        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                double size;
                double delta;

                if (vt->vt.use_xft) {
                        if (vt->vt.xft_fonts[slot] == NULL)
                                continue;
                } else if (LoadSlot(vt, slot) == NULL) {
                        continue;
                }
                size = vt->vt.use_xft ? vt->vt.xft_sizes[slot]
                                      : (double)SlotWidth(vt, slot) * SlotHeight(vt, slot);
                if ((direction > 0 && size <= current_size) ||
                    (direction < 0 && size >= current_size))
                        continue;
                delta = size > current_size ? size - current_size : current_size - size;
                if (delta < best_delta) {
                        best_delta = delta;
                        best = slot;
                }
        }

        if (best >= 0) {
                XtpLog(XTP_LOG_INFO, "font",
                       "relative direction=%s current-slot=%d selected-slot=%d delta=%.2f",
                       direction > 0 ? "larger" : "smaller", vt->vt.current_font, best, best_delta);
                (void)XtpVtSelectFont(widget, best);
        } else {
                XtpLog(XTP_LOG_WARNING, "font",
                       "relative direction=%s current-slot=%d no candidate",
                       direction > 0 ? "larger" : "smaller", vt->vt.current_font);
                XBell(XtDisplay(widget), 0);
        }
}

static Boolean
AcceptLocalKeyAction(Vt100Rec *vt, XEvent *event, XtpLocalKeyAction action)
{
        KeyActionIdentity *identity;
        unsigned int slot;

        if (event == NULL || event->type != KeyPress)
                return True;

        for (slot = 0; slot < XTP_RECENT_KEY_ACTIONS; ++slot) {
                identity = &vt->vt.recent_key_actions[slot];
                if (!identity->used || identity->serial != event->xkey.serial ||
                    identity->time != event->xkey.time ||
                    identity->keycode != event->xkey.keycode ||
                    identity->state != event->xkey.state || identity->action != action)
                        continue;
                if (!identity->duplicate_logged) {
                        XtpLog(XTP_LOG_DEBUG, "input",
                               "ignored duplicate local key action=%d serial=%lu time=%lu "
                               "keycode=%u state=0x%x",
                               action, event->xkey.serial, event->xkey.time, event->xkey.keycode,
                               event->xkey.state);
                        identity->duplicate_logged = True;
                }
                return False;
        }

        identity = &vt->vt.recent_key_actions[vt->vt.next_key_action];
        identity->used = True;
        identity->duplicate_logged = False;
        identity->serial = event->xkey.serial;
        identity->time = event->xkey.time;
        identity->keycode = event->xkey.keycode;
        identity->state = event->xkey.state;
        identity->action = action;
        vt->vt.next_key_action = (vt->vt.next_key_action + 1U) % XTP_RECENT_KEY_ACTIONS;
        return True;
}

static void
LargerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        (void)params;
        (void)num_params;
        if (!AcceptLocalKeyAction(AsVt(widget), event, XTP_KEY_ACTION_FONT_LARGER))
                return;
        XtpLog(
            XTP_LOG_INFO, "input",
            "action larger-vt-font event=%d serial=%lu synthetic=%s time=%lu keycode=%u state=0x%x",
            event != NULL ? event->type : 0, event != NULL ? event->xany.serial : 0,
            event != NULL && event->xany.send_event ? "true" : "false",
            event != NULL ? event->xkey.time : 0, event != NULL ? event->xkey.keycode : 0,
            event != NULL ? event->xkey.state : 0);
        RelativeFont(widget, 1);
}

static void
SmallerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        (void)params;
        (void)num_params;
        if (!AcceptLocalKeyAction(AsVt(widget), event, XTP_KEY_ACTION_FONT_SMALLER))
                return;
        XtpLog(XTP_LOG_INFO, "input",
               "action smaller-vt-font event=%d serial=%lu synthetic=%s time=%lu keycode=%u "
               "state=0x%x",
               event != NULL ? event->type : 0, event != NULL ? event->xany.serial : 0,
               event != NULL && event->xany.send_event ? "true" : "false",
               event != NULL ? event->xkey.time : 0, event != NULL ? event->xkey.keycode : 0,
               event != NULL ? event->xkey.state : 0);
        RelativeFont(widget, -1);
}

static void
SetRenderFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Boolean enabled = XtpVtUsingXft(widget);

        (void)event;
        if (*num_params == 0 || strcmp(params[0], "toggle") == 0) {
                enabled = !enabled;
        } else if (*num_params == 1 &&
                   (strcmp(params[0], "on") == 0 || strcmp(params[0], "true") == 0)) {
                enabled = True;
        } else if (*num_params == 1 &&
                   (strcmp(params[0], "off") == 0 || strcmp(params[0], "false") == 0)) {
                enabled = False;
        } else {
                XtpLog(XTP_LOG_WARNING, "font", "set-render-font expects on, off, or toggle");
                XBell(XtDisplay(widget), 0);
                return;
        }
        if (!XtpVtSetRenderFont(widget, enabled))
                XBell(XtDisplay(widget), 0);
}

static void
SetSelectAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Boolean enabled = XtpVtSelectToClipboard(widget);

        (void)event;
        if (*num_params == 0 || strcmp(params[0], "toggle") == 0) {
                enabled = !enabled;
        } else if (*num_params == 1 &&
                   (strcmp(params[0], "on") == 0 || strcmp(params[0], "true") == 0)) {
                enabled = True;
        } else if (*num_params == 1 &&
                   (strcmp(params[0], "off") == 0 || strcmp(params[0], "false") == 0)) {
                enabled = False;
        } else {
                XtpLog(XTP_LOG_WARNING, "selection", "set-select expects on, off, or toggle");
                XBell(XtDisplay(widget), 0);
                return;
        }
        XtpVtSetSelectToClipboard(widget, enabled);
}

static void
PopupMenuAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        XtpPopupMenu popup;

        if (*num_params != 1) {
                XBell(XtDisplay(widget), 0);
                return;
        }
        popup.name = params[0];
        popup.event = event;
        XtpLog(XTP_LOG_INFO, "menu", "action popup-menu name=%s", popup.name);
        XtCallCallbacks(widget, XtNpopupMenuCallback, &popup);
}

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

static XtpSelectionSource
ResolveSelectionSource(Vt100Rec *vt, const char *name)
{
        XtpSelectionSource source = {XTP_SELECTION_SOURCE_ATOM, None, -1};

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
        size_t need;
        size_t index;

        *consumed = 1;
        if (length == 0)
                return '?';
        if (bytes[0] < 0x80)
                return bytes[0];
        if (bytes[0] >= 0xc2 && bytes[0] <= 0xdf) {
                codepoint = bytes[0] & 0x1fU;
                need = 2;
        } else if (bytes[0] >= 0xe0 && bytes[0] <= 0xef) {
                codepoint = bytes[0] & 0x0fU;
                need = 3;
        } else if (bytes[0] >= 0xf0 && bytes[0] <= 0xf4) {
                codepoint = bytes[0] & 0x07U;
                need = 4;
        } else {
                return '?';
        }
        if (length < need)
                return '?';
        for (index = 1; index < need; ++index) {
                if ((bytes[index] & 0xc0U) != 0x80U)
                        return '?';
                codepoint = (codepoint << 6) | (bytes[index] & 0x3fU);
        }
        if ((need == 3 && codepoint < 0x800U) || (need == 4 && codepoint < 0x10000U) ||
            codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU))
                return '?';
        *consumed = need;
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
        Vt100Rec *vt = AsVt(widget);
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
        Vt100Rec *vt = AsVt(widget);
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
        int grid_x = x - TerminalX(vt);
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
HyperlinkAtPointer(Vt100Rec *vt, int x, int y, uint8_t **uri, size_t *length)
{
        uint16_t column;
        uint16_t row;

        *uri = NULL;
        *length = 0;
        return vt->vt.terminal != NULL && SelectionCell(vt, x, y, &column, &row) &&
               XtpTerminalHyperlinkAt(vt->vt.terminal, column, row, uri, length) == 0 &&
               *length != 0;
}

static Boolean
HyperlinkUriEqualsCell(Vt100Rec *vt, const XtpRenderCell *cell)
{
        uint8_t *uri = NULL;
        size_t length = 0;
        Boolean matches = False;

        if (!cell->hyperlink || vt->vt.hovered_hyperlink == NULL || vt->vt.terminal == NULL)
                return False;
        if (XtpTerminalHyperlinkAt(vt->vt.terminal, cell->column, cell->row, &uri, &length) == 0)
                matches =
                    SameUri(uri, length, vt->vt.hovered_hyperlink, vt->vt.hovered_hyperlink_length);
        free(uri);
        return matches;
}

static void
SetHoveredHyperlink(Vt100Rec *vt, int x, int y, unsigned int state)
{
        uint8_t *uri = NULL;
        size_t length = 0;
        Boolean found = False;

        if ((state & ShiftMask) != 0)
                found = HyperlinkAtPointer(vt, x, y, &uri, &length);
        if (!found) {
                free(uri);
                uri = NULL;
                length = 0;
        }
        if (SameUri(uri, length, vt->vt.hovered_hyperlink, vt->vt.hovered_hyperlink_length)) {
                free(uri);
                return;
        }
        free(vt->vt.hovered_hyperlink);
        vt->vt.hovered_hyperlink = uri;
        vt->vt.hovered_hyperlink_length = length;
        if (uri != NULL)
                XtpLogBytePreview(XTP_LOG_DEBUG, "hyperlink", "hover", uri, length);
        else
                XtpLog(XTP_LOG_DEBUG, "hyperlink", "hover cleared");
        if (XtIsRealized((Widget)vt) && vt->vt.terminal != NULL) {
                if (RenderTerminal(vt, True) != 0)
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

static void
HyperlinkEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
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

static void
HyperlinkStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
        uint8_t *uri = NULL;
        size_t length = 0;

        if (event == NULL || event->type != ButtonPress || event->xbutton.button != Button1)
                return;
        if (HyperlinkAtPointer(vt, event->xbutton.x, event->xbutton.y, &uri, &length)) {
                free(vt->vt.pressed_hyperlink);
                vt->vt.pressed_hyperlink = uri;
                vt->vt.pressed_hyperlink_length = length;
                SetHoveredHyperlink(vt, event->xbutton.x, event->xbutton.y,
                                    event->xbutton.state | ShiftMask);
                XtpLogBytePreview(XTP_LOG_DEBUG, "hyperlink", "press", uri, length);
                return;
        }
        free(uri);
        SelectStartAction(widget, event, params, num_params);
}

static Boolean
FinishHyperlinkPress(Vt100Rec *vt, const XButtonEvent *event)
{
        uint8_t *uri = NULL;
        size_t length = 0;
        Boolean matches;
        int opened;

        if (vt->vt.pressed_hyperlink == NULL || event->button != Button1)
                return False;
        matches = (event->state & ShiftMask) != 0 &&
                  HyperlinkAtPointer(vt, event->x, event->y, &uri, &length) &&
                  SameUri(uri, length, vt->vt.pressed_hyperlink, vt->vt.pressed_hyperlink_length);
        if (matches) {
                opened = OpenHttpUri(uri, length);
                if (opened == 0) {
                        XtpLogBytePreview(XTP_LOG_INFO, "hyperlink", "opened", uri, length);
                } else if (opened > 0) {
                        XtpLogBytePreview(XTP_LOG_INFO, "hyperlink", "blocked", uri, length);
                } else {
                        XtpLog(XTP_LOG_ERROR, "hyperlink", "cannot launch xdg-open");
                        XBell(XtDisplay((Widget)vt), 0);
                }
        }
        free(uri);
        free(vt->vt.pressed_hyperlink);
        vt->vt.pressed_hyperlink = NULL;
        vt->vt.pressed_hyperlink_length = 0;
        SetHoveredHyperlink(vt, event->x, event->y, event->state);
        return True;
}

static Boolean
SelectionCellClamped(Vt100Rec *vt, int x, int y, uint16_t *column, uint16_t *row)
{
        int grid_x = x - TerminalX(vt);
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
                    XtpVtCellWidth((Widget)vt), (uint32_t)TerminalX(vt), vt->core.height,
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
        UpdateScrollbar(vt);
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

static unsigned int
MouseModifiers(unsigned int state)
{
        unsigned int result = 0;

        if ((state & ShiftMask) != 0)
                result |= XTP_MOD_SHIFT;
        if ((state & ControlMask) != 0)
                result |= XTP_MOD_CONTROL;
        if ((state & Mod1Mask) != 0)
                result |= XTP_MOD_ALT;
        if ((state & Mod4Mask) != 0)
                result |= XTP_MOD_SUPER;
        if ((state & LockMask) != 0)
                result |= XTP_MOD_CAPS_LOCK;
        if ((state & Mod2Mask) != 0)
                result |= XTP_MOD_NUM_LOCK;
        return result;
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
        unsigned int padding_left = (unsigned int)TerminalX(vt);
        unsigned int padding_top = (unsigned int)vt->vt.internal_border;
        unsigned int occupied_width = padding_left + grid_width;
        unsigned int occupied_height = padding_top + grid_height;
        char encoded[128];
        size_t written = 0;
        XtpMouseEvent event = {
            .action = action,
            .button = button,
            .modifiers = MouseModifiers(state),
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

static void
SelectStartAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
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

static void
SelectExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
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
                    (uint32_t)vt->vt.columns, XtpVtCellWidth(widget), (uint32_t)TerminalX(vt),
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

static void
StartExtendAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
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
                XtpSelectionSource source = ResolveSelectionSource(vt, params[index]);
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

static void
SelectEndAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
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
        XtpSelectionSource sources[];
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
                XtpSelectionSource *source = &request->sources[request->source_index];

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
        Vt100Rec *vt = AsVt(widget);
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
                XtpSelectionSource source = ResolveSelectionSource(vt, params[index]);

                if (source.kind == XTP_SELECTION_SOURCE_ATOM && source.atom == None)
                        continue;
                request->sources[request->source_count++] = source;
        }
        RequestNextPasteSource(widget, request);
}

static void
InsertSelectionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
        Time time = XtLastTimestampProcessed(XtDisplay(widget));

        if (event != NULL && event->type == ButtonRelease &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE))
                return;
        if (event != NULL && event->type == KeyPress &&
            !AcceptLocalKeyAction(vt, event, XTP_KEY_ACTION_PASTE))
                return;
        if (event != NULL && (event->type == ButtonPress || event->type == ButtonRelease))
                time = event->xbutton.time;
        else if (event != NULL && event->type == KeyPress)
                time = event->xkey.time;
        RequestNamedPaste(widget, time, params, *num_params);
}

static void
MousePressAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);

        (void)params;
        (void)num_params;
        if (event != NULL && event->type == ButtonPress)
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS);
}

static void
MouseMotionAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);

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

static void
ScrollBackAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
        intptr_t rows = ScrollActionRows(vt, params, *num_params);

        if (event != NULL && event->type == KeyPress &&
            !AcceptLocalKeyAction(vt, event, XTP_KEY_ACTION_SCROLL_BACK))
                return;
        if (event != NULL && event->type == ButtonPress &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS)) {
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE);
                return;
        }
        (void)ScrollViewportBy(vt, -rows);
}

static void
ScrollForwardAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = AsVt(widget);
        intptr_t rows = ScrollActionRows(vt, params, *num_params);

        if (event != NULL && event->type == KeyPress &&
            !AcceptLocalKeyAction(vt, event, XTP_KEY_ACTION_SCROLL_FORWARD))
                return;
        if (event != NULL && event->type == ButtonPress &&
            ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_PRESS)) {
                (void)ReportMouseButton(vt, &event->xbutton, XTP_MOUSE_ACTION_RELEASE);
                return;
        }
        (void)ScrollViewportBy(vt, rows);
}

unsigned int
XtpVtCellWidth(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        return SlotWidth(vt, vt->vt.current_font);
}

unsigned int
XtpVtCellHeight(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        return SlotHeight(vt, vt->vt.current_font);
}

unsigned int
XtpVtColumns(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        return (unsigned int)vt->vt.columns;
}

unsigned int
XtpVtRows(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        return (unsigned int)vt->vt.rows;
}

Boolean
XtpVtFontSlotInfo(Widget widget, int slot, XtpFontSlotInfo *info)
{
        Vt100Rec *vt = AsVt(widget);
        if (info == NULL || slot < 0 || slot >= XTP_FONT_SLOTS)
                return False;
        if (vt->vt.use_xft)
                info->loaded = vt->vt.xft_fonts[slot] != NULL;
        else
                info->loaded = vt->vt.fonts[slot] != NULL;
        info->cell_width = info->loaded ? SlotWidth(vt, slot) : 0;
        info->cell_height = info->loaded ? SlotHeight(vt, slot) : 0;
        info->point_size = vt->vt.use_xft && info->loaded ? vt->vt.xft_sizes[slot] : 0.0;
        return True;
}

const char *
XtpVtRendererName(Widget widget)
{
        return AsVt(widget)->vt.use_xft ? "xft" : "xlib-bitmap";
}

Boolean
XtpVtUsingXft(Widget widget)
{
        return AsVt(widget)->vt.use_xft;
}

Boolean
XtpVtXftAvailable(Widget widget)
{
        return AsVt(widget)->vt.xft_fonts[0] != NULL;
}

Boolean
XtpVtSetRenderFont(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = AsVt(widget);
        XtpFontChanged changed;

        enabled = enabled ? True : False;
        if (enabled == vt->vt.use_xft)
                return True;
        if (enabled) {
                if (vt->vt.xft_fonts[vt->vt.current_font] == NULL) {
                        if (vt->vt.xft_fonts[0] == NULL)
                                return False;
                        vt->vt.current_font = 0;
                }
        } else if (vt->vt.fonts[vt->vt.current_font] == NULL) {
                vt->vt.current_font = 0;
        }

        XtpLog(XTP_LOG_INFO, "font", "renderer toggle %s -> %s slot=%d",
               vt->vt.use_xft ? "xft" : "xlib-bitmap", enabled ? "xft" : "xlib-bitmap",
               vt->vt.current_font);
        vt->vt.use_xft = enabled;
        vt->vt.frame_valid = False;
        vt->vt.last_cursor_visible = False;
        ReleaseGc(widget);
        CreateGc(widget);

        changed.slot = vt->vt.current_font;
        changed.cell_width = XtpVtCellWidth(widget);
        changed.cell_height = XtpVtCellHeight(widget);
        XtCallCallbacks(widget, XtNfontChangedCallback, &changed);
        XtpVtRedraw(widget);
        return True;
}

Dimension
XtpVtNaturalWidth(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);
        unsigned long value =
            (unsigned long)vt->vt.columns * XtpVtCellWidth(widget) + 2UL * vt->vt.internal_border;

        if (vt->vt.scroll_bar)
                value += ScrollbarTotalWidth(vt);

        return value <= USHRT_MAX ? (Dimension)value : (Dimension)USHRT_MAX;
}

Dimension
XtpVtNaturalHeight(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);
        unsigned long value =
            (unsigned long)vt->vt.rows * XtpVtCellHeight(widget) + 2UL * vt->vt.internal_border;

        return value <= USHRT_MAX ? (Dimension)value : (Dimension)USHRT_MAX;
}

Boolean
XtpVtScrollbarVisible(Widget widget)
{
        return AsVt(widget)->vt.scroll_bar;
}

void
XtpVtSetScrollbar(Widget widget, Boolean visible)
{
        Vt100Rec *vt = AsVt(widget);

        visible = visible ? True : False;
        if (vt->vt.scroll_bar == visible)
                return;
        EnsureScrollbar(vt);
        vt->vt.scroll_bar = visible;
        vt->vt.suppress_grid_resize = True;
        if (visible)
                XtManageChild(vt->vt.scrollbar);
        else
                XtUnmanageChild(vt->vt.scrollbar);
        vt->vt.suppress_grid_resize = False;
        LayoutScrollbar(vt);
        XtpLog(XTP_LOG_INFO, "scrollbar", "visibility=%s side=%s", visible ? "shown" : "hidden",
               vt->vt.right_scroll_bar ? "right" : "left");
}

Boolean
XtpVtScrollKey(Widget widget)
{
        return AsVt(widget)->vt.scroll_key;
}

void
XtpVtSetScrollKey(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = AsVt(widget);

        vt->vt.scroll_key = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "scrollback", "scrollKey=%s", vt->vt.scroll_key ? "true" : "false");
}

Boolean
XtpVtScrollTtyOutput(Widget widget)
{
        return AsVt(widget)->vt.scroll_tty_output;
}

Boolean
XtpVtSelectToClipboard(Widget widget)
{
        return AsVt(widget)->vt.select_to_clipboard;
}

void
XtpVtSetSelectToClipboard(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = AsVt(widget);

        vt->vt.select_to_clipboard = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "selection", "selectToClipboard=%s SELECT=%s",
               vt->vt.select_to_clipboard ? "true" : "false",
               vt->vt.select_to_clipboard ? "CLIPBOARD" : "PRIMARY");
}

void
XtpVtSetScrollTtyOutput(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = AsVt(widget);

        vt->vt.scroll_tty_output = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "scrollback", "scrollTtyOutput=%s",
               vt->vt.scroll_tty_output ? "true" : "false");
}

void
XtpVtScrollOnKeypress(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        if (vt->vt.scroll_key && ScrollViewportToBottom(vt))
                XtpVtUpdate(widget);
}

void
XtpVtSetTerminal(Widget widget, XtpTerminal *terminal)
{
        Vt100Rec *vt = AsVt(widget);

        vt->vt.terminal = terminal;
        if (terminal != NULL && XtpTerminalSetCursorBlinkDefault(
                                    terminal, CursorBlinkDefault(vt->vt.cursor_blink_policy)) != 0)
                XtpLog(XTP_LOG_ERROR, "render", "cannot apply cursorBlink=%s",
                       vt->vt.cursor_blink_name);
        if (terminal != NULL &&
            XtpTerminalSetScrollbackLines(terminal, (size_t)vt->vt.save_lines) != 0)
                XtpLog(XTP_LOG_ERROR, "scrollback", "cannot set history limit=%d",
                       vt->vt.save_lines);
        if (terminal != NULL && XtpTerminalSetCharClass(terminal, vt->vt.char_class) != 0)
                XtpLog(XTP_LOG_ERROR, "selection", "cannot apply charClass=%s",
                       vt->vt.char_class != NULL ? vt->vt.char_class : "(default)");
        EnsureScrollbar(vt);
        if (vt->vt.scroll_bar)
                XtManageChild(vt->vt.scrollbar);
        LayoutScrollbar(vt);
        UpdateScrollbar(vt);
        XtpLog(XTP_LOG_INFO, "terminal", "bound terminal=%s", terminal != NULL ? "yes" : "no");
        XtpVtRedraw(widget);
}

void
XtpVtSetFocus(Widget widget, Boolean focused)
{
        Vt100Rec *vt = AsVt(widget);
        char encoded[8];
        size_t written = 0;
        Boolean repaint_cursor;

        if (vt->vt.focused == focused)
                return;
        if (vt->vt.terminal != NULL &&
            XtpTerminalEncodeFocus(vt->vt.terminal, focused != False, encoded, sizeof(encoded),
                                   &written) != 0) {
                XtpLog(XTP_LOG_ERROR, "input", "focus encoding failed focus=%s",
                       focused ? "in" : "out");
        } else if (written != 0) {
                XtpEncodedInput input = {(const uint8_t *)encoded, written};

                XtCallCallbacks(widget, XtNinputCallback, &input);
        }
        repaint_cursor = XtIsRealized(widget) && vt->vt.frame_valid &&
                         vt->vt.cursor_protocol_visible &&
                         vt->vt.last_cursor_column < vt->vt.frame_columns &&
                         vt->vt.last_cursor_row < vt->vt.frame_rows;
        if (vt->vt.last_cursor_visible)
                EraseLastCursor(vt);
        StopCursorBlink(vt);
        vt->vt.cursor_blink_on = True;
        vt->vt.focused = focused;
        XtpLog(XTP_LOG_INFO, "render",
               "cursor focus=%s requested-shape=%d blink-requested=%s blink-effective=%s",
               focused ? "in" : "out", vt->vt.last_cursor_shape,
               vt->vt.cursor_blink_requested ? "true" : "false",
               vt->vt.cursor_blinking ? "true" : "false");
        if (repaint_cursor) {
                unsigned int column = vt->vt.last_cursor_column;
                unsigned int row = vt->vt.last_cursor_row;

                vt->vt.cursor_cell_seen = False;
                vt->vt.cursor_text_length = 0;
                DrawCursor(vt, True, column, row, vt->vt.last_cursor_shape);
                XtpLog(XTP_LOG_DEBUG, "render", "cursor-only repaint column=%u row=%u", column,
                       row);
                XFlush(XtDisplay(widget));
        }
        ScheduleCursorBlink(vt);
}

void
XtpVtUpdate(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        if (vt->vt.viewport_update_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.viewport_update_timer);
                vt->vt.viewport_update_timer = (XtIntervalId)0;
                vt->vt.viewport_updates_coalesced = 0;
        }
        if (!XtIsRealized(widget) || vt->vt.terminal == NULL)
                return;
        XtpLog(XTP_LOG_DEBUG, "render", "dirty update requested");
        if (RenderTerminal(vt, False) != 0)
                XtpLog(XTP_LOG_ERROR, "render", "dirty update failed");
}

void
XtpVtRedraw(Widget widget)
{
        Vt100Rec *vt = AsVt(widget);

        if (!XtIsRealized(widget))
                return;
        XtpLog(XTP_LOG_DEBUG, "render", "redraw requested cache=%s",
               vt->vt.frame_valid ? "valid" : "invalid");
        if (vt->vt.frame_valid)
                RepaintCached(vt, NULL);
        else if (RenderTerminal(vt, True) != 0)
                Placeholder(vt);
}
