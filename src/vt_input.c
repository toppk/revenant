#include "vt_widgetP.h"

#include "diagnostics.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <string.h>

static XtpKey
KeyFromKeysym(KeySym keysym)
{
        if (keysym >= XK_a && keysym <= XK_z)
                return (XtpKey)(XTP_KEY_A + (keysym - XK_a));
        if (keysym >= XK_A && keysym <= XK_Z)
                return (XtpKey)(XTP_KEY_A + (keysym - XK_A));
        if (keysym >= XK_0 && keysym <= XK_9)
                return (XtpKey)(XTP_KEY_0 + (keysym - XK_0));
        if (keysym >= XK_F1 && keysym <= XK_F25)
                return (XtpKey)(XTP_KEY_F1 + (keysym - XK_F1));

        switch (keysym) {
        case XK_grave:
                return XTP_KEY_BACKQUOTE;
        case XK_backslash:
                return XTP_KEY_BACKSLASH;
        case XK_bracketleft:
                return XTP_KEY_BRACKET_LEFT;
        case XK_bracketright:
                return XTP_KEY_BRACKET_RIGHT;
        case XK_comma:
                return XTP_KEY_COMMA;
        case XK_equal:
                return XTP_KEY_EQUAL;
        case XK_minus:
                return XTP_KEY_MINUS;
        case XK_period:
                return XTP_KEY_PERIOD;
        case XK_apostrophe:
                return XTP_KEY_QUOTE;
        case XK_semicolon:
                return XTP_KEY_SEMICOLON;
        case XK_slash:
                return XTP_KEY_SLASH;
        case XK_Alt_L:
                return XTP_KEY_ALT_LEFT;
        case XK_Alt_R:
                return XTP_KEY_ALT_RIGHT;
        case XK_BackSpace:
                return XTP_KEY_BACKSPACE;
        case XK_Caps_Lock:
                return XTP_KEY_CAPS_LOCK;
        case XK_Menu:
                return XTP_KEY_CONTEXT_MENU;
        case XK_Control_L:
                return XTP_KEY_CONTROL_LEFT;
        case XK_Control_R:
                return XTP_KEY_CONTROL_RIGHT;
        case XK_Return:
                return XTP_KEY_ENTER;
        case XK_Meta_L:
        case XK_Super_L:
                return XTP_KEY_META_LEFT;
        case XK_Meta_R:
        case XK_Super_R:
                return XTP_KEY_META_RIGHT;
        case XK_Shift_L:
                return XTP_KEY_SHIFT_LEFT;
        case XK_Shift_R:
                return XTP_KEY_SHIFT_RIGHT;
        case XK_space:
                return XTP_KEY_SPACE;
        case XK_Tab:
        case XK_ISO_Left_Tab:
                return XTP_KEY_TAB;
        case XK_Delete:
                return XTP_KEY_DELETE;
        case XK_End:
                return XTP_KEY_END;
        case XK_Help:
                return XTP_KEY_HELP;
        case XK_Home:
                return XTP_KEY_HOME;
        case XK_Insert:
                return XTP_KEY_INSERT;
        case XK_Page_Down:
                return XTP_KEY_PAGE_DOWN;
        case XK_Page_Up:
                return XTP_KEY_PAGE_UP;
        case XK_Down:
                return XTP_KEY_ARROW_DOWN;
        case XK_Left:
                return XTP_KEY_ARROW_LEFT;
        case XK_Right:
                return XTP_KEY_ARROW_RIGHT;
        case XK_Up:
                return XTP_KEY_ARROW_UP;
        case XK_Num_Lock:
                return XTP_KEY_NUM_LOCK;
        case XK_KP_0:
        case XK_KP_Insert:
                return XTP_KEY_NUMPAD_0;
        case XK_KP_1:
        case XK_KP_End:
                return XTP_KEY_NUMPAD_1;
        case XK_KP_2:
        case XK_KP_Down:
                return XTP_KEY_NUMPAD_2;
        case XK_KP_3:
        case XK_KP_Page_Down:
                return XTP_KEY_NUMPAD_3;
        case XK_KP_4:
        case XK_KP_Left:
                return XTP_KEY_NUMPAD_4;
        case XK_KP_5:
        case XK_KP_Begin:
                return XTP_KEY_NUMPAD_5;
        case XK_KP_6:
        case XK_KP_Right:
                return XTP_KEY_NUMPAD_6;
        case XK_KP_7:
        case XK_KP_Home:
                return XTP_KEY_NUMPAD_7;
        case XK_KP_8:
        case XK_KP_Up:
                return XTP_KEY_NUMPAD_8;
        case XK_KP_9:
        case XK_KP_Page_Up:
                return XTP_KEY_NUMPAD_9;
        case XK_KP_Add:
                return XTP_KEY_NUMPAD_ADD;
        case XK_KP_Decimal:
        case XK_KP_Delete:
                return XTP_KEY_NUMPAD_DECIMAL;
        case XK_KP_Divide:
                return XTP_KEY_NUMPAD_DIVIDE;
        case XK_KP_Enter:
                return XTP_KEY_NUMPAD_ENTER;
        case XK_KP_Equal:
                return XTP_KEY_NUMPAD_EQUAL;
        case XK_KP_Multiply:
                return XTP_KEY_NUMPAD_MULTIPLY;
        case XK_KP_Subtract:
                return XTP_KEY_NUMPAD_SUBTRACT;
        case XK_KP_Separator:
                return XTP_KEY_NUMPAD_SEPARATOR;
        case XK_KP_F1:
                return XTP_KEY_F1;
        case XK_KP_F2:
                return XTP_KEY_F2;
        case XK_KP_F3:
                return XTP_KEY_F3;
        case XK_KP_F4:
                return XTP_KEY_F4;
        case XK_Escape:
                return XTP_KEY_ESCAPE;
        case XK_Print:
                return XTP_KEY_PRINT_SCREEN;
        case XK_Scroll_Lock:
                return XTP_KEY_SCROLL_LOCK;
        case XK_Pause:
                return XTP_KEY_PAUSE;
        default:
                return XTP_KEY_UNIDENTIFIED;
        }
}

