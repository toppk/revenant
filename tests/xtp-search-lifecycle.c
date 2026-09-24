#include "terminal.h"
#include "vt_widgetP.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

static int
Check(bool condition, const char *message)
{
        if (!condition)
                fprintf(stderr, "search lifecycle: %s\n", message);
        return condition ? 0 : 1;
}

static XtpTerminal *
NewTerminal(Widget widget)
{
        return XtpTerminalNewWithGraphemeWidth((uint16_t)XtpVtColumns(widget),
                                               (uint16_t)XtpVtRows(widget), XtpVtCellWidth(widget),
                                               XtpVtCellHeight(widget), false);
}

static int
Seed(XtpTerminal *terminal, size_t lines)
{
        char *bytes = malloc(lines * 24U + 1U);
        size_t length = 0;
        size_t index;
        int result;

        if (bytes == NULL)
                return -1;
        for (index = 0; index < lines; ++index)
                length += (size_t)snprintf(bytes + length, lines * 24U + 1U - length,
                                           "line %04zu needle\r\n", index);
        result = XtpTerminalFeedOutput(terminal, (const uint8_t *)bytes, length, false);
        free(bytes);
        return result;
}

static int
CheckResources(Widget widget)
{
        int duration = 0;
        Boolean scrollbar = False;

        XtVaGetValues(widget, "copyFlashDuration", &duration, "scrollBar", &scrollbar, NULL);
        return Check(duration == 123 && scrollbar == True,
                     "Xt resources on both sides of search state changed");
}

static void
CompletionTimer(XtPointer closure, XtIntervalId *timer)
{
        bool *completed = closure;

        (void)timer;
        *completed = true;
}

/* A test-owned timer proves the event loop ran long enough to dispatch stale timers. */
static int
DrainThroughTimer(XtAppContext context)
{
        bool completed = false;
        XtIntervalId timer = XtAppAddTimeOut(context, 30UL, CompletionTimer, &completed);
        unsigned int processed;

        for (processed = 0; processed < 10000U && !completed; ++processed)
                XtAppProcessEvent(context, XtIMAll);
        if (!completed)
                XtRemoveTimeOut(timer);
        return Check(completed, "event drain did not reach its completion timer");
}

static void
CountShellConfigure(Widget widget, XtPointer closure, XEvent *event, Boolean *continue_dispatch)
{
        unsigned int *received = closure;

        (void)widget;
        (void)continue_dispatch;
        if (event->type == ConfigureNotify)
                ++*received;
}

static void
SendShellConfigure(Widget shell)
{
        XEvent event = {0};

        event.xconfigure.type = ConfigureNotify;
        event.xconfigure.display = XtDisplay(shell);
        event.xconfigure.event = XtWindow(shell);
        event.xconfigure.window = XtWindow(shell);
        (void)XSendEvent(XtDisplay(shell), XtWindow(shell), False, StructureNotifyMask, &event);
        XFlush(XtDisplay(shell));
}

