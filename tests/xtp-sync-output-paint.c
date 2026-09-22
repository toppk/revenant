/* Painted-frame evidence for DEC mode 2026 transitions inside one parser batch.
 *
 * A PTY write can arrive as several reads, and several writes can arrive as one, so
 * a terminal-level test cannot promise where a parser batch ends.  This helper feeds
 * the real backend directly: each Feed() below is exactly one parser batch, and a
 * frame is painted only where Paint() is called.  What the window then shows is read
 * back from the X server, so the assertions are about painted pixels, not logs.
 */

#include "diagnostics.h"
#include "terminal.h"
#include "vt_widget.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLACK 0x000000UL
#define WHITE 0xffffffUL
#define BLUE 0x0000ffUL
#define RED 0xff0000UL
#define CURSOR 0x00ff00UL

typedef struct
{
        XtAppContext context;
        Widget vt;
        XtpTerminal *terminal;
        const char *scenario;
        int failures;
} Harness;

static void
Feed(Harness *h, const char *bytes)
{
        (void)XtpTerminalFeedOutput(h->terminal, (const uint8_t *)bytes, strlen(bytes), false);
}

/* The application's own dirty-update path, then let Xt finish any expose. */
static void
Paint(Harness *h)
{
        XtpVtUpdate(h->vt);
        XSync(XtDisplay(h->vt), False);
        while (XtAppPending(h->context) != 0)
                XtAppProcessEvent(h->context, XtIMAll);
        XSync(XtDisplay(h->vt), False);
}

/* Whether the cell at ROW, COLUMN (zero-based) carries any ink. */
static bool
Ink(Harness *h, unsigned int row, unsigned int column)
{
        Display *display = XtDisplay(h->vt);
        unsigned int width = XtpVtCellWidth(h->vt);
        unsigned int height = XtpVtCellHeight(h->vt);
        XImage *image;
        unsigned int x;
        unsigned int y;
        bool ink = false;

        XSync(display, False);
        image = XGetImage(display, XtWindow(h->vt), (int)(column * width), (int)(row * height),
                          width, height, AllPlanes, ZPixmap);
        if (image == NULL)
                return false;
        for (y = 0; y < height && !ink; ++y)
                for (x = 0; x < width && !ink; ++x)
                        ink = XGetPixel(image, (int)x, (int)y) != 0UL;
        XDestroyImage(image);
        return ink;
}

static unsigned long
SamplePixel(Harness *h, int x, int y)
{
        XImage *image =
            XGetImage(XtDisplay(h->vt), XtWindow(h->vt), x, y, 1, 1, AllPlanes, ZPixmap);
        unsigned long pixel;

        if (image == NULL)
                return ~0UL;
        pixel = XGetPixel(image, 0, 0);
        XDestroyImage(image);
        return pixel;
}

/* The background actually painted, read from the middle of a cell nothing draws in. */
static unsigned long
Background(Harness *h)
{
        XSync(XtDisplay(h->vt), False);
        return SamplePixel(h, (int)(20U * XtpVtCellWidth(h->vt) + XtpVtCellWidth(h->vt) / 2U),
                           (int)(6U * XtpVtCellHeight(h->vt) + XtpVtCellHeight(h->vt) / 2U));
}

/* The first pixel of a cell that differs from BACKGROUND: the glyph's color. */
static unsigned long
InkColor(Harness *h, unsigned int row, unsigned int column, unsigned long background)
{
        unsigned int width = XtpVtCellWidth(h->vt);
        unsigned int height = XtpVtCellHeight(h->vt);
        unsigned long found = background;
        XImage *image;
        unsigned int x;
        unsigned int y;

        XSync(XtDisplay(h->vt), False);
        image = XGetImage(XtDisplay(h->vt), XtWindow(h->vt), (int)(column * width),
                          (int)(row * height), width, height, AllPlanes, ZPixmap);
        if (image == NULL)
                return found;
        for (y = 0; y < height && found == background; ++y)
                for (x = 0; x < width && found == background; ++x)
                        found = XGetPixel(image, (int)x, (int)y);
        XDestroyImage(image);
        return found;
}

