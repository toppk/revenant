/* Verify the live X cursor, including application changes during hyperlink hover. */
#include "terminal.h"
#include "vt_widgetP.h"

#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
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

/* A deliberately distinctive theme, independent of installed desktop artwork. */
static void
CreateCursorFixture(void)
{
        const char *root = getenv("XTP_CURSOR_FIXTURE");
        char path[4096];
        XcursorImages *images;
        int index;

        if (root == NULL)
                return;
        snprintf(path, sizeof(path), "%s/test-theme", root);
        mkdir(path, 0700);
        snprintf(path, sizeof(path), "%s/test-theme/cursors", root);
        mkdir(path, 0700);
        images = XcursorImagesCreate(2);
        if (images == NULL)
                exit(1);
        images->nimage = 2;
        for (index = 0; index < 2; ++index) {
                int size = index == 0 ? 24 : 44;
                int pixel;
                static const XcursorPixel colors[] = {0xffff00ffU, 0xff00ffffU, 0xff00ff00U,
                                                      0x80800080U};

                images->images[index] = XcursorImageCreate(size, size);
                if (images->images[index] == NULL)
                        exit(1);
                images->images[index]->size = (XcursorDim)size;
                images->images[index]->xhot = 0;
                images->images[index]->yhot = 0;
                images->images[index]->delay = 0;
                for (pixel = 0; pixel < size * size; ++pixel)
                        images->images[index]->pixels[pixel] = colors[pixel % 4];
        }
        snprintf(path, sizeof(path), "%s/test-theme/cursors/text", root);
        if (!XcursorFilenameSaveImages(path, images)) {
                perror(path);
                exit(1);
        }
        XcursorImagesDestroy(images);
}

static void
CheckCursorPolicy(Harness *h, const char *expected, unsigned long foreground,
                  unsigned long background)
{
        XFixesCursorImage *image = XFixesGetCursorImage(XtDisplay(h->widget));
        size_t pixel;
        int opaque = 0;
        int saw_foreground = 0;
        int saw_background = 0;

        if (image == NULL) {
                Expect(h, 0, "cannot inspect cursor policy");
                return;
        }
        if (strcmp(expected, "core") != 0)
                Expect(h, image->width == atoi(expected) && image->height == atoi(expected),
                       "themed cursor size does not match resource/environment precedence");
        for (pixel = 0; pixel < (size_t)image->width * image->height; ++pixel) {
                unsigned long color = image->pixels[pixel] & 0xffffffUL;

                if ((image->pixels[pixel] & 0xff000000UL) == 0)
                        continue;
                ++opaque;
                if (strcmp(expected, "core") == 0) {
                        Expect(h, color == foreground || color == background,
                               "core cursor ignored pointer colors or used themed artwork");
                        saw_foreground |= color == foreground;
                        saw_background |= color == background;
                } else {
                        Expect(h,
                               color == 0xff00ffUL || color == 0x00ffffUL || color == 0x00ff00UL ||
                                   color == 0x800080UL,
                               "multicolor themed artwork was recolored");
                }
        }
        Expect(h, opaque != 0, "cursor is completely transparent");
        if (strcmp(expected, "core") == 0)
                Expect(h, saw_foreground && saw_background, "core cursor lost shape or mask color");
        XFree(image);
}

