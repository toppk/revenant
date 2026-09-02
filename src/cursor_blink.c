#include "cursor_blink.h"

#include <limits.h>

#define XTP_MAX_CSI_PARAMETER 65535U
#define XTP_MAX_CSI_PARAMETERS 24U

bool
XtpCursorBlinkEffective(XtpCursorBlinkPolicy policy, bool xor_policy, bool requested)
{
        bool configured;

        if (policy == XTP_CURSOR_BLINK_ALWAYS)
                return true;
        if (policy == XTP_CURSOR_BLINK_NEVER)
                return false;
        configured = policy == XTP_CURSOR_BLINK_DEFAULT_TRUE;
        return xor_policy ? configured != requested : configured || requested;
}

static void
BeginCsi(XtpCursorBlinkObserver *observer)
{
        observer->state = XTP_CURSOR_CONTROL_CSI;
        observer->csi_private = false;
        observer->csi_invalid = false;
        observer->csi_parameter_bytes_seen = false;
        observer->csi_parameter_present = false;
        observer->csi_mode_12 = false;
        observer->csi_intermediate = 0;
        observer->csi_intermediate_count = 0;
        observer->csi_parameter = 0;
        observer->csi_first_parameter = 0;
        observer->csi_parameter_count = 0;
}

static void
BeginString(XtpCursorBlinkObserver *observer, XtpCursorControlString kind)
{
        observer->state = XTP_CURSOR_CONTROL_STRING;
        observer->string_kind = kind;
}

static void
BeginDcs(XtpCursorBlinkObserver *observer)
{
        observer->state = XTP_CURSOR_CONTROL_DCS_HEADER;
        observer->dcs_intermediate_seen = false;
        observer->dcs_parameter_seen = false;
}

static void
BeforeChange(const XtpCursorBlinkObserverEffects *effects, size_t offset)
{
        if (effects != NULL && effects->before_change != NULL)
                effects->before_change(offset, effects->closure);
}

static void
ResetBlink(XtpCursorBlinkObserver *observer, const XtpCursorBlinkObserverEffects *effects,
           size_t offset)
{
        BeforeChange(effects, offset);
        observer->blink_requested = false;
        observer->saved_blink_requested = false;
        observer->saved_blink_valid = false;
        if (effects != NULL && effects->reset != NULL)
                effects->reset(effects->closure);
}

static void
SetBlinkRequested(XtpCursorBlinkObserver *observer, bool requested,
                  const XtpCursorBlinkObserverEffects *effects, size_t offset)
{
        if (observer->blink_requested == requested)
                return;
        BeforeChange(effects, offset);
        observer->blink_requested = requested;
}

static void
FinishCsiParameter(XtpCursorBlinkObserver *observer, bool include_empty, bool separator)
{
        if (!observer->csi_parameter_present && !include_empty)
                return;
        if (observer->csi_parameter_count >= XTP_MAX_CSI_PARAMETERS) {
                observer->csi_invalid = true;
                observer->csi_parameter_present = false;
                observer->csi_parameter = 0;
                return;
        }
        if (observer->csi_parameter_count == 0U)
                observer->csi_first_parameter = observer->csi_parameter;
        ++observer->csi_parameter_count;
        if (observer->csi_parameter == 12U)
                observer->csi_mode_12 = true;
        if (separator && observer->csi_parameter_count >= XTP_MAX_CSI_PARAMETERS)
                observer->csi_invalid = true;
        observer->csi_parameter_present = false;
        observer->csi_parameter = 0;
}

