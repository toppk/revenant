#include "menus.h"

#include "diagnostics.h"
#include "sme_slider.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Shell.h>

#include <locale.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
        MENU_SPEC_ITEM,
        MENU_SPEC_LINE,
        MENU_SPEC_OPACITY_LINE,
} MenuSpecKind;

typedef struct
{
        const char *name;
        XtpMenuItem item;
        MenuSpecKind kind;
        Boolean implemented;
} MenuSpec;

#define ITEM(name, item, implemented) {name, item, MENU_SPEC_ITEM, implemented}
#define INERT(name) ITEM(name, XTP_MENU_ITEM_NONE, False)
#define ACTIVE(name, item) ITEM(name, item, True)
#define LINE(name) {name, XTP_MENU_ITEM_NONE, MENU_SPEC_LINE, False}
#define OPACITY_LINE(name) {name, XTP_MENU_ITEM_NONE, MENU_SPEC_OPACITY_LINE, False}

static const MenuSpec main_specs[] = {
    INERT("toolbar"),
    INERT("fullscreen"),
    INERT("securekbd"),
    INERT("allowsends"),
    ACTIVE("redraw", XTP_MENU_ITEM_REDRAW),
    LINE("line1"),
    INERT("logging"),
    INERT("print-immediate"),
    INERT("print-on-error"),
    INERT("print"),
    INERT("print-redir"),
    INERT("dump-html"),
    INERT("dump-svg"),
    OPACITY_LINE("line2"),
    INERT("8-bit control"),
    ACTIVE("backarrow key", XTP_MENU_ITEM_BACKARROW_KEY),
    ACTIVE("num-lock", XTP_MENU_ITEM_NUM_LOCK),
    ACTIVE("alt-esc", XTP_MENU_ITEM_ALT_ESC),
    INERT("meta-esc"),
    INERT("delete-is-del"),
    INERT("oldFunctionKeys"),
    INERT("tcapFunctionKeys"),
    INERT("hpFunctionKeys"),
    INERT("scoFunctionKeys"),
    INERT("sunFunctionKeys"),
    INERT("sunKeyboard"),
    LINE("line3"),
    INERT("suspend"),
    INERT("continue"),
    INERT("interrupt"),
    INERT("hangup"),
    INERT("terminate"),
    INERT("kill"),
    LINE("line4"),
    ACTIVE("quit", XTP_MENU_ITEM_QUIT),
};

static const MenuSpec vt_specs[] = {
    ACTIVE("scrollbar", XTP_MENU_ITEM_SCROLLBAR),
    INERT("jumpscroll"),
    ACTIVE("reversevideo", XTP_MENU_ITEM_REVERSE_VIDEO),
    ACTIVE("autowrap", XTP_MENU_ITEM_AUTOWRAP),
    ACTIVE("reversewrap", XTP_MENU_ITEM_REVERSE_WRAP),
    ACTIVE("autolinefeed", XTP_MENU_ITEM_AUTOLINEFEED),
    ACTIVE("appcursor", XTP_MENU_ITEM_APPLICATION_CURSOR),
    ACTIVE("appkeypad", XTP_MENU_ITEM_APPLICATION_KEYPAD),
    ACTIVE("scrollkey", XTP_MENU_ITEM_SCROLL_KEY),
    ACTIVE("scrollttyoutput", XTP_MENU_ITEM_SCROLL_TTY_OUTPUT),
    INERT("allow132"),
    INERT("keepSelection"),
    INERT("keepClipboard"),
    ACTIVE("selectToClipboard", XTP_MENU_ITEM_SELECT_TO_CLIPBOARD),
    INERT("visualbell"),
    INERT("bellIsUrgent"),
    INERT("poponbell"),
    INERT("cursorblink"),
    INERT("titeInhibit"),
    INERT("activeicon"),
    LINE("line1"),
    INERT("softreset"),
    INERT("hardreset"),
    INERT("clearsavedlines"),
    LINE("line2"),
    INERT("tekshow"),
    INERT("tekmode"),
    INERT("vthide"),
    INERT("altscreen"),
    INERT("copy_area"),
    INERT("sixelScrolling"),
    INERT("privateColorRegisters"),
};

