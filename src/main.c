#include "command_options.h"
#include "menus.h"
#include "config_report.h"
#include "diagnostics.h"
#include "pty_process.h"
#include "selftest.h"
#include "terminal.h"
#include "version.h"
#include "vt_widget.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
        XtAppContext context;
        Display *display;
        Widget shell;
        Widget vt;
        XtpMenus menus;
        XtpTerminal *terminal;
        XtpPty *pty;
        XtInputId pty_input;
        XtInputId pty_output;
        XIM input_method;
        XIC input_context;
        Atom wm_delete_window;
        uint8_t pressed_keycodes[32];
        uint8_t filtered_keycodes[32];
        Boolean detectable_autorepeat;
        Boolean running;
} App;

typedef struct
{
        const char *menu_name;
        const char *entry_name;
        XtpTerminalMode mode;
} TerminalModeMenuItem;

static const TerminalModeMenuItem terminal_mode_menu_items[] = {
    {"mainMenu", "backarrow key", XTP_TERMINAL_MODE_BACKARROW_KEY},
    {"mainMenu", "num-lock", XTP_TERMINAL_MODE_NUMLOCK_KEYPAD},
    {"mainMenu", "alt-esc", XTP_TERMINAL_MODE_ALT_SENDS_ESCAPE},
    {"vtMenu", "autowrap", XTP_TERMINAL_MODE_AUTOWRAP},
    {"vtMenu", "reversewrap", XTP_TERMINAL_MODE_REVERSE_WRAP},
    {"vtMenu", "autolinefeed", XTP_TERMINAL_MODE_AUTOLINEFEED},
    {"vtMenu", "appcursor", XTP_TERMINAL_MODE_APPLICATION_CURSOR},
    {"vtMenu", "appkeypad", XTP_TERMINAL_MODE_APPLICATION_KEYPAD},
};

typedef struct
{
        String menu_locale;
        Boolean debug;
        Boolean report_config;
} AppResources;

static XtResource application_resources[] = {
    {
        "menuLocale",
        "MenuLocale",
        XtRString,
        sizeof(String),
        XtOffsetOf(AppResources, menu_locale),
        XtRString,
        (XtPointer) "C",
    },
    {
        "debug",
        "Debug",
        XtRBoolean,
        sizeof(Boolean),
        XtOffsetOf(AppResources, debug),
        XtRImmediate,
        (XtPointer)False,
    },
    {
        "reportConfig",
        "ReportConfig",
        XtRBoolean,
        sizeof(Boolean),
        XtOffsetOf(AppResources, report_config),
        XtRImmediate,
        (XtPointer)False,
    },
};

static const char *const fallback_resources[] = {
    "*SimpleMenu*menuLabel.vertSpace: 100",
    "*SimpleMenu*HorizontalMargins: 16",
    "*SimpleMenu*Sme.height: 16",
    "*SimpleMenu*Cursor: left_ptr",
    "*mainMenu.Label:  Main Options",
    "*vtMenu.Label:  VT Options",
    "*fontMenu.Label:  VT Fonts",
    "*title: xterm+",
    NULL,
};

static void UpdateGeometry(App *app);

typedef struct
{
        const char *name;
        const char *class_name;
} ResourceProbe;

static const ResourceProbe resource_probes[] = {
    {"xterm.geometry", "XTerm.Geometry"},
    {"xterm.menuLocale", "XTerm.MenuLocale"},
    {"xterm.debug", "XTerm.Debug"},
    {"xterm.vt100.background", "XTerm.VT100.Background"},
    {"xterm.vt100.foreground", "XTerm.VT100.Foreground"},
    {"xterm.vt100.font", "XTerm.VT100.Font"},
    {"xterm.vt100.font1", "XTerm.VT100.Font1"},
    {"xterm.vt100.font2", "XTerm.VT100.Font2"},
    {"xterm.vt100.font3", "XTerm.VT100.Font3"},
    {"xterm.vt100.font4", "XTerm.VT100.Font4"},
    {"xterm.vt100.font5", "XTerm.VT100.Font5"},
    {"xterm.vt100.font6", "XTerm.VT100.Font6"},
    {"xterm.vt100.font7", "XTerm.VT100.Font7"},
    {"xterm.vt100.faceName", "XTerm.VT100.FaceName"},
    {"xterm.vt100.faceNameDoublesize", "XTerm.VT100.FaceNameDoublesize"},
    {"xterm.vt100.faceSize", "XTerm.VT100.FaceSize"},
    {"xterm.vt100.renderFont", "XTerm.VT100.RenderFont"},
    {"xterm.vt100.internalBorder", "XTerm.VT100.BorderWidth"},
    {"xterm.vt100.saveLines", "XTerm.VT100.SaveLines"},
    {"xterm.vt100.scrollBar", "XTerm.VT100.ScrollBar"},
    {"xterm.vt100.rightScrollBar", "XTerm.VT100.RightScrollBar"},
    {"xterm.vt100.cursorColor", "XTerm.VT100.CursorColor"},
    {"xterm.vt100.alwaysHighlight", "XTerm.VT100.AlwaysHighlight"},
    {"xterm.vt100.cursorBlink", "XTerm.VT100.CursorBlink"},
    {"xterm.vt100.cursorBlinkXOR", "XTerm.VT100.CursorBlinkXOR"},
    {"xterm.vt100.cursorOnTime", "XTerm.VT100.CursorOnTime"},
    {"xterm.vt100.cursorOffTime", "XTerm.VT100.CursorOffTime"},
    {"xterm.vt100.pointerColor", "XTerm.VT100.PointerColor"},
    {"xterm.vt100.pointerShape", "XTerm.VT100.PointerShape"},
    {"xterm.vt100.color0", "XTerm.VT100.Color"},
    {"xterm.vt100.color1", "XTerm.VT100.Color"},
    {"xterm.vt100.color2", "XTerm.VT100.Color"},
    {"xterm.vt100.color3", "XTerm.VT100.Color"},
    {"xterm.vt100.color4", "XTerm.VT100.Color"},
    {"xterm.vt100.color5", "XTerm.VT100.Color"},
    {"xterm.vt100.color6", "XTerm.VT100.Color"},
    {"xterm.vt100.color7", "XTerm.VT100.Color"},
    {"xterm.vt100.color8", "XTerm.VT100.Color"},
    {"xterm.vt100.color9", "XTerm.VT100.Color"},
    {"xterm.vt100.color10", "XTerm.VT100.Color"},
    {"xterm.vt100.color11", "XTerm.VT100.Color"},
    {"xterm.vt100.color12", "XTerm.VT100.Color"},
    {"xterm.vt100.color13", "XTerm.VT100.Color"},
    {"xterm.vt100.color14", "XTerm.VT100.Color"},
    {"xterm.vt100.color15", "XTerm.VT100.Color"},
};