int
main(int argc, char **argv)
{
        XtAppContext context;
        Widget shell;
        const char *policy = getenv("XTP_EXPECT_CURSOR");

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

        CreateCursorFixture();
        shell = XtVaAppInitialize(&context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
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
        if (policy != NULL)
                CheckCursorPolicy(&h, policy, 0x0000ffUL, 0xffff00UL);
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
        Feed(&h, "\033]22;\033\\");
        Expect(&h, CursorImage(&h, &serial) == text_image,
               "empty OSC 22 did not restore the initial text pointer");
        {
                unsigned long arrow_image;
                String configured = NULL;

                XtVaSetValues(h.widget, "pointerShape", "arrow", NULL);
                XtVaGetValues(h.widget, "pointerShape", &configured, NULL);
                Expect(&h, configured != NULL && strcmp(configured, "arrow") == 0,
                       "pointerShape resource was not stored");
                XSync(XtDisplay(shell), False);
                arrow_image = CursorImage(&h, &serial);
                Expect(&h, arrow_image != text_image, "configured arrow did not replace text");
                Feed(&h, "\033]22;text\007");
                Expect(&h, CursorImage(&h, &serial) == text_image,
                       "explicit text did not override arrow");
                Feed(&h, "\033]22;\030\033]22;\032");
                Expect(&h, CursorImage(&h, &serial) == text_image,
                       "aborted empty OSC reset the pointer");
                Feed(&h, "\033]22;");
                Expect(&h, CursorImage(&h, &serial) == text_image,
                       "incomplete reset changed pointer");
                Feed(&h, "\033");
                Feed(&h, "\\");
                Expect(&h, CursorImage(&h, &serial) == arrow_image,
                       "split empty reset did not restore configured arrow");
                Feed(&h, "\033]22;invalid-name\007");
                Expect(&h, CursorImage(&h, &serial) == arrow_image,
                       "invalid name changed configured arrow");
                Feed(&h, "\033[H\033]8;;https://example.com\033\\LINK\033]8;;\033\\");
                Hover(&h, ShiftMask);
                Feed(&h, "\033]22;text\007\033]22;\007");
                Expect(&h, CursorImage(&h, &serial) == hand_image,
                       "configured reset replaced hover cursor");
                Hover(&h, 0);
                Expect(&h, CursorImage(&h, &serial) == arrow_image,
                       "leaving hover did not restore configured arrow");
                XtpVtSetTerminal(h.widget, NULL);
                Expect(&h, CursorImage(&h, &serial) == arrow_image,
                       "detaching did not restore configured arrow");
        }
        {
                unsigned long native_image;

                XtpVtSetTerminal(h.widget, h.terminal);
                XtVaSetValues(h.widget, "pointerShape", "boat", NULL);
                XSync(XtDisplay(shell), False);
                native_image = CursorImage(&h, &serial);
                Expect(&h, XtpTerminalPointerUsesDefault(h.terminal), "native default not tracked");
                Expect(&h, native_image != text_image, "native boat glyph not loaded");
                /* boat's backend fallback and explicit text share the same enum:
                 * acceptance metadata must still switch away from the native glyph. */
                Feed(&h, "\033]22;te");
                Feed(&h, "xt\007");
                Expect(&h, !XtpTerminalPointerUsesDefault(h.terminal),
                       "explicit text retained native default");
                Expect(&h, CursorImage(&h, &serial) == text_image,
                       "text did not override native boat");
                Feed(&h, "\033]22;");
                Feed(&h, "\007");
                Expect(&h, CursorImage(&h, &serial) == native_image,
                       "empty reset lost native boat");
                Feed(&h, "\033]22;invalid-name\007\033]22;text\030");
                Expect(&h, XtpTerminalPointerUsesDefault(h.terminal),
                       "invalid or aborted request lost default");
                Expect(&h, CursorImage(&h, &serial) == native_image,
                       "invalid request changed native glyph");
                XtVaSetValues(h.widget, "pointerShape", "arrow", NULL);
                XtpVtSetTerminal(h.widget, NULL);
        }
        if (policy != NULL) {
                XtpVtSetTerminal(h.widget, h.terminal);
                Feed(&h, "\033]22;text\007");
                XtVaSetValues(h.widget, "pointerColor", "red", "pointerFont", "cursor", NULL);
                XSync(XtDisplay(shell), False);
                CheckCursorPolicy(&h, policy, 0xff0000UL, 0xffff00UL);
                Feed(&h, "\033]22;\007");
                XtpVtSetTerminal(h.widget, NULL);
        }
        XtpVtSetTerminal(h.widget, NULL);
        Expect(&h, XtpTerminalPointerShape(h.terminal) == XTP_POINTER_SHAPE_DEFAULT,
               "configured reset was not reflected in core state");
        XtpTerminalFree(h.terminal);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(context);
        printf("pointer shape: %d unexpected observation(s)\n", h.failures);
        return h.failures == 0 ? 0 : 1;
}
