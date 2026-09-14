#include "desktop_notification.h"

#include "config.h"
#include "diagnostics.h"
#include "notification_policy.h"
#include "version.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBNOTIFY
#include <gio/gio.h>
#include <libnotify/notify.h>

#include <time.h>
#endif

static const char report_rule[] =
    "\n! ----------------------------------------------------------------------\n"
    "! Desktop notifications\n";

#ifndef HAVE_LIBNOTIFY

struct XtpDesktopNotifier
{
        bool unavailable_reported;
};

XtpDesktopNotifier *
XtpDesktopNotifierNew(void)
{
        return calloc(1, sizeof(XtpDesktopNotifier));
}

void
XtpDesktopNotifierFree(XtpDesktopNotifier *notifier)
{
        free(notifier);
}

XtpDesktopNotifyResult
XtpDesktopNotify(XtpDesktopNotifier *notifier, const uint8_t *title, size_t title_length,
                 const uint8_t *body, size_t body_length, bool focused)
{
        (void)title;
        (void)title_length;
        (void)body;
        (void)body_length;
        if (focused)
                return XTP_DESKTOP_NOTIFY_FOCUSED;
        if (notifier != NULL && !notifier->unavailable_reported) {
                XtpLog(XTP_LOG_INFO, "notify",
                       "desktop notifications are not compiled in; OSC 9/777 set urgency only");
                notifier->unavailable_reported = true;
        }
        return XTP_DESKTOP_NOTIFY_UNAVAILABLE;
}

bool
XtpDesktopNotificationsCompiled(void)
{
        return false;
}

void
XtpDesktopNotificationReport(FILE *stream)
{
        fputs(report_rule, stream);
        fputs("! desktop notifications: compiled without libnotify\n", stream);
        fputs("! OSC 9 and OSC 777 requests are logged and set WM urgency while unfocused.\n",
              stream);
}

#else

/* Requests waiting for the delivery thread; a request finding it full counts as limited. */
#define XTP_NOTIFY_QUEUE_LIMIT 4U

typedef struct
{
        XtpNotifyText title;
        XtpNotifyText body;
} NotifyRequest;

struct XtpDesktopNotifier
{
        GMutex lock;
        GCond wake;
        GThread *thread;
        NotifyRequest queue[XTP_NOTIFY_QUEUE_LIMIT];
        size_t queued;
        bool stopping;
        bool busy;
        bool unavailable_reported;
        XtpNotifyGate gate;
        /* Owned by the delivery thread. */
        bool markup_known;
        bool markup;
};

static uint64_t
MonotonicMs(void)
{
        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
                return 0;
        return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
}

static void
FreeRequest(NotifyRequest *request)
{
        XtpNotifyTextFree(&request->title);
        XtpNotifyTextFree(&request->body);
}

static bool
ServerParsesBodyMarkup(void)
{
        GList *capabilities = notify_get_server_caps();
        GList *item;
        bool markup = false;

        for (item = capabilities; item != NULL; item = item->next)
                if (g_strcmp0(item->data, "body-markup") == 0)
                        markup = true;
        g_list_free_full(capabilities, g_free);
        return markup;
}

/* Runs on the delivery thread, the only thread that calls libnotify while the terminal runs. */
static bool
ShowRequest(XtpDesktopNotifier *notifier, const NotifyRequest *request, GError **error)
{
        NotifyNotification *notification;
        char *escaped = NULL;
        const char *body = NULL;
        bool delivered;

        if (!notify_is_initted() && !notify_init(XTP_PROGRAM_NAME)) {
                g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                    "libnotify initialization failed");
                return false;
        }
        if (!notifier->markup_known) {
                notifier->markup = ServerParsesBodyMarkup();
                notifier->markup_known = true;
        }
        if (request->body.length != 0) {
                if (notifier->markup) {
                        escaped = XtpNotifyEscapeMarkup(request->body.text, request->body.length);
                        if (escaped == NULL) {
                                g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                                    "no memory to escape markup");
                                return false;
                        }
                        body = escaped;
                } else {
                        body = request->body.text;
                }
        }
        /* An empty title would be an invalid summary, so the application label stands in. */
        notification = notify_notification_new(request->title.length != 0 ? request->title.text
                                                                          : XTP_PROGRAM_NAME,
                                               body, XTP_PROGRAM_NAME);
        delivered = notify_notification_show(notification, error) != FALSE;
        g_object_unref(notification);
        free(escaped);
        if (!delivered) {
                notifier->markup_known = false;
                /* The cached proxy never follows a restarted daemon without a main loop. */
                notify_uninit();
        }
        return delivered;
}