static void
SetEarlyDebug(int argc, char **argv)
{
        int argument;
        int enabled = 0;

        for (argument = 1; argument < argc; ++argument) {
                if (strcmp(argv[argument], "-e") == 0)
                        break;
                if (strcmp(argv[argument], "-debug") == 0)
                        enabled = 1;
                else if (strcmp(argv[argument], "+debug") == 0)
                        enabled = 0;
        }
        XtpLogSetDebug(enabled);
}

static void
LogCommandLine(int argc, char **argv)
{
        int argument;

        XtpLog(XTP_LOG_INFO, "startup", "command-line argc=%d", argc);
        for (argument = 0; argument < argc; ++argument)
                XtpLog(XTP_LOG_DEBUG, "startup", "argv[%d]=%s", argument, argv[argument]);
}

static Boolean
RelevantServerResource(const char *line)
{
        return strncmp(line, "XTerm", 5) == 0 || strncmp(line, "Xterm", 5) == 0 ||
               strncmp(line, "xterm", 5) == 0 || strncmp(line, "Xft.", 4) == 0 ||
               strncmp(line, "Xcursor.", 8) == 0;
}

static void
LogResourceDatabases(Display *display)
{
        const char *manager = XResourceManagerString(display);
        char *copy = manager != NULL ? strdup(manager) : NULL;
        char *line;
        char *state = NULL;
        size_t probe;
        XrmDatabase database = XtDatabase(display);

        XtpLog(XTP_LOG_DEBUG, "xresource", "RESOURCE_MANAGER present=%s",
               manager != NULL ? "true" : "false");
        for (line = copy != NULL ? strtok_r(copy, "\n", &state) : NULL; line != NULL;
             line = strtok_r(NULL, "\n", &state)) {
                if (RelevantServerResource(line))
                        XtpLog(XTP_LOG_DEBUG, "xresource", "server %s", line);
        }
        free(copy);

        for (probe = 0; probe < XtNumber(resource_probes); ++probe) {
                XrmValue value;
                String type = NULL;

                if (XrmGetResource(database, resource_probes[probe].name,
                                   resource_probes[probe].class_name, &type, &value)) {
                        int length = (int)value.size;

                        if (length > 0 && ((const char *)value.addr)[length - 1] == '\0')
                                --length;
                        XtpLog(XTP_LOG_DEBUG, "xresource",
                               "merged name=%s class=%s type=%s value=%.*s",
                               resource_probes[probe].name, resource_probes[probe].class_name,
                               type != NULL ? type : "(null)", length, (const char *)value.addr);
                }
        }
        XtpLog(XTP_LOG_INFO, "config",
               "resource precedence effective=command-line > server RESOURCE_MANAGER > "
               "app-defaults/fallbacks > compiled resource defaults");
        XtpLog(XTP_LOG_WARNING, "compat",
               "renderer is resolved by the VT100 widget; cursorColor is applied; color0..color15, "
               "pointerColor, and pointerShape are merged but not applied yet");
        XtpLog(XTP_LOG_INFO, "scrollback",
               "saveLines, scrollbar visibility/side, wheel navigation, thumb dragging, and "
               "scroll-back/scroll-forw actions are active");
}

static void PtyOutputReady(XtPointer closure, int *source, XtInputId *input_id);

static void
WatchPtyOutput(App *app)
{
        if (app->pty_output == (XtInputId)0)
                app->pty_output =
                    XtAppAddInput(app->context, XtpPtyFd(app->pty),
                                  (XtPointer)(uintptr_t)XtInputWriteMask, PtyOutputReady, app);
}

static void
StopWatchingPtyOutput(App *app)
{
        if (app->pty_output != (XtInputId)0) {
                XtRemoveInput(app->pty_output);
                app->pty_output = (XtInputId)0;
        }
}

static void
FlushPtyOutput(App *app)
{
        int result;

        if (app->pty == NULL)
                return;
        result = XtpPtyFlush(app->pty);
        if (result > 0) {
                WatchPtyOutput(app);
        } else if (result == 0) {
                StopWatchingPtyOutput(app);
        } else {
                XtpLog(XTP_LOG_ERROR, "pty", "write failed errno=%d queued=%zu", errno,
                       XtpPtyPending(app->pty));
                StopWatchingPtyOutput(app);
                app->running = False;
        }
}

