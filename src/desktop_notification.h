#ifndef XTERM_PLUS_DESKTOP_NOTIFICATION_H
#define XTERM_PLUS_DESKTOP_NOTIFICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum
{
        XTP_DESKTOP_NOTIFY_QUEUED,
        XTP_DESKTOP_NOTIFY_FOCUSED,
        XTP_DESKTOP_NOTIFY_QUEUE_FULL,
        XTP_DESKTOP_NOTIFY_UNAVAILABLE,
} XtpDesktopNotifyResult;

typedef struct XtpDesktopNotifier XtpDesktopNotifier;

/* Delivery starts lazily on the first unfocused request; NULL only when out of memory. */
XtpDesktopNotifier *XtpDesktopNotifierNew(void);
/* Stops delivery and releases libnotify; it waits only for an idle delivery thread. */
void XtpDesktopNotifierFree(XtpDesktopNotifier *notifier);
/* Copies the borrowed spans and queues them; it never waits for the notification daemon. */
XtpDesktopNotifyResult XtpDesktopNotify(XtpDesktopNotifier *notifier, const uint8_t *title,
                                        size_t title_length, const uint8_t *body,
                                        size_t body_length, bool focused);
bool XtpDesktopNotificationsCompiled(void);
/* The -report-config section: build support and, when compiled, whether a daemon answers. */
void XtpDesktopNotificationReport(FILE *stream);

#endif
