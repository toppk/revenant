#ifndef XTERM_PLUS_CURSOR_BLINK_H
#define XTERM_PLUS_CURSOR_BLINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
        XTP_CURSOR_CONTROL_GROUND,
        XTP_CURSOR_CONTROL_ESCAPE,
        XTP_CURSOR_CONTROL_CSI,
        XTP_CURSOR_CONTROL_DCS_HEADER,
        XTP_CURSOR_CONTROL_STRING,
} XtpCursorControlState;

typedef enum
{
        XTP_CURSOR_STRING_OSC,
        XTP_CURSOR_STRING_DCS,
        XTP_CURSOR_STRING_APC,
} XtpCursorControlString;

typedef enum
{
        XTP_CURSOR_BLINK_DEFAULT_FALSE,
        XTP_CURSOR_BLINK_DEFAULT_TRUE,
        XTP_CURSOR_BLINK_ALWAYS,
        XTP_CURSOR_BLINK_NEVER,
} XtpCursorBlinkPolicy;

typedef void (*XtpCursorBlinkResetFn)(void *closure);
/* Called before the byte at offset changes blink_requested or resets policy. */
typedef void (*XtpCursorBlinkBeforeChangeFn)(size_t offset, void *closure);

typedef struct
{
        XtpCursorBlinkBeforeChangeFn before_change;
        XtpCursorBlinkResetFn reset;
        void *closure;
} XtpCursorBlinkObserverEffects;

/*
 * libghostty exposes the resolved cursor blink value, but xterm's resource
 * policy needs the application's uncombined operand.  This narrowly scoped
 * observer records that operand; libghostty remains authoritative for cursor
 * shape and all other terminal state.  Keep its accepted control syntax
 * covered by differential tests when either parser changes.
 */
typedef struct
{
        XtpCursorControlState state;
        bool blink_requested;
        bool saved_blink_requested;
        bool saved_blink_valid;
        bool ignore_requests;
        bool csi_private;
        bool csi_invalid;
        bool csi_parameter_bytes_seen;
        bool csi_parameter_present;
        bool csi_mode_12;
        bool dcs_intermediate_seen;
        bool dcs_parameter_seen;
        uint8_t utf8_remaining;
        XtpCursorControlString string_kind;
        uint8_t csi_intermediate;
        uint8_t csi_intermediate_count;
        unsigned int csi_parameter;
        unsigned int csi_first_parameter;
        size_t csi_parameter_count;
} XtpCursorBlinkObserver;

void XtpCursorBlinkObserverFeed(XtpCursorBlinkObserver *observer, const uint8_t *bytes,
                                size_t length, const XtpCursorBlinkObserverEffects *effects);
void XtpCursorBlinkObserverSetRequestsEnabled(XtpCursorBlinkObserver *observer, bool enabled);
bool XtpCursorBlinkEffective(XtpCursorBlinkPolicy policy, bool xor_policy, bool requested);

#endif
