#include "vt_widgetP.h"

#include "diagnostics.h"

#include <X11/StringDefs.h>
#include <X11/Xaw/Simple.h>

/* Step interval of the moving block that shows indeterminate progress. */
#define XTP_PROGRESS_TICK_MS 120UL
#define XTP_PROGRESS_HEIGHT 6U
#define XTP_PROGRESS_MIN_WIDTH 40U
#define XTP_PROGRESS_MAX_WIDTH 160U
#define XTP_PROGRESS_BORDER 1U

static const char *
ProgressStateName(XtpProgressState state)
{
        switch (state) {
        case XTP_PROGRESS_REMOVE:
                return "remove";
        case XTP_PROGRESS_SET:
                return "set";
        case XTP_PROGRESS_ERROR:
                return "error";
        case XTP_PROGRESS_INDETERMINATE:
                return "indeterminate";
        case XTP_PROGRESS_PAUSE:
                return "pause";
        }
        return "?";
}

static void
DrawProgress(Vt100Rec *vt)
{
        Widget indicator = vt->vt.progress_widget;
        Display *display;
        Window window;
        unsigned int width;
        unsigned int height;
        unsigned int fill_x = 0;
        unsigned int fill_width;
        Pixel fill = VtOpaquePixel(vt, vt->vt.effective_foreground);

        if (indicator == NULL || !XtIsRealized(indicator) ||
            vt->vt.progress_state == XTP_PROGRESS_REMOVE)
                return;
        display = XtDisplay(indicator);
        window = XtWindow(indicator);
        if (vt->vt.progress_gc == NULL)
                vt->vt.progress_gc = XCreateGC(display, window, 0, NULL);
        width = indicator->core.width;
        height = indicator->core.height;
        if (vt->vt.progress_state == XTP_PROGRESS_INDETERMINATE) {
                unsigned int travel;

                fill_width = width / 4U;
                travel = width - fill_width;
                fill_x = travel * (vt->vt.progress_phase % 9U) / 8U;
        } else {
                fill_width = width * (unsigned int)vt->vt.progress_percent / 100U;
                if (vt->vt.progress_state == XTP_PROGRESS_ERROR)
                        fill = vt->vt.progress_status_allocated[0]
                                   ? vt->vt.progress_status_pixels[0]
                                   : fill;
                else if (vt->vt.progress_state == XTP_PROGRESS_PAUSE)
                        fill = vt->vt.progress_status_allocated[1]
                                   ? vt->vt.progress_status_pixels[1]
                                   : fill;
        }
        /* The track is the terminal's own background surface, translucent when opacity is set. */
        XSetForeground(display, vt->vt.progress_gc, vt->vt.effective_background_pixel);
        XFillRectangle(display, window, vt->vt.progress_gc, 0, 0, width, height);
        if (fill_width != 0) {
                XSetForeground(display, vt->vt.progress_gc, fill);
                XFillRectangle(display, window, vt->vt.progress_gc, (int)fill_x, 0, fill_width,
                               height);
        }
}

static void
ProgressExpose(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        (void)widget;
        (void)continue_dispatch;
        if (event->type == Expose && event->xexpose.count == 0)
                DrawProgress(closure);
}

static void
ProgressTick(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;

        (void)timer;
        vt->vt.progress_timer = (XtIntervalId)0;
        if (vt->vt.progress_state != XTP_PROGRESS_INDETERMINATE)
                return;
        ++vt->vt.progress_phase;
        DrawProgress(vt);
        XtpLog(XTP_LOG_DEBUG, "progress", "indeterminate phase=%u", vt->vt.progress_phase);
        vt->vt.progress_timer = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt),
                                                XTP_PROGRESS_TICK_MS, ProgressTick, vt);
}

/* Allocated once and freed with the widget; a failed one draws in the current foreground. */
static void
AllocateStatusColor(Vt100Rec *vt, unsigned int slot, uint8_t red, uint8_t green, uint8_t blue)
{
        XColor color = {0};

        color.red = (unsigned short)(red * 257U);
        color.green = (unsigned short)(green * 257U);
        color.blue = (unsigned short)(blue * 257U);
        color.flags = DoRed | DoGreen | DoBlue;
        vt->vt.progress_status_allocated[slot] =
            XAllocColor(XtDisplay((Widget)vt), vt->core.colormap, &color) != 0;
        if (!vt->vt.progress_status_allocated[slot])
                return;
        vt->vt.progress_status_allocations[slot] = color.pixel;
        vt->vt.progress_status_pixels[slot] = VtOpaquePixel(vt, color.pixel);
}

/* A plain Athena child inside the terminal: no keyboard handling, so focus stays on the VT. */
static Boolean
EnsureProgressWidget(Vt100Rec *vt)
{
        if (vt->vt.progress_widget != NULL)
                return True;
        vt->vt.progress_widget = XtVaCreateWidget(
            "progress", simpleWidgetClass, (Widget)vt, XtNborderWidth, XTP_PROGRESS_BORDER,
            XtNborderColor, VtOpaquePixel(vt, vt->vt.effective_foreground), XtNbackground,
            vt->vt.effective_background_pixel, XtNwidth, XTP_PROGRESS_MIN_WIDTH, XtNheight,
            XTP_PROGRESS_HEIGHT, NULL);
        if (vt->vt.progress_widget == NULL)
                return False;
        XtAddEventHandler(vt->vt.progress_widget, ExposureMask, False, ProgressExpose, vt);
        AllocateStatusColor(vt, 0, 0xe0, 0x1b, 0x24);
        AllocateStatusColor(vt, 1, 0xf5, 0xc2, 0x11);
        return True;
}