static void
CollectCsiByte(XtpCursorBlinkObserver *observer, uint8_t byte)
{
        if (byte >= 0x30U) {
                unsigned int digit;

                if (observer->csi_intermediate_count != 0U) {
                        observer->csi_invalid = true;
                        return;
                }
                if (byte == '?' && !observer->csi_parameter_bytes_seen) {
                        observer->csi_private = true;
                        observer->csi_parameter_bytes_seen = true;
                        return;
                }
                observer->csi_parameter_bytes_seen = true;
                if (byte == ';') {
                        FinishCsiParameter(observer, true, true);
                        return;
                }
                if (byte < '0' || byte > '9') {
                        observer->csi_invalid = true;
                        return;
                }
                observer->csi_parameter_present = true;
                digit = (unsigned int)(byte - '0');
                if (observer->csi_parameter > (XTP_MAX_CSI_PARAMETER - digit) / 10U)
                        observer->csi_parameter = XTP_MAX_CSI_PARAMETER;
                else
                        observer->csi_parameter = observer->csi_parameter * 10U + digit;
        } else {
                FinishCsiParameter(observer, false, false);
                if (observer->csi_intermediate_count == 0U)
                        observer->csi_intermediate = byte;
                if (observer->csi_intermediate_count != UINT8_MAX)
                        ++observer->csi_intermediate_count;
        }
}

static void
CompleteCsi(XtpCursorBlinkObserver *observer, uint8_t final,
            const XtpCursorBlinkObserverEffects *effects, size_t offset)
{
        unsigned int style;

        FinishCsiParameter(observer, false, false);
        if (observer->csi_invalid)
                return;
        if (final == 'q' && !observer->csi_private && observer->csi_intermediate_count == 1U &&
            observer->csi_intermediate == ' ' && observer->csi_parameter_count <= 1U) {
                style = observer->csi_parameter_count == 0U ? 0U : observer->csi_first_parameter;
                if (observer->ignore_requests)
                        return;
                switch (style) {
                case 0:
                case 1:
                case 3:
                case 5:
                        SetBlinkRequested(observer, true, effects, offset);
                        break;
                case 2:
                case 4:
                case 6:
                        SetBlinkRequested(observer, false, effects, offset);
                        break;
                default:
                        break;
                }
        } else if (!observer->ignore_requests && observer->csi_private &&
                   observer->csi_intermediate_count == 0U && observer->csi_mode_12) {
                if (final == 'h' || final == 'l') {
                        SetBlinkRequested(observer, final == 'h', effects, offset);
                } else if (final == 's') {
                        observer->saved_blink_requested = observer->blink_requested;
                        observer->saved_blink_valid = true;
                } else if (final == 'r' && observer->saved_blink_valid) {
                        SetBlinkRequested(observer, observer->saved_blink_requested, effects,
                                          offset);
                }
        } else if (final == 'p' && !observer->csi_private &&
                   observer->csi_intermediate_count == 1U && observer->csi_intermediate == '!' &&
                   observer->csi_parameter_count == 0U) {
                ResetBlink(observer, effects, offset);
        }
}

static bool
HandleAnywhere(XtpCursorBlinkObserver *observer, uint8_t byte)
{
        bool raw_high_string = observer->state == XTP_CURSOR_CONTROL_STRING &&
                               (observer->string_kind == XTP_CURSOR_STRING_DCS ||
                                observer->string_kind == XTP_CURSOR_STRING_OSC);

        if (byte == 0x1bU) {
                observer->state = XTP_CURSOR_CONTROL_ESCAPE;
                return true;
        }
        if (byte == 0x18U || byte == 0x1aU) {
                observer->state = XTP_CURSOR_CONTROL_GROUND;
                return true;
        }
        if (raw_high_string && byte >= 0x80U)
                return false;
        if (byte == 0x9bU) {
                BeginCsi(observer);
                return true;
        }
        if (byte == 0x90U) {
                BeginDcs(observer);
                return true;
        }
        if (byte == 0x9dU) {
                BeginString(observer, XTP_CURSOR_STRING_OSC);
                return true;
        }
        if (byte == 0x98U || byte == 0x9eU || byte == 0x9fU) {
                BeginString(observer, XTP_CURSOR_STRING_APC);
                return true;
        }
        if (byte == 0x9cU || (byte >= 0x80U && byte <= 0x8fU) || (byte >= 0x91U && byte <= 0x97U) ||
            byte == 0x99U || byte == 0x9aU) {
                observer->state = XTP_CURSOR_CONTROL_GROUND;
                return true;
        }
        return false;
}