/* Rows of a cell, top to bottom, carrying COLOR: a cursor's footprint in that cell. */
static void
Footprint(Harness *h, unsigned int row, unsigned int column, unsigned long color, bool *any,
          bool *top)
{
        unsigned int width = XtpVtCellWidth(h->vt);
        unsigned int height = XtpVtCellHeight(h->vt);
        XImage *image;
        unsigned int x;
        unsigned int y;

        *any = false;
        *top = false;
        XSync(XtDisplay(h->vt), False);
        image = XGetImage(XtDisplay(h->vt), XtWindow(h->vt), (int)(column * width),
                          (int)(row * height), width, height, AllPlanes, ZPixmap);
        if (image == NULL)
                return;
        for (y = 0; y < height; ++y)
                for (x = 0; x < width; ++x)
                        if (XGetPixel(image, (int)x, (int)y) == color) {
                                *any = true;
                                if (y < height / 4U)
                                        *top = true;
                        }
        XDestroyImage(image);
}

static void
ExpectPixel(Harness *h, const char *moment, const char *what, unsigned long actual,
            unsigned long expected, const char *meaning)
{
        printf("%s %s %s=#%06lx expected=#%06lx (%s)\n", h->scenario, moment, what, actual,
               expected, meaning);
        if (actual != expected)
                ++h->failures;
}

static void
ExpectCursor(Harness *h, const char *moment, unsigned int row, unsigned int column, bool present,
             const char *meaning)
{
        bool any;
        bool top;

        Footprint(h, row, column, CURSOR, &any, &top);
        printf("%s %s cursor@%u,%u=%s expected=%s (%s)\n", h->scenario, moment, row + 1U,
               column + 1U, any ? "drawn" : "absent", present ? "drawn" : "absent", meaning);
        if (any != present)
                ++h->failures;
}

static void
ExpectCursorShape(Harness *h, const char *moment, unsigned int row, unsigned int column,
                  bool top_edge, const char *meaning)
{
        bool any;
        bool top;

        Footprint(h, row, column, CURSOR, &any, &top);
        printf("%s %s cursor-top-edge=%s expected=%s (%s)\n", h->scenario, moment,
               any ? (top ? "yes" : "no") : "no-cursor", top_edge ? "yes" : "no", meaning);
        if (!any || top != top_edge)
                ++h->failures;
}

/* One observation: the row's marker is painted (VISIBLE) or not. */
static void
Expect(Harness *h, const char *moment, unsigned int row, bool visible, const char *meaning)
{
        bool ink = Ink(h, row, 0);

        printf("%s %s row%u=%s expected=%s (%s)\n", h->scenario, moment, row + 1U,
               ink ? "ink" : "blank", visible ? "ink" : "blank", meaning);
        if (ink != visible)
                ++h->failures;
}

/* Visible output, then a hold, in the same parser batch. */
static void
SameWriteOutputThenHold(Harness *h)
{
        h->scenario = "same-write-output-then-hold";
        Feed(h, "\033[?2026l\033[2J\033[H");
        Paint(h);
        Feed(h, "\033[1;1H#\033[?2026h\033[2;1H#");
        Paint(h);
        Expect(h, "held", 0, true, "output completed before the hold is the frame to show");
        Expect(h, "held", 1, false, "output after the hold stays hidden");
        Feed(h, "\033[?2026l");
        Paint(h);
        Expect(h, "released", 0, true, "completed frame kept");
        Expect(h, "released", 1, true, "release shows the held update");
}

/* Establish a held frame showing row 1 and hiding row 3. */
static void
HeldBaseline(Harness *h)
{
        Feed(h, "\033[?2026l\033[2J\033[H\033[1;1H#");
        Paint(h);
        Feed(h, "\033[?2026h\033[2J\033[3;1H#");
        Paint(h);
}