static unsigned int
KeyModifiers(unsigned int state)
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

static bool
PrintableAsciiKeysym(KeySym keysym, char *text)
{
        if (keysym < XK_space || keysym > XK_asciitilde)
                return false;
        *text = (char)keysym;
        return true;
}

static uint32_t
KeysymCodepoint(KeySym keysym)
{
        uint32_t codepoint;

        if (keysym >= 0x20 && keysym <= 0xff)
                codepoint = (uint32_t)keysym;
        else if ((keysym & 0xff000000UL) == 0x01000000UL)
                codepoint = (uint32_t)(keysym & 0x00ffffffUL);
        else
                return 0;
        if (codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU))
                return 0;
        return codepoint;
}

static int
EncodeCodepoint(uint32_t codepoint, char text[4])
{
        if (codepoint <= 0x7fU) {
                text[0] = (char)codepoint;
                return 1;
        }
        if (codepoint <= 0x7ffU) {
                text[0] = (char)(0xc0U | (codepoint >> 6));
                text[1] = (char)(0x80U | (codepoint & 0x3fU));
                return 2;
        }
        if (codepoint <= 0xffffU) {
                text[0] = (char)(0xe0U | (codepoint >> 12));
                text[1] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
                text[2] = (char)(0x80U | (codepoint & 0x3fU));
                return 3;
        }
        text[0] = (char)(0xf0U | (codepoint >> 18));
        text[1] = (char)(0x80U | ((codepoint >> 12) & 0x3fU));
        text[2] = (char)(0x80U | ((codepoint >> 6) & 0x3fU));
        text[3] = (char)(0x80U | (codepoint & 0x3fU));
        return 4;
}

static const char *
KeyActionName(XtpKeyAction action)
{
        switch (action) {
        case XTP_KEY_ACTION_PRESS:
                return "press";
        case XTP_KEY_ACTION_REPEAT:
                return "repeat";
        case XTP_KEY_ACTION_RELEASE:
                return "release";
        }
        return "unknown";
}

