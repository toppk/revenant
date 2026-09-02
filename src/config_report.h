#ifndef XTERM_PLUS_CONFIG_REPORT_H
#define XTERM_PLUS_CONFIG_REPORT_H

#include <X11/Intrinsic.h>
#include <X11/Xresource.h>

XrmDatabase XtpConfigCommandDatabase(int argc, char **argv, const char *application_name);
void XtpLogResourceDatabases(Display *display, const char *application_name,
                             const char *application_class);
void XtpReportConfig(Display *display, Widget vt, XrmDatabase command_database,
                     const char *application_name, const char *application_class);

#endif
