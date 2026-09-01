#include "vt_widgetP.h"

#include "diagnostics.h"
#include "font_chain.h"
#include "font_metrics.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xmu/Converters.h>

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args);
static void Realize(Widget widget, XtValueMask *value_mask, XSetWindowAttributes *attributes);
static void Destroy(Widget widget);
static void ResizeWidget(Widget widget);
static Boolean SetValues(Widget current, Widget request, Widget new_widget, ArgList args,
                         Cardinal *num_args);
static void LargerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SmallerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SetRenderFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void SetSelectAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void ReportFontRoutingAction(Widget widget, XEvent *event, String *params,
                                    Cardinal *num_params);
static void PopupMenuAction(Widget widget, XEvent *event, String *params, Cardinal *num_params);
static void ClassInitialize(void);

static XtActionsRec actions[] = {
    {"larger-vt-font", LargerFontAction},
    {"smaller-vt-font", SmallerFontAction},
    {"set-render-font", SetRenderFontAction},
    {"set-select", SetSelectAction},
    {"report-font-routing", ReportFontRoutingAction},
    {"popup-menu", PopupMenuAction},
    {"scroll-back", VtScrollBackAction},
    {"scroll-forw", VtScrollForwardAction},
    {"select-start", VtSelectStartAction},
    {"select-extend", VtSelectExtendAction},
    {"select-end", VtSelectEndAction},
    {"start-extend", VtStartExtendAction},
    {"insert-selection", VtInsertSelectionAction},
    {"mouse-press", VtMousePressAction},
    {"mouse-motion", VtMouseMotionAction},
    {"hyperlink-start", VtHyperlinkStartAction},
};

/*
 * Implemented subset of patch-411 VTInitTranslations(). Keep this table and
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
    {"faceNameEmoji", "FaceNameEmoji", XtRString, sizeof(String), OFFSET(face_name_emoji),
     XtRString, NULL},
    {"faceNameHan", "FaceNameHan", XtRString, sizeof(String), OFFSET(face_name_han), XtRString,
     NULL},
    {"boldFont", "BoldFont", XtRString, sizeof(String), OFFSET(bold_font_name), XtRString, NULL},
    {"wideBoldFont", "WideBoldFont", XtRString, sizeof(String), OFFSET(wide_bold_font_name),
     XtRString, NULL},
    {"emojiPresentation", "EmojiPresentation", XtRString, sizeof(String),
     OFFSET(emoji_presentation_name), XtRString, (XtPointer) "unicode"},
    {"graphemeWidth", "GraphemeWidth", XtRString, sizeof(String), OFFSET(grapheme_width_name),
     XtRString, (XtPointer) "legacy"},
    {"colorGlyphs", "ColorGlyphs", XtRBoolean, sizeof(Boolean), OFFSET(color_glyphs), XtRImmediate,
     (XtPointer)True},
    {"systemFallback", "SystemFallback", XtRBoolean, sizeof(Boolean), OFFSET(system_fallback),
     XtRImmediate, (XtPointer)True},
    {"reportFontRouting", "ReportFontRouting", XtRBoolean, sizeof(Boolean),
     OFFSET(report_font_routing), XtRImmediate, (XtPointer)False},
    {"limitFontsets", "LimitFontsets", XtRInt, sizeof(int), OFFSET(limit_fontsets), XtRImmediate,
     (XtPointer)50},
    {"limitFontHeight", "LimitFontHeight", XtRInt, sizeof(int), OFFSET(limit_fontheight),
     XtRImmediate, (XtPointer)10},
    {"limitFontWidth", "LimitFontWidth", XtRInt, sizeof(int), OFFSET(limit_fontwidth), XtRImmediate,
     (XtPointer)10},
    {"fallbackFace1", "FallbackFace1", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[0]), XtRString, NULL},
    {"fallbackFace2", "FallbackFace2", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[1]), XtRString, NULL},
    {"fallbackFace3", "FallbackFace3", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[2]), XtRString, NULL},
    {"fallbackFace4", "FallbackFace4", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[3]), XtRString, NULL},
    {"fallbackFace5", "FallbackFace5", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[4]), XtRString, NULL},
    {"fallbackFace6", "FallbackFace6", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[5]), XtRString, NULL},
    {"fallbackFace7", "FallbackFace7", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[6]), XtRString, NULL},
    {"fallbackFace8", "FallbackFace8", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[7]), XtRString, NULL},
    {"fallbackFace9", "FallbackFace9", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[8]), XtRString, NULL},
    {"fallbackFace10", "FallbackFace10", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[9]), XtRString, NULL},
    {"fallbackFace11", "FallbackFace11", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[10]), XtRString, NULL},
    {"fallbackFace12", "FallbackFace12", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[11]), XtRString, NULL},
    {"fallbackFace13", "FallbackFace13", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[12]), XtRString, NULL},
    {"fallbackFace14", "FallbackFace14", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[13]), XtRString, NULL},
    {"fallbackFace15", "FallbackFace15", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[14]), XtRString, NULL},
    {"fallbackFace16", "FallbackFace16", XtRString, sizeof(String),
     XtOffsetOf(Vt100Rec, vt.fallback_face_names[15]), XtRString, NULL},
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
    {"backgroundOpacity", "BackgroundOpacity", XtRString, sizeof(String),
     OFFSET(background_opacity_name), XtRString, (XtPointer) "1.0"},
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
    {XtNreverseVideo, XtCReverseVideo, XtRBoolean, sizeof(Boolean), OFFSET(reverse_video),
     XtRImmediate, (XtPointer)False},
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
        Realize,
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
        VtRedisplay,
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

static void
Realize(Widget widget, XtValueMask *value_mask, XSetWindowAttributes *attributes)
{
        WidgetClass superclass = vt100ClassRec.core_class.superclass;

        (*superclass->core_class.realize)(widget, value_mask, attributes);
        VtInitializeInput(VtAsRecord(widget));
}

Vt100Rec *
VtAsRecord(Widget widget)
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

unsigned int
VtSlotWidth(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft) {
                unsigned int width = vt->vt.xft_cell_widths[slot];

                return width != 0 ? width : XftFontWidth(vt->vt.xft_fonts[slot]);
        }
        return FontWidth(vt->vt.fonts[slot]);
}

unsigned int
VtSlotHeight(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft)
                return XftFontHeight(vt->vt.xft_fonts[slot]);
        return FontHeight(vt->vt.fonts[slot]);
}

int
VtSlotAscent(const Vt100Rec *vt, int slot)
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

static CursorBlinkPolicy
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

static XtpEmojiPolicy
ParseEmojiPolicy(const char *value)
{
        if (value == NULL || strcasecmp(value, "unicode") == 0)
                return XTP_EMOJI_POLICY_UNICODE;
        if (strcasecmp(value, "text") == 0)
                return XTP_EMOJI_POLICY_TEXT;
        if (strcasecmp(value, "emoji") == 0)
                return XTP_EMOJI_POLICY_EMOJI;
        XtpLog(XTP_LOG_ERROR, "config",
               "invalid emojiPresentation=%s; using unicode (expected unicode, text, or emoji)",
               value);
        return XTP_EMOJI_POLICY_UNICODE;
}

static Boolean
ParseGraphemeWidth(const char *value)
{
        if (value == NULL || strcasecmp(value, "legacy") == 0)
                return False;
        if (strcasecmp(value, "unicode") == 0)
                return True;
        XtpLog(XTP_LOG_ERROR, "config",
               "invalid graphemeWidth=%s; using legacy (expected legacy or unicode)", value);
        return False;
}

static Boolean
CursorBlinkDefault(CursorBlinkPolicy policy)
{
        return policy == XTP_CURSOR_BLINK_DEFAULT_TRUE || policy == XTP_CURSOR_BLINK_ALWAYS;
}

Boolean
VtEffectiveCursorBlink(CursorBlinkPolicy policy, Boolean requested)
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

/*
 * Xterm treats a size embedded in the first faceName item as the Default
 * menu's faceSize, with precedence over the separate faceSize resource.  It
 * removes the field before constructing the Xft pattern so derived menu slots
 * can supply their own sizes.  Preserve that slightly surprising resource
 * interaction for zero-option compatibility with existing XTerm resources.
 */
static Boolean
TrimFaceSize(char *face, double *size)
{
        char *field;
        char *end;
        char *tail;
        Boolean valid;
        double parsed;

        if (!Nonempty(face))
                return False;
        field = strstr(face, ":size=");
        if (field != NULL)
                ++field;
        else if (strncmp(face, "size=", 5) == 0)
                field = face;
        else
                return False;

        tail = strchr(field, ':');
        if (tail != NULL)
                *tail = '\0';
        parsed = strtod(field + 5, &end);
        valid = end != field + 5 && *end == '\0';
        if (tail != NULL)
                *tail = ':';

        if (tail != NULL)
                memmove(field, tail + 1, strlen(tail + 1) + 1);
        else if (field == face)
                *field = '\0';
        else
                field[-1] = '\0';

        if (!valid)
                return False;
        if (size != NULL)
                *size = parsed;
        return True;
}

Dimension
VtScrollbarTotalWidth(Vt100Rec *vt)
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

void
VtUpdateScrollbar(Vt100Rec *vt)
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

Boolean
VtScrollViewportBy(Vt100Rec *vt, intptr_t rows)
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
        (void)VtScrollViewportBy(vt, rows);
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
        Arg args[6];

        if (vt->vt.scrollbar != NULL)
                return;
        XtSetArg(args[0], XtNorientation, XtorientVertical);
        XtSetArg(args[1], XtNborderWidth, vt->vt.scroll_bar_border);
        XtSetArg(args[2], XtNheight, vt->core.height);
        XtSetArg(args[3], XtNforeground, VtOpaquePixel(vt, vt->vt.foreground));
        XtSetArg(args[4], XtNbackground, vt->core.background_pixel);
        XtSetArg(args[5], XtNborderColor, VtOpaquePixel(vt, vt->core.border_pixel));
        vt->vt.scrollbar =
            XtCreateWidget("scrollbar", scrollbarWidgetClass, (Widget)vt, args, XtNumber(args));
        XtAddCallback(vt->vt.scrollbar, XtNscrollProc, ScrollbarScroll, vt);
        XtAddCallback(vt->vt.scrollbar, XtNjumpProc, ScrollbarJump, vt);
        VtUpdateScrollbar(vt);
}

static void
ReleaseGc(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        if (vt->vt.gc != NULL) {
                XFreeGC(XtDisplay(widget), vt->vt.gc);
                vt->vt.gc = NULL;
        }
}