static void
WritePtyBytes(App *app, const uint8_t *bytes, size_t length)
{
        size_t before;

        if (app->pty == NULL || length == 0)
                return;
        before = XtpPtyPending(app->pty);
        if (XtpPtyQueue(app->pty, bytes, length) != 0) {
                XtpLog(XTP_LOG_ERROR, "pty", "cannot queue write bytes=%zu errno=%d", length,
                       errno);
                app->running = False;
                return;
        }
        XtpLog(XTP_LOG_DEBUG, "pty", "write queued=%zu pending=%zu->%zu", length, before,
               XtpPtyPending(app->pty));
        FlushPtyOutput(app);
}

static void
PtyOutputReady(XtPointer closure, int *source, XtInputId *input_id)
{
        App *app = closure;

        (void)source;
        (void)input_id;
        FlushPtyOutput(app);
}

static void
TerminalWritePty(const uint8_t *bytes, size_t length, void *closure)
{
        WritePtyBytes(closure, bytes, length);
}

static void
PasteReceived(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpPaste *paste = call_data;
        uint8_t *encoded = NULL;
        size_t length = 0;

        (void)widget;
        if (paste == NULL || XtpTerminalEncodePaste(app->terminal, paste->bytes, paste->length,
                                                    &encoded, &length) != 0) {
                XBell(app->display, 0);
                return;
        }
        XtpVtScrollOnKeypress(app->vt);
        WritePtyBytes(app, encoded, length);
        XtpLog(XTP_LOG_INFO, "selection", "paste encoded input=%zu output=%zu", paste->length,
               length);
        free(encoded);
}

static void
EncodedInputReceived(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpEncodedInput *input = call_data;

        (void)widget;
        if (input != NULL && input->bytes != NULL && input->length != 0)
                WritePtyBytes(app, input->bytes, input->length);
}

static void
TerminalBell(void *closure)
{
        App *app = closure;

        XBell(app->display, 0);
        XtpLog(XTP_LOG_INFO, "shell", "bell requested");
}

static void
TerminalTitle(const char *title, size_t length, void *closure)
{
        App *app = closure;
        char *value = malloc(length + 1U);
        char preview[129];
        size_t shown = length < sizeof(preview) - 1U ? length : sizeof(preview) - 1U;
        size_t index;

        if (value == NULL)
                return;
        memcpy(value, title, length);
        value[length] = '\0';
        for (index = 0; index < shown; ++index) {
                unsigned char byte = (unsigned char)title[index];

                preview[index] = byte >= 0x20U && byte != 0x7fU ? (char)byte : '.';
        }
        preview[shown] = '\0';
        XtVaSetValues(app->shell, XtNtitle, value, NULL);
        XtpLog(XTP_LOG_INFO, "shell", "title changed bytes=%zu preview=%s%s", length, preview,
               shown < length ? "..." : "");
        free(value);
}

static void
FontChanged(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpFontChanged *change = call_data;

        (void)widget;
        XtpLog(XTP_LOG_INFO, "font", "callback renderer=%s cell=%ux%u grid=%ux%u",
               XtpVtRendererName(app->vt), change->cell_width, change->cell_height,
               XtpVtColumns(app->vt), XtpVtRows(app->vt));
        if (app->pty != NULL &&
            XtpPtyResize(app->pty, (uint16_t)XtpVtColumns(app->vt), (uint16_t)XtpVtRows(app->vt),
                         change->cell_width, change->cell_height) != 0)
                XBell(app->display, 0);
        if (XtpTerminalResize(app->terminal, (uint16_t)XtpVtColumns(app->vt),
                              (uint16_t)XtpVtRows(app->vt), change->cell_width,
                              change->cell_height) != 0) {
                XBell(app->display, 0);
        }
        UpdateGeometry(app);
        XtpMenusSetRenderFont(&app->menus, XtpVtUsingXft(app->vt), XtpVtXftAvailable(app->vt));
}

static void
SizeChanged(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpSizeChanged *change = call_data;

        (void)widget;
        XtpLog(XTP_LOG_INFO, "resize", "callback grid=%ux%u cell=%ux%u", change->columns,
               change->rows, change->cell_width, change->cell_height);
        if (app->pty != NULL &&
            XtpPtyResize(app->pty, (uint16_t)change->columns, (uint16_t)change->rows,
                         change->cell_width, change->cell_height) != 0)
                XBell(app->display, 0);
        if (app->terminal != NULL &&
            XtpTerminalResize(app->terminal, (uint16_t)change->columns, (uint16_t)change->rows,
                              change->cell_width, change->cell_height) != 0)
                XBell(app->display, 0);
}

static void
PopupRequested(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpPopupMenu *popup = call_data;

        (void)widget;
        {
                size_t index;

                for (index = 0; index < XtNumber(terminal_mode_menu_items); ++index) {
                        const TerminalModeMenuItem *item = &terminal_mode_menu_items[index];
                        bool enabled;

                        if (app->terminal != NULL &&
                            XtpTerminalGetMode(app->terminal, item->mode, &enabled) == 0)
                                XtpMenusSetChecked(&app->menus, item->menu_name, item->entry_name,
                                                   enabled ? True : False);
                }
        }
        XtpMenusSetChecked(&app->menus, "vtMenu", "scrollkey", XtpVtScrollKey(app->vt));
        XtpMenusSetChecked(&app->menus, "vtMenu", "scrollttyoutput", XtpVtScrollTtyOutput(app->vt));
        XtpMenusSetChecked(&app->menus, "vtMenu", "selectToClipboard",
                           XtpVtSelectToClipboard(app->vt));
        XtpLog(XTP_LOG_INFO, "menu", "popup requested name=%s", popup->name);
        XtpMenusPopup(&app->menus, popup->name, popup->event);
}

