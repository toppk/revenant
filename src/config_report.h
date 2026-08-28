#ifndef XTERM_PLUS_CONFIG_REPORT_H
#define XTERM_PLUS_CONFIG_REPORT_H

#include <X11/Intrinsic.h>
#include <X11/Xresource.h>

XrmDatabase XtpConfigCommandDatabase(int argc, char **argv);
void XtpLogResourceDatabases(Display *display);
void XtpReportConfig(Display *display, Widget vt, XrmDatabase command_database);

#endif