static void
CreateGc(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);
        XFontStruct *font = vt->vt.fonts[vt->vt.current_font];
        XGCValues values;
        Drawable drawable;
        Pixmap pixmap = None;
        XtGCMask mask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;

        if (font == NULL)
                font = vt->vt.initial_font;
        values.foreground = vt->vt.foreground;
        values.background = vt->core.background_pixel;
        values.font = font->fid;
        values.graphics_exposures = False;
        if (XtIsRealized(widget)) {
                drawable = XtWindow(widget);
        } else {
                pixmap = XCreatePixmap(XtDisplay(widget), RootWindowOfScreen(XtScreen(widget)), 1,
                                       1, vt->core.depth);
                drawable = pixmap;
        }
        vt->vt.gc = XCreateGC(XtDisplay(widget), drawable, mask, &values);
        if (pixmap != None)
                XFreePixmap(XtDisplay(widget), pixmap);
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

static unsigned int
XftStyleIndex(Boolean bold, Boolean italic)
{
        return (bold ? 1U : 0U) | (italic ? 2U : 0U);
}

static const char *
XftStyleName(Boolean bold, Boolean italic)
{
        if (bold && italic)
                return "bold-italic";
        if (bold)
                return "bold";
        if (italic)
                return "italic";
        return "normal";
}

static void
LogXftResolved(Vt100Rec *vt, const char *role, int slot, const char *style, int entry,
               const char *request, XftFont *font)
{
        FcChar8 *file = NULL;
        int index = 0;

        if (font == NULL || font->pattern == NULL) {
                XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot, style, entry,
                                         request, NULL, "active", vt->vt.font_generation);
                return;
        }
        (void)FcPatternGetString(font->pattern, FC_FILE, 0, &file);
        (void)FcPatternGetInteger(font->pattern, FC_INDEX, 0, &index);
        XtpLog(XTP_LOG_INFO, "font",
               "resolved Xft role=%s slot=%d style=%s entry=%d request=%s file=%s index=%d", role,
               slot, style, entry, request != NULL ? request : "(unset)",
               file != NULL ? (const char *)file : "(unknown)", index);
        XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot, style, entry, request,
                                 font->pattern, "active", vt->vt.font_generation);
}

static void
SetXftStyle(FcPattern *pattern, Boolean bold, Boolean italic)
{
        FcPatternDel(pattern, FC_WEIGHT);
        FcPatternDel(pattern, FC_SLANT);
        FcPatternAddInteger(pattern, FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
}

static XftFont *
OpenXftFont(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic)
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
        SetXftStyle(pattern, bold, italic);
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(XtDisplay((Widget)vt), XScreenNumberOfScreen(XtScreen((Widget)vt)),
                             pattern);
        match = FcFontMatch(NULL, pattern, &result);
        FcPatternDestroy(pattern);
        font = match != NULL ? XftFontOpenPattern(XtDisplay((Widget)vt), match) : NULL;
        return font;
}

static FcPattern *
ResolveXftPattern(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic)
{
        FcPattern *pattern;
        FcPattern *match;
        FcResult result;

        if (!Nonempty(face))
                return NULL;
        pattern = FcNameParse((const FcChar8 *)face);
        if (pattern == NULL)
                return NULL;
        FcPatternDel(pattern, FC_SIZE);
        FcPatternDel(pattern, FC_PIXEL_SIZE);
        FcPatternAddDouble(pattern, FC_SIZE, size);
        SetXftStyle(pattern, bold, italic);
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(XtDisplay((Widget)vt), XScreenNumberOfScreen(XtScreen((Widget)vt)),
                             pattern);
        match = FcFontMatch(NULL, pattern, &result);
        FcPatternDestroy(pattern);
        return match;
}

static Boolean
ScaleXftPatternSize(FcPattern *pattern, double scale)
{
        double point_size;
        double pixel_size;
        Boolean changed = False;

        if (pattern == NULL || !(scale > 0.0))
                return False;
        if (FcPatternGetDouble(pattern, FC_SIZE, 0, &point_size) == FcResultMatch) {
                FcPatternDel(pattern, FC_SIZE);
                changed = FcPatternAddDouble(pattern, FC_SIZE, point_size * scale) || changed;
        }
        if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &pixel_size) == FcResultMatch) {
                FcPatternDel(pattern, FC_PIXEL_SIZE);
                changed = FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixel_size * scale) || changed;
        }
        return changed;
}

XftFont *
VtOpenNormalizedXftPattern(Vt100Rec *vt, FcPattern *pattern, int slot, double *scale_out)
{
        XftFont *primary;
        XftFont *probe;
        XftFont *normalized;
        FcPattern *probe_pattern;
        unsigned int target_height;
        unsigned int source_height;
        double scale = 1.0;

        if (scale_out != NULL)
                *scale_out = 1.0;
        if (pattern == NULL)
                return NULL;
        probe_pattern = FcPatternDuplicate(pattern);
        probe =
            probe_pattern != NULL ? XftFontOpenPattern(XtDisplay((Widget)vt), probe_pattern) : NULL;
        if (probe == NULL) {
                /* Xft consumes a pattern only when opening succeeds. */
                if (probe_pattern != NULL)
                        FcPatternDestroy(probe_pattern);
                FcPatternDestroy(pattern);
                return NULL;
        }
        primary = slot >= 0 && slot < XTP_FONT_SLOTS ? vt->vt.xft_fonts[slot] : NULL;
        target_height = XftFontHeight(primary);
        source_height = XftFontHeight(probe);
        if (primary == NULL || target_height == 0 || source_height == 0 ||
            target_height == source_height) {
                FcPatternDestroy(pattern);
                return probe;
        }
        scale = XtpFontHeightScale(target_height, source_height);
        if (!ScaleXftPatternSize(pattern, scale)) {
                FcPatternDestroy(pattern);
                return probe;
        }
        normalized = XftFontOpenPattern(XtDisplay((Widget)vt), pattern);
        if (normalized == NULL) {
                /* Xft consumes a pattern only when opening succeeds. */
                FcPatternDestroy(pattern);
                return probe;
        }
        XtpLog(XTP_LOG_DEBUG, "font",
               "normalized Xft slot=%d target-height=%u source-height=%u scale=%.9f "
               "result-height=%u",
               slot, target_height, source_height, scale, XftFontHeight(normalized));
        XftFontClose(XtDisplay((Widget)vt), probe);
        if (scale_out != NULL)
                *scale_out = scale;
        return normalized;
}

static XftFont *
OpenNormalizedXftFont(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic,
                      int slot, double *scale_out)
{
        return VtOpenNormalizedXftPattern(vt, ResolveXftPattern(vt, face, size, bold, italic), slot,
                                          scale_out);
}

static const char *
XftPatternFamily(const FcPattern *pattern)
{
        FcChar8 *family = NULL;

        if (pattern == NULL || FcPatternGetString(pattern, FC_FAMILY, 0, &family) != FcResultMatch)
                return "(unknown)";
        return (const char *)family;
}

static Boolean
SameXftFamily(const FcPattern *left, const FcPattern *right)
{
        const char *left_family = XftPatternFamily(left);
        const char *right_family = XftPatternFamily(right);

        return strcmp(left_family, "(unknown)") != 0 && strcmp(right_family, "(unknown)") != 0 &&
               FcStrCmpIgnoreCase((const FcChar8 *)left_family, (const FcChar8 *)right_family) == 0;
}

static void
LoadXftBoldOverride(Vt100Rec *vt, int slot, const char *role, const char *face, double size,
                    XftFont *normal, XftFont **bold_font)
{
        XftFont *font;

        if (!Nonempty(face) || normal == NULL || bold_font == NULL)
                return;
        font = OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
        if (font == NULL) {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft boldFont role=%s slot=%d face=%s", role,
                       slot, face);
                return;
        }
        if (*bold_font != NULL)
                XftFontClose(XtDisplay((Widget)vt), *bold_font);
        *bold_font = font;
        LogXftResolved(vt, role, slot, "bold", 1, face, font);
        if (!SameXftFamily(normal->pattern, font->pattern)) {
                XtpLog(XTP_LOG_WARNING, "font",
                       "FR-STYLEFAMILY slot=%s style=bold roleFamily=%s resolvedFamily=%s", role,
                       XftPatternFamily(normal->pattern), XftPatternFamily(font->pattern));
                XtpFontRoutingReportStyleFamily(vt->vt.font_routing_report, role, "bold",
                                                XftPatternFamily(normal->pattern),
                                                XftPatternFamily(font->pattern));
        }
}

static unsigned int
PackedXftCellWidth(Vt100Rec *vt, XftFont *font)
{
        unsigned int width = 0;
        FcChar32 codepoint;

        if (font == NULL)
                return 1U;
        for (codepoint = 32; codepoint < 256; ++codepoint) {
                XGlyphInfo extents;

                if (!XftCharExists(XtDisplay((Widget)vt), font, codepoint))
                        continue;
                XftTextExtents32(XtDisplay((Widget)vt), font, &codepoint, 1, &extents);
                if (extents.xOff > 0 && (unsigned int)extents.xOff > width &&
                    extents.xOff <= font->max_advance_width)
                        width = (unsigned int)extents.xOff;
        }
        /* Preserve the packed fractional pixel without trusting specialist glyph maxima. */
        if (width != 0 && font->max_advance_width > (int)width &&
            (font->max_advance_width == (int)width + 1 ||
             font->max_advance_width > (int)(2U * width)))
                ++width;
        return width != 0 ? width : XftFontWidth(font);
}

