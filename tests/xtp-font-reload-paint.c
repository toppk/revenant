/* Painting a text emoji across a geometry reload, the explicit rescue and a rejected
 * reload, in one live widget.
 *
 * The stub-backed reload test covers routing, generation and cache behavior.  It
 * cannot cover painting: the stub backend accepts input, reports cursor column
 * zero and emits no cells, so nothing reaches the screen.  This variant links the
 * real terminal backend and asserts that before it tries to paint anything.
 */

#include "diagnostics.h"
#include "font_role.h"
#include "font_router.h"
#include "terminal.h"
#include "vt_widget.h"
#include "vt_widgetP.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Bare U+1F6E0: a one-cell text-presentation emoji atom that needs fitting. */
#define TOOLS "\xf0\x9f\x9b\xa0"
/* Unassigned in every staged face, so it paints the deterministic tofu box. */
#define PRIVATE_USE "\xee\x80\x80"

typedef struct
{
        unsigned int cursor_column;
        unsigned int cells;
} FrameProbe;

typedef struct
{
        unsigned long ink;
        unsigned long checksum;
} CellInk;

/* The face every phase must be served from, whether automatic discovery or the
 * explicit rescue found it. */
#define EMOJI_FILE "NotoEmoji-Regular-3.003.ttf"

static int
Fail(const char *message)
{
        fprintf(stderr, "font reload paint: %s\n", message);
        return 1;
}

static void
ProbeBegin(const XtpRenderFrame *frame, void *closure)
{
        FrameProbe *probe = closure;

        probe->cursor_column = frame->cursor_column;
}

static void
ProbeCell(const XtpRenderCell *cell, void *closure)
{
        FrameProbe *probe = closure;

        (void)cell;
        ++probe->cells;
}

static void
ProbeFrame(const XtpRenderFrame *frame, void *closure)
{
        (void)frame;
        (void)closure;
}

/* The cursor column the core committed, which is the advance measurement: it is a
 * backend fact and must not change when the serving font changes. */
static int
CursorColumn(XtpTerminal *terminal, unsigned int *column, unsigned int *cells)
{
        static const XtpRenderer probe_renderer = {
            .begin = ProbeBegin,
            .cell = ProbeCell,
            .end = ProbeFrame,
            .abort = ProbeFrame,
        };
        FrameProbe probe = {0};

        if (XtpTerminalRender(terminal, &probe_renderer, &probe, true) != 0)
                return -1;
        *column = probe.cursor_column;
        *cells = probe.cells;
        return 0;
}

/* Ink in one rectangle of the widget's window.  The widget is white on black here,
 * so any non-zero pixel is ink, and the checksum distinguishes one drawing from
 * another.  Containment is judged by reading the cells *around* the atom: a bound
 * computed inside the sampled rectangle could never be exceeded. */
static int
SampleRect(Widget vt, unsigned int left, unsigned int top, unsigned int width, unsigned int height,
           CellInk *out)
{
        Display *display = XtDisplay(vt);
        XImage *image;
        unsigned int x;
        unsigned int y;

        memset(out, 0, sizeof(*out));
        XSync(display, False);
        image = XGetImage(display, XtWindow(vt), (int)left, (int)top, width, height, AllPlanes,
                          ZPixmap);
        if (image == NULL)
                return -1;
        for (y = 0; y < height; ++y) {
                for (x = 0; x < width; ++x) {
                        unsigned long pixel = XGetPixel(image, (int)x, (int)y);

                        out->checksum = out->checksum * 1099511628211UL + pixel;
                        if (pixel != 0UL)
                                ++out->ink;
                }
        }
        XDestroyImage(image);
        return 0;
}

static int
SampleCell(Widget vt, unsigned int column, unsigned int row, CellInk *out)
{
        unsigned int width = XtpVtCellWidth(vt);
        unsigned int height = XtpVtCellHeight(vt);

        return SampleRect(vt, column * width, row * height, width, height, out);
}

/* Clear the screen, write TEXT, paint it, and let Xt finish the expose.
 *
 * SHOW_CURSOR selects what the observation is for: the cursor block inks the cell
 * after the atom, so containment is sampled with it hidden, while the core only
 * reports a cursor column for a visible cursor, so advance is read with it shown.
 */
static void
Paint(XtAppContext context, Widget vt, XtpTerminal *terminal, const char *text, bool show_cursor)
{
        char bytes[64];
        int length = snprintf(bytes, sizeof(bytes), "\033[2J\033[H\033[?25%c%s",
                              show_cursor ? 'h' : 'l', text);

        if (length <= 0)
                return;
        (void)XtpTerminalFeedOutput(terminal, (const uint8_t *)bytes, (size_t)length, false);
        /* The dirty-update path, which is what the application uses when output
         * arrives.  XtpVtRedraw repaints the cached frame, so it would redraw the
         * previous screen and every sample would look identical. */
        XtpVtUpdate(vt);
        XSync(XtDisplay(vt), False);
        while (XtAppPending(context) != 0)
                XtAppProcessEvent(context, XtIMAll);
        XSync(XtDisplay(vt), False);
}

