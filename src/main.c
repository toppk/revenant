#include "command_options.h"
#include "menus.h"
#include "config_report.h"
#include "diagnostics.h"
#include "pty_process.h"
#include "selftest.h"
#include "terminal.h"
#include "version.h"
#include "vt_widget.h"
#include "x11_opacity.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

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
        Atom wm_delete_window;
        Visual *visual;
        Colormap colormap;
        int depth;
        uint16_t background_alpha;
        Boolean owns_colormap;
        Boolean argb_visual;
        Boolean running;
} App;

typedef struct
{
        XtpMenuItem item;
        XtpTerminalMode mode;
} TerminalModeMenuItem;

static const TerminalModeMenuItem terminal_mode_menu_items[] = {
    {XTP_MENU_ITEM_BACKARROW_KEY, XTP_TERMINAL_MODE_BACKARROW_KEY},
    {XTP_MENU_ITEM_NUM_LOCK, XTP_TERMINAL_MODE_NUMLOCK_KEYPAD},
    {XTP_MENU_ITEM_ALT_ESC, XTP_TERMINAL_MODE_ALT_SENDS_ESCAPE},
    {XTP_MENU_ITEM_AUTOWRAP, XTP_TERMINAL_MODE_AUTOWRAP},
    {XTP_MENU_ITEM_REVERSE_WRAP, XTP_TERMINAL_MODE_REVERSE_WRAP},
    {XTP_MENU_ITEM_AUTOLINEFEED, XTP_TERMINAL_MODE_AUTOLINEFEED},
    {XTP_MENU_ITEM_APPLICATION_CURSOR, XTP_TERMINAL_MODE_APPLICATION_CURSOR},
    {XTP_MENU_ITEM_APPLICATION_KEYPAD, XTP_TERMINAL_MODE_APPLICATION_KEYPAD},
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
    "*mainMenu*backgroundOpacity.height: 22",
    "*SimpleMenu*Cursor: left_ptr",
    "*mainMenu.Label:  Main Options",
    "*vtMenu.Label:  VT Options",
    "*fontMenu.Label:  VT Fonts",
    "*title: xterm+",
    NULL,
};

static void UpdateGeometry(App *app);

static char *
BackgroundOpacityResource(Display *display)
{
        XrmValue value = {0};
        char *type = NULL;
        char *result;

        if (!XrmGetResource(XtDatabase(display), "xterm.vt100.backgroundOpacity",
                            "XTerm.VT100.BackgroundOpacity", &type, &value) ||
            value.addr == NULL)
                return XtNewString("1.0");
        result = XtMalloc(value.size + 1U);
        if (result == NULL)
                return NULL;
        memcpy(result, value.addr, value.size);
        result[value.size] = '\0';
        return result;
}

static void
ConfigureApplicationVisual(App *app)
{
        XVisualInfo visual_info = {0};
        XtpX11AlphaFormat alpha_format = {0};
        char *opacity = BackgroundOpacityResource(app->display);
        int screen = DefaultScreen(app->display);

        app->visual = DefaultVisual(app->display, screen);
        app->colormap = DefaultColormap(app->display, screen);
        app->depth = DefaultDepth(app->display, screen);
        app->background_alpha = UINT16_MAX;
        if (opacity == NULL || XtpBackgroundOpacityParse(opacity, &app->background_alpha) != 0) {
                XtpLog(XTP_LOG_WARNING, "render",
                       "invalid backgroundOpacity=%s; using opaque background",
                       opacity != NULL ? opacity : "(allocation failure)");
                app->background_alpha = UINT16_MAX;
                if (opacity != NULL)
                        XtFree(opacity);
                return;
        }
        if (app->background_alpha == UINT16_MAX) {
                XtpLog(XTP_LOG_INFO, "render", "backgroundOpacity=%s visual=default depth=%d",
                       opacity, app->depth);
                XtFree(opacity);
                return;
        }
        if (!XtpX11CompositorPresent(app->display, screen)) {
                XtpLog(XTP_LOG_WARNING, "render",
                       "backgroundOpacity=%s requested but compositor is unavailable; using "
                       "opaque default visual",
                       opacity);
                app->background_alpha = UINT16_MAX;
                XtFree(opacity);
                return;
        }
        if (!XtpX11FindArgbVisual(app->display, screen, &visual_info, &alpha_format)) {
                XtpLog(XTP_LOG_WARNING, "render",
                       "backgroundOpacity=%s requested but no 32-bit ARGB visual is available; "
                       "using opaque default visual",
                       opacity);
                app->background_alpha = UINT16_MAX;
                XtFree(opacity);
                return;
        }
        app->visual = visual_info.visual;
        app->depth = visual_info.depth;
        app->colormap =
            XCreateColormap(app->display, RootWindow(app->display, screen), app->visual, AllocNone);
        app->owns_colormap = True;
        app->argb_visual = True;
        XtpLog(XTP_LOG_INFO, "render",
               "backgroundOpacity=%s visual=0x%lx depth=%d alpha-mask=0x%lx compositor=present",
               opacity, visual_info.visualid, app->depth, alpha_format.mask);
        XtFree(opacity);
}

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

        if (value == NULL)
                return;
        memcpy(value, title, length);
        value[length] = '\0';
        XtVaSetValues(app->shell, XtNtitle, value, NULL);
        XtpLogBytePreview(XTP_LOG_INFO, "shell", "title changed", title, length);
        free(value);
}

