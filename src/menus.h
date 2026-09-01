#ifndef XTERM_PLUS_MENUS_H
#define XTERM_PLUS_MENUS_H

#include <X11/Intrinsic.h>

#define XTP_MAIN_MENU_ENTRIES 35
#define XTP_VT_MENU_ENTRIES 32
#define XTP_FONT_MENU_ENTRIES 28
#define XTP_MENU_BINDINGS (XTP_MAIN_MENU_ENTRIES + XTP_VT_MENU_ENTRIES + XTP_FONT_MENU_ENTRIES)

typedef enum
{
        XTP_MENU_ITEM_NONE,
        XTP_MENU_ITEM_REDRAW,
        XTP_MENU_ITEM_BACKGROUND_OPACITY,
        XTP_MENU_ITEM_BACKARROW_KEY,
        XTP_MENU_ITEM_NUM_LOCK,
        XTP_MENU_ITEM_ALT_ESC,
        XTP_MENU_ITEM_QUIT,
        XTP_MENU_ITEM_SCROLLBAR,
        XTP_MENU_ITEM_REVERSE_VIDEO,
        XTP_MENU_ITEM_AUTOWRAP,
        XTP_MENU_ITEM_REVERSE_WRAP,
        XTP_MENU_ITEM_AUTOLINEFEED,
        XTP_MENU_ITEM_APPLICATION_CURSOR,
        XTP_MENU_ITEM_APPLICATION_KEYPAD,
        XTP_MENU_ITEM_SCROLL_KEY,
        XTP_MENU_ITEM_SCROLL_TTY_OUTPUT,
        XTP_MENU_ITEM_SELECT_TO_CLIPBOARD,
        XTP_MENU_ITEM_FONT_DEFAULT,
        XTP_MENU_ITEM_FONT_1,
        XTP_MENU_ITEM_FONT_2,
        XTP_MENU_ITEM_FONT_3,
        XTP_MENU_ITEM_FONT_4,
        XTP_MENU_ITEM_FONT_5,
        XTP_MENU_ITEM_FONT_6,
        XTP_MENU_ITEM_FONT_7,
        XTP_MENU_ITEM_RENDER_FONT,
} XtpMenuItem;

typedef void (*XtpMenuDispatch)(Widget source, XtpMenuItem item, XtPointer closure);

struct XtpMenus;

typedef struct
{
        struct XtpMenus *menus;
        Widget widget;
        XtpMenuItem item;
} XtpMenuBinding;

typedef struct XtpMenus
{
        Widget main_menu;
        Widget vt_menu;
        Widget font_menu;
        Widget scrollbar_item;
        Widget render_font_item;
        Widget opacity_slider;
        Pixmap checkmark;
        XtpMenuDispatch dispatch;
        XtPointer closure;
        XtpMenuBinding bindings[XTP_MENU_BINDINGS];
        Cardinal binding_count;
} XtpMenus;

void XtpMenusCreate(XtpMenus *menus, Widget parent, const char *menu_locale,
                    XtpMenuDispatch dispatch, XtPointer closure);
void XtpMenusPopup(XtpMenus *menus, const char *name, XEvent *event);
void XtpMenusSetScrollbar(XtpMenus *menus, Boolean visible);
void XtpMenusSetRenderFont(XtpMenus *menus, Boolean enabled, Boolean available);
void XtpMenusSetChecked(XtpMenus *menus, XtpMenuItem item, Boolean checked);
void XtpMenusSetOpacity(XtpMenus *menus, int percent, Boolean available);
int XtpMenusOpacity(const XtpMenus *menus);
void XtpMenusDestroy(XtpMenus *menus, Display *display);

#endif