/* What serves TEXT right now.  This is the route the paint below draws from, so
 * every phase can state the role and the effective file it expects rather than
 * settling for "something was drawn". */
static const char *
RouteOf(Vt100Rec *record, const char *text, const char **file)
{
        XtpGlyphRun run = {0};
        /* The router reports tofu by naming the role and returning no font, and it
         * leaves the caller's value alone for anything it cannot classify. */
        const char *role = "primary-missing";
        XftFont *font;

        *file = NULL;
        font =
            VtSelectXftFont(record, text, strlen(text), 1, False, False, &role, NULL, NULL, &run);
        if (font == NULL)
                return role;
        if (run.missing || run.count != 1U)
                return NULL;
        *file = XtpFontFileName(font);
        return role;
}

/* Paint the reference character and return its checksum, having first confirmed it
 * really is the tofu box: any nonblank drawing would otherwise pass for one.  It is
 * measured per phase because the cell geometry changes across a reload. */
static const char *
TofuReference(XtAppContext context, Widget vt, XtpTerminal *terminal, Vt100Rec *record,
              unsigned long *checksum)
{
        CellInk box;
        const char *file = NULL;
        const char *role = RouteOf(record, PRIVATE_USE, &file);

        if (role == NULL || strcmp(role, "tofu") != 0)
                return "the reference character is served by a font instead of the tofu box";
        Paint(context, vt, terminal, PRIVATE_USE, false);
        if (SampleCell(vt, 0, 0, &box) != 0 || box.ink == 0)
                return "the tofu reference did not paint";
        *checksum = box.checksum;
        return NULL;
}

/* One painted observation of the emoji atom: served by the expected role and file,
 * ink present, not the tofu box, nothing outside its cell, one column of advance. */
static const char *
CheckPaintedTools(XtAppContext context, Widget vt, XtpTerminal *terminal, Vt100Rec *record,
                  const char *expect_role, unsigned long tofu)
{
        CellInk atom;
        CellInk right;
        CellInk below;
        const char *file = NULL;
        const char *role = RouteOf(record, TOOLS, &file);
        unsigned int column = 0;
        unsigned int cells = 0;

        if (role == NULL || strcmp(role, expect_role) != 0)
                return "the painted atom is served by an unexpected role";
        if (file == NULL || strstr(file, EMOJI_FILE) == NULL)
                return "the painted atom is served by an unexpected effective file";
        Paint(context, vt, terminal, TOOLS, false);
        if (SampleCell(vt, 0, 0, &atom) != 0 || SampleCell(vt, 1, 0, &right) != 0 ||
            SampleRect(vt, 0, XtpVtCellHeight(vt), 2 * XtpVtCellWidth(vt), XtpVtCellHeight(vt),
                       &below) != 0)
                return "cannot sample the painted cells";
        if (atom.ink == 0)
                return "the painted atom left no ink";
        if (atom.checksum == tofu)
                return "the painted atom is pixel-identical to the tofu box";
        if (right.ink != 0)
                return "painted ink reached the cell beside the atom";
        if (below.ink != 0)
                return "painted ink reached the row below the atom";
        Paint(context, vt, terminal, TOOLS, true);
        if (CursorColumn(terminal, &column, &cells) != 0)
                return "cannot read the committed cursor position";
        if (column != 1U)
                return "the atom did not advance the cursor exactly one column";
        return NULL;
}