static void
ResizeChild(App *app, unsigned int columns, unsigned int rows, unsigned int cell_width,
            unsigned int cell_height)
{
        if (app->pty != NULL &&
            XtpPtyResize(app->pty, (uint16_t)columns, (uint16_t)rows, cell_width, cell_height) != 0)
                XBell(app->display, 0);
        if (app->terminal != NULL &&
            XtpTerminalResize(app->terminal, (uint16_t)columns, (uint16_t)rows, cell_width,
                              cell_height) != 0)
                XBell(app->display, 0);
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
        ResizeChild(app, XtpVtColumns(app->vt), XtpVtRows(app->vt), change->cell_width,
                    change->cell_height);
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
        ResizeChild(app, change->columns, change->rows, change->cell_width, change->cell_height);
}

static void
SyncTerminalModeChecks(App *app)
{
        size_t index;

        for (index = 0; index < XtNumber(terminal_mode_menu_items); ++index) {
                const TerminalModeMenuItem *item = &terminal_mode_menu_items[index];
                bool enabled;

                if (app->terminal != NULL &&
                    XtpTerminalGetMode(app->terminal, item->mode, &enabled) == 0)
                        XtpMenusSetChecked(&app->menus, item->item, enabled ? True : False);
        }
}

static void
PopupRequested(Widget widget, XtPointer closure, XtPointer call_data)
{
        App *app = closure;
        XtpPopupMenu *popup = call_data;

        (void)widget;
        SyncTerminalModeChecks(app);
        XtpMenusSetChecked(&app->menus, XTP_MENU_ITEM_SCROLL_KEY, XtpVtScrollKey(app->vt));
        XtpMenusSetChecked(&app->menus, XTP_MENU_ITEM_SCROLL_TTY_OUTPUT,
                           XtpVtScrollTtyOutput(app->vt));
        XtpMenusSetChecked(&app->menus, XTP_MENU_ITEM_SELECT_TO_CLIPBOARD,
                           XtpVtSelectToClipboard(app->vt));
        XtpMenusSetOpacity(&app->menus, (int)XtpVtBackgroundOpacityPercent(app->vt),
                           XtpVtBackgroundOpacityAvailable(app->vt));
        XtpLog(XTP_LOG_INFO, "menu", "popup requested name=%s", popup->name);
        XtpMenusPopup(&app->menus, popup->name, popup->event);
}