static XftFont *
LoadXftSlot(Vt100Rec *vt, int slot, const char *face, double size)
{
        if (slot < 0 || slot >= XTP_FONT_SLOTS)
                return NULL;
        if (vt->vt.xft_fonts[slot] != NULL)
                return vt->vt.xft_fonts[slot];
        vt->vt.xft_fonts[slot] = OpenXftFont(vt, face, size, False, False);
        if (vt->vt.xft_fonts[slot] != NULL) {
                vt->vt.xft_bold_fonts[slot] =
                    OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
                vt->vt.xft_italic_fonts[slot] =
                    OpenNormalizedXftFont(vt, face, size, False, True, slot, NULL);
                vt->vt.xft_bold_italic_fonts[slot] =
                    OpenNormalizedXftFont(vt, face, size, True, True, slot, NULL);
                vt->vt.xft_sizes[slot] = size;
                vt->vt.xft_cell_widths[slot] = PackedXftCellWidth(vt, vt->vt.xft_fonts[slot]);
                LogXftResolved(vt, "primary", slot, "normal", 1, face, vt->vt.xft_fonts[slot]);
                LogXftResolved(vt, "primary", slot, "bold", 1, face, vt->vt.xft_bold_fonts[slot]);
                LogXftResolved(vt, "primary", slot, "italic", 1, face,
                               vt->vt.xft_italic_fonts[slot]);
                LogXftResolved(vt, "primary", slot, "bold-italic", 1, face,
                               vt->vt.xft_bold_italic_fonts[slot]);
                XtpLog(XTP_LOG_INFO, "font",
                       "loaded Xft slot=%d face=%s points=%.2f cell=%ux%u ascent=%d "
                       "bold=%s italic=%s bold-italic=%s",
                       slot, face, size, vt->vt.xft_cell_widths[slot],
                       XftFontHeight(vt->vt.xft_fonts[slot]), vt->vt.xft_fonts[slot]->ascent,
                       vt->vt.xft_bold_fonts[slot] != NULL ? "yes" : "fallback",
                       vt->vt.xft_italic_fonts[slot] != NULL ? "yes" : "fallback",
                       vt->vt.xft_bold_italic_fonts[slot] != NULL ? "yes" : "fallback");
        } else {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft slot=%d face=%s points=%.2f", slot,
                       face, size);
                LogXftResolved(vt, "primary", slot, "normal", 1, face, NULL);
        }
        return vt->vt.xft_fonts[slot];
}

static void
LoadXftRoleSlot(Vt100Rec *vt, int slot, const char *role, const char *face, double size,
                XftFont **fonts, XftFont **bold_fonts, XftFont **italic_fonts,
                XftFont **bold_italic_fonts)
{
        double cell_scale = 1.0;

        if (slot < 0 || slot >= XTP_FONT_SLOTS || !Nonempty(face) || vt->vt.xft_fonts[slot] == NULL)
                return;
        fonts[slot] = OpenNormalizedXftFont(vt, face, size, False, False, slot, &cell_scale);
        if (fonts[slot] == NULL) {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft role=%s slot=%d face=%s points=%.2f",
                       role, slot, face, size);
                return;
        }
        bold_fonts[slot] = OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
        italic_fonts[slot] = OpenNormalizedXftFont(vt, face, size, False, True, slot, NULL);
        bold_italic_fonts[slot] = OpenNormalizedXftFont(vt, face, size, True, True, slot, NULL);
        LogXftResolved(vt, role, slot, "normal", 1, face, fonts[slot]);
        LogXftResolved(vt, role, slot, "bold", 1, face, bold_fonts[slot]);
        LogXftResolved(vt, role, slot, "italic", 1, face, italic_fonts[slot]);
        LogXftResolved(vt, role, slot, "bold-italic", 1, face, bold_italic_fonts[slot]);
        XtpLog(XTP_LOG_INFO, "font",
               "loaded Xft role=%s slot=%d face=%s points=%.2f glyph-box=%ux%u ascent=%d "
               "cell-scale=%.3f bold=%s italic=%s bold-italic=%s",
               role, slot, face, size, XftFontWidth(fonts[slot]), XftFontHeight(fonts[slot]),
               fonts[slot]->ascent, cell_scale, bold_fonts[slot] != NULL ? "yes" : "fallback",
               italic_fonts[slot] != NULL ? "yes" : "fallback",
               bold_italic_fonts[slot] != NULL ? "yes" : "fallback");
}

static Boolean
SameFontPattern(const FcPattern *left, const FcPattern *right)
{
        FcChar8 *left_file;
        FcChar8 *right_file;
        FcChar8 *left_variations = NULL;
        FcChar8 *right_variations = NULL;
        int left_index = 0;
        int right_index = 0;
        FcResult left_variation_result;
        FcResult right_variation_result;

        if (left == NULL || right == NULL ||
            FcPatternGetString(left, FC_FILE, 0, &left_file) != FcResultMatch ||
            FcPatternGetString(right, FC_FILE, 0, &right_file) != FcResultMatch)
                return False;
        (void)FcPatternGetInteger(left, FC_INDEX, 0, &left_index);
        (void)FcPatternGetInteger(right, FC_INDEX, 0, &right_index);
        left_variation_result = FcPatternGetString(left, FC_FONT_VARIATIONS, 0, &left_variations);
        right_variation_result =
            FcPatternGetString(right, FC_FONT_VARIATIONS, 0, &right_variations);
        if (left_index != right_index ||
            strcmp((const char *)left_file, (const char *)right_file) != 0 ||
            (left_variation_result == FcResultMatch) != (right_variation_result == FcResultMatch))
                return False;
        return left_variation_result != FcResultMatch ||
               strcmp((const char *)left_variations, (const char *)right_variations) == 0;
}

static void
ResolveNamedFallbackRoles(Vt100Rec *vt, double size, Boolean enabled[XTP_FALLBACK_FACE_COUNT])
{
        FcPattern *roles[XTP_FALLBACK_FACE_COUNT][XTP_XFT_STYLE_COUNT] = {{NULL}};
        int fallback;

        memset(enabled, 0, sizeof(Boolean) * XTP_FALLBACK_FACE_COUNT);
        for (fallback = 0; fallback < XTP_FALLBACK_FACE_COUNT; ++fallback) {
                const char *face = vt->vt.fallback_face_names[fallback];
                unsigned int style;
                Boolean valid = True;
                int earlier;

                if (!Nonempty(face))
                        continue;
                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        roles[fallback][style] =
                            ResolveXftPattern(vt, face, size, (style & 1U) != 0, (style & 2U) != 0);
                        if (roles[fallback][style] == NULL)
                                valid = False;
                }
                if (!valid) {
                        XtpLog(XTP_LOG_WARNING, "font",
                               "FR-BADPATTERN resource=fallbackFace%d value=%s", fallback + 1,
                               face);
                        {
                                char resource[32];

                                (void)snprintf(resource, sizeof(resource), "fallbackFace%d",
                                               fallback + 1);
                                XtpFontRoutingReportBadPattern(vt->vt.font_routing_report, resource,
                                                               face);
                        }
                        continue;
                }
                for (earlier = 0; earlier < fallback; ++earlier) {
                        Boolean duplicate = enabled[earlier];

                        for (style = 0; duplicate && style < XTP_XFT_STYLE_COUNT; ++style)
                                duplicate =
                                    SameFontPattern(roles[fallback][style], roles[earlier][style]);
                        if (duplicate) {
                                FcChar8 *file = NULL;
                                int index = 0;

                                (void)FcPatternGetString(roles[fallback][0], FC_FILE, 0, &file);
                                (void)FcPatternGetInteger(roles[fallback][0], FC_INDEX, 0, &index);
                                XtpLog(XTP_LOG_WARNING, "font",
                                       "FR-DUPROLE kept=fallbackFace%d dropped=fallbackFace%d "
                                       "file=%s index=%d",
                                       earlier + 1, fallback + 1,
                                       file != NULL ? (const char *)file : "(unknown)", index);
                                {
                                        char kept[32];
                                        char dropped[32];

                                        (void)snprintf(kept, sizeof(kept), "fallbackFace%d",
                                                       earlier + 1);
                                        (void)snprintf(dropped, sizeof(dropped), "fallbackFace%d",
                                                       fallback + 1);
                                        XtpFontRoutingReportDuplicate(vt->vt.font_routing_report,
                                                                      kept, dropped,
                                                                      roles[fallback][0]);
                                }
                                valid = False;
                                break;
                        }
                }
                enabled[fallback] = valid;
        }
        for (fallback = 0; fallback < XTP_FALLBACK_FACE_COUNT; ++fallback) {
                unsigned int style;

                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        if (roles[fallback][style] != NULL)
                                FcPatternDestroy(roles[fallback][style]);
                }
        }
}

static Boolean
XftFallbackDuplicate(XtpXftFallbackSet *fallbacks, int slot, unsigned int style, XftFont *primary,
                     FcPattern *pattern)
{
        uint8_t fallback;

        if (SameFontPattern(primary->pattern, pattern))
                return True;
        for (fallback = 0; fallback < fallbacks->counts[slot][style]; ++fallback) {
                if (SameFontPattern(fallbacks->candidates[slot][style][fallback].pattern, pattern))
                        return True;
        }
        return False;
}

static Boolean
AppendXftFallback(XtpXftFallbackSet *fallbacks, int slot, unsigned int style, XftFont *primary,
                  FcPattern *pattern, uint8_t named_index)
{
        uint8_t fallback;

        if (pattern == NULL)
                return False;
        if (fallbacks->counts[slot][style] == XTP_XFT_FALLBACK_CAPACITY ||
            XftFallbackDuplicate(fallbacks, slot, style, primary, pattern)) {
                FcPatternDestroy(pattern);
                return False;
        }
        fallback = fallbacks->counts[slot][style]++;
        fallbacks->candidates[slot][style][fallback].pattern = pattern;
        fallbacks->candidates[slot][style][fallback].named_index = named_index;
        return True;
}