static const TerminalModeMenuItem *
FindTerminalModeMenuItem(const char *menu_name, const char *entry_name)
{
        size_t index;

        for (index = 0; index < XtNumber(terminal_mode_menu_items); ++index) {
                const TerminalModeMenuItem *item = &terminal_mode_menu_items[index];

                if (strcmp(item->menu_name, menu_name) == 0 &&
                    strcmp(item->entry_name, entry_name) == 0)
                        return item;
        }
        return NULL;
}

static void
ToggleTerminalMode(App *app, const TerminalModeMenuItem *item)
{
        bool enabled;

        if (app->terminal == NULL || XtpTerminalGetMode(app->terminal, item->mode, &enabled) != 0 ||
            XtpTerminalSetMode(app->terminal, item->mode, !enabled) != 0 ||
            XtpTerminalGetMode(app->terminal, item->mode, &enabled) != 0) {
                XBell(app->display, 0);
                return;
        }
        XtpMenusSetChecked(&app->menus, item->menu_name, item->entry_name, enabled ? True : False);
        XtpLog(XTP_LOG_INFO, "menu", "mode item=%s enabled=%s", item->entry_name,
               enabled ? "true" : "false");
}

static void
ToggleScrollbar(App *app)
{
        XtpLog(XTP_LOG_INFO, "scrollbar", "menu action toggle");
        XtpVtSetScrollbar(app->vt, !XtpVtScrollbarVisible(app->vt));
        XtpMenusSetScrollbar(&app->menus, XtpVtScrollbarVisible(app->vt));
        UpdateGeometry(app);
}

static void
ToggleReverseVideo(App *app)
{
        Pixel foreground;
        Pixel background;

        XtVaGetValues(app->vt, XtNforeground, &foreground, XtNbackground, &background, NULL);
        XtVaSetValues(app->vt, XtNforeground, background, XtNbackground, foreground, NULL);
        XtpLog(XTP_LOG_INFO, "menu", "reverse-video foreground=%lu background=%lu", background,
               foreground);
        XtpVtRedraw(app->vt);
}

static void
MenuDispatch(Widget source, const char *menu_name, const char *entry_name, XtPointer closure)
{
        App *app = closure;
        const TerminalModeMenuItem *mode_item;

        (void)source;
        XtpLog(XTP_LOG_INFO, "menu", "dispatch menu=%s item=%s", menu_name, entry_name);
        mode_item = FindTerminalModeMenuItem(menu_name, entry_name);
        if (mode_item != NULL) {
                ToggleTerminalMode(app, mode_item);
                return;
        }
        if (strcmp(menu_name, "mainMenu") == 0) {
                if (strcmp(entry_name, "quit") == 0)
                        app->running = False;
                if (strcmp(entry_name, "redraw") == 0)
                        XtpVtRedraw(app->vt);
                return;
        }

        if (strcmp(menu_name, "vtMenu") == 0) {
                if (strcmp(entry_name, "scrollbar") == 0)
                        ToggleScrollbar(app);
                else if (strcmp(entry_name, "reversevideo") == 0)
                        ToggleReverseVideo(app);
                else if (strcmp(entry_name, "scrollkey") == 0) {
                        XtpVtSetScrollKey(app->vt, !XtpVtScrollKey(app->vt));
                        XtpMenusSetChecked(&app->menus, menu_name, entry_name,
                                           XtpVtScrollKey(app->vt));
                } else if (strcmp(entry_name, "scrollttyoutput") == 0) {
                        XtpVtSetScrollTtyOutput(app->vt, !XtpVtScrollTtyOutput(app->vt));
                        XtpMenusSetChecked(&app->menus, menu_name, entry_name,
                                           XtpVtScrollTtyOutput(app->vt));
                } else if (strcmp(entry_name, "selectToClipboard") == 0) {
                        XtpVtSetSelectToClipboard(app->vt, !XtpVtSelectToClipboard(app->vt));
                        XtpMenusSetChecked(&app->menus, menu_name, entry_name,
                                           XtpVtSelectToClipboard(app->vt));
                }
                return;
        }

        if (strcmp(menu_name, "fontMenu") == 0) {
                int slot = -1;

                if (strcmp(entry_name, "render-font") == 0) {
                        if (!XtpVtSetRenderFont(app->vt, !XtpVtUsingXft(app->vt)))
                                XBell(app->display, 0);
                        return;
                }
                if (strcmp(entry_name, "fontdefault") == 0) {
                        slot = 0;
                } else if (strncmp(entry_name, "font", 4) == 0 && entry_name[4] >= '1' &&
                           entry_name[4] <= '7' && entry_name[5] == '\0') {
                        slot = entry_name[4] - '0';
                }
                if (slot >= 0 && !XtpVtSelectFont(app->vt, slot))
                        XBell(app->display, 0);
        }
}