static bool
DeliverRequest(XtpDesktopNotifier *notifier, const NotifyRequest *request, char *reason,
               size_t reason_size)
{
        GError *error = NULL;
        bool delivered = ShowRequest(notifier, request, &error);

        if (!delivered && (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
                           g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))) {
                g_clear_error(&error);
                delivered = ShowRequest(notifier, request, &error);
        }
        if (!delivered)
                (void)snprintf(reason, reason_size, "%s",
                               error != NULL && error->message != NULL ? error->message
                                                                       : "unknown error");
        g_clear_error(&error);
        return delivered;
}

static gpointer
DeliveryThread(gpointer data)
{
        XtpDesktopNotifier *notifier = data;

        g_mutex_lock(&notifier->lock);
        for (;;) {
                NotifyRequest request;
                char reason[256];
                bool delivered;
                bool first_denial = false;
                XtpNotifyGateResult gate;

                while (!notifier->stopping && notifier->queued == 0)
                        g_cond_wait(&notifier->wake, &notifier->lock);
                if (notifier->stopping)
                        break;
                request = notifier->queue[0];
                memmove(&notifier->queue[0], &notifier->queue[1],
                        (notifier->queued - 1U) * sizeof(notifier->queue[0]));
                --notifier->queued;
                /* Gated at call time so queued work honors a later backoff and stalls. */
                gate = XtpNotifyGateCheck(&notifier->gate, MonotonicMs(), &first_denial);
                if (gate != XTP_NOTIFY_GATE_ALLOW) {
                        FreeRequest(&request);
                        if (first_denial && gate == XTP_NOTIFY_GATE_BACKOFF)
                                XtpLog(XTP_LOG_INFO, "notify",
                                       "desktop notifications paused for %u ms after a failure",
                                       XTP_NOTIFY_FAILURE_BACKOFF_MS);
                        else if (first_denial)
                                XtpLog(XTP_LOG_INFO, "notify",
                                       "desktop notifications rate limited: at most %u per %u ms",
                                       XTP_NOTIFY_RATE_BURST, XTP_NOTIFY_RATE_WINDOW_MS);
                        continue;
                }
                notifier->busy = true;
                g_mutex_unlock(&notifier->lock);
                reason[0] = '\0';
                delivered = DeliverRequest(notifier, &request, reason, sizeof(reason));
                FreeRequest(&request);
                g_mutex_lock(&notifier->lock);
                notifier->busy = false;
                if (delivered) {
                        if (XtpNotifyGateSucceeded(&notifier->gate))
                                XtpLog(XTP_LOG_INFO, "notify", "desktop notifications recovered");
                        XtpLog(XTP_LOG_DEBUG, "notify", "desktop notification delivered");
                } else if (XtpNotifyGateFailed(&notifier->gate, MonotonicMs())) {
                        XtpLog(XTP_LOG_WARNING, "notify",
                               "desktop notification failed: %.200s; retrying after %u ms", reason,
                               XTP_NOTIFY_FAILURE_BACKOFF_MS);
                } else {
                        XtpLog(XTP_LOG_DEBUG, "notify", "desktop notification failed again: %.200s",
                               reason);
                }
        }
        g_mutex_unlock(&notifier->lock);
        if (notify_is_initted())
                notify_uninit();
        return NULL;
}