static void
LoadXftFallbacks(Vt100Rec *vt, int slot, const char *role, const char *face,
                 const char *explicit_face, double size, Boolean bold, Boolean italic,
                 XftFont *primary, XtpXftFallbackSet *fallbacks,
                 const Boolean named_enabled[XTP_FALLBACK_FACE_COUNT])
{
        unsigned int style = XftStyleIndex(bold, italic);
        FcPattern *request;
        FcFontSet *set;
        FcResult result;
        int index;

        if (slot < 0 || slot >= XTP_FONT_SLOTS || !Nonempty(face) || primary == NULL ||
            fallbacks == NULL)
                return;
        if (Nonempty(explicit_face)) {
                FcPattern *pattern = ResolveXftPattern(vt, explicit_face, size, bold, italic);
                uint8_t before = fallbacks->counts[slot][style];

                (void)AppendXftFallback(fallbacks, slot, style, primary, pattern, 0);
                if (fallbacks->counts[slot][style] != before) {
                        XtpLog(XTP_LOG_INFO, "font",
                               "queued Xft explicit fallback role=%s slot=%d style=%u face=%s",
                               role, slot, style, explicit_face);
                        XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot,
                                                 XftStyleName(bold, italic), 2, explicit_face,
                                                 fallbacks->candidates[slot][style][before].pattern,
                                                 "active", vt->vt.font_generation);
                }
        }
        fallbacks->explicit_counts[slot][style] = fallbacks->counts[slot][style];
        for (index = 0; index < XTP_FALLBACK_FACE_COUNT; ++index) {
                const char *named_face = vt->vt.fallback_face_names[index];
                FcPattern *pattern;
                uint8_t before;

                if (!named_enabled[index])
                        continue;
                pattern = ResolveXftPattern(vt, named_face, size, bold, italic);
                if (pattern == NULL)
                        continue;
                before = fallbacks->counts[slot][style];
                if (!AppendXftFallback(fallbacks, slot, style, primary, pattern,
                                       (uint8_t)(index + 1)))
                        continue;
                XtpLog(XTP_LOG_INFO, "font",
                       "queued Xft named fallback role=%s slot=%d style=%u resource=fallbackFace%d "
                       "entry=%u face=%s",
                       role, slot, style, index + 1, (unsigned int)before + 1U, named_face);
        }
        fallbacks->named_counts[slot][style] = fallbacks->counts[slot][style];
        if (!vt->vt.effective_system_fallback)
                return;
        request = FcNameParse((const FcChar8 *)face);
        if (request == NULL)
                return;
        FcPatternDel(request, FC_SIZE);
        FcPatternDel(request, FC_PIXEL_SIZE);
        FcPatternAddDouble(request, FC_SIZE, size);
        SetXftStyle(request, bold, italic);
        FcConfigSubstitute(NULL, request, FcMatchPattern);
        XftDefaultSubstitute(XtDisplay((Widget)vt), XScreenNumberOfScreen(XtScreen((Widget)vt)),
                             request);
        set = FcFontSort(NULL, request, FcTrue, NULL, &result);
        if (set == NULL) {
                FcPatternDestroy(request);
                return;
        }
        for (index = 0;
             index < set->nfont && fallbacks->counts[slot][style] < XTP_XFT_FALLBACK_CAPACITY;
             ++index) {
                FcPattern *render;

                if (XftFallbackDuplicate(fallbacks, slot, style, primary, set->fonts[index]))
                        continue;
                render = FcFontRenderPrepare(NULL, request, set->fonts[index]);
                if (render == NULL)
                        continue;
                (void)AppendXftFallback(fallbacks, slot, style, primary, render, 0);
        }
        XtpLog(XTP_LOG_INFO, "font", "queued Xft fallback role=%s slot=%d style=%u count=%u", role,
               slot, style, fallbacks->counts[slot][style]);
        FcFontSetDestroy(set);
        FcPatternDestroy(request);
}

static void
LoadXftRoleFallbacks(Vt100Rec *vt, int slot, const char *role, const char *face,
                     const char *explicit_face, const char *bold_face,
                     const char *bold_explicit_face, double size, XftFont *normal, XftFont *bold,
                     XftFont *italic, XftFont *bold_italic, XtpXftFallbackSet *fallbacks,
                     const Boolean named_enabled[XTP_FALLBACK_FACE_COUNT])
{
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, False, False, normal, fallbacks,
                         named_enabled);
        LoadXftFallbacks(vt, slot, role, Nonempty(bold_face) ? bold_face : face,
                         Nonempty(bold_face) ? bold_explicit_face : explicit_face, size, True,
                         False, bold, fallbacks, named_enabled);
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, False, True, italic, fallbacks,
                         named_enabled);
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, True, True, bold_italic,
                         fallbacks, named_enabled);
}

static void
ResetXftUniverse(Vt100Rec *vt)
{
        vt->vt.use_xft = False;
        vt->vt.xft_draw = NULL;
        vt->vt.cairo_draw = NULL;
        vt->vt.shaper = NULL;
        vt->vt.font_route_cache = NULL;
        memset(vt->vt.xft_fonts, 0, sizeof(vt->vt.xft_fonts));
        memset(vt->vt.xft_bold_fonts, 0, sizeof(vt->vt.xft_bold_fonts));
        memset(vt->vt.xft_italic_fonts, 0, sizeof(vt->vt.xft_italic_fonts));
        memset(vt->vt.xft_bold_italic_fonts, 0, sizeof(vt->vt.xft_bold_italic_fonts));
        memset(vt->vt.xft_wide_fonts, 0, sizeof(vt->vt.xft_wide_fonts));
        memset(vt->vt.xft_wide_bold_fonts, 0, sizeof(vt->vt.xft_wide_bold_fonts));
        memset(vt->vt.xft_wide_italic_fonts, 0, sizeof(vt->vt.xft_wide_italic_fonts));
        memset(vt->vt.xft_wide_bold_italic_fonts, 0, sizeof(vt->vt.xft_wide_bold_italic_fonts));
        memset(vt->vt.xft_emoji_fonts, 0, sizeof(vt->vt.xft_emoji_fonts));
        memset(vt->vt.xft_emoji_bold_fonts, 0, sizeof(vt->vt.xft_emoji_bold_fonts));
        memset(vt->vt.xft_emoji_italic_fonts, 0, sizeof(vt->vt.xft_emoji_italic_fonts));
        memset(vt->vt.xft_emoji_bold_italic_fonts, 0, sizeof(vt->vt.xft_emoji_bold_italic_fonts));
        memset(vt->vt.xft_han_fonts, 0, sizeof(vt->vt.xft_han_fonts));
        memset(vt->vt.xft_han_bold_fonts, 0, sizeof(vt->vt.xft_han_bold_fonts));
        memset(vt->vt.xft_han_italic_fonts, 0, sizeof(vt->vt.xft_han_italic_fonts));
        memset(vt->vt.xft_han_bold_italic_fonts, 0, sizeof(vt->vt.xft_han_bold_italic_fonts));
        memset(&vt->vt.xft_fallbacks, 0, sizeof(vt->vt.xft_fallbacks));
        memset(&vt->vt.xft_wide_fallbacks, 0, sizeof(vt->vt.xft_wide_fallbacks));
        memset(&vt->vt.xft_emoji_fallbacks, 0, sizeof(vt->vt.xft_emoji_fallbacks));
        memset(&vt->vt.xft_han_fallbacks, 0, sizeof(vt->vt.xft_han_fallbacks));
        memset(vt->vt.xft_sizes, 0, sizeof(vt->vt.xft_sizes));
        memset(vt->vt.xft_cell_widths, 0, sizeof(vt->vt.xft_cell_widths));
        memset(vt->vt.glyph_ink_cache, 0, sizeof(vt->vt.glyph_ink_cache));
        vt->vt.next_glyph_ink_cache = 0;
}

static void
CloseFallbackSet(Vt100Rec *vt, XtpXftFallbackSet *set)
{
        int slot;

        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                unsigned int style;

                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        uint8_t fallback;

                        for (fallback = 0; fallback < set->counts[slot][style]; ++fallback) {
                                XtpXftFallbackCandidate *candidate =
                                    &set->candidates[slot][style][fallback];

                                if (candidate->font != NULL)
                                        XftFontClose(XtDisplay((Widget)vt), candidate->font);
                                if (candidate->pattern != NULL)
                                        FcPatternDestroy(candidate->pattern);
                        }
                }
        }
}

static void
CloseXftUniverse(Vt100Rec *vt)
{
        XtpXftFallbackSet *sets[] = {
            &vt->vt.xft_fallbacks,
            &vt->vt.xft_wide_fallbacks,
            &vt->vt.xft_emoji_fallbacks,
            &vt->vt.xft_han_fallbacks,
        };
        XftFont **font_arrays[] = {
            vt->vt.xft_fonts,
            vt->vt.xft_bold_fonts,
            vt->vt.xft_italic_fonts,
            vt->vt.xft_bold_italic_fonts,
            vt->vt.xft_wide_fonts,
            vt->vt.xft_wide_bold_fonts,
            vt->vt.xft_wide_italic_fonts,
            vt->vt.xft_wide_bold_italic_fonts,
            vt->vt.xft_emoji_fonts,
            vt->vt.xft_emoji_bold_fonts,
            vt->vt.xft_emoji_italic_fonts,
            vt->vt.xft_emoji_bold_italic_fonts,
            vt->vt.xft_han_fonts,
            vt->vt.xft_han_bold_fonts,
            vt->vt.xft_han_italic_fonts,
            vt->vt.xft_han_bold_italic_fonts,
        };
        size_t array;
        int slot;

        if (vt->vt.xft_draw != NULL)
                XftDrawDestroy(vt->vt.xft_draw);
        XtpCairoDestroy(vt->vt.cairo_draw);
        XtpShaperDestroy(vt->vt.shaper);
        XtpFontRouteCacheDestroy(vt->vt.font_route_cache);
        for (array = 0; array < XtNumber(font_arrays); ++array) {
                for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                        if (font_arrays[array][slot] != NULL)
                                XftFontClose(XtDisplay((Widget)vt), font_arrays[array][slot]);
                }
        }
        for (array = 0; array < XtNumber(sets); ++array)
                CloseFallbackSet(vt, sets[array]);
        ResetXftUniverse(vt);
}

#define MOVE_XFT_ARRAY(member)                                                                     \
        do {                                                                                       \
                memcpy(destination->vt.member, source->vt.member, sizeof(destination->vt.member)); \
                memset(source->vt.member, 0, sizeof(source->vt.member));                           \
        } while (0)

