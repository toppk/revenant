#ifndef XTERM_PLUS_SME_SLIDER_H
#define XTERM_PLUS_SME_SLIDER_H

#include <X11/Xaw/Sme.h>

#ifndef XtNvalue
#define XtNvalue "value"
#endif
#ifndef XtCValue
#define XtCValue "Value"
#endif

typedef struct _XtpSmeSliderClassRec *XtpSmeSliderObjectClass;
typedef struct _XtpSmeSliderRec *XtpSmeSliderObject;

extern WidgetClass xtpSmeSliderObjectClass;

Boolean XtpSmeSliderHandlePointer(Widget widget, int menu_x, int menu_y);
void XtpSmeSliderSetValue(Widget widget, int value);
int XtpSmeSliderGetValue(Widget widget);

#endif
