#include "diagnostics.h"
#include "font_role.h"
#include "font_router.h"
#include "vt_widgetP.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
Fail(const char *message)
{
        fprintf(stderr, "font reload test: %s\n", message);
        return 1;
}

/* U+1F6E0 with no selector: a one-cell text-presentation emoji atom, which the
 * advance rule refuses until the span fitting policy shrinks a face for it. */
static const char tools[] = "\xf0\x9f\x9b\xa0";

/* Phase markers, so the driver can attribute each route-cache decision in the log
 * to the transition that caused it rather than to the run as a whole. */
static void
Phase(const char *name)
{
        fprintf(stderr, "PHASE %s\n", name);
        fflush(stderr);
}

/* The fitted instance the universe is currently holding for SPAN, or NULL.  The
 * table is the observable record of what fitting produced; a returned pointer on
 * its own would not say which span it was built for, or that it is still owned. */
static XftFont *
FittedForSpan(Vt100Rec *record, unsigned int span)
{
        XtpFontUniverse *universe = record->vt.font_universe;
        size_t index;

        for (index = 0; index < universe->fitted_face_count; ++index) {
                if (universe->fitted_faces[index].span == span)
                        return (XftFont *)universe->fitted_faces[index].fitted;
        }
        return NULL;
}

/* Route the atom and report what served it.  ROLE and FILE identify the choice;
 * a non-tofu role with a shaped glyph says the route resolved and the cluster
 * shaped.  It is not evidence of visible ink; nothing here paints. */
static XftFont *
RouteTools(Vt100Rec *record, const char **role, const char **file)
{
        XtpGlyphRun run = {0};
        XftFont *font;

        *role = NULL;
        *file = NULL;
        font = VtSelectXftFont(record, tools, sizeof(tools) - 1, 1, False, False, role, NULL, NULL,
                               &run);
        if (font == NULL || run.missing || run.count != 1U)
                return NULL;
        *file = XtpFontFileName(font);
        return font;
}

