#ifndef XTERM_PLUS_DIAGNOSTICS_H
#define XTERM_PLUS_DIAGNOSTICS_H

#include <stddef.h>

#if defined(__GNUC__)
#define XTP_PRINTF(format_index, arguments_index)                                                  \
        __attribute__((format(printf, format_index, arguments_index)))
#else
#define XTP_PRINTF(format_index, arguments_index)
#endif

typedef enum
{
        XTP_LOG_DEBUG,
        XTP_LOG_INFO,
        XTP_LOG_WARNING,
        XTP_LOG_ERROR,
} XtpLogLevel;

void XtpLogSetDebug(int enabled);
int XtpLogDebugEnabled(void);
void XtpLogSetQuiet(int enabled);
void XtpLog(XtpLogLevel level, const char *subsystem, const char *format, ...) XTP_PRINTF(3, 4);
void XtpLogBytePreview(XtpLogLevel level, const char *subsystem, const char *event,
                       const void *bytes, size_t length);

#undef XTP_PRINTF

#endif