static const TerminalModeMenuItem *
FindTerminalModeMenuItem(XtpMenuItem menu_item)
{
        size_t index;

        for (index = 0; index < XtNumber(terminal_mode_menu_items); ++index) {
                const TerminalModeMenuItem *item = &terminal_mode_menu_items[index];

                if (item->item == menu_item)
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
        XtpMenusSetChecked(&app->menus, item->item, enabled ? True : False);
        XtpLog(XTP_LOG_INFO, "menu", "mode item=%d enabled=%s", (int)item->item,
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
SelectFont(App *app, int slot)
{
        if (!XtpVtSelectFont(app->vt, slot))
                XBell(app->display, 0);
}

static void
MenuDispatch(Widget source, XtpMenuItem menu_item, XtPointer closure)
{
        App *app = closure;
        const TerminalModeMenuItem *mode_item;

        XtpLog(XTP_LOG_INFO, "menu", "dispatch menu=%s item=%s id=%d", XtName(XtParent(source)),
               XtName(source), (int)menu_item);
        mode_item = FindTerminalModeMenuItem(menu_item);
        if (mode_item != NULL) {
                ToggleTerminalMode(app, mode_item);
                return;
        }
        switch (menu_item) {
        case XTP_MENU_ITEM_NONE:
                return;
        case XTP_MENU_ITEM_REDRAW:
                XtpVtRedraw(app->vt);
                return;
        case XTP_MENU_ITEM_BACKGROUND_OPACITY:
                if (!XtpVtSetBackgroundOpacityPercent(app->vt,
                                                      (unsigned int)XtpMenusOpacity(&app->menus))) {
                        XBell(app->display, 0);
                }
                XtpMenusSetOpacity(&app->menus, (int)XtpVtBackgroundOpacityPercent(app->vt),
                                   XtpVtBackgroundOpacityAvailable(app->vt));
                return;
        case XTP_MENU_ITEM_QUIT:
                app->running = False;
                return;
        case XTP_MENU_ITEM_SCROLLBAR:
                ToggleScrollbar(app);
                return;
        case XTP_MENU_ITEM_REVERSE_VIDEO:
                ToggleReverseVideo(app);
                return;
        case XTP_MENU_ITEM_SCROLL_KEY:
                XtpVtSetScrollKey(app->vt, !XtpVtScrollKey(app->vt));
                XtpMenusSetChecked(&app->menus, menu_item, XtpVtScrollKey(app->vt));
                return;
        case XTP_MENU_ITEM_SCROLL_TTY_OUTPUT:
                XtpVtSetScrollTtyOutput(app->vt, !XtpVtScrollTtyOutput(app->vt));
                XtpMenusSetChecked(&app->menus, menu_item, XtpVtScrollTtyOutput(app->vt));
                return;
        case XTP_MENU_ITEM_SELECT_TO_CLIPBOARD:
                XtpVtSetSelectToClipboard(app->vt, !XtpVtSelectToClipboard(app->vt));
                XtpMenusSetChecked(&app->menus, menu_item, XtpVtSelectToClipboard(app->vt));
                return;
        case XTP_MENU_ITEM_RENDER_FONT:
                if (!XtpVtSetRenderFont(app->vt, !XtpVtUsingXft(app->vt)))
                        XBell(app->display, 0);
                return;
        case XTP_MENU_ITEM_FONT_DEFAULT:
                SelectFont(app, 0);
                return;
        case XTP_MENU_ITEM_FONT_1:
                SelectFont(app, 1);
                return;
        case XTP_MENU_ITEM_FONT_2:
                SelectFont(app, 2);
                return;
        case XTP_MENU_ITEM_FONT_3:
                SelectFont(app, 3);
                return;
        case XTP_MENU_ITEM_FONT_4:
                SelectFont(app, 4);
                return;
        case XTP_MENU_ITEM_FONT_5:
                SelectFont(app, 5);
                return;
        case XTP_MENU_ITEM_FONT_6:
                SelectFont(app, 6);
                return;
        case XTP_MENU_ITEM_FONT_7:
                SelectFont(app, 7);
                return;
        case XTP_MENU_ITEM_BACKARROW_KEY:
        case XTP_MENU_ITEM_NUM_LOCK:
        case XTP_MENU_ITEM_ALT_ESC:
        case XTP_MENU_ITEM_AUTOWRAP:
        case XTP_MENU_ITEM_REVERSE_WRAP:
        case XTP_MENU_ITEM_AUTOLINEFEED:
        case XTP_MENU_ITEM_APPLICATION_CURSOR:
        case XTP_MENU_ITEM_APPLICATION_KEYPAD:
                /* Handled by terminal_mode_menu_items above. */
                return;
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
ResolveChildCommand(int *argc, char **argv, char *default_command[2], char ***command)
{
        int argument;

        *command = NULL;
        for (argument = 1; argument < *argc; ++argument) {
                if (strcmp(argv[argument], "-e") == 0) {
                        if (argument + 1 == *argc) {
                                XtpLog(XTP_LOG_ERROR, "startup", "-e requires a command");
                                return -1;
                        }
                        *command = &argv[argument + 1];
                        *argc = argument;
                        break;
                }
        }
        if (*command == NULL) {
                default_command[0] = getenv("SHELL");
                if (default_command[0] == NULL || default_command[0][0] == '\0')
                        default_command[0] = (char *)"/bin/sh";
                default_command[1] = NULL;
                *command = default_command;
        }
        return 0;
}

static int
OpenApplication(App *app, int *argc, char **argv, AppResources *resources)
{
        Arg args[7];
        Cardinal num_args = 0;

        XtSetLanguageProc(NULL, NULL, NULL);
        XtToolkitInitialize();
        app->context = XtCreateApplicationContext();
        if (app->context == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot create application context");
                return -1;
        }
        XtAppSetFallbackResources(app->context, (String *)fallback_resources);

        app->display = XtOpenDisplay(app->context, NULL, "xterm", "XTerm", XtpCommandOptions,
                                     XtpCommandOptionCount, argc, argv);
        if (app->display == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot open display");
                return -1;
        }
        XtpLog(XTP_LOG_INFO, "startup", "display opened name=%s remaining-argc=%d",
               DisplayString(app->display), *argc);

        ConfigureApplicationVisual(app);

        XtSetArg(args[num_args], XtNallowShellResize, True);
        ++num_args;
        XtSetArg(args[num_args], XtNtitle, "xterm+");
        ++num_args;
        XtSetArg(args[num_args], XtNinput, True);
        ++num_args;
        XtSetArg(args[num_args], XtNmappedWhenManaged, False);
        ++num_args;
        XtSetArg(args[num_args], XtNvisual, app->visual);
        ++num_args;
        XtSetArg(args[num_args], XtNdepth, app->depth);
        ++num_args;
        XtSetArg(args[num_args], XtNcolormap, app->colormap);
        ++num_args;
        app->shell = XtAppCreateShell("xterm", "XTerm", applicationShellWidgetClass, app->display,
                                      args, num_args);
        if (app->shell == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot create application shell");
                return -1;
        }
        XtpLog(XTP_LOG_INFO, "shell", "created instance=xterm class=XTerm title=xterm+");

        XtGetApplicationResources(app->shell, resources, application_resources,
                                  XtNumber(application_resources), NULL, 0);
        XtpLogSetDebug(resources->debug);
        XtpLog(XTP_LOG_INFO, "config", "application resolved menuLocale=%s debug=%s",
               resources->menu_locale != NULL ? resources->menu_locale : "(null)",
               XtpLogDebugEnabled() ? "true" : "false");
        XtpLogResourceDatabases(app->display);

        app->vt = XtVaCreateManagedWidget("vt100", vt100WidgetClass, app->shell, XtNdepth,
                                          app->depth, XtNcolormap, app->colormap, NULL);
        if (app->vt == NULL) {
                XtpLog(XTP_LOG_ERROR, "startup", "cannot create VT100 widget");
                return -1;
        }
        XtpLog(XTP_LOG_INFO, "shell", "created child instance=vt100 class=VT100");
        XtpLog(XTP_LOG_INFO, "config", "active renderer=%s", XtpVtRendererName(app->vt));
        return 0;
}

static void
WireApplication(App *app, const AppResources *resources)
{
        XtAddCallback(app->vt, XtNfontChangedCallback, FontChanged, app);
        XtAddCallback(app->vt, XtNsizeChangedCallback, SizeChanged, app);
        XtAddCallback(app->vt, XtNpopupMenuCallback, PopupRequested, app);
        XtAddCallback(app->vt, XtNpasteCallback, PasteReceived, app);
        XtAddCallback(app->vt, XtNinputCallback, EncodedInputReceived, app);
        XtpMenusCreate(&app->menus, app->shell, resources->menu_locale, MenuDispatch, app);
        XtpMenusSetScrollbar(&app->menus, XtpVtScrollbarVisible(app->vt));
        XtpMenusSetRenderFont(&app->menus, XtpVtUsingXft(app->vt), XtpVtXftAvailable(app->vt));
        XtpMenusSetChecked(&app->menus, XTP_MENU_ITEM_SELECT_TO_CLIPBOARD,
                           XtpVtSelectToClipboard(app->vt));
        XtpMenusSetOpacity(&app->menus, (int)XtpVtBackgroundOpacityPercent(app->vt),
                           XtpVtBackgroundOpacityAvailable(app->vt));
}

static int
CreateTerminal(App *app)
{
        XtpTerminalEffects effects = {
            TerminalWritePty,
            TerminalBell,
            TerminalTitle,
            app,
        };

        app->terminal =
            XtpTerminalNew((uint16_t)XtpVtColumns(app->vt), (uint16_t)XtpVtRows(app->vt),
                           XtpVtCellWidth(app->vt), XtpVtCellHeight(app->vt));
        if (app->terminal == NULL) {
                XtpLog(XTP_LOG_ERROR, "terminal", "cannot initialize backend=%s",
                       XtpTerminalBackend());
                return -1;
        }
        XtpLog(XTP_LOG_INFO, "config", "terminal backend=%s", XtpTerminalBackend());
        XtpTerminalSetEffects(app->terminal, &effects);
        XtpVtSetTerminal(app->vt, app->terminal);
        return 0;
}

static void
RealizeApplication(App *app)
{
        UpdateGeometry(app);
        XtRealizeWidget(app->shell);
        XtpLog(XTP_LOG_INFO, "shell",
               "realized window=0x%lx pixels=%ux%u depth=%d argb=%s background-alpha=%u",
               XtWindow(app->shell), XtpVtNaturalWidth(app->vt), XtpVtNaturalHeight(app->vt),
               app->depth, app->argb_visual ? "true" : "false", app->background_alpha);
        UpdateGeometry(app);
        XtSetKeyboardFocus(app->shell, app->vt);
        app->wm_delete_window = XInternAtom(app->display, "WM_DELETE_WINDOW", False);
        (void)XSetWMProtocols(app->display, XtWindow(app->shell), &app->wm_delete_window, 1);
        XtAddEventHandler(app->shell, StructureNotifyMask, True, ShellEvent, app);
}

static int
StartChild(App *app, char **command)
{
        if (XtpTerminalBackendIsStub())
                return 0;

        app->pty =
            XtpPtySpawn(command, (uint16_t)XtpVtColumns(app->vt), (uint16_t)XtpVtRows(app->vt),
                        XtpVtCellWidth(app->vt), XtpVtCellHeight(app->vt));
        if (app->pty == NULL) {
                XtpLog(XTP_LOG_ERROR, "pty", "cannot start command=%s", command[0]);
                return -1;
        }
        app->pty_input = XtAppAddInput(app->context, XtpPtyFd(app->pty),
                                       (XtPointer)(uintptr_t)XtInputReadMask, PtyReady, app);
        return 0;
}

static void
DestroyApplication(App *app)
{
        if (app->pty_input != (XtInputId)0) {
                XtRemoveInput(app->pty_input);
                app->pty_input = (XtInputId)0;
        }
        StopWatchingPtyOutput(app);
        XtpPtyFree(app->pty);
        app->pty = NULL;
        if (app->vt != NULL)
                XtpVtSetTerminal(app->vt, NULL);
        if (app->terminal != NULL) {
                XtpTerminalFree(app->terminal);
                app->terminal = NULL;
        }
        if (app->shell != NULL) {
                XtDestroyWidget(app->shell);
                app->shell = NULL;
                app->vt = NULL;
        }
        if (app->owns_colormap && app->display != NULL) {
                XFreeColormap(app->display, app->colormap);
                app->colormap = None;
                app->owns_colormap = False;
        }
        XtpMenusDestroy(&app->menus, app->display);
        if (app->display != NULL) {
                XtCloseDisplay(app->display);
                app->display = NULL;
        }
        if (app->context != NULL) {
                XtDestroyApplicationContext(app->context);
                app->context = NULL;
        }
}

int
main(int argc, char **argv)
{
        App app;
        AppResources resources;
        char **command = NULL;
        char *default_command[2];
        int argument;
        int original_argc = argc;
        int status = EXIT_FAILURE;
        XrmDatabase command_database = NULL;
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

        if (ResolveChildCommand(&argc, argv, default_command, &command) != 0)
                return EXIT_FAILURE;
        command_database = XtpConfigCommandDatabase(original_argc, argv);
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
        if (OpenApplication(&app, &argc, argv, &resources) != 0)
                goto done;

        if (resources.report_config) {
                XtpReportConfig(app.display, app.vt, command_database);
                status = EXIT_SUCCESS;
                goto done;
        }

        WireApplication(&app, &resources);
        if (CreateTerminal(&app) != 0)
                goto done;
        RealizeApplication(&app);
        if (StartChild(&app, command) != 0)
                goto done;

        app.running = True;
        XtpLog(XTP_LOG_INFO, "startup", "event loop starting");
        XtMapWidget(app.shell);
        while (app.running)
                XtAppProcessEvent(app.context, XtIMAll);

        XtpLog(XTP_LOG_INFO, "startup", "event loop stopping");
        status = EXIT_SUCCESS;

done:
        DestroyApplication(&app);
        if (command_database != NULL)
                XrmDestroyDatabase(command_database);
        XtpLog(XTP_LOG_INFO, "startup", "shutdown complete status=%d", status);
        return status;
}