static void
MoveXftUniverse(Vt100Rec *destination, Vt100Rec *source)
{
        destination->vt.use_xft = source->vt.use_xft;
        destination->vt.emoji_presentation = source->vt.emoji_presentation;
        destination->vt.effective_color_glyphs = source->vt.effective_color_glyphs;
        destination->vt.effective_system_fallback = source->vt.effective_system_fallback;
        destination->vt.effective_limit_fontsets = source->vt.effective_limit_fontsets;
        destination->vt.effective_limit_fontheight = source->vt.effective_limit_fontheight;
        destination->vt.effective_limit_fontwidth = source->vt.effective_limit_fontwidth;
        MOVE_XFT_ARRAY(xft_fonts);
        MOVE_XFT_ARRAY(xft_bold_fonts);
        MOVE_XFT_ARRAY(xft_italic_fonts);
        MOVE_XFT_ARRAY(xft_bold_italic_fonts);
        MOVE_XFT_ARRAY(xft_wide_fonts);
        MOVE_XFT_ARRAY(xft_wide_bold_fonts);
        MOVE_XFT_ARRAY(xft_wide_italic_fonts);
        MOVE_XFT_ARRAY(xft_wide_bold_italic_fonts);
        MOVE_XFT_ARRAY(xft_emoji_fonts);
        MOVE_XFT_ARRAY(xft_emoji_bold_fonts);
        MOVE_XFT_ARRAY(xft_emoji_italic_fonts);
        MOVE_XFT_ARRAY(xft_emoji_bold_italic_fonts);
        MOVE_XFT_ARRAY(xft_han_fonts);
        MOVE_XFT_ARRAY(xft_han_bold_fonts);
        MOVE_XFT_ARRAY(xft_han_italic_fonts);
        MOVE_XFT_ARRAY(xft_han_bold_italic_fonts);
        destination->vt.xft_fallbacks = source->vt.xft_fallbacks;
        destination->vt.xft_wide_fallbacks = source->vt.xft_wide_fallbacks;
        destination->vt.xft_emoji_fallbacks = source->vt.xft_emoji_fallbacks;
        destination->vt.xft_han_fallbacks = source->vt.xft_han_fallbacks;
        memset(&source->vt.xft_fallbacks, 0, sizeof(source->vt.xft_fallbacks));
        memset(&source->vt.xft_wide_fallbacks, 0, sizeof(source->vt.xft_wide_fallbacks));
        memset(&source->vt.xft_emoji_fallbacks, 0, sizeof(source->vt.xft_emoji_fallbacks));
        memset(&source->vt.xft_han_fallbacks, 0, sizeof(source->vt.xft_han_fallbacks));
        MOVE_XFT_ARRAY(xft_sizes);
        MOVE_XFT_ARRAY(xft_cell_widths);
        destination->vt.xft_draw = source->vt.xft_draw;
        destination->vt.cairo_draw = source->vt.cairo_draw;
        destination->vt.shaper = source->vt.shaper;
        destination->vt.font_route_cache = source->vt.font_route_cache;
        destination->vt.font_generation = source->vt.font_generation;
        source->vt.xft_draw = NULL;
        source->vt.cairo_draw = NULL;
        source->vt.shaper = NULL;
        source->vt.font_route_cache = NULL;
        memcpy(destination->vt.glyph_ink_cache, source->vt.glyph_ink_cache,
               sizeof(destination->vt.glyph_ink_cache));
        destination->vt.next_glyph_ink_cache = source->vt.next_glyph_ink_cache;
        memset(source->vt.glyph_ink_cache, 0, sizeof(source->vt.glyph_ink_cache));
        source->vt.next_glyph_ink_cache = 0;
}

#undef MOVE_XFT_ARRAY

static void
InitializeXft(Vt100Rec *vt)
{
        XtpFontChain face_chain = {0};
        XtpFontChain wide_chain = {0};
        XtpFontChain emoji_chain = {0};
        XtpFontChain han_chain = {0};
        XtpFontChain bold_chain = {0};
        XtpFontChain wide_bold_chain = {0};
        char *face;
        char *wide_face;
        char *emoji_face;
        char *han_face;
        char *bold_face;
        char *wide_bold_face;
        Boolean named_enabled[XTP_FALLBACK_FACE_COUNT] = {False};
        Boolean requested = ResourceBoolean(vt->vt.render_font_name, Nonempty(vt->vt.face_name));
        double base_size = PositiveNumber(vt->vt.face_size_names[0], 8.0);
        double embedded_size;
        unsigned long base_area;
        int slot;

        XtpFontRouteCacheDestroy(vt->vt.font_route_cache);
        vt->vt.font_route_cache = XtpFontRouteCacheCreate(XTP_FONT_ROUTE_CACHE_CAPACITY);
        ++vt->vt.font_generation;
        if (vt->vt.font_generation == 0)
                vt->vt.font_generation = 1;
        if (vt->vt.font_route_cache == NULL)
                XtpLog(XTP_LOG_WARNING, "font",
                       "cannot allocate font routing cache; routing remains uncached");
        vt->vt.use_xft = False;
        vt->vt.effective_color_glyphs = vt->vt.color_glyphs;
        vt->vt.effective_system_fallback = vt->vt.system_fallback;
        vt->vt.effective_limit_fontsets = vt->vt.limit_fontsets;
        /* Retained for the backend-owned DEC double-height gap (LM-04/05). */
        vt->vt.effective_limit_fontheight = vt->vt.limit_fontheight;
        vt->vt.effective_limit_fontwidth = vt->vt.limit_fontwidth;
        if (vt->vt.effective_limit_fontsets < 0) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting number of fontsets to 255 (was %d)",
                       vt->vt.effective_limit_fontsets);
                vt->vt.effective_limit_fontsets = 255;
        } else if (vt->vt.effective_limit_fontsets > 255) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting number of fontsets to 255 (was %d)",
                       vt->vt.effective_limit_fontsets);
                vt->vt.effective_limit_fontsets = 255;
        }
        if (vt->vt.effective_limit_fontheight > 50) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting extra fontheight percent to 50 (was %d)",
                       vt->vt.effective_limit_fontheight);
                vt->vt.effective_limit_fontheight = 50;
        }
        if (vt->vt.effective_limit_fontwidth > 50) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting extra fontwidth percent to 50 (was %d)",
                       vt->vt.effective_limit_fontwidth);
                vt->vt.effective_limit_fontwidth = 50;
        }
        if (XtpFontChainParse(vt->vt.face_name, &face_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_doublesize, &wide_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_emoji, &emoji_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_han, &han_chain) != 0 ||
            XtpFontChainParseXftEntries(vt->vt.bold_font_name, &bold_chain) != 0 ||
            XtpFontChainParseXftEntries(vt->vt.wide_bold_font_name, &wide_bold_chain) != 0) {
                XtpLog(XTP_LOG_WARNING, "font", "cannot parse Xft slot chain");
                goto done;
        }
        face = face_chain.count != 0 ? face_chain.entries[0] : NULL;
        wide_face = wide_chain.count != 0 ? wide_chain.entries[0] : NULL;
        emoji_face = emoji_chain.count != 0 ? emoji_chain.entries[0] : NULL;
        han_face = han_chain.count != 0 ? han_chain.entries[0] : NULL;
        bold_face = bold_chain.count != 0 ? bold_chain.entries[0] : NULL;
        wide_bold_face = wide_bold_chain.count != 0 ? wide_bold_chain.entries[0] : NULL;
        if (face_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font", "faceName discarded %zu Xft list entries after 2",
                       face_chain.discarded);
        if (wide_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameDoublesize discarded %zu Xft list entries after 2",
                       wide_chain.discarded);
        if (emoji_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameEmoji discarded %zu Xft list entries after 2",
                       emoji_chain.discarded);
        if (han_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameHan discarded %zu Xft list entries after 2", han_chain.discarded);
        if (bold_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font", "boldFont discarded %zu Xft list entries after 2",
                       bold_chain.discarded);
        if (wide_bold_chain.discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "wideBoldFont discarded %zu Xft list entries after 2",
                       wide_bold_chain.discarded);
        if (TrimFaceSize(face, &embedded_size)) {
                base_size = embedded_size > 0.0 ? embedded_size : 8.0;
                XtpLog(XTP_LOG_DEBUG, "font", "faceName embedded size selects points=%.2f",
                       base_size);
        }
        (void)TrimFaceSize(wide_face, NULL);
        (void)TrimFaceSize(emoji_face, NULL);
        (void)TrimFaceSize(han_face, NULL);
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
        ResolveNamedFallbackRoles(vt, base_size, named_enabled);
        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                double size =
                    slot == 0 ? base_size : PositiveNumber(vt->vt.face_size_names[slot], 0.0);

                if (size <= 0.0) {
                        XFontStruct *bitmap =
                            vt->vt.fonts[slot] != NULL ? vt->vt.fonts[slot] : vt->vt.initial_font;
                        unsigned long area = (unsigned long)FontWidth(bitmap) * FontHeight(bitmap);

                        size = base_size * sqrt((double)area / (double)base_area);
                }
                (void)LoadXftSlot(vt, slot, face, size);
                LoadXftBoldOverride(vt, slot, "primary", bold_face, size, vt->vt.xft_fonts[slot],
                                    &vt->vt.xft_bold_fonts[slot]);
                LoadXftRoleSlot(vt, slot, "doublesize", wide_face, size, vt->vt.xft_wide_fonts,
                                vt->vt.xft_wide_bold_fonts, vt->vt.xft_wide_italic_fonts,
                                vt->vt.xft_wide_bold_italic_fonts);
                LoadXftBoldOverride(vt, slot, "doublesize", wide_bold_face, size,
                                    vt->vt.xft_wide_fonts[slot], &vt->vt.xft_wide_bold_fonts[slot]);
                LoadXftRoleSlot(vt, slot, "emoji", emoji_face, size, vt->vt.xft_emoji_fonts,
                                vt->vt.xft_emoji_bold_fonts, vt->vt.xft_emoji_italic_fonts,
                                vt->vt.xft_emoji_bold_italic_fonts);
                LoadXftRoleSlot(vt, slot, "han", han_face, size, vt->vt.xft_han_fonts,
                                vt->vt.xft_han_bold_fonts, vt->vt.xft_han_italic_fonts,
                                vt->vt.xft_han_bold_italic_fonts);
                LoadXftRoleFallbacks(
                    vt, slot, "primary", face, face_chain.count > 1 ? face_chain.entries[1] : NULL,
                    bold_face, bold_chain.count > 1 ? bold_chain.entries[1] : NULL, size,
                    vt->vt.xft_fonts[slot], vt->vt.xft_bold_fonts[slot],
                    vt->vt.xft_italic_fonts[slot], vt->vt.xft_bold_italic_fonts[slot],
                    &vt->vt.xft_fallbacks, named_enabled);
                LoadXftRoleFallbacks(
                    vt, slot, "doublesize", wide_face,
                    wide_chain.count > 1 ? wide_chain.entries[1] : NULL, wide_bold_face,
                    wide_bold_chain.count > 1 ? wide_bold_chain.entries[1] : NULL, size,
                    vt->vt.xft_wide_fonts[slot], vt->vt.xft_wide_bold_fonts[slot],
                    vt->vt.xft_wide_italic_fonts[slot], vt->vt.xft_wide_bold_italic_fonts[slot],
                    &vt->vt.xft_wide_fallbacks, named_enabled);
                LoadXftRoleFallbacks(
                    vt, slot, "emoji", emoji_face,
                    emoji_chain.count > 1 ? emoji_chain.entries[1] : NULL, NULL, NULL, size,
                    vt->vt.xft_emoji_fonts[slot], vt->vt.xft_emoji_bold_fonts[slot],
                    vt->vt.xft_emoji_italic_fonts[slot], vt->vt.xft_emoji_bold_italic_fonts[slot],
                    &vt->vt.xft_emoji_fallbacks, named_enabled);
                LoadXftRoleFallbacks(
                    vt, slot, "han", han_face, han_chain.count > 1 ? han_chain.entries[1] : NULL,
                    NULL, NULL, size, vt->vt.xft_han_fonts[slot], vt->vt.xft_han_bold_fonts[slot],
                    vt->vt.xft_han_italic_fonts[slot], vt->vt.xft_han_bold_italic_fonts[slot],
                    &vt->vt.xft_han_fallbacks, named_enabled);
        }
        if (requested && vt->vt.xft_fonts[0] != NULL) {
                vt->vt.use_xft = True;
                vt->vt.shaper = XtpShaperCreate();
                if (vt->vt.shaper == NULL)
                        XtpLog(XTP_LOG_WARNING, "font", "cannot create HarfBuzz shaper");
        }

