#ifndef XTERM_PLUS_NOTIFICATION_POLICY_H
#define XTERM_PLUS_NOTIFICATION_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bytes of sanitized UTF-8 kept from a title and a body, before markup escaping. */
#define XTP_NOTIFY_TITLE_LIMIT 256U
#define XTP_NOTIFY_BODY_LIMIT 1024U
/* At most this many delivery attempts in any sliding window of this many milliseconds. */
#define XTP_NOTIFY_RATE_BURST 5U
#define XTP_NOTIFY_RATE_WINDOW_MS 10000U
/* After a failed delivery, attempts wait this long before the daemon is tried again. */
#define XTP_NOTIFY_FAILURE_BACKOFF_MS 5000U

typedef enum
{
        XTP_NOTIFY_GATE_ALLOW,
        XTP_NOTIFY_GATE_RATE_LIMITED,
        XTP_NOTIFY_GATE_BACKOFF,
} XtpNotifyGateResult;

typedef struct
{
        uint64_t attempts[XTP_NOTIFY_RATE_BURST];
        size_t attempt_count;
        size_t oldest;
        uint64_t backoff_until;
        bool failing;
        bool limit_reported;
        bool backoff_reported;
} XtpNotifyGate;

/* Records an allowed attempt; *first_denial is true only for the first denial of a period. */
XtpNotifyGateResult XtpNotifyGateCheck(XtpNotifyGate *gate, uint64_t now_ms, bool *first_denial);
/* Starts or restarts the backoff; true when this failure begins a failing period. */
bool XtpNotifyGateFailed(XtpNotifyGate *gate, uint64_t now_ms);
/* Ends a failing period; true when one was in progress. */
bool XtpNotifyGateSucceeded(XtpNotifyGate *gate);

typedef struct
{
        char *text;
        size_t length;
        bool truncated;
        bool replaced;
} XtpNotifyText;

/* NUL-terminated valid UTF-8 within limit bytes; bad bytes, NUL and controls become U+FFFD. */
bool XtpNotifyTextPrepare(const uint8_t *bytes, size_t length, size_t limit, bool multiline,
                          XtpNotifyText *text);
void XtpNotifyTextFree(XtpNotifyText *text);
/* Escapes & < > " ' for servers that parse body markup; NULL when out of memory. */
char *XtpNotifyEscapeMarkup(const char *text, size_t length);

#endif