static void
KeyEvent(Vt100Rec *vt, XKeyEvent *xkey, XtpKeyAction action)
{
        char text[128];
        char encoded[256];
        KeySym keysym = NoSymbol;
        KeySym physical = XLookupKeysym(xkey, 0);
        Status status = XLookupNone;
        int length;
        size_t written = 0;
        XtpKeyEvent event = {0};

        if (action != XTP_KEY_ACTION_RELEASE && vt->vt.input_context != NULL) {
                length = Xutf8LookupString(vt->vt.input_context, xkey, text, (int)sizeof(text),
                                           &keysym, &status);
        } else {
                length = XLookupString(xkey, text, (int)sizeof(text), &keysym, NULL);
                status = length > 0 ? XLookupBoth : XLookupKeySym;
        }
        if (status == XBufferOverflow)
                return;
        if (action != XTP_KEY_ACTION_RELEASE && vt->vt.input_context == NULL) {
                uint32_t codepoint = KeysymCodepoint(keysym);

                if (codepoint >= 0x80U) {
                        length = EncodeCodepoint(codepoint, text);
                        status = XLookupBoth;
                }
        }

        event.action = action;
        event.key = KeyFromKeysym(physical != NoSymbol ? physical : keysym);
        event.modifiers = KeyModifiers(xkey->state);
        if (event.key == XTP_KEY_SHIFT_LEFT || event.key == XTP_KEY_SHIFT_RIGHT) {
                event.modifiers |= XTP_MOD_SHIFT;
        } else if (event.key == XTP_KEY_CONTROL_LEFT || event.key == XTP_KEY_CONTROL_RIGHT) {
                event.modifiers |= XTP_MOD_CONTROL;
        } else if (event.key == XTP_KEY_ALT_LEFT || event.key == XTP_KEY_ALT_RIGHT) {
                event.modifiers |= XTP_MOD_ALT;
        } else if (event.key == XTP_KEY_META_LEFT || event.key == XTP_KEY_META_RIGHT) {
                event.modifiers |= XTP_MOD_SUPER;
        }
        if ((status == XLookupChars || status == XLookupBoth) && length > 0 &&
            (unsigned char)text[0] >= 0x20U && (unsigned char)text[0] != 0x7fU) {
                event.utf8 = text;
                event.utf8_length = (size_t)length;
        } else if ((xkey->state & ControlMask) != 0 &&
                   PrintableAsciiKeysym(keysym != NoSymbol ? keysym : physical, text)) {
                /*
                 * XLookupString collapses combinations such as Ctrl-I to their
                 * C0 byte.  The Ghostty encoder needs the printable logical key
                 * to distinguish Ctrl-I from Tab using its fixterms fallback.
                 */
                event.utf8 = text;
                event.utf8_length = 1;
        }
        event.unshifted_codepoint = KeysymCodepoint(physical);

        XtpLog(XTP_LOG_DEBUG, "input",
               "key action=%s keycode=%u keysym=0x%lx physical=0x%lx mapped=%d state=0x%x "
               "text-bytes=%zu",
               KeyActionName(action), xkey->keycode, keysym, physical, (int)event.key, xkey->state,
               event.utf8_length);

        if (vt->vt.terminal != NULL &&
            XtpTerminalEncodeKey(vt->vt.terminal, &event, encoded, sizeof(encoded), &written) ==
                0 &&
            written != 0) {
                XtpEncodedInput input = {(const uint8_t *)encoded, written};

                if (action != XTP_KEY_ACTION_RELEASE)
                        XtpVtScrollOnKeypress((Widget)vt);
                XtCallCallbacks((Widget)vt, XtNinputCallback, &input);
        }
}

static bool
KeycodeSet(const uint8_t keycodes[32], unsigned int keycode)
{
        return keycode < 256U && (keycodes[keycode / 8U] & (uint8_t)(1U << (keycode % 8U))) != 0;
}

static void
SetKeycode(uint8_t keycodes[32], unsigned int keycode, bool set)
{
        uint8_t mask;

        if (keycode >= 256U)
                return;
        mask = (uint8_t)(1U << (keycode % 8U));
        if (set)
                keycodes[keycode / 8U] |= mask;
        else
                keycodes[keycode / 8U] &= (uint8_t)~mask;
}

static bool
ClassicAutoRepeatRelease(Vt100Rec *vt, const XKeyEvent *release)
{
        XEvent next;
        Display *display = XtDisplay((Widget)vt);

        if (vt->vt.detectable_autorepeat || XEventsQueued(display, QueuedAfterReading) == 0)
                return false;
        XPeekEvent(display, &next);
        return next.type == KeyPress && next.xkey.keycode == release->keycode &&
               next.xkey.time == release->time;
}

static bool
TranslationOwnsKey(const XKeyEvent *event)
{
        /* Keep these Shift gestures aligned with translations[] in vt_widget.c. */
        static const KeySym translated_keys[] = {
            XK_Insert, XK_Page_Up, XK_Page_Down, XK_KP_Add, XK_KP_Subtract,
        };
        KeySym physical;
        size_t index;

        if (event == NULL || (event->state & ShiftMask) == 0)
                return false;
        physical = XLookupKeysym((XKeyEvent *)event, 0);
        for (index = 0; index < XtNumber(translated_keys); ++index) {
                if (physical == translated_keys[index])
                        return true;
        }
        return false;
}

static void
SetFocus(Vt100Rec *vt, Boolean focused)
{
        Widget widget = (Widget)vt;
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
                VtEraseLastCursor(vt);
        VtStopCursorBlink(vt);
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
                VtDrawCursor(vt, True, column, row, vt->vt.last_cursor_shape);
                XtpLog(XTP_LOG_DEBUG, "render", "cursor-only repaint column=%u row=%u", column,
                       row);
                XFlush(XtDisplay(widget));
        }
        VtScheduleCursorBlink(vt);
}

