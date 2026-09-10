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

/* Called for XTWINOPS 20-23 (CSI Ps ; ... t) that libghostty parses without
 * a public hook; parameters holds the first XTP_CSI_OBSERVED_PARAMETERS. */
#define XTP_CSI_OBSERVED_PARAMETERS 3U
typedef void (*XtpWindowOpFn)(unsigned int op, unsigned int parameter_count,
                              const unsigned int *parameters, size_t offset, void *closure);

typedef struct
{
        XtpCursorBlinkBeforeChangeFn before_change;
        XtpCursorBlinkResetFn reset;
        XtpWindowOpFn window_op;
        /* OSC selector boundary; payload interpretation stays in libghostty. */
        void (*osc_header)(unsigned int selector, size_t offset, void *closure);
        /* First byte of each ';'-separated OSC payload item; query is "?". */
        void (*osc_payload)(unsigned int selector, bool query, size_t offset, void *closure);
        /* The ';' that ends an OSC payload item. */
        void (*osc_item_end)(size_t offset, void *closure);
        /* First byte of an OSC terminator (BEL, ESC, ST) or the byte aborting
         * the OSC; libghostty dispatches the command at that byte. */
        void (*osc_end)(size_t offset, void *closure);
        void *closure;
} XtpCursorBlinkObserverEffects;

/*
 * libghostty exposes the resolved cursor blink value, but xterm's resource
 * policy needs the application's uncombined operand.  This narrowly scoped
 * observer records that operand, the XTWINOPS title operations that
 * libghostty accepts without exposing, and OSC selectors for dynamic-color
 * permission checks. Remove these when equivalent public hooks exist.
 * libghostty remains authoritative for color payload parsing,
 * cursor shape and all other terminal state.  Keep its accepted control syntax
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
        bool osc_header_done;
        bool osc_selector_present;
        bool osc_item_start;
        unsigned int osc_selector;
        uint8_t csi_intermediate;
        uint8_t csi_intermediate_count;
        unsigned int csi_parameter;
        unsigned int csi_first_parameter;
        unsigned int csi_parameters[XTP_CSI_OBSERVED_PARAMETERS];
        size_t csi_parameter_count;
} XtpCursorBlinkObserver;

void XtpCursorBlinkObserverFeed(XtpCursorBlinkObserver *observer, const uint8_t *bytes,
                                size_t length, const XtpCursorBlinkObserverEffects *effects);
void XtpCursorBlinkObserverSetRequestsEnabled(XtpCursorBlinkObserver *observer, bool enabled);
bool XtpCursorBlinkEffective(XtpCursorBlinkPolicy policy, bool xor_policy, bool requested);

#endif