static const MenuSpec font_specs[] = {
    ACTIVE("fontdefault", XTP_MENU_ITEM_FONT_DEFAULT),
    ACTIVE("font1", XTP_MENU_ITEM_FONT_1),
    ACTIVE("font2", XTP_MENU_ITEM_FONT_2),
    ACTIVE("font3", XTP_MENU_ITEM_FONT_3),
    ACTIVE("font4", XTP_MENU_ITEM_FONT_4),
    ACTIVE("font5", XTP_MENU_ITEM_FONT_5),
    ACTIVE("font6", XTP_MENU_ITEM_FONT_6),
    ACTIVE("font7", XTP_MENU_ITEM_FONT_7),
    INERT("fontescape"),
    INERT("fontsel"),
    LINE("line1"),
    INERT("allow-bold-fonts"),
    INERT("font-linedrawing"),
    INERT("font-packed"),
    INERT("font-doublesize"),
    INERT("font-loadable"),
    LINE("line2"),
    ACTIVE("render-font", XTP_MENU_ITEM_RENDER_FONT),
    INERT("utf8-mode"),
    INERT("utf8-fonts"),
    INERT("utf8-title"),
    LINE("line3"),
    INERT("allow-color-ops"),
    INERT("allow-font-ops"),
    INERT("allow-mouse-ops"),
    INERT("allow-tcap-ops"),
    INERT("allow-title-ops"),
    INERT("allow-window-ops"),
};

#undef ITEM
#undef INERT
#undef ACTIVE
#undef LINE
#undef OPACITY_LINE

_Static_assert(XtNumber(main_specs) == XTP_MAIN_MENU_ENTRIES, "main menu inventory changed");
_Static_assert(XtNumber(vt_specs) == XTP_VT_MENU_ENTRIES, "VT menu inventory changed");
_Static_assert(XtNumber(font_specs) == XTP_FONT_MENU_ENTRIES, "font menu inventory changed");

static void
Activate(Widget widget, XtPointer closure, XtPointer call_data)
{
        XtpMenuBinding *binding = closure;

        (void)call_data;
        XtpLog(XTP_LOG_INFO, "menu", "selected menu=%s item=%s", XtName(XtParent(widget)),
               XtName(widget));
        binding->menus->dispatch(widget, binding->item, binding->menus->closure);
}

static void
OpacityChanged(Widget widget, XtPointer closure, XtPointer call_data)
{
        XtpMenus *menus = closure;

        XtpLog(XTP_LOG_INFO, "menu", "opacity slider value=%ld", (long)(intptr_t)call_data);
        menus->dispatch(widget, XTP_MENU_ITEM_BACKGROUND_OPACITY, menus->closure);
}

static void
OpacityPointerEvent(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        XtpMenus *menus = closure;
        int x;
        int y;

        (void)widget;
        (void)continue_dispatch;
        if (menus->opacity_slider == NULL)
                return;
        if (event->type == MotionNotify) {
                x = event->xmotion.x;
                y = event->xmotion.y;
        } else if (event->type == ButtonPress || event->type == ButtonRelease) {
                x = event->xbutton.x;
                y = event->xbutton.y;
        } else {
                return;
        }
        (void)XtpSmeSliderHandlePointer(menus->opacity_slider, x, y);
}

static void
CreateOpacitySlider(XtpMenus *menus, Widget menu)
{
        menus->opacity_slider =
            XtCreateManagedWidget("backgroundOpacity", xtpSmeSliderObjectClass, menu, NULL, 0);
        XtAddCallback(menus->opacity_slider, XtNcallback, OpacityChanged, menus);
        XtAddEventHandler(menu, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, False,
                          OpacityPointerEvent, menus);
}

