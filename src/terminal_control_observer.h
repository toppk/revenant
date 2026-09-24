#ifndef XTERM_PLUS_TERMINAL_CONTROL_OBSERVER_H
#define XTERM_PLUS_TERMINAL_CONTROL_OBSERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
        XTP_TERMINAL_CONTROL_GROUND,
        XTP_TERMINAL_CONTROL_ESCAPE,
        XTP_TERMINAL_CONTROL_CSI,
        XTP_TERMINAL_CONTROL_DCS_HEADER,
        XTP_TERMINAL_CONTROL_STRING,
} XtpTerminalControlState;

typedef enum
{
        XTP_TERMINAL_STRING_OSC,
        XTP_TERMINAL_STRING_DCS,
        XTP_TERMINAL_STRING_APC,
} XtpTerminalControlString;

typedef enum
{
        XTP_CURSOR_BLINK_DEFAULT_FALSE,
        XTP_CURSOR_BLINK_DEFAULT_TRUE,
        XTP_CURSOR_BLINK_ALWAYS,
        XTP_CURSOR_BLINK_NEVER,
} XtpCursorBlinkPolicy;

typedef void (*XtpCursorBlinkResetFn)(void *closure);
/* Flushes the observed input prefix before a callback inspects or changes state. */
typedef void (*XtpFlushObservedInputPrefixFn)(size_t offset, void *closure);

/* Called for observed XTWINOPS 20-23 and CSI 14/16/18 t before the final byte
 * reaches libghostty; observation alone does not establish core acceptance.
 * parameters holds the first XTP_CSI_OBSERVED_PARAMETERS. */
#define XTP_CSI_OBSERVED_PARAMETERS 3U
typedef void (*XtpWindowOpFn)(unsigned int op, unsigned int parameter_count,
                              const unsigned int *parameters, size_t offset, void *closure);

typedef struct
{
        XtpFlushObservedInputPrefixFn flush_prefix;
        XtpCursorBlinkResetFn reset;
        XtpWindowOpFn window_op;
        /* OSC selector boundary; the observer only inspects item starts and
         * boundaries, while libghostty owns full payload parsing. */
        void (*osc_header)(unsigned int selector, size_t offset, void *closure);
        /* First byte of each ';'-separated OSC payload item; query is "?". */
        void (*osc_payload)(unsigned int selector, bool query, size_t offset, void *closure);
        /* The ';' that ends an OSC payload item. */
        void (*osc_item_end)(size_t offset, void *closure);
        /* First byte of an OSC terminator (BEL, ESC, ST) or an aborting byte;
         * libghostty dispatches accepted commands only at valid termination. */
        void (*osc_end)(size_t offset, void *closure);
        void *closure;
} XtpTerminalControlObserverEffects;

/*
 * libghostty exposes the resolved cursor blink value, but xterm's resource
 * policy needs the application's uncombined operand.  This narrowly scoped
 * observer records that operand, Window Ops requests, and OSC boundaries for
 * color policy, prompt tracking and dropped-report diagnostics. These have
 * different before/after-acceptance needs; no single getter replaces them.
 * libghostty remains authoritative for full color payload parsing, cursor
 * shape and terminal state. Keep accepted syntax covered by differential tests
 * when either parser changes.
 */
typedef struct
{
        XtpTerminalControlState state;
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
        XtpTerminalControlString string_kind;
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
} XtpTerminalControlObserver;

void XtpTerminalControlObserverFeed(XtpTerminalControlObserver *observer, const uint8_t *bytes,
                                    size_t length,
                                    const XtpTerminalControlObserverEffects *effects);
void XtpTerminalControlObserverSetRequestsEnabled(XtpTerminalControlObserver *observer,
                                                  bool enabled);
bool XtpCursorBlinkEffective(XtpCursorBlinkPolicy policy, bool xor_policy, bool requested);

#endif
