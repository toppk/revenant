#include "sme_sliderP.h"

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/XawInit.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define XTP_SLIDER_PAD_X 8
#define XTP_SLIDER_TRACK_HEIGHT 4
#define XTP_SLIDER_THUMB_WIDTH 8
#define XTP_SLIDER_THUMB_HEIGHT 14
#define XTP_SLIDER_VALUE_WIDTH 44

static void Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args);
static void Destroy(Widget widget);
static void Redisplay(Widget widget, XEvent *event, Region region);
static Boolean SetValues(Widget current, Widget request, Widget new_widget, ArgList args,
                         Cardinal *num_args);
static void Highlight(Widget widget);
static void Unhighlight(Widget widget);
static void Notify(Widget widget);

#define OFFSET(field) XtOffsetOf(XtpSmeSliderRec, slider.field)

static XtResource resources[] = {
    {XtNlabel, XtCLabel, XtRString, sizeof(String), OFFSET(label), XtRString,
     (XtPointer) "Opacity"},
    {XtNvalue, XtCValue, XtRInt, sizeof(int), OFFSET(value), XtRImmediate, (XtPointer)100},
    {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *), OFFSET(font), XtRString,
     XtDefaultFont},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel), OFFSET(foreground), XtRString,
     XtDefaultForeground},
    {XtNcallback, XtCCallback, XtRCallback, sizeof(XtCallbackList), OFFSET(callbacks), XtRCallback,
     NULL},
};

#undef OFFSET

XtpSmeSliderClassRec xtpSmeSliderClassRec = {
    {
        (WidgetClass)&smeClassRec,
        "SmeSlider",
        sizeof(XtpSmeSliderRec),
        XawInitializeWidgetSet,
        NULL,
        False,
        Initialize,
        NULL,
        NULL,
        NULL,
        0,
        resources,
        XtNumber(resources),
        NULLQUARK,
        False,
        False,
        False,
        False,
        Destroy,
        NULL,
        Redisplay,
        SetValues,
        NULL,
        NULL,
        NULL,
        NULL,
        XtVersion,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
    },
    {
        Highlight,
        Unhighlight,
        Notify,
        NULL,
    },
    {NULL},
};

WidgetClass xtpSmeSliderObjectClass = (WidgetClass)&xtpSmeSliderClassRec;

static int
ClampValue(int value)
{
        if (value < 0)
                return 0;
        if (value > 100)
                return 100;
        return value;
}

static void
ReleaseGcs(XtpSmeSliderObject slider)
{
        Widget menu = XtParent((Widget)slider);

        if (slider->slider.gc != NULL)
                XtReleaseGC(menu, slider->slider.gc);
        if (slider->slider.background_gc != NULL)
                XtReleaseGC(menu, slider->slider.background_gc);
        slider->slider.gc = NULL;
        slider->slider.background_gc = NULL;
}

static void
CreateGcs(XtpSmeSliderObject slider)
{
        Widget menu = XtParent((Widget)slider);
        XGCValues values;
        XtGCMask mask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;

        values.foreground = slider->slider.foreground;
        values.background = menu->core.background_pixel;
        values.font = slider->slider.font->fid;
        values.graphics_exposures = False;
        slider->slider.gc = XtGetGC(menu, mask, &values);

        values.foreground = menu->core.background_pixel;
        values.background = slider->slider.foreground;
        slider->slider.background_gc = XtGetGC(menu, mask, &values);
}

static void
Initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)new_widget;

        (void)request;
        (void)args;
        (void)num_args;
        slider->slider.value = ClampValue(slider->slider.value);
        slider->slider.label = XtNewString(slider->slider.label);
        slider->slider.highlighted = False;
        slider->slider.gc = NULL;
        slider->slider.background_gc = NULL;
        CreateGcs(slider);
        if (slider->rectangle.height == 0)
                slider->rectangle.height = XTP_SLIDER_THUMB_HEIGHT + 8;
        if (slider->rectangle.width == 0)
                slider->rectangle.width = 220;
}

static void
Destroy(Widget widget)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;

        ReleaseGcs(slider);
        if (slider->slider.label != NULL)
                XtFree(slider->slider.label);
}

