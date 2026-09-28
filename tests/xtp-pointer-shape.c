/* Verify the live X cursor, including application changes during hyperlink hover. */
#include "terminal.h"
#include "vt_widgetP.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/extensions/Xfixes.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
        Widget widget;
        XtpTerminal *terminal;
        int failures;
} Harness;

static void
Feed(Harness *h, const char *bytes)
{
        XtpTerminalFeed(h->terminal, (const uint8_t *)bytes, strlen(bytes));
        XtpVtUpdate(h->widget);
        XSync(XtDisplay(h->widget), False);
}

static unsigned long
CursorImage(Harness *h, unsigned long *serial)
{
        XFixesCursorImage *image = XFixesGetCursorImage(XtDisplay(h->widget));
        unsigned long hash = 5381;
        size_t index;

        if (image == NULL) {
                ++h->failures;
                return 0;
        }
        hash = hash * 33 + image->width;
        hash = hash * 33 + image->height;
        hash = hash * 33 + image->xhot;
        hash = hash * 33 + image->yhot;
        for (index = 0; index < (size_t)image->width * image->height; ++index)
                hash = hash * 33 + image->pixels[index];
        *serial = image->cursor_serial;
        XFree(image);
        return hash;
}

static void
Expect(Harness *h, int accepted, const char *description)
{
        if (!accepted) {
                fprintf(stderr, "pointer shape: %s\n", description);
                ++h->failures;
        }
}

static void
Hover(Harness *h, unsigned int state)
{
        XEvent event = {0};
        Boolean dispatch = True;

        event.xmotion.type = MotionNotify;
        event.xmotion.x = VtTerminalX(VtAsRecord(h->widget)) + (int)XtpVtCellWidth(h->widget) / 2;
        event.xmotion.y =
            VtAsRecord(h->widget)->vt.internal_border + (int)XtpVtCellHeight(h->widget) / 2;
        event.xmotion.state = state;
        VtHyperlinkEvent(h->widget, VtAsRecord(h->widget), &event, &dispatch);
        XSync(XtDisplay(h->widget), False);
}

int
main(int argc, char **argv)
{
        XtAppContext context;
        Widget shell = XtVaAppInitialize(&context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
        Harness h = {0};
        unsigned long serial = 0;
        unsigned long previous_serial;
        unsigned long text_image;
        unsigned long text_serial;
        unsigned long crosshair_image;
        unsigned long crosshair_serial;
        unsigned long hand_image;
        unsigned long hand_serial;
        int event_base;
        int error_base;

        h.widget =
            XtVaCreateManagedWidget("vt100", vt100WidgetClass, shell, "renderFont", "false", NULL);
        XtRealizeWidget(shell);
        if (!XFixesQueryExtension(XtDisplay(shell), &event_base, &error_base))
                return 77;
        h.terminal = XtpTerminalNewWithGraphemeWidth(80, 24, XtpVtCellWidth(h.widget),
                                                     XtpVtCellHeight(h.widget), false);
        if (h.terminal == NULL)
                return 1;
        XtpVtSetTerminal(h.widget, h.terminal);
        XWarpPointer(XtDisplay(shell), None, XtWindow(h.widget), 0, 0, 0, 0, 100, 100);
        XSync(XtDisplay(shell), False);
        text_image = CursorImage(&h, &serial);
        text_serial = serial;
        Expect(&h, text_image != 0, "initial text cursor missing");

        Feed(&h, "\033]22;crosshair\007");
        crosshair_image = CursorImage(&h, &serial);
        Expect(&h, crosshair_image != text_image, "crosshair did not change the X cursor");
        crosshair_serial = serial;
        previous_serial = serial;
        Feed(&h, "\033]22;invalid-name\007\033]22;wait");
        Expect(&h, CursorImage(&h, &serial) == crosshair_image && serial == previous_serial,
               "invalid or incomplete OSC changed the cursor");
        Feed(&h, "\007");
        (void)CursorImage(&h, &serial);
        Expect(&h, serial != previous_serial, "split OSC was not applied at termination");

        Feed(&h, "\033]8;;https://example.com\033\\LINK\033]8;;\033\\");
        Hover(&h, ShiftMask);
        Expect(&h, VtAsRecord(h.widget)->vt.hyperlink_cursor_active, "hover did not activate");
        hand_image = CursorImage(&h, &serial);
        hand_serial = serial;
        previous_serial = serial;
        Feed(&h, "\033]22;crosshair\007");
        Expect(&h, CursorImage(&h, &serial) == hand_image && serial == previous_serial,
               "application request replaced the hover cursor");
        Hover(&h, 0);
        Expect(&h, CursorImage(&h, &serial) == crosshair_image && serial == crosshair_serial,
               "leaving hover did not restore the cached latest application cursor");
        Hover(&h, ShiftMask);
        Expect(&h, CursorImage(&h, &serial) == hand_image && serial == hand_serial,
               "reentering hover did not reuse the cached hand cursor");
        {
                XEvent event = {0};
                Boolean dispatch = True;

                event.type = LeaveNotify;
                VtHyperlinkEvent(h.widget, VtAsRecord(h.widget), &event, &dispatch);
        }
        Expect(&h, CursorImage(&h, &serial) == crosshair_image,
               "LeaveNotify did not restore the application cursor");

        Feed(&h, "\033[?2026h\033]22;text\007");
        Expect(&h, CursorImage(&h, &serial) == text_image && serial == text_serial,
               "synchronized output incorrectly deferred the pointer");
        Feed(&h, "\033[?2026l\033]22;default\007");
        Expect(&h, CursorImage(&h, &serial) != text_image,
               "explicit default did not select an arrow");
        Feed(&h, "\033]22;crosshair\033\\");
        XtVaSetValues(shell, "width", 500, "height", 300, NULL);
        XtpVtUpdate(h.widget);
        Expect(&h, CursorImage(&h, &serial) == crosshair_image,
               "resize changed the application cursor");
        /* Backend resets preserve the accepted shape at this pin. */
        Feed(&h, "\033[!p\033c");
        Expect(&h, XtpTerminalPointerShape(h.terminal) == XTP_POINTER_SHAPE_CROSSHAIR,
               "backend reset semantics changed");
        XtpVtSetTerminal(h.widget, NULL);
        Expect(&h, CursorImage(&h, &serial) == text_image,
               "detaching the terminal did not restore text");
        XtpTerminalFree(h.terminal);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(context);
        printf("pointer shape: %d unexpected observation(s)\n", h.failures);
        return h.failures == 0 ? 0 : 1;
}