static Widget
CreateMenu(XtpMenus *menus, Widget parent, const char *name, const MenuSpec *specs, Cardinal count)
{
        Screen *screen = XtScreen(parent);
        Arg args[3];
        Widget menu;
        Cardinal index;
        Cardinal implemented = 0;

        XtSetArg(args[0], XtNvisual, DefaultVisualOfScreen(screen));
        XtSetArg(args[1], XtNdepth, DefaultDepthOfScreen(screen));
        XtSetArg(args[2], XtNcolormap, DefaultColormapOfScreen(screen));
        menu = XtCreatePopupShell(name, simpleMenuWidgetClass, parent, args, XtNumber(args));

        for (index = 0; index < count; ++index) {
                if (specs[index].kind == MENU_SPEC_ITEM && specs[index].implemented)
                        ++implemented;
        }
        XtpLog(XTP_LOG_DEBUG, "menu",
               "creating name=%s entries=%u implemented=%u locale-sensitive=true", name,
               (unsigned int)count, (unsigned int)implemented);

        for (index = 0; index < count; ++index) {
                Widget item;

                if (specs[index].kind == MENU_SPEC_OPACITY_LINE)
                        CreateOpacitySlider(menus, menu);
                item = XtCreateManagedWidget(
                    specs[index].name,
                    specs[index].kind == MENU_SPEC_ITEM ? smeBSBObjectClass : smeLineObjectClass,
                    menu, NULL, 0);

                if (specs[index].kind == MENU_SPEC_ITEM) {
                        if (!specs[index].implemented)
                                XtSetSensitive(item, False);
                        if (specs[index].item != XTP_MENU_ITEM_NONE) {
                                XtpMenuBinding *binding;

                                if (menus->binding_count >= XTP_MENU_BINDINGS)
                                        XtAppError(XtWidgetToApplicationContext(parent),
                                                   "xterm+: menu binding inventory overflow");
                                binding = &menus->bindings[menus->binding_count++];
                                binding->menus = menus;
                                binding->widget = item;
                                binding->item = specs[index].item;
                                XtAddCallback(item, XtNcallback, Activate, binding);
                        }
                        if (specs[index].item == XTP_MENU_ITEM_SCROLLBAR)
                                menus->scrollbar_item = item;
                        if (specs[index].item == XTP_MENU_ITEM_RENDER_FONT)
                                menus->render_font_item = item;
                }
        }
        return menu;
}

void
XtpMenusCreate(XtpMenus *menus, Widget parent, const char *menu_locale, XtpMenuDispatch dispatch,
               XtPointer closure)
{
        const char *current_locale = setlocale(LC_CTYPE, NULL);
        String saved_locale = current_locale != NULL ? XtNewString(current_locale) : NULL;

        menus->dispatch = dispatch;
        menus->closure = closure;
        menus->scrollbar_item = NULL;
        menus->render_font_item = NULL;
        menus->opacity_slider = NULL;
        menus->checkmark = None;
        menus->binding_count = 0;
        XtpLog(XTP_LOG_INFO, "menu", "initializing menuLocale=%s previous-locale=%s",
               menu_locale != NULL ? menu_locale : "(null)",
               saved_locale != NULL ? saved_locale : "(null)");
        if (menu_locale != NULL)
                (void)setlocale(LC_CTYPE, menu_locale);

        XawSimpleMenuAddGlobalActions(XtWidgetToApplicationContext(parent));
        {
#define CHECK_WIDTH 9
#define CHECK_HEIGHT 8
                static const unsigned char check_bits[] = {
                    0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
                    0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00,
                };

                menus->checkmark =
                    XCreateBitmapFromData(XtDisplay(parent), RootWindowOfScreen(XtScreen(parent)),
                                          (const char *)check_bits, CHECK_WIDTH, CHECK_HEIGHT);
#undef CHECK_WIDTH
#undef CHECK_HEIGHT
        }
        menus->main_menu = CreateMenu(menus, parent, "mainMenu", main_specs, XtNumber(main_specs));
        menus->vt_menu = CreateMenu(menus, parent, "vtMenu", vt_specs, XtNumber(vt_specs));
        menus->font_menu = CreateMenu(menus, parent, "fontMenu", font_specs, XtNumber(font_specs));

        if (saved_locale != NULL) {
                (void)setlocale(LC_CTYPE, saved_locale);
                XtFree(saved_locale);
        }
}