static XtpKey
KeyFromKeysym(KeySym keysym)
{
        if (keysym >= XK_a && keysym <= XK_z)
                return (XtpKey)(XTP_KEY_A + (keysym - XK_a));
        if (keysym >= XK_A && keysym <= XK_Z)
                return (XtpKey)(XTP_KEY_A + (keysym - XK_A));
        if (keysym >= XK_0 && keysym <= XK_9)
                return (XtpKey)(XTP_KEY_0 + (keysym - XK_0));
        if (keysym >= XK_F1 && keysym <= XK_F12)
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
        case XK_KP_Multiply:
                return XTP_KEY_NUMPAD_MULTIPLY;
        case XK_KP_Subtract:
                return XTP_KEY_NUMPAD_SUBTRACT;
        case XK_Escape:
                return XTP_KEY_ESCAPE;
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
KeyEvent(App *app, XKeyEvent *xkey, XtpKeyAction action)
{
        char text[128];
        char encoded[256];
        KeySym keysym = NoSymbol;
        KeySym physical = XLookupKeysym(xkey, 0);
        Status status = XLookupNone;
        int length;
        size_t written = 0;
        XtpKeyEvent event = {0};

        if ((xkey->state & ShiftMask) != 0 &&
            (physical == XK_KP_Add || physical == XK_KP_Subtract || physical == XK_Page_Up ||
             physical == XK_Page_Down)) {
                XtpLog(XTP_LOG_DEBUG, "input",
                       "key handled by VT translation keysym=0x%lx state=0x%x", physical,
                       xkey->state);
                return;
        }

        if (action != XTP_KEY_ACTION_RELEASE && app->input_context != NULL) {
                length = Xutf8LookupString(app->input_context, xkey, text, (int)sizeof(text),
                                           &keysym, &status);
        } else {
                length = XLookupString(xkey, text, (int)sizeof(text), &keysym, NULL);
                status = length > 0 ? XLookupBoth : XLookupKeySym;
        }
        if (status == XBufferOverflow)
                return;

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
        if (physical >= XK_space && physical <= XK_asciitilde)
                event.unshifted_codepoint = (uint32_t)physical;

        XtpLog(XTP_LOG_DEBUG, "input",
               "key action=%s keycode=%u keysym=0x%lx physical=0x%lx mapped=%d state=0x%x "
               "text-bytes=%zu",
               KeyActionName(action), xkey->keycode, keysym, physical, (int)event.key, xkey->state,
               event.utf8_length);

        if (XtpTerminalEncodeKey(app->terminal, &event, encoded, sizeof(encoded), &written) == 0 &&
            written != 0) {
                if (action != XTP_KEY_ACTION_RELEASE)
                        XtpVtScrollOnKeypress(app->vt);
                WritePtyBytes(app, (const uint8_t *)encoded, written);
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
ClassicAutoRepeatRelease(App *app, const XKeyEvent *release)
{
        XEvent next;

        if (app->detectable_autorepeat || XEventsQueued(app->display, QueuedAfterReading) == 0)
                return false;
        XPeekEvent(app->display, &next);
        return next.type == KeyPress && next.xkey.keycode == release->keycode &&
               next.xkey.time == release->time;
}

static bool
TranslationOwnsKey(const XKeyEvent *event)
{
        KeySym physical;

        if (event == NULL || (event->state & ShiftMask) == 0)
                return false;
        physical = XLookupKeysym((XKeyEvent *)event, 0);
        return physical == XK_Insert || physical == XK_Page_Up || physical == XK_Page_Down ||
               physical == XK_KP_Add || physical == XK_KP_Subtract;
}

static void
InputEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        App *app = closure;

        (void)widget;
        (void)continue_dispatch;
        if (event->type == FocusIn) {
                XtpLog(XTP_LOG_DEBUG, "input", "focus in event-window=0x%lx ic-focus-window=0x%lx",
                       event->xfocus.window, XtWindow(app->shell));
                XtpVtSetFocus(app->vt, True);
                if (app->input_context != NULL)
                        XSetICFocus(app->input_context);
        } else if (event->type == FocusOut) {
                XtpLog(XTP_LOG_DEBUG, "input", "focus out event-window=0x%lx ic-focus-window=0x%lx",
                       event->xfocus.window, XtWindow(app->shell));
                XtpVtSetFocus(app->vt, False);
                if (app->input_context != NULL)
                        XUnsetICFocus(app->input_context);
                memset(app->pressed_keycodes, 0, sizeof(app->pressed_keycodes));
                memset(app->filtered_keycodes, 0, sizeof(app->filtered_keycodes));
        } else if (event->type == KeyPress || event->type == KeyRelease) {
                XtpKeyAction action;
                bool filtered = XFilterEvent(event, XtWindow(app->shell));

                if (event->type == KeyPress && filtered) {
                        SetKeycode(app->filtered_keycodes, event->xkey.keycode, true);
                        return;
                }
                if (event->type == KeyRelease &&
                    (filtered || KeycodeSet(app->filtered_keycodes, event->xkey.keycode))) {
                        SetKeycode(app->filtered_keycodes, event->xkey.keycode, false);
                        SetKeycode(app->pressed_keycodes, event->xkey.keycode, false);
                        return;
                }
                if (filtered)
                        return;

                if (event->type == KeyRelease && ClassicAutoRepeatRelease(app, &event->xkey)) {
                        XtpLog(XTP_LOG_DEBUG, "input",
                               "key release suppressed for classic autorepeat keycode=%u",
                               event->xkey.keycode);
                        return;
                }
                if (event->type == KeyRelease) {
                        action = XTP_KEY_ACTION_RELEASE;
                        SetKeycode(app->pressed_keycodes, event->xkey.keycode, false);
                } else if (KeycodeSet(app->pressed_keycodes, event->xkey.keycode)) {
                        action = XTP_KEY_ACTION_REPEAT;
                } else {
                        action = XTP_KEY_ACTION_PRESS;
                        SetKeycode(app->pressed_keycodes, event->xkey.keycode, true);
                }
                if (TranslationOwnsKey(&event->xkey)) {
                        XtpLog(XTP_LOG_DEBUG, "input", "key %s reserved for Xt translation",
                               KeyActionName(action));
                } else {
                        KeyEvent(app, &event->xkey, action);
                }
        }
}

static void
PtyReady(XtPointer closure, int *source, XtInputId *input_id)
{
        App *app = closure;
        uint8_t buffer[65536];
        ssize_t amount;

        (void)source;
        do {
                amount = XtpPtyRead(app->pty, buffer, sizeof(buffer));
        } while (amount < 0 && errno == EINTR);

        if (amount > 0) {
                XtpLogBytePreview(XTP_LOG_DEBUG, "pty", "read", buffer, (size_t)amount);
                if (XtpTerminalFeedOutput(app->terminal, buffer, (size_t)amount,
                                          XtpVtScrollTtyOutput(app->vt) != False) != 0)
                        XtpLog(XTP_LOG_WARNING, "scrollback",
                               "cannot preserve tty-output viewport policy");
                XtpVtUpdate(app->vt);
                XFlush(app->display);
        } else if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
        } else {
                if (amount == 0)
                        XtpLog(XTP_LOG_INFO, "pty", "EOF");
                else
                        XtpLog(XTP_LOG_INFO, "pty", "read closed errno=%d", errno);
                XtRemoveInput(*input_id);
                app->pty_input = (XtInputId)0;
                app->running = False;
        }
}

static void
ShellEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        App *app = closure;

        (void)widget;
        (void)continue_dispatch;
        if (event->type == ClientMessage &&
            (Atom)event->xclient.data.l[0] == app->wm_delete_window) {
                XtpLog(XTP_LOG_INFO, "shell", "WM_DELETE_WINDOW");
                app->running = False;
        } else if (event->type == MapNotify) {
                XtpLog(XTP_LOG_INFO, "shell", "mapped");
        } else if (event->type == UnmapNotify) {
                XtpLog(XTP_LOG_INFO, "shell", "unmapped");
        } else if (event->type == ConfigureNotify) {
                XtpLog(XTP_LOG_DEBUG, "shell", "configure position=%d,%d pixels=%dx%d border=%d",
                       event->xconfigure.x, event->xconfigure.y, event->xconfigure.width,
                       event->xconfigure.height, event->xconfigure.border_width);
        }
}

