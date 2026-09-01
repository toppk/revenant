#include "font_router.h"

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

        XtVaSetValues(vt, "faceName", "", "systemFallback", False, NULL);
        if (XtpVtFontGeneration(vt) != reloaded_generation)
                return Fail("failed SetValues advanced the effective generation");
        if (!XtpVtUsingXft(vt) || XtpVtCellHeight(vt) != reloaded_height)
                return Fail("failed SetValues did not retain the prior effective universe");
        XtVaGetValues(vt, "faceName", &configured, "systemFallback", &configured_system_fallback,
                      NULL);
        if (configured == NULL || strcmp(configured, "") != 0 || configured_system_fallback)
                return Fail("failed SetValues did not retain the configured resource value");

        XtCallActionProc(vt, "report-font-routing", NULL, NULL, 0);
        XtDestroyWidget(shell);
        XtDestroyApplicationContext(context);
        return 0;
}