/* Release, complete a frame, and hold again -- all in one parser batch. */
static void
ReleaseFrameRehold(Harness *h, bool one_batch)
{
        static const char *const pieces[] = {
            "\033[?2026l",
            "\033[2J\033[5;1H#",
            "\033[?2026h\033[6;1H#",
        };

        h->scenario = one_batch ? "same-write-release-frame-rehold"
                                : "consecutive-batches-release-frame-rehold";
        HeldBaseline(h);
        Expect(h, "baseline", 0, true, "the frame captured when the first hold began");
        Expect(h, "baseline", 2, false, "the first hold hides its half-drawn row");
        if (one_batch) {
                Feed(h, "\033[?2026l\033[2J\033[5;1H#\033[?2026h\033[6;1H#");
        } else {
                /* Three parser batches, and no paint between them. */
                Feed(h, pieces[0]);
                Feed(h, pieces[1]);
                Feed(h, pieces[2]);
        }
        Paint(h);
        Expect(h, "held", 4, true, "the frame completed between the holds becomes visible");
        Expect(h, "held", 0, false, "the previous frame is replaced");
        Expect(h, "held", 5, false, "output after the second hold stays hidden");
        Feed(h, "\033[?2026l");
        Paint(h);
        Expect(h, "released", 4, true, "completed frame kept");
        Expect(h, "released", 5, true, "release shows the held update");
}

/* A completed frame, then a hold, then reverse video -- one batch. */
static void
HeldReverseVideo(Harness *h)
{
        h->scenario = "same-write-hold-then-reverse-video";
        Feed(h, "\033[?2026l\033[?5l\033[2J\033[H#");
        Paint(h);
        Feed(h, "\033[2;1H#\033[?2026h\033[?5h\033[3;1H#");
        Paint(h);
        Expect(h, "held", 1, true, "the frame completed before the hold is shown");
        ExpectPixel(h, "held", "background", Background(h), BLACK,
                    "reverse video parsed after the hold stays hidden");
        Feed(h, "\033[?2026l");
        Paint(h);
        ExpectPixel(h, "released", "background", Background(h), WHITE,
                    "release applies reverse video");
        Feed(h, "\033[?5l");
        Paint(h);
}

/* A completed frame, then a hold, then OSC 10/11 color changes -- one batch. */
static void
HeldColorChanges(Harness *h)
{
        h->scenario = "same-write-hold-then-osc-colors";
        Feed(h, "\033[?2026l\033[2J\033[H#");
        Paint(h);
        Feed(h, "\033[2;1H#\033[?2026h\033]11;#0000ff\007\033]10;#ff0000\007\033[3;1H#");
        Paint(h);
        ExpectPixel(h, "held", "background", Background(h), BLACK,
                    "OSC 11 parsed after the hold stays hidden");
        ExpectPixel(h, "held", "row2-glyph", InkColor(h, 1, 0, BLACK), WHITE,
                    "OSC 10 parsed after the hold stays hidden");
        Feed(h, "\033[?2026l");
        Paint(h);
        ExpectPixel(h, "released", "background", Background(h), BLUE, "release applies OSC 11");
        ExpectPixel(h, "released", "row2-glyph", InkColor(h, 1, 0, BLUE), RED,
                    "release applies OSC 10");
        Feed(h, "\033]110\007\033]111\007");
        Paint(h);
}

/* Paint at A, move to B, hold, move to C: the held frame shows B. */
static void
HeldCursorMove(Harness *h)
{
        h->scenario = "cursor-move-then-hold";
        Feed(h, "\033[?2026l\033[2J\033[?25h\033[2 q\033[1;1H");
        Paint(h);
        ExpectCursor(h, "before", 0, 0, true, "cursor painted at A");
        Feed(h, "\033[3;5H\033[?2026h\033[5;9H");
        Paint(h);
        ExpectCursor(h, "held", 2, 4, true, "the cursor completed before the hold (B) is shown");
        ExpectCursor(h, "held", 0, 0, false, "the previously painted cursor (A) is gone");
        ExpectCursor(h, "held", 4, 8, false, "the cursor moved after the hold (C) is hidden");
        Feed(h, "\033[?2026l");
        Paint(h);
        ExpectCursor(h, "released", 4, 8, true, "release shows C");
        ExpectCursor(h, "released", 2, 4, false, "B is gone after release");
}

