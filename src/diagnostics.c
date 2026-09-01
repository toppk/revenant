#include "diagnostics.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static XtpLogLevel minimum_level = XTP_LOG_WARNING;
static int quiet_enabled;

enum
{
        BYTE_PREVIEW_LIMIT = 256,
};

int
XtpLogLevelParse(const char *text, XtpLogLevel *level)
{
        if (text == NULL || level == NULL)
                return -1;
        if (strcasecmp(text, "debug") == 0)
                *level = XTP_LOG_DEBUG;
        else if (strcasecmp(text, "info") == 0)
                *level = XTP_LOG_INFO;
        else if (strcasecmp(text, "warning") == 0 || strcasecmp(text, "warn") == 0)
                *level = XTP_LOG_WARNING;
        else if (strcasecmp(text, "error") == 0)
                *level = XTP_LOG_ERROR;
        else
                return -1;
        return 0;
}

const char *
XtpLogLevelName(XtpLogLevel level)
{
        switch (level) {
        case XTP_LOG_DEBUG:
                return "debug";
        case XTP_LOG_INFO:
                return "info";
        case XTP_LOG_WARNING:
                return "warning";
        case XTP_LOG_ERROR:
                return "error";
        }
        return "warning";
}

void
XtpLogSetLevel(XtpLogLevel level)
{
        minimum_level = level;
}

XtpLogLevel
XtpLogLevelCurrent(void)
{
        return minimum_level;
}

void
XtpLogSetQuiet(int enabled)
{
        quiet_enabled = enabled != 0;
}

int
XtpLogEnabled(XtpLogLevel level)
{
        return !quiet_enabled && level >= minimum_level;
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

        if (!XtpLogEnabled(level))
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

void
XtpLogBytePreview(XtpLogLevel level, const char *subsystem, const char *event, const void *bytes,
                  size_t length)
{
        static const char hex[] = "0123456789abcdef";
        const unsigned char *input = bytes;
        char preview[BYTE_PREVIEW_LIMIT * 4U + 1U];
        size_t shown = length < BYTE_PREVIEW_LIMIT ? length : BYTE_PREVIEW_LIMIT;
        size_t input_offset;
        size_t output_offset = 0;

        if (!XtpLogEnabled(level))
                return;
        if (input == NULL && length != 0) {
                XtpLog(level, subsystem, "%s bytes=%zu preview=<invalid-null-buffer>", event,
                       length);
                return;
        }

        for (input_offset = 0; input_offset < shown; ++input_offset) {
                unsigned char byte = input[input_offset];
                const char *escape = NULL;

                switch (byte) {
                case '\a':
                        escape = "\\a";
                        break;
                case '\b':
                        escape = "\\b";
                        break;
                case '\t':
                        escape = "\\t";
                        break;
                case '\n':
                        escape = "\\n";
                        break;
                case '\v':
                        escape = "\\v";
                        break;
                case '\f':
                        escape = "\\f";
                        break;
                case '\r':
                        escape = "\\r";
                        break;
                case 0x1b:
                        escape = "\\e";
                        break;
                case '\\':
                        escape = "\\\\";
                        break;
                case '"':
                        escape = "\\\"";
                        break;
                default:
                        break;
                }
                if (escape != NULL) {
                        preview[output_offset++] = escape[0];
                        preview[output_offset++] = escape[1];
                } else if (byte >= 0x20U && byte <= 0x7eU) {
                        preview[output_offset++] = (char)byte;
                } else {
                        preview[output_offset++] = '\\';
                        preview[output_offset++] = 'x';
                        preview[output_offset++] = hex[byte >> 4U];
                        preview[output_offset++] = hex[byte & 0x0fU];
                }
        }
        preview[output_offset] = '\0';

        if (shown < length)
                XtpLog(level, subsystem, "%s bytes=%zu preview=\"%s\" omitted=%zu", event, length,
                       preview, length - shown);
        else
                XtpLog(level, subsystem, "%s bytes=%zu preview=\"%s\"", event, length, preview);
}