XtpDesktopNotifier *
XtpDesktopNotifierNew(void)
{
        XtpDesktopNotifier *notifier = calloc(1, sizeof(*notifier));

        if (notifier == NULL)
                return NULL;
        g_mutex_init(&notifier->lock);
        g_cond_init(&notifier->wake);
        return notifier;
}

void
XtpDesktopNotifierFree(XtpDesktopNotifier *notifier)
{
        bool busy;

        if (notifier == NULL)
                return;
        g_mutex_lock(&notifier->lock);
        notifier->stopping = true;
        while (notifier->queued != 0)
                FreeRequest(&notifier->queue[--notifier->queued]);
        busy = notifier->busy;
        g_cond_signal(&notifier->wake);
        g_mutex_unlock(&notifier->lock);
        if (notifier->thread != NULL && busy) {
                /* A daemon call can last its whole D-Bus timeout; exit without waiting or freeing.
                 */
                XtpLog(XTP_LOG_INFO, "notify",
                       "desktop notification still in progress at exit; not waiting");
                g_thread_unref(notifier->thread);
                return;
        }
        if (notifier->thread != NULL)
                g_thread_join(notifier->thread);
        g_mutex_clear(&notifier->lock);
        g_cond_clear(&notifier->wake);
        free(notifier);
        XtpLog(XTP_LOG_INFO, "notify", "desktop notifications stopped");
}

XtpDesktopNotifyResult
XtpDesktopNotify(XtpDesktopNotifier *notifier, const uint8_t *title, size_t title_length,
                 const uint8_t *body, size_t body_length, bool focused)
{
        NotifyRequest request;
        bool queue_full = false;
        bool thread_failed = false;

        if (notifier == NULL)
                return XTP_DESKTOP_NOTIFY_UNAVAILABLE;
        if (focused) {
                XtpLog(XTP_LOG_INFO, "notify", "desktop notification suppressed reason=focused");
                return XTP_DESKTOP_NOTIFY_FOCUSED;
        }
        memset(&request, 0, sizeof(request));
        if (!XtpNotifyTextPrepare(title, title_length, XTP_NOTIFY_TITLE_LIMIT, false,
                                  &request.title) ||
            !XtpNotifyTextPrepare(body, body_length, XTP_NOTIFY_BODY_LIMIT, true, &request.body)) {
                FreeRequest(&request);
                XtpLog(XTP_LOG_WARNING, "notify", "no memory for a desktop notification");
                return XTP_DESKTOP_NOTIFY_UNAVAILABLE;
        }
        if (request.title.truncated || request.body.truncated)
                XtpLog(XTP_LOG_INFO, "notify",
                       "desktop notification truncated title-bytes=%zu body-bytes=%zu "
                       "limits=%u/%u",
                       title_length, body_length, XTP_NOTIFY_TITLE_LIMIT, XTP_NOTIFY_BODY_LIMIT);
        if (request.title.replaced || request.body.replaced)
                XtpLog(XTP_LOG_INFO, "notify",
                       "desktop notification replaced invalid or control bytes");
        g_mutex_lock(&notifier->lock);
        if (notifier->thread == NULL)
                notifier->thread = g_thread_try_new("xtp-notify", DeliveryThread, notifier, NULL);
        if (notifier->thread == NULL) {
                thread_failed = !notifier->unavailable_reported;
                notifier->unavailable_reported = true;
        } else if (notifier->queued == XTP_NOTIFY_QUEUE_LIMIT) {
                queue_full = true;
        } else {
                notifier->queue[notifier->queued++] = request;
                memset(&request, 0, sizeof(request));
                g_cond_signal(&notifier->wake);
        }
        g_mutex_unlock(&notifier->lock);
        if (notifier->thread == NULL) {
                FreeRequest(&request);
                if (thread_failed)
                        XtpLog(XTP_LOG_WARNING, "notify",
                               "cannot start desktop notification delivery; urgency only");
                return XTP_DESKTOP_NOTIFY_UNAVAILABLE;
        }
        if (queue_full) {
                FreeRequest(&request);
                XtpLog(XTP_LOG_DEBUG, "notify",
                       "desktop notification dropped; delivery queue full");
                return XTP_DESKTOP_NOTIFY_QUEUE_FULL;
        }
        XtpLog(XTP_LOG_DEBUG, "notify", "desktop notification queued");
        return XTP_DESKTOP_NOTIFY_QUEUED;
}