int
main(int argc, char **argv)
{
        XtAppContext context;
        Widget shell;
        Widget vt;
        XtpTerminal *terminal;
        Vt100Rec *record;
        CellInk ascii;
        const char *painted;
        unsigned long tofu = 0;
        unsigned int column = 0;
        unsigned int cells = 0;
        unsigned int span;
        uint32_t generation;

        XtpLogSetLevel(XTP_LOG_DEBUG);
        shell = XtVaAppInitialize(&context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
        vt = XtVaCreateManagedWidget(
            "vt100", vt100WidgetClass, shell, "renderFont", "true", "faceName",
            "DejaVu Sans Mono:rgba=none", "faceSize", "16.0", "faceNameDoublesize", "",
            "faceNameEmoji", "", "reportFontRouting", True,
            /* No border and known colors, so cell zero starts at the window origin.
             * internalBorder is a Dimension and the colors are Pixels, so these need
             * the converting form; a plain string would be stored as its pointer. */
            XtVaTypedArg, "internalBorder", XtRString, "0", 2, XtVaTypedArg, "background",
            XtRString, "#000000", 8, XtVaTypedArg, "foreground", XtRString, "#FFFFFF", 8, NULL);
        /* Without an explicit size the shell realizes at the widget's pre-font
         * natural size, which is enormous; a few cells are all this needs. */
        XtVaSetValues(shell, "width", 400, "height", 120, NULL);
        XtRealizeWidget(shell);
        if (!XtpVtUsingXft(vt) || !XtpVtXftAvailable(vt))
                return Fail("initial Xft universe unavailable");

        /* Backend identity first: with the stub linked, everything below would
         * report zero cells and column zero and prove nothing. */
        if (XtpTerminalBackendIsStub()) {
                fprintf(stderr, "font reload paint: backend is %s; painting needs the real one\n",
                        XtpTerminalBackend());
                return 1;
        }
        record = VtAsRecord(vt);
        terminal = XtpTerminalNewWithGraphemeWidth(
            (uint16_t)XtpVtColumns(vt), (uint16_t)XtpVtRows(vt), XtpVtCellWidth(vt),
            XtpVtCellHeight(vt), XtpVtGraphemeWidthUnicode(vt));
        if (terminal == NULL)
                return Fail("cannot create the terminal core");
        XtpVtSetTerminal(vt, terminal);

        /* Establish that painting works at all before asserting anything about
         * emoji: plain ASCII must produce ink, one column of advance, and cells. */
        Paint(context, vt, terminal, "A", true);
        if (SampleCell(vt, 0, 0, &ascii) != 0)
                return Fail("cannot sample the painted ASCII cell");
        if (ascii.ink == 0)
                return Fail("plain ASCII did not paint, so the harness cannot judge emoji");
        if (CursorColumn(terminal, &column, &cells) != 0 || column != 1U || cells == 0U)
                return Fail("the core did not report a painted ASCII cell and its advance");

        /* Phase 1: automatic monochrome fallback, fitted, painted. */
        fprintf(stderr, "PHASE paint-automatic\n");
        painted = TofuReference(context, vt, terminal, record, &tofu);
        if (painted == NULL)
                painted = CheckPaintedTools(context, vt, terminal, record, "fallback", tofu);
        if (painted != NULL)
                return Fail(painted);

        /* Phase 2: a successful reload that changes this slot's geometry.  The atom
         * must be painted again from a face fitted to the new cell, so the tofu
         * reference is measured again at that geometry. */
        span = XtpVtCellWidth(vt);
        generation = XtpVtFontGeneration(vt);
        XtVaSetValues(vt, "faceSize", "22.0", NULL);
        if (XtpVtFontGeneration(vt) != generation + 1U)
                return Fail("the successful reload did not advance the generation once");
        if (XtpVtCellWidth(vt) == span)
                return Fail("the successful reload did not change the cell width");
        if (XtpTerminalResize(terminal, (uint16_t)XtpVtColumns(vt), (uint16_t)XtpVtRows(vt),
                              XtpVtCellWidth(vt), XtpVtCellHeight(vt)) != 0)
                return Fail("cannot resize the core to the reloaded geometry");
        fprintf(stderr, "PHASE paint-after-reload\n");
        painted = TofuReference(context, vt, terminal, record, &tofu);
        if (painted == NULL)
                painted = CheckPaintedTools(context, vt, terminal, record, "fallback", tofu);
        if (painted != NULL)
                return Fail(painted);

        /* Phase 3: the explicit rescue takes over.  The serving role changes while
         * the file does not, so only the role distinguishes this from the automatic
         * discovery above -- and it has to keep painting. */
        generation = XtpVtFontGeneration(vt);
        XtVaSetValues(vt, "faceNameEmojiText", "Noto Emoji:color=false", NULL);
        if (XtpVtFontGeneration(vt) != generation + 1U)
                return Fail("the rescue reload did not advance the generation once");
        if (XtpTerminalResize(terminal, (uint16_t)XtpVtColumns(vt), (uint16_t)XtpVtRows(vt),
                              XtpVtCellWidth(vt), XtpVtCellHeight(vt)) != 0)
                return Fail("cannot resize the core to the rescued geometry");
        fprintf(stderr, "PHASE paint-after-rescue\n");
        painted = TofuReference(context, vt, terminal, record, &tofu);
        if (painted == NULL)
                painted = CheckPaintedTools(context, vt, terminal, record, "emoji-text", tofu);
        if (painted != NULL)
                return Fail(painted);

        /* Phase 4: a rejected reload.  The retained universe must still paint the
         * atom, from the rescue role it was holding, with the same advance. */
        generation = XtpVtFontGeneration(vt);
        XtVaSetValues(vt, "faceName", "", "systemFallback", False, NULL);
        if (XtpVtFontGeneration(vt) != generation)
                return Fail("the rejected reload advanced the effective generation");
        fprintf(stderr, "PHASE paint-after-rollback\n");
        painted = CheckPaintedTools(context, vt, terminal, record, "emoji-text", tofu);
        if (painted != NULL)
                return Fail(painted);

        XtCallActionProc(vt, "report-font-routing", NULL, NULL, 0);
        XtpVtSetTerminal(vt, NULL);
        XtpTerminalFree(terminal);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(context);
        return 0;
}