static void
UpdateNormalHints(App *app, Dimension total_width, Dimension total_height, Dimension base_width,
                  Dimension base_height)
{
        XSizeHints hints;
        Dimension shell_width;
        Dimension shell_height;
        Dimension vt_width;
        Dimension vt_height;

        if (!XtIsRealized(app->shell))
                return;

        memset(&hints, 0, sizeof(hints));
        hints.flags = PSize | PBaseSize | PMinSize | PResizeInc;
        hints.width = total_width;
        hints.height = total_height;
        hints.base_width = base_width;
        hints.base_height = base_height;
        hints.min_width = base_width + (int)XtpVtCellWidth(app->vt);
        hints.min_height = base_height + (int)XtpVtCellHeight(app->vt);
        hints.width_inc = (int)XtpVtCellWidth(app->vt);
        hints.height_inc = (int)XtpVtCellHeight(app->vt);

        /*
         * Keep the Shell widget's private size hints synchronized with the
         * property we publish below.  XtMakeResizeRequest consults the Shell's
         * copy; updating only WM_NORMAL_HINTS lets it negotiate with stale font
         * increments and can turn a width-only font change into extra rows.
         */
        XtVaGetValues(app->shell, XtNwidth, &shell_width, XtNheight, &shell_height, NULL);
        XtVaGetValues(app->vt, XtNwidth, &vt_width, XtNheight, &vt_height, NULL);
        XtpLog(XTP_LOG_DEBUG, "shell", "size hints before shell=%ux%u vt=%ux%u target=%ux%u",
               shell_width, shell_height, vt_width, vt_height, total_width, total_height);
        XtVaSetValues(app->shell, XtNbaseWidth, (int)base_width, XtNbaseHeight, (int)base_height,
                      XtNminWidth, hints.min_width, XtNminHeight, hints.min_height, XtNwidthInc,
                      hints.width_inc, XtNheightInc, hints.height_inc, NULL);
        XtVaGetValues(app->shell, XtNwidth, &shell_width, XtNheight, &shell_height, NULL);
        XtVaGetValues(app->vt, XtNwidth, &vt_width, XtNheight, &vt_height, NULL);
        XtpLog(XTP_LOG_DEBUG, "shell", "size hints after shell=%ux%u vt=%ux%u", shell_width,
               shell_height, vt_width, vt_height);
        XSetWMNormalHints(app->display, XtWindow(app->shell), &hints);
        XtpLog(XTP_LOG_DEBUG, "shell", "WM_NORMAL_HINTS size=%ux%u base=%ux%u increment=%ux%u",
               total_width, total_height, base_width, base_height, XtpVtCellWidth(app->vt),
               XtpVtCellHeight(app->vt));
}