static void
DrawSlider(Widget widget)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;
        Display *display = XtDisplayOfObject(widget);
        Window window = XtWindowOfObject(widget);
        XFontStruct *font = slider->slider.font;
        char value_text[8];
        int x = slider->rectangle.x;
        int y = slider->rectangle.y;
        int width = slider->rectangle.width;
        int height = slider->rectangle.height;
        int label_width;
        int track_x;
        int track_width;
        int thumb_x;
        int baseline;

        if (window == None || width <= 0 || height <= 0)
                return;
        XFillRectangle(display, window, slider->slider.background_gc, x, y, (unsigned int)width,
                       (unsigned int)height);
        if (slider->slider.highlighted && width > 2 && height > 2)
                XDrawRectangle(display, window, slider->slider.gc, x + 1, y + 1,
                               (unsigned int)(width - 3), (unsigned int)(height - 3));

        baseline = y + (height + font->ascent - font->descent) / 2;
        label_width = XTextWidth(font, slider->slider.label, (int)strlen(slider->slider.label));
        XDrawString(display, window, slider->slider.gc, x + XTP_SLIDER_PAD_X, baseline,
                    slider->slider.label, (int)strlen(slider->slider.label));

        (void)snprintf(value_text, sizeof(value_text), "%d%%", slider->slider.value);
        XDrawString(display, window, slider->slider.gc,
                    x + width - XTP_SLIDER_PAD_X -
                        XTextWidth(font, value_text, (int)strlen(value_text)),
                    baseline, value_text, (int)strlen(value_text));

        track_x = x + XTP_SLIDER_PAD_X + label_width + XTP_SLIDER_PAD_X;
        track_width = width - (track_x - x) - XTP_SLIDER_VALUE_WIDTH - XTP_SLIDER_PAD_X;
        if (track_width <= XTP_SLIDER_THUMB_WIDTH)
                return;
        XFillRectangle(display, window, slider->slider.gc, track_x,
                       y + (height - XTP_SLIDER_TRACK_HEIGHT) / 2, (unsigned int)track_width,
                       XTP_SLIDER_TRACK_HEIGHT);
        thumb_x = track_x + (track_width - XTP_SLIDER_THUMB_WIDTH) * slider->slider.value / 100;
        XFillRectangle(display, window, slider->slider.background_gc, thumb_x - 1,
                       y + (height - XTP_SLIDER_THUMB_HEIGHT) / 2 - 1, XTP_SLIDER_THUMB_WIDTH + 2,
                       XTP_SLIDER_THUMB_HEIGHT + 2);
        XFillRectangle(display, window, slider->slider.gc, thumb_x,
                       y + (height - XTP_SLIDER_THUMB_HEIGHT) / 2, XTP_SLIDER_THUMB_WIDTH,
                       XTP_SLIDER_THUMB_HEIGHT);
}

static void
Redisplay(Widget widget, XEvent *event, Region region)
{
        (void)event;
        (void)region;
        DrawSlider(widget);
}

static Boolean
SetValues(Widget current, Widget request, Widget new_widget, ArgList args, Cardinal *num_args)
{
        XtpSmeSliderObject old_slider = (XtpSmeSliderObject)current;
        XtpSmeSliderObject new_slider = (XtpSmeSliderObject)new_widget;
        Boolean redraw = False;

        (void)request;
        (void)args;
        (void)num_args;
        new_slider->slider.value = ClampValue(new_slider->slider.value);
        if (old_slider->slider.value != new_slider->slider.value)
                redraw = True;
        if (old_slider->slider.label != new_slider->slider.label) {
                new_slider->slider.label = XtNewString(new_slider->slider.label);
                if (old_slider->slider.label != NULL)
                        XtFree(old_slider->slider.label);
                redraw = True;
        }
        if (old_slider->slider.font != new_slider->slider.font ||
            old_slider->slider.foreground != new_slider->slider.foreground) {
                ReleaseGcs(new_slider);
                CreateGcs(new_slider);
                redraw = True;
        }
        return redraw;
}

static void
Highlight(Widget widget)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;

        slider->slider.highlighted = True;
        DrawSlider(widget);
}

static void
Unhighlight(Widget widget)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;

        slider->slider.highlighted = False;
        DrawSlider(widget);
}

static void
Notify(Widget widget)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;

        XtCallCallbackList(widget, slider->slider.callbacks,
                           (XtPointer)(intptr_t)slider->slider.value);
}

Boolean
XtpSmeSliderHandlePointer(Widget widget, int menu_x, int menu_y)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;
        int x = slider->rectangle.x;
        int y = slider->rectangle.y;
        int width = slider->rectangle.width;
        int height = slider->rectangle.height;
        int label_width;
        int track_x;
        int track_width;
        int value;

        if (!XtIsSensitive(widget) || menu_y < y || menu_y >= y + height)
                return False;
        label_width = XTextWidth(slider->slider.font, slider->slider.label,
                                 (int)strlen(slider->slider.label));
        track_x = x + XTP_SLIDER_PAD_X + label_width + XTP_SLIDER_PAD_X;
        track_width = width - (track_x - x) - XTP_SLIDER_VALUE_WIDTH - XTP_SLIDER_PAD_X;
        if (track_width <= XTP_SLIDER_THUMB_WIDTH)
                return False;
        value = (menu_x - track_x - XTP_SLIDER_THUMB_WIDTH / 2) * 100 /
                (track_width - XTP_SLIDER_THUMB_WIDTH);
        value = ClampValue(value);
        if (value == slider->slider.value)
                return True;
        slider->slider.value = value;
        DrawSlider(widget);
        XtCallCallbackList(widget, slider->slider.callbacks, (XtPointer)(intptr_t)value);
        return True;
}

void
XtpSmeSliderSetValue(Widget widget, int value)
{
        XtpSmeSliderObject slider = (XtpSmeSliderObject)widget;

        value = ClampValue(value);
        if (slider->slider.value == value)
                return;
        slider->slider.value = value;
        DrawSlider(widget);
}

int
XtpSmeSliderGetValue(Widget widget)
{
        return ((XtpSmeSliderObject)widget)->slider.value;
}
