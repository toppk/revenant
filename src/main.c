#include "menus.h"
#include "config_report.h"
#include "diagnostics.h"
#include "pty_process.h"
#include "terminal.h"
#include "version.h"
#include "vt_widget.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <poll.h>
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

static XrmOptionDescRec options[] = {
    {"-geometry", "*geometry", XrmoptionSepArg, NULL},
    {"-fn", "*vt100.font", XrmoptionSepArg, NULL},
    {"-fa", "*vt100.faceName", XrmoptionSepArg, NULL},
    {"-fd", "*vt100.faceNameDoublesize", XrmoptionSepArg, NULL},
    {"-fs", "*vt100.faceSize", XrmoptionSepArg, NULL},
    {"-b", "*vt100.internalBorder", XrmoptionSepArg, NULL},
    {"-sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"+sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sl", "*vt100.saveLines", XrmoptionSepArg, NULL},
    {"-cc", "*vt100.charClass", XrmoptionSepArg, NULL},
    {"-mc", "*vt100.multiClickTime", XrmoptionSepArg, NULL},
    {"-rightbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"-leftbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "true"},
    {"+sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "false"},
    {"-si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "false"},
    {"+si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "true"},
    {"-ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "true"},
    {"+ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "false"},
    {"-bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "true"},
    {"+bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "false"},
    {"-bcf", "*vt100.cursorOffTime", XrmoptionSepArg, NULL},
    {"-bcn", "*vt100.cursorOnTime", XrmoptionSepArg, NULL},
    {"-debug", "*debug", XrmoptionNoArg, (XPointer) "true"},
    {"+debug", "*debug", XrmoptionNoArg, (XPointer) "false"},
    {"-report-config", "*reportConfig", XrmoptionNoArg, (XPointer) "true"},
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
        case XK_BackSpace:
                return XTP_KEY_BACKSPACE;
        case XK_Return:
                return XTP_KEY_ENTER;
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

static void
KeyEvent(App *app, XKeyEvent *xkey)
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

        if (app->input_context != NULL) {
                length = Xutf8LookupString(app->input_context, xkey, text, (int)sizeof(text),
                                           &keysym, &status);
        } else {
                length = XLookupString(xkey, text, (int)sizeof(text), &keysym, NULL);
                status = length > 0 ? XLookupBoth : XLookupKeySym;
        }
        if (status == XBufferOverflow)
                return;

        event.key = KeyFromKeysym(physical != NoSymbol ? physical : keysym);
        event.modifiers = KeyModifiers(xkey->state);
        if ((status == XLookupChars || status == XLookupBoth) && length > 0 &&
            (unsigned char)text[0] >= 0x20U && (unsigned char)text[0] != 0x7fU) {
                event.utf8 = text;
                event.utf8_length = (size_t)length;
        }
        if (physical >= XK_space && physical <= XK_asciitilde)
                event.unshifted_codepoint = (uint32_t)physical;

        XtpLog(
            XTP_LOG_DEBUG, "input",
            "keypress keycode=%u keysym=0x%lx physical=0x%lx mapped=%d state=0x%x text-bytes=%zu",
            xkey->keycode, keysym, physical, (int)event.key, xkey->state, event.utf8_length);

        if (XtpTerminalEncodeKey(app->terminal, &event, encoded, sizeof(encoded), &written) == 0 &&
            written != 0) {
                XtpVtScrollOnKeypress(app->vt);
                WritePtyBytes(app, (const uint8_t *)encoded, written);
        }
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
        } else if (event->type == KeyPress && !XFilterEvent(event, XtWindow(app->shell))) {
                if (TranslationOwnsKey(&event->xkey)) {
                        XtpLog(XTP_LOG_DEBUG, "input", "keypress reserved for Xt translation");
                } else {
                        KeyEvent(app, &event->xkey);
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

static int
SelfTestPty(void)
{
        char *command[] = {
            (char *)"/bin/sh",
            (char *)"-c",
            (char *)"printf xterm-plus-pty",
            NULL,
        };
        XtpPty *pty = XtpPtySpawn(command, 80, 24, 8, 16);
        char output[256];
        size_t used = 0;
        int attempts;

        if (pty == NULL)
                return -1;
        for (attempts = 0; attempts < 10 && used + 1U < sizeof(output); ++attempts) {
                struct pollfd descriptor = {
                    XtpPtyFd(pty),
                    POLLIN | POLLHUP,
                    0,
                };
                ssize_t amount;

                if (poll(&descriptor, 1, 200) < 0 && errno != EINTR)
                        break;
                amount = XtpPtyRead(pty, output + used, sizeof(output) - used - 1U);
                if (amount > 0) {
                        used += (size_t)amount;
                } else if (amount < 0 &&
                           (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                        continue;
                } else {
                        break;
                }
        }
        output[used] = '\0';
        XtpPtyFree(pty);
        return strstr(output, "xterm-plus-pty") != NULL ? 0 : -1;
}

static int
SelfTestPtyQueue(void)
{
        static const size_t first_write = 96U * 1024U;
        static const size_t payload_size = 160U * 1024U;
        char *command[] = {
            (char *)"/bin/sh",
            (char *)"-c",
            (char *)"stty raw -echo; printf R; sleep 0.2; exec cat",
            NULL,
        };
        XtpPty *pty = XtpPtySpawn(command, 80, 24, 8, 16);
        uint8_t *payload = NULL;
        uint8_t buffer[8192];
        size_t received = 0;
        int attempts;
        int result = -1;

        if (pty == NULL)
                return -1;
        for (attempts = 0; attempts < 20; ++attempts) {
                struct pollfd descriptor = {XtpPtyFd(pty), POLLIN, 0};
                ssize_t amount;

                if (poll(&descriptor, 1, 100) < 0 && errno != EINTR)
                        goto done;
                amount = XtpPtyRead(pty, buffer, sizeof(buffer));
                if (amount == 1 && buffer[0] == 'R')
                        break;
                if (amount < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                        continue;
                goto done;
        }
        if (attempts == 20)
                goto done;
        payload = malloc(payload_size);
        if (payload == NULL)
                goto done;
        for (received = 0; received < payload_size; ++received)
                payload[received] = (uint8_t)(received * 31U + 7U);
        received = 0;
        if (XtpPtyQueue(pty, payload, first_write) != 0 || XtpPtyFlush(pty) != 1 ||
            XtpPtyPending(pty) == 0 ||
            XtpPtyQueue(pty, payload + first_write, payload_size - first_write) != 0)
                goto done;
        for (attempts = 0; attempts < 200 && (XtpPtyPending(pty) != 0 || received < payload_size);
             ++attempts) {
                struct pollfd descriptor = {XtpPtyFd(pty),
                                            POLLIN | (XtpPtyPending(pty) != 0 ? POLLOUT : 0), 0};
                ssize_t amount;

                if (poll(&descriptor, 1, 100) < 0) {
                        if (errno == EINTR)
                                continue;
                        goto done;
                }
                if ((descriptor.revents & POLLOUT) != 0 && XtpPtyFlush(pty) < 0)
                        goto done;
                if ((descriptor.revents & POLLIN) == 0)
                        continue;
                amount = XtpPtyRead(pty, buffer, sizeof(buffer));
                if (amount > 0) {
                        if ((size_t)amount > payload_size - received ||
                            memcmp(buffer, payload + received, (size_t)amount) != 0)
                                goto done;
                        received += (size_t)amount;
                } else if (amount < 0 && errno != EINTR && errno != EAGAIN &&
                           errno != EWOULDBLOCK) {
                        goto done;
                }
        }
        if (XtpPtyPending(pty) == 0 && received == payload_size)
                result = 0;
done:
        free(payload);
        XtpPtyFree(pty);
        return result;
}

static uint64_t
SelfTestRowsBelow(const XtpTerminalScrollbar *state)
{
        uint64_t end = state->offset + state->length;

        return state->total > end ? state->total - end : 0;
}

static int
SelfTestScrollTtyOutput(void)
{
        XtpTerminal *terminal;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        int line;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(20, 4, 8, 16);
        if (terminal == NULL || XtpTerminalSetScrollbackLines(terminal, 64) != 0)
                goto done;
        for (line = 0; line < 12; ++line) {
                char text[32];
                int length = snprintf(text, sizeof(text), "anchor-%02d\r\n", line);

                XtpTerminalFeed(terminal, (const uint8_t *)text, (size_t)length);
        }
        if (XtpTerminalScrollTo(terminal, 3) != 0 ||
            XtpTerminalGetScrollbar(terminal, &before) != 0 ||
            XtpTerminalFeedOutput(terminal, (const uint8_t *)"next\r\n", 6, false) != 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 || after.total <= before.total ||
            after.offset <= before.offset ||
            SelfTestRowsBelow(&after) != SelfTestRowsBelow(&before))
                goto done;
        if (XtpTerminalScrollTo(terminal, 2) != 0 ||
            XtpTerminalFeedOutput(terminal, (const uint8_t *)"bottom\r\n", 8, true) != 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 ||
            after.offset + after.length != after.total)
                goto done;
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        size_t nonempty_cells;
        size_t frame_cells;
        size_t last_frame_cells;
        size_t begin_calls;
        size_t end_calls;
        Boolean saw_styled_cell;
        Boolean saw_hyperlink_cell;
        Boolean saw_wide_cell;
        Boolean saw_wide_tail;
        Boolean saw_selected_cell;
        size_t selected_cells;
        XtpRenderFrame frame;
} SelfTestRender;

static void
SelfTestBegin(const XtpRenderFrame *frame, void *closure)
{
        SelfTestRender *render = closure;

        render->frame = *frame;
        render->frame_cells = 0;
        ++render->begin_calls;
}

static void
SelfTestCell(const XtpRenderCell *cell, void *closure)
{
        SelfTestRender *render = closure;

        ++render->frame_cells;
        if (cell->utf8_length != 0)
                ++render->nonempty_cells;
        if (cell->foreground.kind != XTP_COLOR_DEFAULT)
                render->saw_styled_cell = True;
        if (cell->hyperlink)
                render->saw_hyperlink_cell = True;
        if (cell->width == 2)
                render->saw_wide_cell = True;
        if (cell->width == 0)
                render->saw_wide_tail = True;
        if (cell->selected)
                render->saw_selected_cell = True;
        if (cell->selected)
                ++render->selected_cells;
}

static void
SelfTestEnd(const XtpRenderFrame *frame, void *closure)
{
        SelfTestRender *render = closure;

        render->frame = *frame;
        render->last_frame_cells = render->frame_cells;
        ++render->end_calls;
}

static int
SelfTestCursorOnly(XtpTerminal *terminal, const XtpRenderer *renderer, SelfTestRender *render)
{
        static const uint8_t text[] = "abc";
        static const uint8_t cursor_left[] = "\033[D";
        size_t begin_calls;
        size_t end_calls;
        uint16_t column;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        XtpTerminalFeed(terminal, text, sizeof(text) - 1U);
        if (XtpTerminalRender(terminal, renderer, render, false) != 0 ||
            !render->frame.cursor_visible || render->frame.cursor_column == 0)
                return -1;
        column = render->frame.cursor_column;
        begin_calls = render->begin_calls;
        end_calls = render->end_calls;
        XtpTerminalFeed(terminal, cursor_left, sizeof(cursor_left) - 1U);
        if (XtpTerminalRender(terminal, renderer, render, false) != 0 ||
            render->begin_calls != begin_calls + 1U || render->end_calls != end_calls + 1U ||
            render->last_frame_cells != 0 || !render->frame.cursor_visible ||
            render->frame.cursor_column + 1U != column)
                return -1;
        return 0;
}

static int
SelfTestCursorStyles(const XtpRenderer *renderer)
{
        static const struct
        {
                const char *sequence;
                XtpCursorShape shape;
                bool blinking;
        } cases[] = {
            {"\033[0 q", XTP_CURSOR_SHAPE_BLOCK, false},
            {"\033[1 q", XTP_CURSOR_SHAPE_BLOCK, true},
            {"\033[2 q", XTP_CURSOR_SHAPE_BLOCK, false},
            {"\033[3 q", XTP_CURSOR_SHAPE_UNDERLINE, true},
            {"\033[4 q", XTP_CURSOR_SHAPE_UNDERLINE, false},
            {"\033[5 q", XTP_CURSOR_SHAPE_BAR, true},
            {"\033[6 q", XTP_CURSOR_SHAPE_BAR, false},
        };
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        size_t item;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(8, 3, 8, 16);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BLOCK || render.frame.cursor_blinking)
                goto done;
        if (XtpTerminalSetCursorBlinkDefault(terminal, true) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blinking ||
            XtpTerminalSetCursorBlinkDefault(terminal, false) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blinking)
                goto done;
        for (item = 0; item < XtNumber(cases); ++item) {
                size_t begin_calls = render.begin_calls;
                size_t end_calls = render.end_calls;

                XtpTerminalFeed(terminal, (const uint8_t *)cases[item].sequence,
                                strlen(cases[item].sequence));
                if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
                    render.begin_calls != begin_calls + 1U || render.end_calls != end_calls + 1U ||
                    render.frame.cursor_shape != cases[item].shape ||
                    render.frame.cursor_blinking != cases[item].blinking) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "cursor style case=%zu shape=%d blink=%s", item,
                               render.frame.cursor_shape,
                               render.frame.cursor_blinking ? "true" : "false");
                        goto done;
                }
        }
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12h", 6);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blinking)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12l", 6);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blinking)
                goto done;
        if (XtpTerminalSetCursorBlinkDefault(terminal, true) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blinking)
                goto done;
        if (XtpTerminalSetCursorBlinkDefault(terminal, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blinking)
                goto done;
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestHyperlinks(const XtpRenderer *renderer)
{
        static const uint8_t content[] =
            "\033]8;;http://example.com\033\\This is a link\033]8;;\033\\ plain";
        static const uint8_t expected[] = "http://example.com";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        uint8_t *uri = NULL;
        size_t length = 0;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(30, 2, 8, 16);
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, content, sizeof(content) - 1U);
        if (XtpTerminalHyperlinkAt(terminal, 0, 0, &uri, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(uri, expected, length) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            !render.saw_hyperlink_cell) {
                XtpLog(XTP_LOG_ERROR, "self-test", "OSC 8 URI length=%zu rendered=%s", length,
                       render.saw_hyperlink_cell ? "true" : "false");
                goto done;
        }
        free(uri);
        uri = NULL;
        length = 0;
        if (XtpTerminalHyperlinkAt(terminal, 14, 0, &uri, &length) != 0 || uri != NULL ||
            length != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "OSC 8 terminator left URI length=%zu", length);
                goto done;
        }
        result = 0;
done:
        free(uri);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestModes(XtpTerminal *terminal)
{
        XtpTerminalMode mode;

        for (mode = 0; mode < XTP_TERMINAL_MODE_COUNT; ++mode) {
                bool initial;
                bool changed;

                if (XtpTerminalGetMode(terminal, mode, &initial) != 0 ||
                    XtpTerminalSetMode(terminal, mode, !initial) != 0 ||
                    XtpTerminalGetMode(terminal, mode, &changed) != 0 || changed == initial ||
                    XtpTerminalSetMode(terminal, mode, initial) != 0)
                        return -1;
        }
        return 0;
}

static int
SelfTestSelection(const XtpRenderer *renderer)
{
        static const uint8_t content[] = "hello   world\r\nsecond line\r\n~/workspace/xterm-plus";
        static const char url_char_class[] =
            "33:48,35:48,37-38:48,43-47:48,58:48,61:48,63-64:48,95:48,126:48";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        uint8_t *text = NULL;
        uint8_t *paste = NULL;
        size_t length = 0;
        size_t paste_length = 0;
        int start_result;
        int extend_result;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(30, 4, 8, 16);
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, content, sizeof(content) - 1U);
        start_result = XtpTerminalSelectionStart(terminal, 0, 0, 1.0, 1.0, 1000000000U,
                                                 XTP_SELECTION_CELL, false);
        extend_result = XtpTerminalSelectionExtend(terminal, 4, 0, 38.0, 1.0, 30, 8, 0, 64, false);
        if (start_result != 0 || extend_result != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "selection gesture start=%d extend=%d",
                       start_result, extend_result);
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 4, 0, true);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 5 ||
            memcmp(text, "hello", 5) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            !render.saw_selected_cell) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "selection output length=%zu text=%.*s selected=%s", length, (int)length,
                       text != NULL ? (const char *)text : "",
                       render.saw_selected_cell ? "yes" : "no");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendStart(terminal, 11, 0, XTP_SELECTION_CELL) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cell-granular right-click extension failed");
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 12 ||
            memcmp(text, "hello   worl", 12) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cell extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 6, 0, 51.0, 1.0, 2000000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 6, 0, true);
        if (XtpTerminalSelectionStart(terminal, 6, 0, 51.0, 1.0, 2100000000U, XTP_SELECTION_WORD,
                                      true) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "double-click whitespace selection failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 3) {
                XtpLog(XTP_LOG_ERROR, "self-test", "whitespace selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        if (XtpTerminalSelectionExtendStart(terminal, 11, 0, XTP_SELECTION_WORD) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word-granular right-click extension failed");
                goto done;
        }
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 8 ||
            memcmp(text, "   world", 8) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendActive(terminal, 0, 0, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "crossing word extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 8) {
                XtpLog(XTP_LOG_ERROR, "self-test", "crossed word selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3000000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line sequence initial click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 1, 0, true);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3100000000U, XTP_SELECTION_WORD,
                                      true) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line sequence double click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 1, 0, true);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3200000000U, XTP_SELECTION_LINE,
                                      true) != 1 ||
            XtpTerminalSelectionExtendStart(terminal, 2, 1, XTP_SELECTION_LINE) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line-granular right-click extension failed");
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 25 ||
            memcmp(text, "hello   world\nsecond line", 25) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 3, 2, 25.0, 33.0, 4000000000U, XTP_SELECTION_WORD,
                                      false) != 1 ||
            XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 9 ||
            memcmp(text, "workspace", 9) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "default charClass path length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendStart(terminal, 25, 2, XTP_SELECTION_WORD) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word extension into undrawn suffix failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 28) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word-to-undrawn selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 20, 0, 161.0, 1.0, 4200000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn initial click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 20, 0, true);
        if (XtpTerminalSelectionStart(terminal, 20, 0, 161.0, 1.0, 4300000000U, XTP_SELECTION_WORD,
                                      true) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn double click selected text");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn double-click cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 0, 0, 1.0, 1.0, 4400000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 20, 0, 165.0, 1.0, 30, 8, 0, 64, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn suffix cell extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 30) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn suffix selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 0, 2, 1.0, 33.0, 4500000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 10, 3, 85.0, 49.0, 30, 8, 0, 64, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn row cell extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 60) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn row selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 10, 3, 85.0, 49.0, 4600000000U, XTP_SELECTION_LINE,
                                      false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn triple-click line failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 30) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn line selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSetCharClass(terminal, url_char_class) != 0)
                goto done;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 3, 2, 25.0, 33.0, 4100000000U, XTP_SELECTION_WORD,
                                      false) != 1 ||
            XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 22 ||
            memcmp(text, "~/workspace/xterm-plus", 22) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "custom charClass path length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?2004h", 8);
        if (XtpTerminalEncodePaste(terminal, (const uint8_t *)"hello\n", 6, &paste,
                                   &paste_length) != 0 ||
            paste_length != 18 || memcmp(paste, "\033[200~hello\n\033[201~", 18) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "paste output length=%zu", paste_length);
                goto done;
        }
        result = 0;