bool
XtpDesktopNotificationsCompiled(void)
{
        return true;
}

/* How long -report-config waits for the daemon before reporting that it did not answer. */
#define XTP_NOTIFY_REPORT_TIMEOUT_MS 1000U

typedef struct
{
        GMutex lock;
        GCond answered;
        bool done;
        bool abandoned;
        bool initialized;
        bool known;
        char *name;
        char *version;
        char *specification;
} ReportQuery;

/* Static so a query abandoned at the deadline never outlives its storage. */
static ReportQuery report_query;

static void
FreeReportStrings(ReportQuery *query)
{
        g_clear_pointer(&query->name, g_free);
        g_clear_pointer(&query->version, g_free);
        g_clear_pointer(&query->specification, g_free);
}

static gpointer
ReportThread(gpointer data)
{
        ReportQuery *query = data;
        char *name = NULL;
        char *vendor = NULL;
        char *version = NULL;
        char *specification = NULL;
        bool initialized = notify_init(XTP_PROGRAM_NAME) != FALSE;
        bool known =
            initialized && notify_get_server_info(&name, &vendor, &version, &specification);

        if (initialized)
                notify_uninit();
        g_free(vendor);
        g_mutex_lock(&query->lock);
        query->initialized = initialized;
        query->known = known;
        query->name = name;
        query->version = version;
        query->specification = specification;
        query->done = true;
        if (query->abandoned)
                FreeReportStrings(query);
        g_cond_signal(&query->answered);
        g_mutex_unlock(&query->lock);
        return NULL;
}

void
XtpDesktopNotificationReport(FILE *stream)
{
        ReportQuery *query = &report_query;
        GThread *thread;
        gint64 deadline;

        fputs(report_rule, stream);
        fputs("! desktop notifications: compiled with libnotify\n", stream);
        g_mutex_init(&query->lock);
        g_cond_init(&query->answered);
        thread = g_thread_try_new("xtp-notify-report", ReportThread, query, NULL);
        if (thread == NULL) {
                fputs("! notification daemon: not queried (no thread)\n", stream);
                return;
        }
        deadline = g_get_monotonic_time() + (gint64)XTP_NOTIFY_REPORT_TIMEOUT_MS * 1000;
        g_mutex_lock(&query->lock);
        while (!query->done)
                if (!g_cond_wait_until(&query->answered, &query->lock, deadline))
                        break;
        if (!query->done) {
                query->abandoned = true;
                g_mutex_unlock(&query->lock);
                /* The thread may sit in a D-Bus call for its whole timeout; exit without it. */
                g_thread_unref(thread);
                fprintf(stream, "! notification daemon: no reply within %u ms\n",
                        XTP_NOTIFY_REPORT_TIMEOUT_MS);
                return;
        }
        g_mutex_unlock(&query->lock);
        g_thread_join(thread);
        if (!query->initialized)
                fputs("! notification daemon: unavailable (libnotify initialization failed)\n",
                      stream);
        else if (query->known)
                fprintf(stream, "! notification daemon: %.64s %.32s (specification %.16s)\n",
                        query->name != NULL ? query->name : "?",
                        query->version != NULL ? query->version : "?",
                        query->specification != NULL ? query->specification : "?");
        else
                fputs("! notification daemon: unavailable\n", stream);
        FreeReportStrings(query);
        g_mutex_clear(&query->lock);
        g_cond_clear(&query->answered);
}

#endif