/* The indicator sits in the top-right corner, left of a right-hand scrollbar. */
void
VtLayoutProgress(Vt100Rec *vt)
{
        unsigned int width;
        int right;
        int x;

        if (vt->vt.progress_widget == NULL || vt->vt.progress_state == XTP_PROGRESS_REMOVE)
                return;
        width = vt->core.width / 5U;
        if (width < XTP_PROGRESS_MIN_WIDTH)
                width = XTP_PROGRESS_MIN_WIDTH;
        if (width > XTP_PROGRESS_MAX_WIDTH)
                width = XTP_PROGRESS_MAX_WIDTH;
        right = (int)vt->core.width - (int)vt->vt.internal_border;
        if (vt->vt.scroll_bar && vt->vt.right_scroll_bar)
                right -= (int)VtScrollbarTotalWidth(vt);
        x = right - (int)width - 2 * (int)XTP_PROGRESS_BORDER;
        if (x < 0)
                x = 0;
        XtConfigureWidget(vt->vt.progress_widget, (Position)x, (Position)vt->vt.internal_border,
                          (Dimension)width, (Dimension)XTP_PROGRESS_HEIGHT,
                          (Dimension)XTP_PROGRESS_BORDER);
        XtpLog(XTP_LOG_DEBUG, "progress", "placed x=%d y=%u width=%u height=%u", x,
               (unsigned int)vt->vt.internal_border, width, XTP_PROGRESS_HEIGHT);
        DrawProgress(vt);
}

void
XtpVtSetProgress(Widget widget, XtpProgressState state, int percent)
{
        Vt100Rec *vt = VtAsRecord(widget);

        if (percent > 100)
                percent = 100;
        if (percent < -1)
                percent = -1;
        switch (state) {
        case XTP_PROGRESS_SET:
                vt->vt.progress_percent = percent < 0 ? 0 : percent;
                vt->vt.progress_have_percent = percent >= 0;
                break;
        case XTP_PROGRESS_ERROR:
        case XTP_PROGRESS_PAUSE:
                /* Without a value a reported percentage, even 0, stays; with none the bar is full.
                 */
                if (percent >= 0) {
                        vt->vt.progress_percent = percent;
                        vt->vt.progress_have_percent = True;
                } else if (!vt->vt.progress_have_percent) {
                        vt->vt.progress_percent = 100;
                }
                break;
        case XTP_PROGRESS_REMOVE:
                vt->vt.progress_have_percent = False;
                break;
        case XTP_PROGRESS_INDETERMINATE:
                break;
        }
        vt->vt.progress_state = state;
        if (state != XTP_PROGRESS_INDETERMINATE && vt->vt.progress_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.progress_timer);
                vt->vt.progress_timer = (XtIntervalId)0;
        }
        if (state == XTP_PROGRESS_REMOVE) {
                if (vt->vt.progress_widget != NULL && XtIsManaged(vt->vt.progress_widget))
                        XtUnmanageChild(vt->vt.progress_widget);
        } else if (EnsureProgressWidget(vt)) {
                VtLayoutProgress(vt);
                if (!XtIsManaged(vt->vt.progress_widget))
                        XtManageChild(vt->vt.progress_widget);
                if (state == XTP_PROGRESS_INDETERMINATE && vt->vt.progress_timer == (XtIntervalId)0)
                        vt->vt.progress_timer =
                            XtAppAddTimeOut(XtWidgetToApplicationContext(widget),
                                            XTP_PROGRESS_TICK_MS, ProgressTick, vt);
                DrawProgress(vt);
        }
        XtpLog(XTP_LOG_INFO, "progress", "state=%s percent=%d shown=%s", ProgressStateName(state),
               vt->vt.progress_percent, state != XTP_PROGRESS_REMOVE ? "true" : "false");
}

/* A separate window: effective colors, reverse video and opacity reach it only through here. */
void
VtProgressColorsChanged(Vt100Rec *vt)
{
        if (vt->vt.progress_widget == NULL)
                return;
        XtVaSetValues(vt->vt.progress_widget, XtNborderColor,
                      VtOpaquePixel(vt, vt->vt.effective_foreground), XtNbackground,
                      vt->vt.effective_background_pixel, NULL);
        DrawProgress(vt);
        XtpLog(XTP_LOG_DEBUG, "progress", "colors updated");
}

void
VtProgressDestroy(Vt100Rec *vt)
{
        unsigned int slot;

        for (slot = 0; slot < 2U; ++slot) {
                if (!vt->vt.progress_status_allocated[slot])
                        continue;
                XFreeColors(XtDisplay((Widget)vt), vt->core.colormap,
                            &vt->vt.progress_status_allocations[slot], 1, 0);
                vt->vt.progress_status_allocated[slot] = False;
        }
        if (vt->vt.progress_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.progress_timer);
                vt->vt.progress_timer = (XtIntervalId)0;
        }
        if (vt->vt.progress_gc != NULL) {
                XFreeGC(XtDisplay((Widget)vt), vt->vt.progress_gc);
                vt->vt.progress_gc = NULL;
        }
        vt->vt.progress_state = XTP_PROGRESS_REMOVE;
}