done:
        free(paste);
        free(text);
        XtpTerminalSelectionClear(terminal);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestScrollbackLimit(void)
{
        enum
        {
                configured_lines = 16500,
                emitted_lines = 20000,
                bytes_per_line = 8,
        };
        XtpTerminal *terminal;
        XtpTerminalScrollbar scrollbar = {0};
        char *content;
        int line;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(80, 24, 8, 16);
        content = malloc((size_t)emitted_lines * bytes_per_line + 1U);
        if (terminal == NULL || content == NULL)
                goto done;
        for (line = 0; line < emitted_lines; ++line) {
                int length = snprintf(content + (size_t)line * bytes_per_line, bytes_per_line + 1U,
                                      "L%05d\r\n", line);

                if (length != bytes_per_line)
                        goto done;
        }
        if (XtpTerminalSetScrollbackLines(terminal, configured_lines) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)content, (size_t)emitted_lines * bytes_per_line);
        /*
         * libghostty prunes whole pages, so its documented line limit is an
         * estimate rather than an exact retained-row count. This lower bound
         * is deliberately loose enough for one page of granularity while
         * still catching the independent default byte cap.
         */
        if (XtpTerminalGetScrollbar(terminal, &scrollbar) != 0 ||
            scrollbar.total < configured_lines * 3U / 4U || scrollbar.total >= emitted_lines) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "saveLines not retained configured=%d emitted=%d total=%" PRIu64,
                       configured_lines, emitted_lines, scrollbar.total);
                goto done;
        }
        if (XtpTerminalSetScrollbackLines(terminal, 0) != 0 ||
            XtpTerminalGetScrollbar(terminal, &scrollbar) != 0 ||
            scrollbar.total != scrollbar.length) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "saveLines zero retained history total=%" PRIu64 " length=%" PRIu64,
                       scrollbar.total, scrollbar.length);
                goto done;
        }
        result = 0;