int
main(int argc, char **argv)
{
        XtAppContext context;
        Widget shell;
        Widget vt;
        uint32_t initial_generation;
        uint32_t reloaded_generation;
        unsigned int initial_height;
        unsigned int reloaded_height;
        String configured = NULL;
        Boolean configured_system_fallback = True;
        Vt100Rec *record;
        XtpGlyphRun run = {0};
        XftFont *emoji_font;
        XftFont *emoji_again;
        const char *emoji_role;
        const char *emoji_file;
        char automatic_file[1024];
        unsigned int emoji_span;
        unsigned int refitted_span;
        uint32_t emoji_generation;
        size_t cached_routes;

        /* The route-cache decisions this test asserts are logged at debug level. */
        XtpLogSetLevel(XTP_LOG_DEBUG);
        shell = XtVaAppInitialize(&context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
        vt = XtVaCreateManagedWidget("vt100", vt100WidgetClass, shell, "renderFont", "true",
                                     "faceName", "DejaVu Sans Mono:rgba=none", "faceSize", "16.0",
                                     "faceSize1", "16.0", "faceNameDoublesize", "", "fallbackFace1",
                                     "Noto Sans Mono CJK JP", "reportFontRouting", True, NULL);
        XtRealizeWidget(shell);
        if (!XtpVtUsingXft(vt) || !XtpVtXftAvailable(vt))
                return Fail("initial Xft universe unavailable");
        record = VtAsRecord(vt);
        if (record->vt.font_universe->system_sort_count != 0)
                return Fail("startup performed an eager system fallback sort");
        if (VtSelectXftFont(record, "\xe6\x97\xa5", 3, 2, False, False, NULL, NULL, NULL, &run) ==
            NULL)
                return Fail("named fallback did not serve the staging probe");
        if (record->vt.font_universe->system_sort_count != 0)
                return Fail("named fallback unnecessarily sorted system candidates");
        if (!XtpVtSelectFont(vt, 1))
                return Fail("nonzero Xft slot unavailable before reload");
        initial_generation = XtpVtFontGeneration(vt);
        initial_height = XtpVtCellHeight(vt);

        XtVaSetValues(vt, "faceSize1", "12.0", NULL);
        reloaded_generation = XtpVtFontGeneration(vt);
        reloaded_height = XtpVtCellHeight(vt);
        if (reloaded_generation != initial_generation + 1U)
                return Fail("successful SetValues did not advance generation exactly once");
        if (!XtpVtUsingXft(vt) || reloaded_height >= initial_height)
                return Fail("successful SetValues did not install the smaller Xft universe");
        XtCallActionProc(vt, "report-font-routing", NULL, NULL, 0);

        /* Transition 1: a text emoji that needs fitting, served by automatic
         * monochrome discovery, and served again from the same instance. */
        emoji_span = XtpVtCellWidth(vt);
        cached_routes = XtpFontRouteCacheCount(record->vt.font_universe->route_cache);
        Phase("first-route");
        emoji_font = RouteTools(record, &emoji_role, &emoji_file);
        if (emoji_font == NULL || emoji_role == NULL || strcmp(emoji_role, "fallback") != 0)
                return Fail("automatic discovery did not serve the text emoji atom");
        if (emoji_file == NULL || strstr(emoji_file, "NotoEmoji-Regular-3.003.ttf") == NULL)
                return Fail("automatic discovery chose an unexpected effective file");
        if (FittedForSpan(record, emoji_span) != emoji_font)
                return Fail("the serving font is not the fitted instance for this span");
        snprintf(automatic_file, sizeof(automatic_file), "%s", emoji_file);
        /* Two different reuses, asserted separately, because the same font pointer
         * would come back either way.  The route cache gains exactly one entry for
         * this atom and none when it is routed again; the fitted table keeps exactly
         * one instance.  A cache that re-stored the same key would also leave the
         * count alone, so this establishes that no second entry appeared, not the
         * lookup itself. */
        if (XtpFontRouteCacheCount(record->vt.font_universe->route_cache) != cached_routes + 1U)
                return Fail("routing the atom did not add a route cache entry");
        cached_routes = XtpFontRouteCacheCount(record->vt.font_universe->route_cache);
        Phase("repeat-route");
        emoji_again = RouteTools(record, &emoji_role, &emoji_file);
        if (XtpFontRouteCacheCount(record->vt.font_universe->route_cache) != cached_routes)
                return Fail("repeating the atom stored another route cache entry");
        if (emoji_again != emoji_font || emoji_file == NULL ||
            strcmp(emoji_file, automatic_file) != 0)
                return Fail("repeating the atom did not reuse the fitted instance");
        if (record->vt.font_universe->fitted_face_count != 1U)
                return Fail("repeating the atom fitted the face again");

        /* Transition 2a: a successful reload that changes slot 0 geometry.  The
         * span changes, so a stale fitted instance would be visible as an entry
         * for the old span or a font built for it. */
        emoji_generation = XtpVtFontGeneration(vt);
        /* Slot 1 is the active Xft slot here, so its size is the geometry this
         * atom is routed against. */
        XtVaSetValues(vt, "faceSize1", "20.0", NULL);
        if (XtpVtFontGeneration(vt) != emoji_generation + 1U)
                return Fail("the geometry reload did not advance the generation once");
        refitted_span = XtpVtCellWidth(vt);
        if (refitted_span == emoji_span)
                return Fail("the geometry reload did not change the cell width");
        if (record->vt.font_universe->fitted_face_count != 0U)
                return Fail("the replaced universe kept fitted faces from the previous one");
        if (XtpFontRouteCacheCount(record->vt.font_universe->route_cache) != 0U)
                return Fail("the replaced universe kept route cache entries");
        Phase("after-geometry-reload");
        emoji_again = RouteTools(record, &emoji_role, &emoji_file);
        if (emoji_again == NULL || emoji_role == NULL || strcmp(emoji_role, "tofu") == 0)
                return Fail("the reloaded universe did not serve the text emoji atom");
        if (FittedForSpan(record, refitted_span) != emoji_again)
                return Fail("the reloaded universe did not fit the face to its new span");
        if (FittedForSpan(record, emoji_span) != NULL)
                return Fail("a fitted instance for the previous span survived the reload");
        /* No comparison against the old font pointer: the previous universe was
         * destroyed, and an allocator may hand the same address back, so identity
         * there would prove nothing.  The generation, the empty table, the new span,
         * the role and the effective file are the evidence of replacement. */
        if (strcmp(emoji_role, "fallback") != 0)
                return Fail("the reloaded universe changed the automatic serving role");
        if (emoji_file == NULL || strcmp(emoji_file, automatic_file) != 0)
                return Fail("the reloaded universe changed the effective file");

        /* Transition 2b: a successful reload that changes the relevant font.  The
         * explicit rescue must take over from automatic discovery, which is
         * visible as a different serving role for the same atom. */
        emoji_generation = XtpVtFontGeneration(vt);
        Phase("after-font-reload");
        XtVaSetValues(vt, "faceNameEmojiText", "Noto Emoji:color=false", NULL);
        if (XtpVtFontGeneration(vt) != emoji_generation + 1U)
                return Fail("the font reload did not advance the generation once");
        emoji_font = RouteTools(record, &emoji_role, &emoji_file);
        /* The role, not just the file, has to change: the same face reached through
         * automatic discovery would prove nothing about the rescue taking over. */
        if (emoji_font == NULL || emoji_role == NULL || strcmp(emoji_role, "emoji-text") != 0)
                return Fail("faceNameEmojiText did not take over from automatic discovery");
        if (emoji_file == NULL || strstr(emoji_file, "NotoEmoji-Regular-3.003.ttf") == NULL)
                return Fail("the explicit rescue chose an unexpected effective file");
        if (FittedForSpan(record, XtpVtCellWidth(vt)) != emoji_font)
                return Fail("the explicit rescue was not fitted to the atom span");
        reloaded_generation = XtpVtFontGeneration(vt);
        reloaded_height = XtpVtCellHeight(vt);
        XtCallActionProc(vt, "report-font-routing", NULL, NULL, 0);

        XtVaSetValues(vt, "faceName", "", "systemFallback", False, NULL);
        if (XtpVtFontGeneration(vt) != reloaded_generation)
                return Fail("failed SetValues advanced the effective generation");
        if (!XtpVtUsingXft(vt) || XtpVtCellHeight(vt) != reloaded_height)
                return Fail("failed SetValues did not retain the prior effective universe");
        XtVaGetValues(vt, "faceName", &configured, "systemFallback", &configured_system_fallback,
                      NULL);
        if (configured == NULL || strcmp(configured, "") != 0 || configured_system_fallback)
                return Fail("failed SetValues did not retain the configured resource value");

        /* Transition 3: after the rejected reload, the retained universe still
         * serves the atom from the same fitted instance it held before. */
        Phase("after-rejected-reload");
        emoji_again = RouteTools(record, &emoji_role, &emoji_file);
        if (emoji_again == NULL || emoji_role == NULL || strcmp(emoji_role, "emoji-text") != 0)
                return Fail("the retained universe lost the emoji route after a failed reload");
        if (emoji_again != emoji_font)
                return Fail("the retained universe lost its fitted instance");
        if (emoji_file == NULL || strstr(emoji_file, "NotoEmoji-Regular-3.003.ttf") == NULL)
                return Fail("the retained route changed its effective file");
        if (FittedForSpan(record, XtpVtCellWidth(vt)) != emoji_again)
                return Fail("the retained fitted instance is no longer the one in the table");

        XtCallActionProc(vt, "report-font-routing", NULL, NULL, 0);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(context);
        return 0;
}
