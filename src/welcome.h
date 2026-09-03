#ifndef XTERM_PLUS_WELCOME_H
#define XTERM_PLUS_WELCOME_H

#include <X11/Intrinsic.h>

#include <stdbool.h>
#include <stdio.h>

#define XTP_OS_ID_CAPACITY 64
#define XTP_OS_ID_LIKE_CAPACITY 128
#define XTP_OS_NAME_CAPACITY 160
#define XTP_OS_VERSION_CAPACITY 64

typedef struct
{
        char id[XTP_OS_ID_CAPACITY];
        char id_like[XTP_OS_ID_LIKE_CAPACITY];
        char name[XTP_OS_NAME_CAPACITY];
        char version[XTP_OS_VERSION_CAPACITY];
} XtpOsRelease;

int XtpWelcomeParseOsRelease(const char *contents, XtpOsRelease *release);
const char *XtpWelcomePackageFamily(const XtpOsRelease *release);
bool XtpWelcomeNeedsReadableFont(bool configured_font, bool using_xft, unsigned int cell_height,
                                 double dpi);
void XtpWelcomeReport(FILE *stream, Display *display, Widget vt, const char *application_name,
                      const char *application_class);

#endif