static void
InputEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        Vt100Rec *vt = closure;

        (void)widget;
        (void)continue_dispatch;
        if (event->type == FocusIn) {
                XtpLog(XTP_LOG_DEBUG, "input", "focus in event-window=0x%lx ic-focus-window=0x%lx",
                       event->xfocus.window, vt->vt.input_window);
                SetFocus(vt, True);
                if (vt->vt.input_context != NULL)
                        XSetICFocus(vt->vt.input_context);
        } else if (event->type == FocusOut) {
                XtpLog(XTP_LOG_DEBUG, "input", "focus out event-window=0x%lx ic-focus-window=0x%lx",
                       event->xfocus.window, vt->vt.input_window);
                SetFocus(vt, False);
                if (vt->vt.input_context != NULL)
                        XUnsetICFocus(vt->vt.input_context);
                memset(vt->vt.pressed_keycodes, 0, sizeof(vt->vt.pressed_keycodes));
                memset(vt->vt.filtered_keycodes, 0, sizeof(vt->vt.filtered_keycodes));
        } else if (event->type == KeyPress || event->type == KeyRelease) {
                XtpKeyAction action;
                bool filtered = XFilterEvent(event, vt->vt.input_window);

                if (event->type == KeyPress && filtered) {
                        SetKeycode(vt->vt.filtered_keycodes, event->xkey.keycode, true);
                        return;
                }
                if (event->type == KeyRelease &&
                    (filtered || KeycodeSet(vt->vt.filtered_keycodes, event->xkey.keycode))) {
                        SetKeycode(vt->vt.filtered_keycodes, event->xkey.keycode, false);
                        SetKeycode(vt->vt.pressed_keycodes, event->xkey.keycode, false);
                        return;
                }
                if (filtered)
                        return;

                if (event->type == KeyRelease && ClassicAutoRepeatRelease(vt, &event->xkey)) {
                        XtpLog(XTP_LOG_DEBUG, "input",
                               "key release suppressed for classic autorepeat keycode=%u",
                               event->xkey.keycode);
                        return;
                }
                if (event->type == KeyRelease) {
                        action = XTP_KEY_ACTION_RELEASE;
                        SetKeycode(vt->vt.pressed_keycodes, event->xkey.keycode, false);
                } else if (KeycodeSet(vt->vt.pressed_keycodes, event->xkey.keycode)) {
                        action = XTP_KEY_ACTION_REPEAT;
                } else {
                        action = XTP_KEY_ACTION_PRESS;
                        SetKeycode(vt->vt.pressed_keycodes, event->xkey.keycode, true);
                }
                if (VtLocalKeyActionOwnsEvent(vt, &event->xkey, event->type == KeyRelease)) {
                        XtpLog(XTP_LOG_DEBUG, "input", "key %s owned by local Xt action",
                               KeyActionName(action));
                } else if (TranslationOwnsKey(&event->xkey)) {
                        XtpLog(XTP_LOG_DEBUG, "input", "key %s reserved for Xt translation",
                               KeyActionName(action));
                } else {
                        KeyEvent(vt, &event->xkey, action);
                }
        }
}

void
VtInitializeInput(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;
        Widget top = widget;
        Bool supported = False;

        while (XtParent(top) != NULL)
                top = XtParent(top);
        vt->vt.input_window = XtWindow(top);
        vt->vt.detectable_autorepeat =
            XkbSetDetectableAutoRepeat(XtDisplay(widget), True, &supported) && supported;
        XtpLog(XTP_LOG_INFO, "input", "detectable autorepeat=%s",
               vt->vt.detectable_autorepeat ? "enabled" : "unavailable");

        vt->vt.input_method = XOpenIM(XtDisplay(widget), NULL, NULL, NULL);
        if (vt->vt.input_method != NULL) {
                vt->vt.input_context = XCreateIC(
                    vt->vt.input_method, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, vt->vt.input_window, XNFocusWindow, vt->vt.input_window, NULL);
        }
        XtpLog(XTP_LOG_DEBUG, "input", "input-method=%s input-context=%s",
               vt->vt.input_method != NULL ? "open" : "unavailable",
               vt->vt.input_context != NULL ? "created" : "unavailable");
        XtAddEventHandler(widget, KeyPressMask | KeyReleaseMask | FocusChangeMask, False,
                          InputEvent, vt);
}

void
VtDestroyInput(Vt100Rec *vt)
{
        if (vt->vt.input_context != NULL) {
                XDestroyIC(vt->vt.input_context);
                vt->vt.input_context = NULL;
        }
        if (vt->vt.input_method != NULL) {
                XCloseIM(vt->vt.input_method);
                vt->vt.input_method = NULL;
        }
        vt->vt.input_window = None;
}
