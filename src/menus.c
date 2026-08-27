#include "menus.h"

#include "diagnostics.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#include <locale.h>
#include <string.h>

typedef struct
{
        const char *name;
        Boolean separator;
        Boolean implemented;
} MenuSpec;

#define ITEM(name, implemented) {name, False, implemented}
#define LINE(name) {name, True, False}

static const MenuSpec main_specs[] = {
    ITEM("toolbar", False),
    ITEM("fullscreen", False),
    ITEM("securekbd", False),
    ITEM("allowsends", False),
    ITEM("redraw", True),
    LINE("line1"),
    ITEM("logging", False),
    ITEM("print-immediate", False),
    ITEM("print-on-error", False),
    ITEM("print", False),
    ITEM("print-redir", False),
    ITEM("dump-html", False),
    ITEM("dump-svg", False),
    LINE("line2"),
    ITEM("8-bit control", False),
    ITEM("backarrow key", True),
    ITEM("num-lock", True),
    ITEM("alt-esc", True),
    ITEM("meta-esc", False),
    ITEM("delete-is-del", False),
    ITEM("oldFunctionKeys", False),
    ITEM("tcapFunctionKeys", False),
    ITEM("hpFunctionKeys", False),
    ITEM("scoFunctionKeys", False),
    ITEM("sunFunctionKeys", False),
    ITEM("sunKeyboard", False),
    LINE("line3"),
    ITEM("suspend", False),
    ITEM("continue", False),
    ITEM("interrupt", False),
    ITEM("hangup", False),
    ITEM("terminate", False),
    ITEM("kill", False),
    LINE("line4"),
    ITEM("quit", True),
};

static const MenuSpec vt_specs[] = {
    ITEM("scrollbar", True),
    ITEM("jumpscroll", False),
    ITEM("reversevideo", True),
    ITEM("autowrap", True),
    ITEM("reversewrap", True),
    ITEM("autolinefeed", True),
    ITEM("appcursor", True),
    ITEM("appkeypad", True),
    ITEM("scrollkey", True),
    ITEM("scrollttyoutput", True),
    ITEM("allow132", False),
    ITEM("keepSelection", False),
    ITEM("keepClipboard", False),
    ITEM("selectToClipboard", True),
    ITEM("visualbell", False),
    ITEM("bellIsUrgent", False),
    ITEM("poponbell", False),
    ITEM("cursorblink", False),
    ITEM("titeInhibit", False),
    ITEM("activeicon", False),
    LINE("line1"),
    ITEM("softreset", False),
    ITEM("hardreset", False),
    ITEM("clearsavedlines", False),
    LINE("line2"),
    ITEM("tekshow", False),
    ITEM("tekmode", False),
    ITEM("vthide", False),
    ITEM("altscreen", False),
    ITEM("sixelScrolling", False),
    ITEM("privateColorRegisters", False),
};

static const MenuSpec font_specs[] = {
    ITEM("fontdefault", True),
    ITEM("font1", True),
    ITEM("font2", True),
    ITEM("font3", True),
    ITEM("font4", True),
    ITEM("font5", True),
    ITEM("font6", True),
    ITEM("font7", True),
    ITEM("fontescape", False),
    ITEM("fontsel", False),
    LINE("line1"),
    ITEM("allow-bold-fonts", False),
    ITEM("font-linedrawing", False),
    ITEM("font-packed", False),
    ITEM("font-doublesize", False),
    ITEM("font-loadable", False),
    LINE("line2"),
    ITEM("render-font", True),
    ITEM("utf8-mode", False),
    ITEM("utf8-fonts", False),
    ITEM("utf8-title", False),
    LINE("line3"),
    ITEM("allow-color-ops", False),
    ITEM("allow-font-ops", False),
    ITEM("allow-mouse-ops", False),
    ITEM("allow-tcap-ops", False),
    ITEM("allow-title-ops", False),
    ITEM("allow-window-ops", False),
};

#undef ITEM
#undef LINE

_Static_assert(XtNumber(main_specs) == XTP_MAIN_MENU_ENTRIES, "main menu inventory changed");
_Static_assert(XtNumber(vt_specs) == XTP_VT_MENU_ENTRIES, "VT menu inventory changed");
_Static_assert(XtNumber(font_specs) == XTP_FONT_MENU_ENTRIES, "font menu inventory changed");

static void
Activate(Widget widget, XtPointer closure, XtPointer call_data)
{
        XtpMenus *menus = closure;

        (void)call_data;
        XtpLog(XTP_LOG_INFO, "menu", "selected menu=%s item=%s", XtName(XtParent(widget)),
               XtName(widget));
        menus->dispatch(widget, XtName(XtParent(widget)), XtName(widget), menus->closure);
}

static Widget
CreateMenu(XtpMenus *menus, Widget parent, const char *name, const MenuSpec *specs, Cardinal count)
{
        Widget menu = XtCreatePopupShell(name, simpleMenuWidgetClass, parent, NULL, 0);
        Cardinal index;
        Cardinal implemented = 0;

        for (index = 0; index < count; ++index) {
                if (!specs[index].separator && specs[index].implemented)
                        ++implemented;
        }
        XtpLog(XTP_LOG_DEBUG, "menu",
               "creating name=%s entries=%u implemented=%u locale-sensitive=true", name,
               (unsigned int)count, (unsigned int)implemented);

        for (index = 0; index < count; ++index) {
                Widget item = XtCreateManagedWidget(
                    specs[index].name,
                    specs[index].separator ? smeLineObjectClass : smeBSBObjectClass, menu, NULL, 0);

                if (!specs[index].separator) {
                        XtAddCallback(item, XtNcallback, Activate, menus);
                        if (!specs[index].implemented)
                                XtSetSensitive(item, False);
                        if (strcmp(name, "vtMenu") == 0 &&
                            strcmp(specs[index].name, "scrollbar") == 0)
                                menus->scrollbar_item = item;
                        if (strcmp(name, "fontMenu") == 0 &&
                            strcmp(specs[index].name, "render-font") == 0)
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
        menus->checkmark = None;
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
XtpMenusSetChecked(XtpMenus *menus, const char *menu_name, const char *entry_name, Boolean checked)
{
        Widget menu = NULL;
        Widget item;

        if (strcmp(menu_name, "mainMenu") == 0)
                menu = menus->main_menu;
        else if (strcmp(menu_name, "vtMenu") == 0)
                menu = menus->vt_menu;
        else if (strcmp(menu_name, "fontMenu") == 0)
                menu = menus->font_menu;
        if (menu == NULL)
                return;
        item = XtNameToWidget(menu, entry_name);
        if (item == NULL)
                return;
        XtVaSetValues(item, XtNleftBitmap, checked ? menus->checkmark : None, NULL);
}