done:
        XtpLog(XTP_LOG_INFO, "font",
               "renderer=%s renderFont=%s faceName=%s faceNameDoublesize=%s faceNameEmoji=%s "
               "faceNameHan=%s "
               "faceSize=%.2f emojiPresentation=%s colorGlyphs=%s Unicode=%s",
               vt->vt.use_xft ? "xft" : "xlib-bitmap",
               vt->vt.render_font_name != NULL ? vt->vt.render_font_name : "(null)",
               vt->vt.face_name != NULL ? vt->vt.face_name : "(null)",
               vt->vt.face_name_doublesize != NULL ? vt->vt.face_name_doublesize : "(null)",
               vt->vt.face_name_emoji != NULL ? vt->vt.face_name_emoji : "(null)",
               vt->vt.face_name_han != NULL ? vt->vt.face_name_han : "(null)", base_size,
               vt->vt.emoji_presentation_name != NULL ? vt->vt.emoji_presentation_name : "unicode",
               vt->vt.color_glyphs ? "true" : "false", XtpEmojiUnicodeVersion());
        XtpFontChainClear(&face_chain);
        XtpFontChainClear(&wide_chain);
        XtpFontChainClear(&emoji_chain);
        XtpFontChainClear(&han_chain);
        XtpFontChainClear(&bold_chain);
        XtpFontChainClear(&wide_bold_chain);
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
        background.pixel = vt->vt.opaque_background_pixel;
        XQueryColor(XtDisplay((Widget)vt), vt->core.colormap, &foreground);
        XQueryColor(XtDisplay((Widget)vt), vt->core.colormap, &background);
        XtpLog(XTP_LOG_INFO, "config",
               "VT100 resolved renderer=%s font=%s faceName=%s faceSize=%.2f cell=%ux%u "
               "foreground=#%02x%02x%02x background=#%02x%02x%02x cursorColor=%lu",
               vt->vt.use_xft ? "xft" : "xlib-bitmap", name != NULL ? name : "(unknown)",
               vt->vt.face_name != NULL ? vt->vt.face_name : "(unset)", vt->vt.xft_sizes[0],
               VtSlotWidth(vt, 0), VtSlotHeight(vt, 0), foreground.red >> 8, foreground.green >> 8,
               foreground.blue >> 8, background.red >> 8, background.green >> 8,
               background.blue >> 8, vt->vt.cursor_color);
        XtpLog(XTP_LOG_INFO, "config",
               "VT100 resolved backgroundOpacity=%s effective-alpha=%u visual-alpha=%s depth=%d",
               vt->vt.background_opacity_name != NULL ? vt->vt.background_opacity_name : "(null)",
               vt->vt.background_alpha, vt->vt.alpha_visual ? "true" : "false", vt->core.depth);
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

Pixel
VtOpaquePixel(const Vt100Rec *vt, Pixel pixel)
{
        return XtpX11OpaquePixel(pixel, vt->vt.alpha_visual ? &vt->vt.alpha_format : NULL);
}

uint16_t
VtPixelAlpha(const Vt100Rec *vt, Pixel pixel)
{
        return XtpX11PixelAlpha(pixel, vt->vt.alpha_visual ? &vt->vt.alpha_format : NULL);
}

static void
ResolveBackgroundOpacity(Vt100Rec *vt)
{
        Visual *visual = NULL;
        uint16_t alpha = UINT16_MAX;

        XtVaGetValues(XtParent((Widget)vt), XtNvisual, &visual, NULL);
        vt->vt.alpha_visual =
            XtpX11VisualAlphaFormat(XtDisplay((Widget)vt), visual, &vt->vt.alpha_format);
        if (XtpBackgroundOpacityParse(vt->vt.background_opacity_name, &alpha) != 0) {
                XtpLog(XTP_LOG_WARNING, "render",
                       "invalid backgroundOpacity=%s; using opaque background",
                       vt->vt.background_opacity_name != NULL ? vt->vt.background_opacity_name
                                                              : "(null)");
                alpha = UINT16_MAX;
        }
        vt->vt.background_alpha = vt->vt.alpha_visual ? alpha : UINT16_MAX;
}

static void
NormalizeConfiguredColors(Vt100Rec *vt)
{
        vt->vt.foreground = VtOpaquePixel(vt, vt->vt.foreground);
        vt->vt.cursor_color = VtOpaquePixel(vt, vt->vt.cursor_color);
        vt->core.border_pixel = VtOpaquePixel(vt, vt->core.border_pixel);
        vt->core.background_pixel = XtpX11PixelWithAlpha(
            vt->vt.opaque_background_pixel, vt->vt.alpha_visual ? &vt->vt.alpha_format : NULL,
            vt->vt.background_alpha);
}

static void
SwapDefaultColors(Vt100Rec *vt)
{
        Pixel foreground = vt->vt.foreground;

        vt->vt.foreground = vt->vt.opaque_background_pixel;
        vt->vt.opaque_background_pixel = VtOpaquePixel(vt, foreground);
}

static void
Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        Vt100Rec *vt = VtAsRecord(new_widget);

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
        ResolveBackgroundOpacity(vt);
        vt->vt.opaque_background_pixel = VtOpaquePixel(vt, vt->core.background_pixel);
        if (vt->vt.reverse_video)
                SwapDefaultColors(vt);
        NormalizeConfiguredColors(vt);
        vt->vt.cursor_blink_policy = ParseCursorBlinkPolicy(vt->vt.cursor_blink_name);
        vt->vt.emoji_presentation = ParseEmojiPolicy(vt->vt.emoji_presentation_name);
        vt->vt.grapheme_width_unicode = ParseGraphemeWidth(vt->vt.grapheme_width_name);
        vt->vt.font_route_cache = NULL;
        vt->vt.font_routing_report =
            vt->vt.report_font_routing != False ? XtpFontRoutingReportCreate(true) : NULL;
        vt->vt.font_generation = 0;
        if (vt->vt.report_font_routing != False && vt->vt.font_routing_report == NULL)
                XtpLog(XTP_LOG_WARNING, "font",
                       "cannot allocate font routing report; collection disabled");

        XtpLog(XTP_LOG_INFO, "config",
               "VT100 compiled defaults grid=80x24 font=fixed internalBorder=2 saveLines=1024 "
               "scrollBar=false rightScrollBar=false");

        vt->vt.fonts[0] = vt->vt.initial_font;
        vt->vt.selection_time = CurrentTime;
        vt->vt.select_unit = XTP_SELECTION_CELL;
        vt->vt.cursor_fill = vt->vt.foreground;
        vt->vt.cursor_text_color = vt->core.background_pixel;
        vt->vt.last_cursor_shape = XTP_CURSOR_SHAPE_BLOCK;
        vt->vt.cursor_blink_on = True;
        {
                int slot;

                for (slot = 1; slot < XTP_FONT_SLOTS; ++slot)
                        (void)LoadSlot(vt, slot);
                XtpLog(XTP_LOG_DEBUG, "font", "preloaded configured bitmap slots");
        }
        InitializeXft(vt);
        LogInitialFont(vt);
        if (vt->core.width == 0)
                vt->core.width = XtpVtNaturalWidth(new_widget);
        if (vt->core.height == 0)
                vt->core.height = XtpVtNaturalHeight(new_widget);

        CreateGc(new_widget);
        /* Track pointer motion and Shift state only to update OSC 8 hover highlighting. */
        XtAddEventHandler(new_widget,
                          PointerMotionMask | KeyPressMask | KeyReleaseMask | LeaveWindowMask,
                          False, VtHyperlinkEvent, vt);
}

static void
Destroy(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);
        int slot;
        size_t color;

        if (vt->vt.viewport_update_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.viewport_update_timer);
        if (vt->vt.selection_autoscroll_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.selection_autoscroll_timer);
        if (vt->vt.cursor_blink_timer != (XtIntervalId)0)
                XtRemoveTimeOut(vt->vt.cursor_blink_timer);
        VtDestroyInput(vt);
        ReleaseGc(widget);
        CloseXftUniverse(vt);
        XtpFontRoutingReportDestroy(vt->vt.font_routing_report);
        free(vt->vt.frame_cells);
        free(vt->vt.pending_cells);
        free(vt->vt.dirty_first_columns);
        free(vt->vt.dirty_end_columns);
        free(vt->vt.selection_text);
        free(vt->vt.owned_selections);
        free(vt->vt.hovered_hyperlink);
        free(vt->vt.pressed_hyperlink);
        for (color = 0; color < vt->vt.color_count; ++color) {
                if (vt->vt.colors[color].used && vt->vt.colors[color].owned) {
                        Pixel pixel = vt->vt.colors[color].allocation_pixel;
                        XFreeColors(XtDisplay(widget), vt->core.colormap, &pixel, 1, 0);
                }
        }
        for (slot = 1; slot < XTP_FONT_SLOTS; ++slot) {
                if (vt->vt.owned[slot] && vt->vt.fonts[slot] != NULL)
                        XFreeFont(XtDisplay(widget), vt->vt.fonts[slot]);
        }
}

static void
ResizeWidget(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);
        Dimension scrollbar = vt->vt.scroll_bar ? VtScrollbarTotalWidth(vt) : 0;
        unsigned int cell_width = XtpVtCellWidth(widget);
        unsigned int cell_height = XtpVtCellHeight(widget);
        unsigned int horizontal = 2U * vt->vt.internal_border + scrollbar;
        unsigned int vertical = 2U * vt->vt.internal_border;
        unsigned int columns =
            vt->core.width > horizontal ? (vt->core.width - horizontal) / cell_width : 1U;
        unsigned int rows =
            vt->core.height > vertical ? (vt->core.height - vertical) / cell_height : 1U;

        XtpCairoResize(vt->vt.cairo_draw, (int)vt->core.width, (int)vt->core.height);
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