static void
UpdateGeometry(App *app)
{
        Dimension vt_width = XtpVtNaturalWidth(app->vt);
        Dimension vt_height = XtpVtNaturalHeight(app->vt);
        Dimension base_width = vt_width - XtpVtColumns(app->vt) * XtpVtCellWidth(app->vt);
        Dimension base_height = vt_height - XtpVtRows(app->vt) * XtpVtCellHeight(app->vt);

        XtpLog(XTP_LOG_DEBUG, "shell", "geometry request pixels=%ux%u grid=%ux%u cell=%ux%u",
               vt_width, vt_height, XtpVtColumns(app->vt), XtpVtRows(app->vt),
               XtpVtCellWidth(app->vt), XtpVtCellHeight(app->vt));

        if (!XtIsRealized(app->shell)) {
                XtVaSetValues(app->vt, XtNwidth, vt_width, XtNheight, vt_height, NULL);
                XtVaSetValues(app->shell, XtNwidth, vt_width, XtNheight, vt_height, NULL);
                return;
        }

        UpdateNormalHints(app, vt_width, vt_height, base_width, base_height);
        XResizeWindow(app->display, XtWindow(app->shell), vt_width, vt_height);
        XFlush(app->display);
        XtpLog(XTP_LOG_DEBUG, "resize", "shell geometry requested=%ux%u", vt_width, vt_height);
}