done:
        free(content);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestSelectionScrollback(void)
{
        static const char expected[] = "L00\nL01\nL02\nL03\nL04\nL05\nL06\nL07\nL08\nL09\nL10\nL11";
        XtpTerminal *terminal;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        XtpSelectionAutoscroll direction = XTP_SELECTION_AUTOSCROLL_NONE;
        uint8_t *text = NULL;
        size_t length = 0;
        int line;
        int ticks = 0;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(8, 3, 8, 16);
        if (terminal == NULL || XtpTerminalSetScrollbackLines(terminal, 64) != 0)
                goto done;
        for (line = 0; line < 12; ++line) {
                char row[8];
                int row_length =
                    snprintf(row, sizeof(row), line == 0 ? "L%02d" : "\r\nL%02d", line);

                XtpTerminalFeed(terminal, (const uint8_t *)row, (size_t)row_length);
        }
        if (XtpTerminalGetScrollbar(terminal, &before) != 0 || before.offset == 0 ||
            XtpTerminalSelectionStart(terminal, 2, 2, 23.0, 40.0, 5000000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48, false) != 1 ||
            XtpTerminalSelectionGetAutoscroll(terminal, &direction) != 0 ||
            direction != XTP_SELECTION_AUTOSCROLL_UP) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection did not begin at bottom offset=%" PRIu64
                       " direction=%d",
                       before.offset, direction);
                goto done;
        }
        do {
                before = after = (XtpTerminalScrollbar){0};
                if (XtpTerminalGetScrollbar(terminal, &before) != 0 ||
                    XtpTerminalSelectionAutoscrollTick(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48,
                                                       false) < 0 ||
                    XtpTerminalGetScrollbar(terminal, &after) != 0 ||
                    after.offset > before.offset) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "scrollback selection moved non-monotonically before=%" PRIu64
                               " after=%" PRIu64,
                               before.offset, after.offset);
                        goto done;
                }
                ++ticks;
        } while (after.offset != 0 && ticks < 64);
        if (after.offset != 0 ||
            XtpTerminalSelectionAutoscrollTick(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48, false) < 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection did not stop at oldest row offset=%" PRIu64,
                       after.offset);
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 0, 0, true);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(text, expected, sizeof(expected) - 1U) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection lost or duplicated text length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalScrollToBottom(terminal) != 0 ||
            XtpTerminalSelectionStart(terminal, 0, 2, 1.0, 40.0, 6000000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 2, 2, 23.0, 40.0, 8, 8, 0, 48, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "scrollback Button-3 setup failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 2, 2, true);
        if (XtpTerminalSelectionExtendStart(terminal, 0, 0, XTP_SELECTION_CELL) != 1)
                goto done;
        ticks = 0;
        do {
                before = after = (XtpTerminalScrollbar){0};
                if (XtpTerminalGetScrollbar(terminal, &before) != 0 ||
                    XtpTerminalScrollBy(terminal, -1) != 0 ||
                    XtpTerminalSelectionExtendActive(terminal, 0, 0, false) != 1 ||
                    XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset > before.offset)
                        goto done;
                ++ticks;
        } while (after.offset != 0 && ticks < 64);
        XtpTerminalSelectionExtendEnd(terminal);
        free(text);
        text = NULL;
        if (after.offset != 0 || XtpTerminalSelectionText(terminal, &text, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(text, expected, sizeof(expected) - 1U) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback Button-3 extension lost or duplicated text length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        result = 0;
done:
        free(text);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestFocus(void)
{
        static const uint8_t enable[] = "\033[?1004h";
        static const uint8_t disable[] = "\033[?1004l";
        XtpTerminal *terminal;
        char encoded[8];
        size_t written = 0;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(80, 24, 8, 16);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        XtpTerminalFeed(terminal, enable, sizeof(enable) - 1U);
        if (XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written != 3 || memcmp(encoded, "\033[I", 3) != 0)
                goto mismatch;
        if (XtpTerminalEncodeFocus(terminal, false, encoded, sizeof(encoded), &written) != 0 ||
            written != 3 || memcmp(encoded, "\033[O", 3) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, disable, sizeof(disable) - 1U);
        if (XtpTerminalEncodeFocus(terminal, false, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        result = 0;
        goto done;
mismatch:
        XtpLog(XTP_LOG_ERROR, "self-test", "focus encoding mismatch length=%zu bytes=%.*s", written,
               (int)written, encoded);
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestMouse(void)
{
        static const uint8_t sgr_normal[] = "\033[?1000h\033[?1006h";
        static const uint8_t sgr_button[] = "\033[?1002h";
        static const uint8_t sgr_any[] = "\033[?1003h";
        static const uint8_t x10_mode[] = "\033[?1003l\033[?1006l\033[?9h";
        static const uint8_t urxvt_mode[] = "\033[?9l\033[?1000h\033[?1015h";
        static const uint8_t pixel_mode[] = "\033[?1015l\033[?1016h";
        static const uint8_t utf8_mode[] = "\033[?1016l\033[?1005h";
        static const char x10_left[] = {'\033', '[', 'M', 32, 34, 34};
        static const char utf8_right[] = {'\033', '[', 'M', 34, (char)0xc3, (char)0xa9, 34};
        XtpMouseEvent event = {
            .action = XTP_MOUSE_ACTION_PRESS,
            .button = XTP_MOUSE_BUTTON_LEFT,
            .x = 10.0f,
            .y = 18.0f,
            .screen_width = 644,
            .screen_height = 388,
            .cell_width = 8,
            .cell_height = 16,
            .padding_top = 2,
            .padding_bottom = 2,
            .padding_left = 2,
            .padding_right = 2,
            .any_button_pressed = true,
        };
        XtpTerminal *terminal;
        char encoded[128];
        size_t written = 0;
        int result = -1;

        if (strcmp(XtpTerminalBackend(), "libghostty-vt") != 0)
                return 0;
        terminal = XtpTerminalNew(80, 24, 8, 16);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        XtpTerminalFeed(terminal, sgr_normal, sizeof(sgr_normal) - 1U);
        if (!XtpTerminalMouseTracking(terminal) ||
            XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[<0;2;2M", 9) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_RELEASE;
        event.any_button_pressed = false;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[<0;2;2m", 9) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, sgr_button, sizeof(sgr_button) - 1U);
        event.action = XTP_MOUSE_ACTION_MOTION;
        event.any_button_pressed = true;
        event.x = 18.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<32;3;2M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, sgr_any, sizeof(sgr_any) - 1U);
        event.button = XTP_MOUSE_BUTTON_NONE;
        event.any_button_pressed = false;
        event.x = 26.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<35;4;2M", 10) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_FOUR;
        event.x = 10.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<64;2;2M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, x10_mode, sizeof(x10_mode) - 1U);
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_LEFT;
        event.modifiers = XTP_MOD_CONTROL | XTP_MOD_ALT;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != sizeof(x10_left) || memcmp(encoded, x10_left, sizeof(x10_left)) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_RELEASE;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, urxvt_mode, sizeof(urxvt_mode) - 1U);
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_RIGHT;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[58;2;2M", 9) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, pixel_mode, sizeof(pixel_mode) - 1U);
        event.modifiers = 0;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<2;8;16M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, utf8_mode, sizeof(utf8_mode) - 1U);
        event.screen_width = 2404;
        event.x = 1602.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != sizeof(utf8_right) || memcmp(encoded, utf8_right, sizeof(utf8_right)) != 0)
                goto mismatch;
        result = 0;
        goto done;
mismatch:
        XtpLog(XTP_LOG_ERROR, "self-test", "mouse encoding mismatch length=%zu bytes=%.*s", written,
               (int)written, encoded);
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTest(void)
{
        static const uint8_t sample[] = "plain\033[31m red\033[0m wide=界\r\n";
        XtpTerminal *terminal = XtpTerminalNew(80, 24, 8, 16);
        XtpRenderer renderer = {SelfTestBegin, SelfTestCell, SelfTestEnd};
        SelfTestRender render = {0};
        XtpKeyEvent key = {XTP_KEY_A, 0, "a", 1, 'a'};
        char encoded[32];
        size_t written = 0;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        int line;

        if (terminal == NULL)
                return EXIT_FAILURE;
        if (XtpTerminalSetScrollbackLines(terminal, 64) != 0) {
                XtpTerminalFree(terminal);
                return EXIT_FAILURE;
        }
        XtpTerminalFeed(terminal, sample, sizeof(sample) - 1U);
        for (line = 0; line < 40; ++line) {
                char text[32];
                int length = snprintf(text, sizeof(text), "history-%02d\r\n", line);

                XtpTerminalFeed(terminal, (const uint8_t *)text, (size_t)length);
        }
        XtpTerminalFeed(terminal, sample, sizeof(sample) - 1U);
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            XtpTerminalEncodeKey(terminal, &key, encoded, sizeof(encoded), &written) != 0 ||
            written != 1 || encoded[0] != 'a' || XtpTerminalGetScrollbar(terminal, &before) != 0 ||
            (strcmp(XtpTerminalBackend(), "libghostty-vt") == 0 &&
             (render.nonempty_cells == 0 || !render.saw_styled_cell || !render.saw_wide_cell ||
              !render.saw_wide_tail || before.total <= before.length || before.offset == 0 ||
              XtpTerminalScrollBy(terminal, -3) != 0 ||
              XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset >= before.offset ||
              XtpTerminalScrollToBottom(terminal) != 0 ||
              XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset != before.offset)) ||
            SelfTestCursorOnly(terminal, &renderer, &render) != 0 || SelfTestModes(terminal) != 0 ||
            SelfTestCursorStyles(&renderer) != 0 || SelfTestSelection(&renderer) != 0 ||
            SelfTestHyperlinks(&renderer) != 0 || SelfTestScrollbackLimit() != 0 ||
            SelfTestSelectionScrollback() != 0 || SelfTestScrollTtyOutput() != 0 ||
            SelfTestFocus() != 0 || SelfTestMouse() != 0 ||
            XtpTerminalResize(terminal, 100, 30, 9, 18) != 0) {
                XtpTerminalFree(terminal);
                return EXIT_FAILURE;
        }
        XtpTerminalFree(terminal);
        if (SelfTestPty() != 0 || SelfTestPtyQueue() != 0)
                return EXIT_FAILURE;

        printf("xterm+ self-test: backend=%s menus=%d/%d/%d\n", XtpTerminalBackend(),
               XTP_MAIN_MENU_ENTRIES, XTP_VT_MENU_ENTRIES, XTP_FONT_MENU_ENTRIES);
        return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
        App app;
        AppResources resources;
        Arg args[12];
        Cardinal num_args = 0;
        xcb_connection_t *xcb_connection;
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
                return SelfTest();
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

        app.display = XtOpenDisplay(app.context, NULL, "xterm", "XTerm", options, XtNumber(options),
                                    &argc, argv);
        if (app.display == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot open display");
                return EXIT_FAILURE;
        }
        XtpLog(XTP_LOG_INFO, "startup", "display opened name=%s remaining-argc=%d",
               DisplayString(app.display), argc);

        xcb_connection = XGetXCBConnection(app.display);
        if (xcb_connection == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "Xlib does not expose its XCB connection");
                return EXIT_FAILURE;
        }
        XtpLog(XTP_LOG_INFO, "startup", "shared XCB connection available");

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
        XtAddEventHandler(app.vt, KeyPressMask | FocusChangeMask, False, InputEvent, &app);
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

        if (strcmp(XtpTerminalBackend(), "stub") != 0) {
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
        (void)xcb_connection;
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