static Boolean
StringChanged(const char *old_value, const char *new_value)
{
        if ((old_value == NULL) != (new_value == NULL))
                return True;
        return old_value != NULL && strcmp(old_value, new_value) != 0;
}

static Boolean
FontResourcesChanged(const Vt100Rec *old_vt, const Vt100Rec *new_vt)
{
        int index;

        if (StringChanged(old_vt->vt.render_font_name, new_vt->vt.render_font_name) ||
            StringChanged(old_vt->vt.face_name, new_vt->vt.face_name) ||
            StringChanged(old_vt->vt.face_name_doublesize, new_vt->vt.face_name_doublesize) ||
            StringChanged(old_vt->vt.face_name_emoji, new_vt->vt.face_name_emoji) ||
            StringChanged(old_vt->vt.face_name_han, new_vt->vt.face_name_han) ||
            StringChanged(old_vt->vt.bold_font_name, new_vt->vt.bold_font_name) ||
            StringChanged(old_vt->vt.wide_bold_font_name, new_vt->vt.wide_bold_font_name) ||
            StringChanged(old_vt->vt.emoji_presentation_name, new_vt->vt.emoji_presentation_name) ||
            old_vt->vt.color_glyphs != new_vt->vt.color_glyphs ||
            old_vt->vt.system_fallback != new_vt->vt.system_fallback ||
            old_vt->vt.limit_fontsets != new_vt->vt.limit_fontsets ||
            old_vt->vt.limit_fontheight != new_vt->vt.limit_fontheight ||
            old_vt->vt.limit_fontwidth != new_vt->vt.limit_fontwidth)
                return True;
        for (index = 0; index < XTP_FONT_SLOTS; ++index) {
                if (StringChanged(old_vt->vt.face_size_names[index],
                                  new_vt->vt.face_size_names[index]))
                        return True;
        }
        for (index = 0; index < XTP_FALLBACK_FACE_COUNT; ++index) {
                if (StringChanged(old_vt->vt.fallback_face_names[index],
                                  new_vt->vt.fallback_face_names[index]))
                        return True;
        }
        return False;
}

static void
ReportRetainedPrimary(Vt100Rec *vt, const char *configured)
{
        XftFont **styles[] = {
            vt->vt.xft_fonts,
            vt->vt.xft_bold_fonts,
            vt->vt.xft_italic_fonts,
            vt->vt.xft_bold_italic_fonts,
        };
        static const char *const names[] = {"normal", "bold", "italic", "bold-italic"};
        size_t style;
        int slot;

        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                for (style = 0; style < XtNumber(styles); ++style) {
                        XftFont *font = styles[style][slot];

                        XtpFontRoutingReportLoad(vt->vt.font_routing_report, "primary", slot,
                                                 names[style], 1, configured,
                                                 font != NULL ? font->pattern : NULL, "retained",
                                                 vt->vt.font_generation);
                }
        }
}

static Boolean
ReloadXftUniverse(Vt100Rec *old_vt, Vt100Rec *new_vt)
{
        Vt100Rec candidate = *new_vt;
        Vt100Rec previous = *new_vt;
        XtpFontRoutingReport *build_report =
            new_vt->vt.font_routing_report != NULL ? XtpFontRoutingReportCreate(true) : NULL;
        Boolean requested =
            ResourceBoolean(new_vt->vt.render_font_name, Nonempty(new_vt->vt.face_name));
        XtpFontChanged font_changed;

        (void)old_vt;
        ResetXftUniverse(&candidate);
        candidate.vt.font_generation = new_vt->vt.font_generation;
        candidate.vt.font_routing_report = build_report;
        candidate.vt.emoji_presentation = ParseEmojiPolicy(candidate.vt.emoji_presentation_name);
        InitializeXft(&candidate);
        if (requested && candidate.vt.xft_fonts[0] == NULL) {
                static const char cause[] = "configured faceName has no usable primary";

                XtpLog(XTP_LOG_WARNING, "font", "FR-RELOADFAIL slot=primary cause=%s", cause);
                XtpFontRoutingReportMergeBuild(new_vt->vt.font_routing_report, build_report);
                XtpFontRoutingReportReloadFailure(new_vt->vt.font_routing_report, "primary", cause);
                ReportRetainedPrimary(new_vt, new_vt->vt.face_name);
                candidate.vt.font_routing_report = NULL;
                CloseXftUniverse(&candidate);
                XtpFontRoutingReportDestroy(build_report);
                return False;
        }

        candidate.vt.font_routing_report = NULL;
        MoveXftUniverse(new_vt, &candidate);
        CloseXftUniverse(&previous);
        XtpFontRoutingReportMergeBuild(new_vt->vt.font_routing_report, build_report);
        XtpFontRoutingReportDestroy(build_report);
        if (new_vt->vt.use_xft && new_vt->vt.xft_fonts[new_vt->vt.current_font] == NULL)
                new_vt->vt.current_font = 0;
        memset(new_vt->vt.glyph_ink_cache, 0, sizeof(new_vt->vt.glyph_ink_cache));
        new_vt->vt.next_glyph_ink_cache = 0;
        new_vt->vt.frame_valid = False;
        new_vt->vt.last_cursor_visible = False;
        ReleaseGc((Widget)new_vt);
        CreateGc((Widget)new_vt);
        font_changed.slot = new_vt->vt.current_font;
        font_changed.cell_width = VtSlotWidth(new_vt, new_vt->vt.current_font);
        font_changed.cell_height = VtSlotHeight(new_vt, new_vt->vt.current_font);
        XtCallCallbacks((Widget)new_vt, XtNfontChangedCallback, &font_changed);
        XtpLog(XTP_LOG_INFO, "font", "transactional reload generation=%u renderer=%s",
               new_vt->vt.font_generation, new_vt->vt.use_xft ? "xft" : "xlib-bitmap");
        return True;
}

static Boolean
SetValues(Widget current, Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        Vt100Rec *old_vt = VtAsRecord(current);
        Vt100Rec *new_vt = VtAsRecord(new_widget);
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
        if (old_vt->vt.report_font_routing != new_vt->vt.report_font_routing &&
            new_vt->vt.report_font_routing != False) {
                new_vt->vt.font_routing_report = XtpFontRoutingReportCreate(true);
                if (new_vt->vt.font_routing_report == NULL) {
                        new_vt->vt.report_font_routing = False;
                        XtpLog(XTP_LOG_WARNING, "font",
                               "cannot allocate font routing report; collection disabled");
                }
        }
        if (FontResourcesChanged(old_vt, new_vt)) {
                if (ReloadXftUniverse(old_vt, new_vt))
                        changed = True;
        }
        if ((old_vt->vt.background_opacity_name == NULL) !=
                (new_vt->vt.background_opacity_name == NULL) ||
            (old_vt->vt.background_opacity_name != NULL &&
             new_vt->vt.background_opacity_name != NULL &&
             strcmp(old_vt->vt.background_opacity_name, new_vt->vt.background_opacity_name) != 0))
                ResolveBackgroundOpacity(new_vt);
        if (old_vt->core.background_pixel != new_vt->core.background_pixel)
                new_vt->vt.opaque_background_pixel =
                    VtOpaquePixel(new_vt, new_vt->core.background_pixel);
        if (old_vt->vt.reverse_video != new_vt->vt.reverse_video)
                SwapDefaultColors(new_vt);
        NormalizeConfiguredColors(new_vt);
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
                Boolean effective = VtEffectiveCursorBlink(new_vt->vt.cursor_blink_policy,
                                                           new_vt->vt.cursor_blink_requested);

                if (new_vt->vt.terminal != NULL &&
                    XtpTerminalSetCursorBlinkDefault(
                        new_vt->vt.terminal, CursorBlinkDefault(new_vt->vt.cursor_blink_policy)) !=
                        0)
                        XtpLog(XTP_LOG_ERROR, "render", "cannot apply cursorBlink=%s",
                               new_vt->vt.cursor_blink_name);
                VtStopCursorBlink(new_vt);
                new_vt->vt.cursor_blink_on = True;
                new_vt->vt.cursor_blinking = effective;
                XtpLog(XTP_LOG_INFO, "config", "VT100 cursorBlink=%s effective=%s",
                       new_vt->vt.cursor_blink_name, effective ? "true" : "false");
                changed = True;
        }

        if (old_vt->vt.emoji_presentation != new_vt->vt.emoji_presentation ||
            old_vt->vt.effective_color_glyphs != new_vt->vt.effective_color_glyphs) {
                memset(new_vt->vt.glyph_ink_cache, 0, sizeof(new_vt->vt.glyph_ink_cache));
                new_vt->vt.next_glyph_ink_cache = 0;
                XtpLog(XTP_LOG_INFO, "font", "emojiPresentation=%s colorGlyphs=%s",
                       new_vt->vt.emoji_presentation_name != NULL
                           ? new_vt->vt.emoji_presentation_name
                           : "unicode",
                       new_vt->vt.effective_color_glyphs ? "true" : "false");
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
                if (XtIsRealized(new_widget))
                        XSetWindowBackground(XtDisplay(new_widget), XtWindow(new_widget),
                                             new_vt->core.background_pixel);
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
                VtRestartCursorBlink(new_vt);
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
        if (old_vt->vt.report_font_routing != new_vt->vt.report_font_routing &&
            new_vt->vt.report_font_routing == False) {
                XtpFontRoutingReportDestroy(new_vt->vt.font_routing_report);
                new_vt->vt.font_routing_report = NULL;
        }
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
        Vt100Rec *vt = VtAsRecord(widget);
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
               VtSlotWidth(vt, slot), VtSlotHeight(vt, slot));
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
        Vt100Rec *vt = VtAsRecord(widget);
        double current_size = vt->vt.use_xft ? vt->vt.xft_sizes[vt->vt.current_font]
                                             : (double)VtSlotWidth(vt, vt->vt.current_font) *
                                                   VtSlotHeight(vt, vt->vt.current_font);
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
                                      : (double)VtSlotWidth(vt, slot) * VtSlotHeight(vt, slot);
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

Boolean
VtAcceptLocalKeyAction(Vt100Rec *vt, XEvent *event, LocalKeyAction action)
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

