#include "diagnostics.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int debug_enabled;
static int quiet_enabled;

void
XtpLogSetDebug(int enabled)
{
        debug_enabled = enabled != 0;
}

int
XtpLogDebugEnabled(void)
{
        return debug_enabled;
}

void
XtpLogSetQuiet(int enabled)
{
        quiet_enabled = enabled != 0;
}

static const char *
LevelColor(XtpLogLevel level)
{
        switch (level) {
        case XTP_LOG_DEBUG:
                return "\033[94m";
        case XTP_LOG_INFO:
                return "\033[92m";
        case XTP_LOG_WARNING:
                return "\033[93m";
        case XTP_LOG_ERROR:
                return "\033[91m";
        }
        return "\033[92m";
}

void
XtpLog(XtpLogLevel level, const char *subsystem, const char *format, ...)
{
        static const char bright_cyan[] = "\033[96m";
        static const char reset[] = "\033[0m";
        char timestamp[16];
        time_t now = time(NULL);
        struct tm local;
        va_list arguments;
        int use_color;

        if (quiet_enabled || (level == XTP_LOG_DEBUG && !debug_enabled))
                return;

        if (localtime_r(&now, &local) == NULL ||
            strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &local) == 0)
                (void)snprintf(timestamp, sizeof(timestamp), "??:??:??");

        use_color = isatty(STDERR_FILENO) != 0 && getenv("NO_COLOR") == NULL;
        flockfile(stderr);
        if (use_color)
                fprintf(stderr, "%s%s%s %s%s: ", bright_cyan, timestamp, reset, LevelColor(level),
                        subsystem);
        else
                fprintf(stderr, "%s %s: ", timestamp, subsystem);
        va_start(arguments, format);
        vfprintf(stderr, format, arguments);
        va_end(arguments);
        if (use_color)
                fputs(reset, stderr);
        fputc('\n', stderr);
        fflush(stderr);
        funlockfile(stderr);
}