int
main(int argc, char **argv)
{
        XtAppContext context;
        Widget shell = XtVaAppInitialize(&context, "XTerm", NULL, 0, &argc, argv, NULL, NULL);
        Widget widget = XtVaCreateManagedWidget("vt100", vt100WidgetClass, shell, "renderFont",
                                                "false", "saveLines", 4096, "copyFlashDuration",
                                                123, "scrollBar", True, NULL);
        Vt100Rec *vt = VtAsRecord(widget);
        XtpTerminal *first = NULL;
        XtpTerminal *second = NULL;
        XtpTerminal *third = NULL;
        int failures = 0;
        unsigned int shell_configures = 0;

        XtRealizeWidget(shell);
        XtAddEventHandler(shell, StructureNotifyMask, False, CountShellConfigure,
                          &shell_configures);
        failures += CheckResources(widget);
        failures += Check(!XtpVtSearchActive(widget) && vt->vt.search_runtime.model == NULL &&
                              vt->vt.search_runtime.timer == (XtIntervalId)0,
                          "search state was not initially empty");
        first = NewTerminal(widget);
        failures += Check(first != NULL, "first terminal construction failed");
        if (first == NULL)
                goto done;
        XtpVtSetTerminal(widget, first);

        if (XtpTerminalBackendIsStub()) {
                XtCallActionProc(widget, "start-search", NULL, NULL, 0);
                failures +=
                    Check(!XtpVtSearchActive(widget) && vt->vt.search_runtime.model == NULL &&
                              vt->vt.search_runtime.timer == (XtIntervalId)0,
                          "stub opened an unavailable search");
                XtpVtSetTerminal(widget, NULL);
                XtpTerminalFree(first);
                first = NULL;
                failures += CheckResources(widget);
                goto done;
        }

        failures += Check(Seed(first, 3200U) == 0, "first terminal seed failed");
        XtCallActionProc(widget, "start-search", NULL, NULL, 0);
        VtSearchKey(vt, XK_n, 0, "needle", 6U, CurrentTime);
        failures += Check(XtpVtSearchActive(widget) && vt->vt.search_runtime.model != NULL &&
                              vt->vt.search_runtime.timer != (XtIntervalId)0,
                          "running search did not retain its scan timer");
        VtSearchPrepareFrame(vt);
        failures += Check(vt->vt.search_runtime.visible_count != 0,
                          "first terminal search has no visible highlight");
        if (vt->vt.search_runtime.visible_count != 0) {
                const XtpSemanticSpan *span = &vt->vt.search_runtime.visible[0];
                uint64_t row = span->start_row - vt->vt.search_runtime.frame_top;

                failures +=
                    Check(row < (uint64_t)vt->vt.rows &&
                              VtSearchCellHighlight(vt, (uint16_t)row, span->start_column) != 0,
                          "visible match was not highlighted");
        }

        second = NewTerminal(widget);
        failures += Check(second != NULL, "second terminal construction failed");
        if (second == NULL)
                goto done;
        XtpVtSetTerminal(widget, second);
        XtpTerminalFree(first);
        first = NULL;
        failures += DrainThroughTimer(context);
        failures += Check(!XtpVtSearchActive(widget) && vt->vt.search_runtime.model == NULL &&
                              vt->vt.search_runtime.active_mark == NULL &&
                              vt->vt.search_runtime.viewport_mark == NULL &&
                              vt->vt.search_runtime.timer == (XtIntervalId)0,
                          "rebind retained first terminal search ownership");
        VtSearchPrepareFrame(vt);
        failures +=
            Check(vt->vt.search_runtime.visible_count == 0 && VtSearchCellHighlight(vt, 0, 0) == 0,
                  "rebind retained a stale highlight");
        failures += CheckResources(widget);

        failures += Check(Seed(second, 3200U) == 0, "second terminal seed failed");
        XtCallActionProc(widget, "start-search", NULL, NULL, 0);
        VtSearchKey(vt, XK_n, 0, "needle", 6U, CurrentTime);
        VtSearchPrepareFrame(vt);
        failures +=
            Check(XtpVtSearchActive(widget) && vt->vt.search_runtime.timer != (XtIntervalId)0 &&
                      vt->vt.search_runtime.visible_count != 0,
                  "NULL rebind case lacks an active scan and visible matches");
        XtpVtSetTerminal(widget, NULL);
        XtpTerminalFree(second);
        second = NULL;
        failures += DrainThroughTimer(context);
        VtSearchPrepareFrame(vt);
        failures += Check(!XtpVtSearchActive(widget) && vt->vt.search_runtime.model == NULL &&
                              vt->vt.search_runtime.active_mark == NULL &&
                              vt->vt.search_runtime.viewport_mark == NULL &&
                              vt->vt.search_runtime.timer == (XtIntervalId)0 &&
                              vt->vt.search_runtime.visible_count == 0 &&
                              VtSearchCellHighlight(vt, 0, 0) == 0,
                          "active NULL rebind retained search ownership or highlights");

        third = NewTerminal(widget);
        failures += Check(third != NULL, "third terminal construction failed");
        if (third == NULL)
                goto done;
        XtpVtSetTerminal(widget, third);
        failures += Check(Seed(third, 16U) == 0, "third terminal seed failed");
        XtCallActionProc(widget, "start-search", NULL, NULL, 0);
        VtSearchKey(vt, XK_n, 0, "needle", 6U, CurrentTime);
        VtSearchPrepareFrame(vt);
        failures += Check(XtpVtSearchActive(widget) && vt->vt.search_runtime.model != NULL &&
                              vt->vt.search_runtime.visible_count != 0,
                          "search did not reopen against a fresh terminal");
        VtSearchKey(vt, XK_Escape, 0, NULL, 0, CurrentTime);
        failures +=
            Check(!XtpVtSearchActive(widget) && vt->vt.search_runtime.timer == (XtIntervalId)0,
                  "Escape did not close the reopened search");

        failures += Check(Seed(third, 3200U) == 0, "pending-timer seed failed");
        XtCallActionProc(widget, "start-search", NULL, NULL, 0);
        VtSearchKey(vt, XK_n, 0, "needle", 6U, CurrentTime);
        failures += Check(vt->vt.search_runtime.timer != (XtIntervalId)0,
                          "destruction case has no pending timer");
        XtDestroyWidget(widget);
        widget = NULL;
        vt = NULL;
        XtpTerminalFree(third);
        third = NULL;
        shell_configures = 0;
        SendShellConfigure(shell);
        failures += DrainThroughTimer(context);
        failures +=
            Check(shell_configures != 0, "surviving shell did not receive its configure event");

done:
        if (widget != NULL) {
                XtpVtSetTerminal(widget, NULL);
                XtDestroyWidget(widget);
        }
        XtDestroyWidget(shell);
        XtpTerminalFree(first);
        XtpTerminalFree(second);
        XtpTerminalFree(third);
        XtDestroyApplicationContext(context);
        printf("search lifecycle: %d failure(s)\n", failures);
        return failures == 0 ? 0 : 1;
}