Boolean
VtLocalKeyActionOwnsEvent(Vt100Rec *vt, const XKeyEvent *event, Boolean release)
{
        Boolean owned = False;
        unsigned int slot;

        if (vt == NULL || event == NULL)
                return False;
        for (slot = 0; slot < XTP_RECENT_KEY_ACTIONS; ++slot) {
                KeyActionIdentity *identity = &vt->vt.recent_key_actions[slot];

                if (!identity->used || identity->keycode != event->keycode ||
                    identity->state != event->state)
                        continue;
                if (!release &&
                    (identity->serial != event->serial || identity->time != event->time))
                        continue;
                owned = True;
                if (release)
                        identity->used = False;
        }
        return owned;
}

static void
LargerFontAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        (void)params;
        (void)num_params;
        if (!VtAcceptLocalKeyAction(VtAsRecord(widget), event, XTP_LOCAL_ACTION_FONT_LARGER))
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
        if (!VtAcceptLocalKeyAction(VtAsRecord(widget), event, XTP_LOCAL_ACTION_FONT_SMALLER))
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
ReportFontRoutingAction(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
        Vt100Rec *vt = VtAsRecord(widget);
        XftFont *primary = vt->vt.xft_fonts[vt->vt.current_font];
        double dpi = 0.0;

        (void)params;
        (void)num_params;
        if (!VtAcceptLocalKeyAction(vt, event, XTP_LOCAL_ACTION_REPORT_FONT_ROUTING))
                return;
        if (primary != NULL && primary->pattern != NULL)
                (void)FcPatternGetDouble(primary->pattern, FC_DPI, 0, &dpi);
        XtpFontRoutingReportSnapshot(
            vt->vt.font_routing_report, vt->vt.font_generation, dpi,
            vt->vt.effective_limit_fontsets, vt->vt.effective_limit_fontheight,
            vt->vt.effective_limit_fontwidth, vt->vt.effective_system_fallback != False);
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

unsigned int
XtpVtCellWidth(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        return VtSlotWidth(vt, vt->vt.current_font);
}

unsigned int
XtpVtCellHeight(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        return VtSlotHeight(vt, vt->vt.current_font);
}

unsigned int
XtpVtColumns(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        return (unsigned int)vt->vt.columns;
}

unsigned int
XtpVtRows(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        return (unsigned int)vt->vt.rows;
}

Boolean
XtpVtGraphemeWidthUnicode(Widget widget)
{
        return VtAsRecord(widget)->vt.grapheme_width_unicode;
}

Boolean
XtpVtFontSlotInfo(Widget widget, int slot, XtpFontSlotInfo *info)
{
        Vt100Rec *vt = VtAsRecord(widget);
        if (info == NULL || slot < 0 || slot >= XTP_FONT_SLOTS)
                return False;
        if (vt->vt.use_xft)
                info->loaded = vt->vt.xft_fonts[slot] != NULL;
        else
                info->loaded = vt->vt.fonts[slot] != NULL;
        info->cell_width = info->loaded ? VtSlotWidth(vt, slot) : 0;
        info->cell_height = info->loaded ? VtSlotHeight(vt, slot) : 0;
        info->point_size = vt->vt.use_xft && info->loaded ? vt->vt.xft_sizes[slot] : 0.0;
        return True;
}

const char *
XtpVtRendererName(Widget widget)
{
        return VtAsRecord(widget)->vt.use_xft ? "xft" : "xlib-bitmap";
}

Boolean
XtpVtUsingXft(Widget widget)
{
        return VtAsRecord(widget)->vt.use_xft;
}

Boolean
XtpVtXftAvailable(Widget widget)
{
        return VtAsRecord(widget)->vt.xft_fonts[0] != NULL;
}

uint32_t
XtpVtFontGeneration(Widget widget)
{
        return VtAsRecord(widget)->vt.font_generation;
}

Boolean
XtpVtBackgroundOpacityAvailable(Widget widget)
{
        return VtAsRecord(widget)->vt.alpha_visual;
}

unsigned int
XtpVtBackgroundOpacityPercent(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        return ((unsigned int)vt->vt.background_alpha * 100U + UINT16_MAX / 2U) / UINT16_MAX;
}

Boolean
XtpVtSetBackgroundOpacityPercent(Widget widget, unsigned int percent)
{
        Vt100Rec *vt = VtAsRecord(widget);
        Pixel background;

        if (!vt->vt.alpha_visual)
                return False;
        if (percent > 100U)
                percent = 100U;
        vt->vt.background_alpha = (uint16_t)((percent * (unsigned int)UINT16_MAX + 50U) / 100U);
        background = XtpX11PixelWithAlpha(vt->vt.opaque_background_pixel, &vt->vt.alpha_format,
                                          vt->vt.background_alpha);
        if (background == vt->core.background_pixel)
                return True;
        vt->core.background_pixel = background;
        if (XtIsRealized(widget))
                XSetWindowBackground(XtDisplay(widget), XtWindow(widget), background);
        if (vt->vt.scrollbar != NULL)
                XtVaSetValues(vt->vt.scrollbar, XtNbackground, background, NULL);
        vt->vt.frame_valid = False;
        vt->vt.last_cursor_visible = False;
        XtpLog(XTP_LOG_INFO, "render", "background opacity changed percent=%u alpha=%u", percent,
               vt->vt.background_alpha);
        XtpVtRedraw(widget);
        return True;
}

Boolean
XtpVtSetRenderFont(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = VtAsRecord(widget);
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
        Vt100Rec *vt = VtAsRecord(widget);
        unsigned long value =
            (unsigned long)vt->vt.columns * XtpVtCellWidth(widget) + 2UL * vt->vt.internal_border;

        if (vt->vt.scroll_bar)
                value += VtScrollbarTotalWidth(vt);

        return value <= USHRT_MAX ? (Dimension)value : (Dimension)USHRT_MAX;
}

Dimension
XtpVtNaturalHeight(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);
        unsigned long value =
            (unsigned long)vt->vt.rows * XtpVtCellHeight(widget) + 2UL * vt->vt.internal_border;

        return value <= USHRT_MAX ? (Dimension)value : (Dimension)USHRT_MAX;
}

Boolean
XtpVtScrollbarVisible(Widget widget)
{
        return VtAsRecord(widget)->vt.scroll_bar;
}

void
XtpVtSetScrollbar(Widget widget, Boolean visible)
{
        Vt100Rec *vt = VtAsRecord(widget);

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
        return VtAsRecord(widget)->vt.scroll_key;
}

void
XtpVtSetScrollKey(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = VtAsRecord(widget);

        vt->vt.scroll_key = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "scrollback", "scrollKey=%s", vt->vt.scroll_key ? "true" : "false");
}

Boolean
XtpVtScrollTtyOutput(Widget widget)
{
        return VtAsRecord(widget)->vt.scroll_tty_output;
}

Boolean
XtpVtSelectToClipboard(Widget widget)
{
        return VtAsRecord(widget)->vt.select_to_clipboard;
}

Boolean
XtpVtReverseVideo(Widget widget)
{
        return VtAsRecord(widget)->vt.reverse_video;
}

void
XtpVtSetReverseVideo(Widget widget, Boolean enabled)
{
        XtVaSetValues(widget, XtNreverseVideo, enabled ? True : False, NULL);
}

void
XtpVtSetSelectToClipboard(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = VtAsRecord(widget);

        vt->vt.select_to_clipboard = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "selection", "selectToClipboard=%s SELECT=%s",
               vt->vt.select_to_clipboard ? "true" : "false",
               vt->vt.select_to_clipboard ? "CLIPBOARD" : "PRIMARY");
}

void
XtpVtSetScrollTtyOutput(Widget widget, Boolean enabled)
{
        Vt100Rec *vt = VtAsRecord(widget);

        vt->vt.scroll_tty_output = enabled ? True : False;
        XtpLog(XTP_LOG_INFO, "scrollback", "scrollTtyOutput=%s",
               vt->vt.scroll_tty_output ? "true" : "false");
}

void
XtpVtScrollOnKeypress(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        if (vt->vt.scroll_key && ScrollViewportToBottom(vt))
                XtpVtUpdate(widget);
}

void
XtpVtSetTerminal(Widget widget, XtpTerminal *terminal)
{
        Vt100Rec *vt = VtAsRecord(widget);

        vt->vt.terminal = terminal;
        if (terminal == NULL) {
                XtpLog(XTP_LOG_INFO, "terminal", "bound terminal=no");
                return;
        }
        if (XtpTerminalSetCursorBlinkDefault(terminal,
                                             CursorBlinkDefault(vt->vt.cursor_blink_policy)) != 0)
                XtpLog(XTP_LOG_ERROR, "render", "cannot apply cursorBlink=%s",
                       vt->vt.cursor_blink_name);
        if (XtpTerminalSetScrollbackLines(terminal, (size_t)vt->vt.save_lines) != 0)
                XtpLog(XTP_LOG_ERROR, "scrollback", "cannot set history limit=%d",
                       vt->vt.save_lines);
        if (XtpTerminalSetCharClass(terminal, vt->vt.char_class) != 0)
                XtpLog(XTP_LOG_ERROR, "selection", "cannot apply charClass=%s",
                       vt->vt.char_class != NULL ? vt->vt.char_class : "(default)");
        EnsureScrollbar(vt);
        if (vt->vt.scroll_bar)
                XtManageChild(vt->vt.scrollbar);
        LayoutScrollbar(vt);
        VtUpdateScrollbar(vt);
        XtpLog(XTP_LOG_INFO, "terminal", "bound terminal=yes");
        XtpVtRedraw(widget);
}

void
XtpVtUpdate(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        if (vt->vt.viewport_update_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.viewport_update_timer);
                vt->vt.viewport_update_timer = (XtIntervalId)0;
                vt->vt.viewport_updates_coalesced = 0;
        }
        if (!XtIsRealized(widget) || vt->vt.terminal == NULL)
                return;
        XtpLog(XTP_LOG_DEBUG, "render", "dirty update requested");
        if (VtRenderTerminal(vt, False) != 0)
                XtpLog(XTP_LOG_ERROR, "render", "dirty update failed");
}

void
XtpVtRedraw(Widget widget)
{
        Vt100Rec *vt = VtAsRecord(widget);

        if (!XtIsRealized(widget))
                return;
        XtpLog(XTP_LOG_DEBUG, "render", "redraw requested cache=%s",
               vt->vt.frame_valid ? "valid" : "invalid");
        if (vt->vt.frame_valid)
                VtRepaintCached(vt, NULL);
        else if (VtRenderTerminal(vt, True) != 0)
                VtPlaceholder(vt);
}