int
main(int argc, char **argv)
{
        App app;
        AppResources resources;
        Arg args[12];
        Cardinal num_args = 0;
        char **command = NULL;
        char *default_command[2];
        int argument;
        int original_argc = argc;
        XrmDatabase command_database;
        Boolean report_requested = False;

        for (argument = 1; argument < argc; ++argument) {
                if (strcmp(argv[argument], "-e") == 0)
                        break;
                if (strcmp(argv[argument], "-report-config") == 0) {
                        report_requested = True;
                        break;
                }
        }
        if (report_requested)
                XtpLogSetQuiet(1);
        command_database = XtpConfigCommandDatabase(original_argc, argv);

        SetEarlyDebug(argc, argv);
        LogCommandLine(argc, argv);
        XtpLog(XTP_LOG_INFO, "config",
               "compiled version=" XTP_VERSION " application=xterm class=XTerm widget=vt100/VT100 "
               "backend-option=auto default-grid=80x24 default-font=fixed debug=false");

        if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
                return XtpSelfTest();
        if (argc == 2 && strcmp(argv[1], "--version") == 0) {
                puts("xterm+ " XTP_VERSION);
                return EXIT_SUCCESS;
        }

        for (argument = 1; argument < argc; ++argument) {
                if (strcmp(argv[argument], "-e") == 0) {
                        if (argument + 1 == argc) {
                                XtpLog(XTP_LOG_ERROR, "startup", "-e requires a command");
                                return EXIT_FAILURE;
                        }
                        command = &argv[argument + 1];
                        argc = argument;
                        break;
                }
        }
        if (command == NULL) {
                default_command[0] = getenv("SHELL");
                if (default_command[0] == NULL || default_command[0][0] == '\0')
                        default_command[0] = (char *)"/bin/sh";
                default_command[1] = NULL;
                command = default_command;
        }
        XtpLog(XTP_LOG_INFO, "startup", "child command=%s", command[0]);

        memset(&app, 0, sizeof(app));
        (void)setlocale(LC_ALL, "");
        {
                const char *locale = setlocale(LC_ALL, NULL);

                XtpLog(XTP_LOG_INFO, "startup", "locale=%s DISPLAY=%s SHELL=%s",
                       locale != NULL ? locale : "(unavailable)",
                       getenv("DISPLAY") != NULL ? getenv("DISPLAY") : "(unset)",
                       getenv("SHELL") != NULL ? getenv("SHELL") : "(unset)");
        }
        XtSetLanguageProc(NULL, NULL, NULL);
        XtToolkitInitialize();
        app.context = XtCreateApplicationContext();
        XtAppSetFallbackResources(app.context, (String *)fallback_resources);

        app.display = XtOpenDisplay(app.context, NULL, "xterm", "XTerm", XtpCommandOptions,
                                    XtpCommandOptionCount, &argc, argv);
        if (app.display == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot open display");
                return EXIT_FAILURE;
        }
        {
                Bool supported = False;

                app.detectable_autorepeat =
                    XkbSetDetectableAutoRepeat(app.display, True, &supported) && supported;
                XtpLog(XTP_LOG_INFO, "input", "detectable autorepeat=%s",
                       app.detectable_autorepeat ? "enabled" : "unavailable");
        }
        XtpLog(XTP_LOG_INFO, "startup", "display opened name=%s remaining-argc=%d",
               DisplayString(app.display), argc);

        XtSetArg(args[num_args], XtNallowShellResize, True);
        ++num_args;
        XtSetArg(args[num_args], XtNtitle, "xterm+");
        ++num_args;
        XtSetArg(args[num_args], XtNinput, True);
        ++num_args;
        XtSetArg(args[num_args], XtNmappedWhenManaged, False);
        ++num_args;
        app.shell = XtAppCreateShell("xterm", "XTerm", applicationShellWidgetClass, app.display,
                                     args, num_args);
        XtpLog(XTP_LOG_INFO, "shell", "created instance=xterm class=XTerm title=xterm+");

        XtGetApplicationResources(app.shell, &resources, application_resources,
                                  XtNumber(application_resources), NULL, 0);
        XtpLogSetDebug(resources.debug);
        XtpLog(XTP_LOG_INFO, "config", "application resolved menuLocale=%s debug=%s",
               resources.menu_locale != NULL ? resources.menu_locale : "(null)",
               XtpLogDebugEnabled() ? "true" : "false");
        LogResourceDatabases(app.display);

        app.vt = XtVaCreateManagedWidget("vt100", vt100WidgetClass, app.shell, NULL);
        XtpLog(XTP_LOG_INFO, "shell", "created child instance=vt100 class=VT100");
        XtpLog(XTP_LOG_INFO, "config", "active renderer=%s", XtpVtRendererName(app.vt));

        if (resources.report_config) {
                XtpReportConfig(app.display, app.vt, command_database);
                if (command_database != NULL)
                        XrmDestroyDatabase(command_database);
                XtDestroyWidget(app.shell);
                XtCloseDisplay(app.display);
                XtDestroyApplicationContext(app.context);
                return EXIT_SUCCESS;
        }

        XtAddCallback(app.vt, XtNfontChangedCallback, FontChanged, &app);
        XtAddCallback(app.vt, XtNsizeChangedCallback, SizeChanged, &app);
        XtAddCallback(app.vt, XtNpopupMenuCallback, PopupRequested, &app);
        XtAddCallback(app.vt, XtNpasteCallback, PasteReceived, &app);
        XtAddCallback(app.vt, XtNinputCallback, EncodedInputReceived, &app);
        XtpMenusCreate(&app.menus, app.shell, resources.menu_locale, MenuDispatch, &app);
        XtpMenusSetScrollbar(&app.menus, XtpVtScrollbarVisible(app.vt));
        XtpMenusSetRenderFont(&app.menus, XtpVtUsingXft(app.vt), XtpVtXftAvailable(app.vt));
        XtpMenusSetChecked(&app.menus, "vtMenu", "selectToClipboard",
                           XtpVtSelectToClipboard(app.vt));

        app.terminal = XtpTerminalNew((uint16_t)XtpVtColumns(app.vt), (uint16_t)XtpVtRows(app.vt),
                                      XtpVtCellWidth(app.vt), XtpVtCellHeight(app.vt));
        if (app.terminal == NULL) {
                XtpLog(XTP_LOG_ERROR, "terminal", "cannot initialize backend=%s",
                       XtpTerminalBackend());
                return EXIT_FAILURE;
        }
        XtpLog(XTP_LOG_INFO, "config", "terminal backend=%s", XtpTerminalBackend());
        {
                XtpTerminalEffects effects = {
                    TerminalWritePty,
                    TerminalBell,
                    TerminalTitle,
                    &app,
                };

                XtpTerminalSetEffects(app.terminal, &effects);
        }
        XtpVtSetTerminal(app.vt, app.terminal);

        UpdateGeometry(&app);
        XtRealizeWidget(app.shell);
        XtpLog(XTP_LOG_INFO, "shell", "realized window=0x%lx pixels=%ux%u", XtWindow(app.shell),
               XtpVtNaturalWidth(app.vt), XtpVtNaturalHeight(app.vt));
        UpdateGeometry(&app);
        XtSetKeyboardFocus(app.shell, app.vt);
        XtAddEventHandler(app.vt, KeyPressMask | KeyReleaseMask | FocusChangeMask, False,
                          InputEvent, &app);
        app.input_method = XOpenIM(app.display, NULL, NULL, NULL);
        if (app.input_method != NULL) {
                app.input_context = XCreateIC(
                    app.input_method, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, XtWindow(app.shell), XNFocusWindow, XtWindow(app.shell), NULL);
        }
        XtpLog(XTP_LOG_DEBUG, "input", "input-method=%s input-context=%s",
               app.input_method != NULL ? "open" : "unavailable",
               app.input_context != NULL ? "created" : "unavailable");
        app.wm_delete_window = XInternAtom(app.display, "WM_DELETE_WINDOW", False);
        (void)XSetWMProtocols(app.display, XtWindow(app.shell), &app.wm_delete_window, 1);
        XtAddEventHandler(app.shell, StructureNotifyMask, True, ShellEvent, &app);

        if (!XtpTerminalBackendIsStub()) {
                app.pty = XtpPtySpawn(command, (uint16_t)XtpVtColumns(app.vt),
                                      (uint16_t)XtpVtRows(app.vt), XtpVtCellWidth(app.vt),
                                      XtpVtCellHeight(app.vt));
                if (app.pty == NULL) {
                        XtpLog(XTP_LOG_ERROR, "pty", "cannot start command=%s", command[0]);
                        return EXIT_FAILURE;
                }
                app.pty_input =
                    XtAppAddInput(app.context, XtpPtyFd(app.pty),
                                  (XtPointer)(uintptr_t)XtInputReadMask, PtyReady, &app);
        }

        app.running = True;
        XtpLog(XTP_LOG_INFO, "startup", "event loop starting");
        XtMapWidget(app.shell);
        while (app.running)
                XtAppProcessEvent(app.context, XtIMAll);

        XtpLog(XTP_LOG_INFO, "startup", "event loop stopping");
        if (app.pty_input != (XtInputId)0)
                XtRemoveInput(app.pty_input);
        StopWatchingPtyOutput(&app);
        XtpPtyFree(app.pty);
        if (app.input_context != NULL)
                XDestroyIC(app.input_context);
        if (app.input_method != NULL)
                XCloseIM(app.input_method);
        XtpTerminalFree(app.terminal);
        XtDestroyWidget(app.shell);
        XtCloseDisplay(app.display);
        XtDestroyApplicationContext(app.context);
        if (command_database != NULL)
                XrmDestroyDatabase(command_database);
        XtpLog(XTP_LOG_INFO, "startup", "shutdown complete");
        return EXIT_SUCCESS;
}