/* Hide the cursor, hold, show it again: the held frame shows it hidden. */
static void
HeldCursorVisibility(Harness *h)
{
        h->scenario = "cursor-hide-then-hold";
        Feed(h, "\033[?2026l\033[2J\033[?25h\033[1;1H");
        Paint(h);
        ExpectCursor(h, "before", 0, 0, true, "cursor painted");
        Feed(h, "\033[?25l\033[?2026h\033[?25h");
        Paint(h);
        ExpectCursor(h, "held", 0, 0, false, "hidden before the hold, so hidden while held");
        Feed(h, "\033[?2026l");
        Paint(h);
        ExpectCursor(h, "released", 0, 0, true, "release shows it again");
}

/* Change the cursor style, hold, change it back: the held frame shows the first change. */
static void
HeldCursorStyle(Harness *h)
{
        h->scenario = "cursor-style-then-hold";
        Feed(h, "\033[?2026l\033[2J\033[?25h\033[2 q\033[1;1H");
        Paint(h);
        ExpectCursorShape(h, "before", 0, 0, true, "steady block has a top edge");
        Feed(h, "\033[4 q\033[?2026h\033[2 q");
        Paint(h);
        ExpectCursorShape(h, "held", 0, 0, false, "the underline set before the hold is shown");
        Feed(h, "\033[?2026l");
        Paint(h);
        ExpectCursorShape(h, "released", 0, 0, true, "release shows the block again");
}

int
main(int argc, char **argv)
{
        Harness h = {0};
        Widget shell;

        XtpLogSetLevel(XTP_LOG_DEBUG);
        shell = XtVaAppInitialize(&h.context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
        /* Core font, no border, white on black: a marker cell is ink and nothing else is. */
        h.vt = XtVaCreateManagedWidget(
            "vt100", vt100WidgetClass, shell, "renderFont", "false", XtVaTypedArg, "font",
            XtRString, "fixed", 6, XtVaTypedArg, "internalBorder", XtRString, "0", 2, XtVaTypedArg,
            "background", XtRString, "#000000", 8, XtVaTypedArg, "foreground", XtRString, "#FFFFFF",
            8, XtVaTypedArg, "cursorColor", XtRString, "#000000", 8, NULL);
        XtVaSetValues(shell, "width", 240, "height", 160, NULL);
        XtRealizeWidget(shell);
        if (XtpTerminalBackendIsStub()) {
                fprintf(stderr, "sync output paint: backend is %s; this needs the real one\n",
                        XtpTerminalBackend());
                return 1;
        }
        h.terminal =
            XtpTerminalNewWithGraphemeWidth((uint16_t)XtpVtColumns(h.vt), (uint16_t)XtpVtRows(h.vt),
                                            XtpVtCellWidth(h.vt), XtpVtCellHeight(h.vt), false);
        if (h.terminal == NULL) {
                fprintf(stderr, "sync output paint: cannot create the terminal core\n");
                return 1;
        }
        XtpVtSetTerminal(h.vt, h.terminal);
        /* The cursor would ink the cell after each marker; hide it for the whole run. */
        Feed(&h, "\033[?25l");

        SameWriteOutputThenHold(&h);
        ReleaseFrameRehold(&h, true);
        ReleaseFrameRehold(&h, false);
        HeldReverseVideo(&h);
        HeldColorChanges(&h);
        /* The cursor cases need a cursor color distinct from everything else. */
        Feed(&h, "\033]12;#00ff00\007");
        HeldCursorMove(&h);
        HeldCursorVisibility(&h);
        HeldCursorStyle(&h);
        Feed(&h, "\033[0 q\033]112\007\033[?25l");

        Feed(&h, "\033[?2026l");
        XtpVtSetTerminal(h.vt, NULL);
        XtpTerminalFree(h.terminal);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(h.context);
        printf("sync output paint: %d unexpected observation(s)\n", h.failures);
        return h.failures == 0 ? 0 : 1;
}