void
XtpCursorBlinkObserverFeed(XtpCursorBlinkObserver *observer, const uint8_t *bytes, size_t length,
                           const XtpCursorBlinkObserverEffects *effects)
{
        size_t item;

        if (observer == NULL || (bytes == NULL && length != 0U))
                return;
        for (item = 0; item < length; ++item) {
                uint8_t byte = bytes[item];

                if (observer->state == XTP_CURSOR_CONTROL_GROUND ||
                    observer->state == XTP_CURSOR_CONTROL_STRING) {
                        if (observer->utf8_remaining != 0U) {
                                if (byte >= 0x80U && byte <= 0xbfU) {
                                        --observer->utf8_remaining;
                                        continue;
                                }
                                observer->utf8_remaining = 0U;
                        }
                        if (byte >= 0xc2U && byte <= 0xdfU) {
                                observer->utf8_remaining = 1U;
                                continue;
                        }
                        if (byte >= 0xe0U && byte <= 0xefU) {
                                observer->utf8_remaining = 2U;
                                continue;
                        }
                        if (byte >= 0xf0U && byte <= 0xf4U) {
                                observer->utf8_remaining = 3U;
                                continue;
                        }
                }
                if (HandleAnywhere(observer, byte))
                        continue;
                switch (observer->state) {
                case XTP_CURSOR_CONTROL_GROUND:
                        break;
                case XTP_CURSOR_CONTROL_ESCAPE:
                        if (byte == '[') {
                                BeginCsi(observer);
                        } else if (byte == ']') {
                                BeginString(observer, XTP_CURSOR_STRING_OSC);
                        } else if (byte == 'P') {
                                BeginDcs(observer);
                        } else if (byte == 'X' || byte == '^' || byte == '_') {
                                BeginString(observer, XTP_CURSOR_STRING_APC);
                        } else if (byte == 'c') {
                                ResetBlink(observer, effects, item);
                                observer->state = XTP_CURSOR_CONTROL_GROUND;
                        } else {
                                observer->state = XTP_CURSOR_CONTROL_GROUND;
                        }
                        break;
                case XTP_CURSOR_CONTROL_CSI:
                        if (byte >= 0x40U && byte <= 0x7eU) {
                                CompleteCsi(observer, byte, effects, item);
                                observer->state = XTP_CURSOR_CONTROL_GROUND;
                        } else if (byte >= 0x20U && byte <= 0x3fU) {
                                CollectCsiByte(observer, byte);
                        }
                        break;
                case XTP_CURSOR_CONTROL_DCS_HEADER:
                        if (byte >= 0x40U && byte <= 0x7eU) {
                                BeginString(observer, XTP_CURSOR_STRING_DCS);
                        } else if (byte >= 0x20U && byte <= 0x2fU) {
                                observer->dcs_intermediate_seen = true;
                        } else if (byte == ':' ||
                                   ((observer->dcs_intermediate_seen ||
                                     (observer->dcs_parameter_seen && byte >= 0x3cU)) &&
                                    byte >= 0x30U && byte <= 0x3fU)) {
                                BeginString(observer, XTP_CURSOR_STRING_DCS);
                        } else if (byte >= 0x30U && byte <= 0x3fU) {
                                observer->dcs_parameter_seen = true;
                        }
                        break;
                case XTP_CURSOR_CONTROL_STRING:
                        if (byte == 0x07U && observer->string_kind == XTP_CURSOR_STRING_OSC)
                                observer->state = XTP_CURSOR_CONTROL_GROUND;
                        break;
                }
        }
}

void
XtpCursorBlinkObserverSetRequestsEnabled(XtpCursorBlinkObserver *observer, bool enabled)
{
        if (observer != NULL)
                observer->ignore_requests = !enabled;
}
