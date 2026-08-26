#ifndef XTERM_PLUS_MENUS_H
#define XTERM_PLUS_MENUS_H

#include <X11/Intrinsic.h>

#define XTP_MAIN_MENU_ENTRIES 35
#define XTP_VT_MENU_ENTRIES 31
#define XTP_FONT_MENU_ENTRIES 28

typedef void (*XtpMenuDispatch)(Widget source, const char *menu_name, const char *entry_name,
                                XtPointer closure);

typedef struct
{
        Widget main_menu;
        Widget vt_menu;
        Widget font_menu;
        Widget scrollbar_item;
        Widget render_font_item;
        Pixmap checkmark;
        XtpMenuDispatch dispatch;
        XtPointer closure;
} XtpMenus;

void XtpMenusCreate(XtpMenus *menus, Widget parent, const char *menu_locale,
                    XtpMenuDispatch dispatch, XtPointer closure);
void XtpMenusPopup(XtpMenus *menus, const char *name, XEvent *event);
void XtpMenusSetScrollbar(XtpMenus *menus, Boolean visible);
void XtpMenusSetRenderFont(XtpMenus *menus, Boolean enabled, Boolean available);
void XtpMenusSetChecked(XtpMenus *menus, const char *menu_name, const char *entry_name,
                        Boolean checked);

#endif