void
XtpMenusPopup(XtpMenus *menus, const char *name, XEvent *event)
{
        Widget menu = NULL;
        String params[1];
        Cardinal num_params = 1;

        if (strcmp(name, "mainMenu") == 0)
                menu = menus->main_menu;
        else if (strcmp(name, "vtMenu") == 0)
                menu = menus->vt_menu;
        else if (strcmp(name, "fontMenu") == 0)
                menu = menus->font_menu;

        if (menu == NULL)
                return;

        XtpLog(XTP_LOG_INFO, "menu", "popup name=%s event=%d root=%d,%d", name,
               event != NULL ? event->type : 0, event != NULL ? event->xbutton.x_root : 0,
               event != NULL ? event->xbutton.y_root : 0);

        params[0] = (String)name;
        XtCallActionProc(menu, "XawPositionSimpleMenu", event, params, num_params);
        XtPopupSpringLoaded(menu);
        if (menu == menus->main_menu && menus->opacity_slider != NULL) {
                Position x = 0;
                Position y = 0;
                Dimension width = 0;
                Dimension height = 0;
                Arg args[4];

                XtSetArg(args[0], XtNx, &x);
                XtSetArg(args[1], XtNy, &y);
                XtSetArg(args[2], XtNwidth, &width);
                XtSetArg(args[3], XtNheight, &height);
                XtGetValues(menus->opacity_slider, args, XtNumber(args));
                XtpLog(XTP_LOG_DEBUG, "menu",
                       "opacity slider geometry x=%d y=%d width=%d height=%d", (int)x, (int)y,
                       (int)width, (int)height);
        }
}

void
XtpMenusSetScrollbar(XtpMenus *menus, Boolean visible)
{
        if (menus->scrollbar_item == NULL)
                return;
        XtVaSetValues(menus->scrollbar_item, XtNleftBitmap, visible ? menus->checkmark : None,
                      NULL);
        XtpLog(XTP_LOG_DEBUG, "menu", "scrollbar checked=%s", visible ? "true" : "false");
}

void
XtpMenusSetRenderFont(XtpMenus *menus, Boolean enabled, Boolean available)
{
        if (menus->render_font_item == NULL)
                return;
        XtVaSetValues(menus->render_font_item, XtNleftBitmap, enabled ? menus->checkmark : None,
                      NULL);
        XtSetSensitive(menus->render_font_item, available);
        XtpLog(XTP_LOG_DEBUG, "menu", "render-font checked=%s sensitive=%s",
               enabled ? "true" : "false", available ? "true" : "false");
}

void
XtpMenusSetChecked(XtpMenus *menus, XtpMenuItem item, Boolean checked)
{
        Cardinal index;

        for (index = 0; index < menus->binding_count; ++index) {
                if (menus->bindings[index].item == item) {
                        XtVaSetValues(menus->bindings[index].widget, XtNleftBitmap,
                                      checked ? menus->checkmark : None, NULL);
                        return;
                }
        }
}

void
XtpMenusSetOpacity(XtpMenus *menus, int percent, Boolean available)
{
        if (menus->opacity_slider == NULL)
                return;
        XtpSmeSliderSetValue(menus->opacity_slider, percent);
        XtSetSensitive(menus->opacity_slider, available);
        XtpLog(XTP_LOG_DEBUG, "menu", "opacity slider value=%d sensitive=%s", percent,
               available ? "true" : "false");
}

int
XtpMenusOpacity(const XtpMenus *menus)
{
        return menus->opacity_slider != NULL ? XtpSmeSliderGetValue(menus->opacity_slider) : 100;
}

void
XtpMenusDestroy(XtpMenus *menus, Display *display)
{
        if (menus->checkmark != None && display != NULL)
                XFreePixmap(display, menus->checkmark);
        menus->checkmark = None;
        menus->binding_count = 0;
}
