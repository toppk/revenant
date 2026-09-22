#include "terminal_ghosttyP.h"

#include "device_attributes.h"
#include "diagnostics.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
        XtpTerminal *terminal;
        const uint8_t *bytes;
        size_t written;
} CursorBlinkFeed;

static void RecordPromptMark(XtpTerminal *terminal);
static void FreePromptMarks(XtpTerminal *terminal);
static void RecordCommandEnd(XtpTerminal *terminal);

static const GhosttyKey key_map[XTP_KEY_COUNT] = {
    [XTP_KEY_UNIDENTIFIED] = GHOSTTY_KEY_UNIDENTIFIED,
    [XTP_KEY_BACKQUOTE] = GHOSTTY_KEY_BACKQUOTE,
    [XTP_KEY_BACKSLASH] = GHOSTTY_KEY_BACKSLASH,
    [XTP_KEY_BRACKET_LEFT] = GHOSTTY_KEY_BRACKET_LEFT,
    [XTP_KEY_BRACKET_RIGHT] = GHOSTTY_KEY_BRACKET_RIGHT,
    [XTP_KEY_COMMA] = GHOSTTY_KEY_COMMA,
    [XTP_KEY_0] = GHOSTTY_KEY_DIGIT_0,
    [XTP_KEY_1] = GHOSTTY_KEY_DIGIT_1,
    [XTP_KEY_2] = GHOSTTY_KEY_DIGIT_2,
    [XTP_KEY_3] = GHOSTTY_KEY_DIGIT_3,
    [XTP_KEY_4] = GHOSTTY_KEY_DIGIT_4,
    [XTP_KEY_5] = GHOSTTY_KEY_DIGIT_5,
    [XTP_KEY_6] = GHOSTTY_KEY_DIGIT_6,
    [XTP_KEY_7] = GHOSTTY_KEY_DIGIT_7,
    [XTP_KEY_8] = GHOSTTY_KEY_DIGIT_8,
    [XTP_KEY_9] = GHOSTTY_KEY_DIGIT_9,
    [XTP_KEY_EQUAL] = GHOSTTY_KEY_EQUAL,
    [XTP_KEY_A] = GHOSTTY_KEY_A,
    [XTP_KEY_B] = GHOSTTY_KEY_B,
    [XTP_KEY_C] = GHOSTTY_KEY_C,
    [XTP_KEY_D] = GHOSTTY_KEY_D,
    [XTP_KEY_E] = GHOSTTY_KEY_E,
    [XTP_KEY_F] = GHOSTTY_KEY_F,
    [XTP_KEY_G] = GHOSTTY_KEY_G,
    [XTP_KEY_H] = GHOSTTY_KEY_H,
    [XTP_KEY_I] = GHOSTTY_KEY_I,
    [XTP_KEY_J] = GHOSTTY_KEY_J,
    [XTP_KEY_K] = GHOSTTY_KEY_K,
    [XTP_KEY_L] = GHOSTTY_KEY_L,
    [XTP_KEY_M] = GHOSTTY_KEY_M,
    [XTP_KEY_N] = GHOSTTY_KEY_N,
    [XTP_KEY_O] = GHOSTTY_KEY_O,
    [XTP_KEY_P] = GHOSTTY_KEY_P,
    [XTP_KEY_Q] = GHOSTTY_KEY_Q,
    [XTP_KEY_R] = GHOSTTY_KEY_R,
    [XTP_KEY_S] = GHOSTTY_KEY_S,
    [XTP_KEY_T] = GHOSTTY_KEY_T,
    [XTP_KEY_U] = GHOSTTY_KEY_U,
    [XTP_KEY_V] = GHOSTTY_KEY_V,
    [XTP_KEY_W] = GHOSTTY_KEY_W,
    [XTP_KEY_X] = GHOSTTY_KEY_X,
    [XTP_KEY_Y] = GHOSTTY_KEY_Y,
    [XTP_KEY_Z] = GHOSTTY_KEY_Z,
    [XTP_KEY_MINUS] = GHOSTTY_KEY_MINUS,
    [XTP_KEY_PERIOD] = GHOSTTY_KEY_PERIOD,
    [XTP_KEY_QUOTE] = GHOSTTY_KEY_QUOTE,
    [XTP_KEY_SEMICOLON] = GHOSTTY_KEY_SEMICOLON,
    [XTP_KEY_SLASH] = GHOSTTY_KEY_SLASH,
    [XTP_KEY_ALT_LEFT] = GHOSTTY_KEY_ALT_LEFT,
    [XTP_KEY_ALT_RIGHT] = GHOSTTY_KEY_ALT_RIGHT,
    [XTP_KEY_BACKSPACE] = GHOSTTY_KEY_BACKSPACE,
    [XTP_KEY_CAPS_LOCK] = GHOSTTY_KEY_CAPS_LOCK,
    [XTP_KEY_CONTEXT_MENU] = GHOSTTY_KEY_CONTEXT_MENU,
    [XTP_KEY_CONTROL_LEFT] = GHOSTTY_KEY_CONTROL_LEFT,
    [XTP_KEY_CONTROL_RIGHT] = GHOSTTY_KEY_CONTROL_RIGHT,
    [XTP_KEY_ENTER] = GHOSTTY_KEY_ENTER,
    [XTP_KEY_META_LEFT] = GHOSTTY_KEY_META_LEFT,
    [XTP_KEY_META_RIGHT] = GHOSTTY_KEY_META_RIGHT,
    [XTP_KEY_SHIFT_LEFT] = GHOSTTY_KEY_SHIFT_LEFT,
    [XTP_KEY_SHIFT_RIGHT] = GHOSTTY_KEY_SHIFT_RIGHT,
    [XTP_KEY_SPACE] = GHOSTTY_KEY_SPACE,
    [XTP_KEY_TAB] = GHOSTTY_KEY_TAB,
    [XTP_KEY_DELETE] = GHOSTTY_KEY_DELETE,
    [XTP_KEY_END] = GHOSTTY_KEY_END,
    [XTP_KEY_HELP] = GHOSTTY_KEY_HELP,
    [XTP_KEY_HOME] = GHOSTTY_KEY_HOME,
    [XTP_KEY_INSERT] = GHOSTTY_KEY_INSERT,
    [XTP_KEY_PAGE_DOWN] = GHOSTTY_KEY_PAGE_DOWN,
    [XTP_KEY_PAGE_UP] = GHOSTTY_KEY_PAGE_UP,
    [XTP_KEY_ARROW_DOWN] = GHOSTTY_KEY_ARROW_DOWN,
    [XTP_KEY_ARROW_LEFT] = GHOSTTY_KEY_ARROW_LEFT,
    [XTP_KEY_ARROW_RIGHT] = GHOSTTY_KEY_ARROW_RIGHT,
    [XTP_KEY_ARROW_UP] = GHOSTTY_KEY_ARROW_UP,
    [XTP_KEY_NUM_LOCK] = GHOSTTY_KEY_NUM_LOCK,
    [XTP_KEY_NUMPAD_0] = GHOSTTY_KEY_NUMPAD_0,
    [XTP_KEY_NUMPAD_1] = GHOSTTY_KEY_NUMPAD_1,
    [XTP_KEY_NUMPAD_2] = GHOSTTY_KEY_NUMPAD_2,
    [XTP_KEY_NUMPAD_3] = GHOSTTY_KEY_NUMPAD_3,
    [XTP_KEY_NUMPAD_4] = GHOSTTY_KEY_NUMPAD_4,
    [XTP_KEY_NUMPAD_5] = GHOSTTY_KEY_NUMPAD_5,
    [XTP_KEY_NUMPAD_6] = GHOSTTY_KEY_NUMPAD_6,
    [XTP_KEY_NUMPAD_7] = GHOSTTY_KEY_NUMPAD_7,
    [XTP_KEY_NUMPAD_8] = GHOSTTY_KEY_NUMPAD_8,
    [XTP_KEY_NUMPAD_9] = GHOSTTY_KEY_NUMPAD_9,
    [XTP_KEY_NUMPAD_ADD] = GHOSTTY_KEY_NUMPAD_ADD,
    [XTP_KEY_NUMPAD_DECIMAL] = GHOSTTY_KEY_NUMPAD_DECIMAL,
    [XTP_KEY_NUMPAD_DIVIDE] = GHOSTTY_KEY_NUMPAD_DIVIDE,
    [XTP_KEY_NUMPAD_ENTER] = GHOSTTY_KEY_NUMPAD_ENTER,
    [XTP_KEY_NUMPAD_EQUAL] = GHOSTTY_KEY_NUMPAD_EQUAL,
    [XTP_KEY_NUMPAD_MULTIPLY] = GHOSTTY_KEY_NUMPAD_MULTIPLY,
    [XTP_KEY_NUMPAD_SEPARATOR] = GHOSTTY_KEY_NUMPAD_SEPARATOR,
    [XTP_KEY_NUMPAD_SUBTRACT] = GHOSTTY_KEY_NUMPAD_SUBTRACT,
    [XTP_KEY_ESCAPE] = GHOSTTY_KEY_ESCAPE,
    [XTP_KEY_F1] = GHOSTTY_KEY_F1,
    [XTP_KEY_F2] = GHOSTTY_KEY_F2,
    [XTP_KEY_F3] = GHOSTTY_KEY_F3,
    [XTP_KEY_F4] = GHOSTTY_KEY_F4,
    [XTP_KEY_F5] = GHOSTTY_KEY_F5,
    [XTP_KEY_F6] = GHOSTTY_KEY_F6,
    [XTP_KEY_F7] = GHOSTTY_KEY_F7,
    [XTP_KEY_F8] = GHOSTTY_KEY_F8,
    [XTP_KEY_F9] = GHOSTTY_KEY_F9,
    [XTP_KEY_F10] = GHOSTTY_KEY_F10,
    [XTP_KEY_F11] = GHOSTTY_KEY_F11,
    [XTP_KEY_F12] = GHOSTTY_KEY_F12,
    [XTP_KEY_F13] = GHOSTTY_KEY_F13,
    [XTP_KEY_F14] = GHOSTTY_KEY_F14,
    [XTP_KEY_F15] = GHOSTTY_KEY_F15,
    [XTP_KEY_F16] = GHOSTTY_KEY_F16,
    [XTP_KEY_F17] = GHOSTTY_KEY_F17,
    [XTP_KEY_F18] = GHOSTTY_KEY_F18,
    [XTP_KEY_F19] = GHOSTTY_KEY_F19,
    [XTP_KEY_F20] = GHOSTTY_KEY_F20,
    [XTP_KEY_F21] = GHOSTTY_KEY_F21,
    [XTP_KEY_F22] = GHOSTTY_KEY_F22,
    [XTP_KEY_F23] = GHOSTTY_KEY_F23,
    [XTP_KEY_F24] = GHOSTTY_KEY_F24,
    [XTP_KEY_F25] = GHOSTTY_KEY_F25,
    [XTP_KEY_PRINT_SCREEN] = GHOSTTY_KEY_PRINT_SCREEN,
    [XTP_KEY_SCROLL_LOCK] = GHOSTTY_KEY_SCROLL_LOCK,
    [XTP_KEY_PAUSE] = GHOSTTY_KEY_PAUSE,
};

static const GhosttyMouseButton mouse_button_map[XTP_MOUSE_BUTTON_COUNT] = {
    [XTP_MOUSE_BUTTON_NONE] = GHOSTTY_MOUSE_BUTTON_UNKNOWN,
    [XTP_MOUSE_BUTTON_LEFT] = GHOSTTY_MOUSE_BUTTON_LEFT,
    [XTP_MOUSE_BUTTON_MIDDLE] = GHOSTTY_MOUSE_BUTTON_MIDDLE,
    [XTP_MOUSE_BUTTON_RIGHT] = GHOSTTY_MOUSE_BUTTON_RIGHT,
    [XTP_MOUSE_BUTTON_FOUR] = GHOSTTY_MOUSE_BUTTON_FOUR,
    [XTP_MOUSE_BUTTON_FIVE] = GHOSTTY_MOUSE_BUTTON_FIVE,
    [XTP_MOUSE_BUTTON_SIX] = GHOSTTY_MOUSE_BUTTON_SIX,
    [XTP_MOUSE_BUTTON_SEVEN] = GHOSTTY_MOUSE_BUTTON_SEVEN,
    [XTP_MOUSE_BUTTON_EIGHT] = GHOSTTY_MOUSE_BUTTON_EIGHT,
    [XTP_MOUSE_BUTTON_NINE] = GHOSTTY_MOUSE_BUTTON_NINE,
    [XTP_MOUSE_BUTTON_TEN] = GHOSTTY_MOUSE_BUTTON_TEN,
    [XTP_MOUSE_BUTTON_ELEVEN] = GHOSTTY_MOUSE_BUTTON_ELEVEN,
};

static GhosttyMods
ConvertModifiers(unsigned int modifiers)
{
        GhosttyMods result = 0;

        if ((modifiers & XTP_MOD_SHIFT) != 0)
                result |= GHOSTTY_MODS_SHIFT;
        if ((modifiers & XTP_MOD_CONTROL) != 0)
                result |= GHOSTTY_MODS_CTRL;
        if ((modifiers & XTP_MOD_ALT) != 0)
                result |= GHOSTTY_MODS_ALT;
        if ((modifiers & XTP_MOD_SUPER) != 0)
                result |= GHOSTTY_MODS_SUPER;
        if ((modifiers & XTP_MOD_CAPS_LOCK) != 0)
                result |= GHOSTTY_MODS_CAPS_LOCK;
        if ((modifiers & XTP_MOD_NUM_LOCK) != 0)
                result |= GHOSTTY_MODS_NUM_LOCK;
        return result;
}

static GhosttyMode
ConvertMode(XtpTerminalMode mode)
{
        switch (mode) {
        case XTP_TERMINAL_MODE_BACKARROW_KEY:
                return GHOSTTY_MODE_BACKARROW_KEY_MODE;
        case XTP_TERMINAL_MODE_NUMLOCK_KEYPAD:
                return GHOSTTY_MODE_NUMLOCK_KEYPAD;
        case XTP_TERMINAL_MODE_ALT_SENDS_ESCAPE:
                return GHOSTTY_MODE_ALT_SENDS_ESC;
        case XTP_TERMINAL_MODE_META_SENDS_ESCAPE:
                return GHOSTTY_MODE_ALT_ESC_PREFIX;
        case XTP_TERMINAL_MODE_AUTOWRAP:
                return GHOSTTY_MODE_WRAPAROUND;
        case XTP_TERMINAL_MODE_REVERSE_WRAP:
                return GHOSTTY_MODE_REVERSE_WRAP;
        case XTP_TERMINAL_MODE_AUTOLINEFEED:
                return GHOSTTY_MODE_LINEFEED;
        case XTP_TERMINAL_MODE_APPLICATION_CURSOR:
                return GHOSTTY_MODE_DECCKM;
        case XTP_TERMINAL_MODE_APPLICATION_KEYPAD:
                return GHOSTTY_MODE_KEYPAD_KEYS;
        case XTP_TERMINAL_MODE_ALLOW_132:
                return GHOSTTY_MODE_ENABLE_MODE_3;
        case XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT:
                return GHOSTTY_MODE_SYNC_OUTPUT;
        case XTP_TERMINAL_MODE_CURSOR_BLINKING:
                return GHOSTTY_MODE_CURSOR_BLINKING;
        case XTP_TERMINAL_MODE_COUNT:
                break;
        }
        return ghostty_mode_new(0, false);
}

/*
 * TODO(libghostty): remove this rewrite with cursor_blink.c once libghostty
 * exposes the raw mode-12 operand and uses it in DECRQM/DECRQSS replies.  See
 * the XtpCursorBlinkObserver rationale in cursor_blink.h.
 */
static const uint8_t *
RewriteCursorBlinkReport(const XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                         uint8_t rewritten[10])
{
        if (length == 9U && memcmp(bytes, "\033[?12;", 6) == 0 && bytes[7] == '$' &&
            bytes[8] == 'y') {
                memcpy(rewritten, bytes, length);
                rewritten[6] = terminal->cursor_blink.blink_requested ? '1' : '2';
                return rewritten;
        }
        if (length == 10U && memcmp(bytes, "\033P1$r", 5) == 0 && bytes[6] == ' ' &&
            bytes[7] == 'q' && bytes[8] == 0x1bU && bytes[9] == '\\' && bytes[5] >= '1' &&
            bytes[5] <= '6') {
                memcpy(rewritten, bytes, length);
                if (terminal->cursor_blink.blink_requested) {
                        if ((rewritten[5] & 1U) == 0U)
                                --rewritten[5];
                } else if ((rewritten[5] & 1U) != 0U) {
                        ++rewritten[5];
                }
                return rewritten;
        }
        return bytes;
}

static bool FilterColorReplies(XtpTerminal *terminal, const uint8_t *bytes, size_t length);

static void
WritePtyEffect(GhosttyTerminal handle, void *userdata, const uint8_t *bytes, size_t length)
{
        XtpTerminal *terminal = userdata;
        uint8_t rewritten[10];
        const uint8_t *output;

        (void)handle;
        /* These are complete XTGETTCAP replies generated by libghostty, not
         * application input. Match only their DCS prefix, without a VT parser. */
        if (length >= 5U && bytes[0] == 0x1b && bytes[1] == 'P' &&
            (bytes[2] == '0' || bytes[2] == '1') && bytes[3] == '+' && bytes[4] == 'r' &&
            !XtpTcapOpAllowed(terminal->allow_tcap_ops, &terminal->tcap_ops, XTP_TCAP_OP_GET)) {
                XtpLog(XTP_LOG_INFO, "terminal", "XTGETTCAP reply denied by Tcap Ops");
                return;
        }
        if (FilterColorReplies(terminal, bytes, length))
                return;
        output = RewriteCursorBlinkReport(terminal, bytes, length, rewritten);
        if (terminal->effects.write_pty != NULL) {
                XtpLog(XTP_LOG_DEBUG, "terminal", "generated PTY response bytes=%zu", length);
                terminal->effects.write_pty(output, length, terminal->effects.closure);
        }
}

static void
BellEffect(GhosttyTerminal handle, void *userdata)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        if (terminal->effects.bell != NULL) {
                XtpLog(XTP_LOG_INFO, "terminal", "BEL effect");
                terminal->effects.bell(terminal->effects.closure);
        }
}

static void
TitleEffect(GhosttyTerminal handle, void *userdata)
{
        XtpTerminal *terminal = userdata;
        GhosttyString title = {0};

        if (terminal->effects.title_changed != NULL &&
            ghostty_terminal_get(handle, GHOSTTY_TERMINAL_DATA_TITLE, &title) == GHOSTTY_SUCCESS) {
                terminal->effects.title_changed((const char *)title.ptr, title.len,
                                                terminal->effects.closure);
                XtpLog(XTP_LOG_INFO, "terminal", "title effect bytes=%zu", title.len);
        }
}

static void
WorkingDirectoryEffect(GhosttyTerminal handle, void *userdata)
{
        XtpTerminal *terminal = userdata;
        GhosttyString value = {0};

        ++terminal->pwd_reports_delivered;
        if (terminal->effects.working_directory_changed != NULL &&
            ghostty_terminal_get(handle, GHOSTTY_TERMINAL_DATA_PWD, &value) == GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_INFO, "terminal", "working directory effect bytes=%zu", value.len);
                terminal->effects.working_directory_changed(value.ptr, value.len,
                                                            terminal->effects.closure);
        }
}

static const void *
WorkingDirectoryEffectPointer(void)
{
        GhosttyTerminalPwdChangedFn function = WorkingDirectoryEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static bool
SizeEffect(GhosttyTerminal handle, void *userdata, GhosttySizeReportSize *size)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        if (terminal == NULL || size == NULL)
                return false;
        size->columns = terminal->geometry_columns;
        size->rows = terminal->geometry_rows;
        size->cell_width = terminal->geometry_cell_width;
        size->cell_height = terminal->geometry_cell_height;
        return true;
}

static GhosttyString
XtversionEffect(GhosttyTerminal handle, void *userdata)
{
        static const uint8_t version[] = XTP_PROGRAM_NAME "(" XTP_VERSION ")";

        (void)handle;
        (void)userdata;
        return (GhosttyString){.ptr = version, .len = sizeof(version) - 1U};
}

/* The answerback is user-configured text, not a terminal reply: send it
 * straight through the host PTY effect so the reply filters (mode-12 rewrite,
 * Color Ops, Tcap Ops) leave it alone and libghostty's reply buffer does not
 * bound its length. The empty result keeps libghostty from sending anything. */
static GhosttyString
EnquiryEffect(GhosttyTerminal handle, void *userdata)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        if (terminal->answerback_length != 0 && terminal->effects.write_pty != NULL) {
                XtpLog(XTP_LOG_INFO, "terminal", "ENQ answered bytes=%zu",
                       terminal->answerback_length);
                terminal->effects.write_pty((const uint8_t *)terminal->answerback,
                                            terminal->answerback_length, terminal->effects.closure);
        }
        return (GhosttyString){.ptr = NULL, .len = 0};
}

static bool
DeviceAttributesEffect(GhosttyTerminal handle, void *userdata, GhosttyDeviceAttributes *attributes)
{
        size_t count;
        const uint16_t *features = XtpDeviceAttributesFeatures(&count);
        size_t index;

        (void)handle;
        (void)userdata;
        if (attributes == NULL)
                return false;
        memset(attributes, 0, sizeof(*attributes));
        attributes->primary.conformance_level = XTP_DA1_CONFORMANCE_LEVEL;
        if (count > sizeof(attributes->primary.features) / sizeof(attributes->primary.features[0]))
                count =
                    sizeof(attributes->primary.features) / sizeof(attributes->primary.features[0]);
        for (index = 0; index < count; ++index)
                attributes->primary.features[index] = features[index];
        attributes->primary.num_features = count;
        attributes->secondary.device_type = XTP_DA2_DEVICE_TYPE;
        attributes->secondary.firmware_version = (uint16_t)XtpDeviceAttributesFirmware(XTP_VERSION);
        attributes->secondary.rom_cartridge = 0;
        attributes->tertiary.unit_id = XTP_DA3_UNIT_ID;
        return true;
}

static const void *
DeviceAttributesEffectPointer(void)
{
        GhosttyTerminalDeviceAttributesFn function = DeviceAttributesEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static void
UnknownSequenceEffect(GhosttyTerminal handle, void *userdata,
                      const GhosttyTerminalUnknownSequence *sequence)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        if (terminal == NULL || sequence == NULL || terminal->effects.unknown_apc == NULL ||
            sequence->tag != GHOSTTY_TERMINAL_UNKNOWN_SEQUENCE_APC)
                return;
        terminal->effects.unknown_apc(sequence->value.apc.content.ptr,
                                      sequence->value.apc.content.len,
                                      sequence->value.apc.truncated, terminal->effects.closure);
}

static const void *
UnknownSequenceEffectPointer(void)
{
        GhosttyTerminalUnknownSequenceFn function = UnknownSequenceEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static void
DesktopNotificationEffect(GhosttyTerminal handle, void *userdata,
                          const GhosttyTerminalDesktopNotification *notification)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        if (terminal == NULL || notification == NULL || terminal->effects.notification == NULL ||
            notification->size < sizeof(*notification))
                return;
        terminal->effects.notification(notification->title.ptr, notification->title.len,
                                       notification->body.ptr, notification->body.len,
                                       terminal->effects.closure);
}

static const void *
DesktopNotificationEffectPointer(void)
{
        GhosttyTerminalDesktopNotificationFn function = DesktopNotificationEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static void
ProgressReportEffect(GhosttyTerminal handle, void *userdata,
                     const GhosttyTerminalProgressReport *report)
{
        XtpTerminal *terminal = userdata;
        XtpProgressState state;
        int percent;

        (void)handle;
        if (terminal == NULL || report == NULL || terminal->effects.progress == NULL ||
            report->size < sizeof(*report))
                return;
        switch (report->state) {
        case GHOSTTY_TERMINAL_PROGRESS_STATE_REMOVE:
                state = XTP_PROGRESS_REMOVE;
                break;
        case GHOSTTY_TERMINAL_PROGRESS_STATE_SET:
                state = XTP_PROGRESS_SET;
                break;
        case GHOSTTY_TERMINAL_PROGRESS_STATE_ERROR:
                state = XTP_PROGRESS_ERROR;
                break;
        case GHOSTTY_TERMINAL_PROGRESS_STATE_INDETERMINATE:
                state = XTP_PROGRESS_INDETERMINATE;
                break;
        case GHOSTTY_TERMINAL_PROGRESS_STATE_PAUSE:
                state = XTP_PROGRESS_PAUSE;
                break;
        case GHOSTTY_TERMINAL_PROGRESS_STATE_MAX_VALUE:
        default:
                return;
        }
        percent = report->progress;
        if (percent > 100)
                percent = 100;
        if (percent < -1)
                percent = -1;
        terminal->effects.progress(state, percent, terminal->effects.closure);
}

static const void *
ProgressReportEffectPointer(void)
{
        GhosttyTerminalProgressReportFn function = ProgressReportEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

/* Presentation state the live terminal would paint with now. Reading terminal data
 * is permitted inside libghostty's effect callbacks, and the cursor-blink observer
 * flushes bytes to libghostty before each change it tracks, so both are in step with
 * the parser at a hold boundary. */
static int
ReadLivePresentation(XtpTerminal *terminal, XtpGhosttyPresentation *presentation)
{
        GhosttyTerminalModeConfig reverse = {GHOSTTY_MODE_REVERSE_COLORS, false};

        memset(presentation, 0, sizeof(*presentation));
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &reverse) !=
            GHOSTTY_SUCCESS)
                return -1;
        presentation->reverse_colors = reverse.value;
        presentation->colors_valid =
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND,
                                 &presentation->foreground) == GHOSTTY_SUCCESS &&
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND,
                                 &presentation->background) == GHOSTTY_SUCCESS;
        if (presentation->colors_valid &&
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLOR_CURSOR,
                                 &presentation->cursor) != GHOSTTY_SUCCESS)
                presentation->cursor = presentation->foreground;
        presentation->blink_requested = terminal->cursor_blink.blink_requested;
        return 0;
}

/* Cursor metadata from the render state, which is the captured one during a hold. */
static int
ReadCursorMeta(XtpTerminal *terminal, XtpGhosttyFrameMeta *meta)
{
        bool in_viewport = false;
        bool wide_tail = false;

        meta->cursor_column = 0;
        meta->cursor_row = 0;
        if (ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,
                                     &meta->cursor_visible) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE,
                                     &meta->cursor_style) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,
                                     &in_viewport) != GHOSTTY_SUCCESS)
                return -1;
        meta->cursor_visible = meta->cursor_visible && in_viewport;
        if (!meta->cursor_visible)
                return 0;
        if (ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,
                                     &meta->cursor_column) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,
                                     &meta->cursor_row) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_WIDE_TAIL,
                                     &wide_tail) != GHOSTTY_SUCCESS)
                return -1;
        if (wide_tail && meta->cursor_column != 0)
                --meta->cursor_column;
        return 0;
}

static bool
SameColor(GhosttyColorRgb a, GhosttyColorRgb b)
{
        return a.r == b.r && a.g == b.g && a.b == b.b;
}

static bool
SameFrameMeta(const XtpGhosttyFrameMeta *a, const XtpGhosttyFrameMeta *b)
{
        const XtpGhosttyPresentation *pa = &a->presentation;
        const XtpGhosttyPresentation *pb = &b->presentation;

        return a->cursor_visible == b->cursor_visible && a->cursor_style == b->cursor_style &&
               (!a->cursor_visible ||
                (a->cursor_column == b->cursor_column && a->cursor_row == b->cursor_row)) &&
               pa->reverse_colors == pb->reverse_colors && pa->colors_valid == pb->colors_valid &&
               pa->blink_requested == pb->blink_requested &&
               (!pa->colors_valid ||
                (SameColor(pa->foreground, pb->foreground) &&
                 SameColor(pa->background, pb->background) && SameColor(pa->cursor, pb->cursor)));
}

/* Enter or leave a hold. Entering captures the render state now, which is the frame
 * the application completed before the hold: libghostty reports the hold before it
 * parses anything after it, even later in the same write. Updating the render state
 * is the operation libghostty permits inside this callback. */
static void
SetRenderHeld(XtpTerminal *terminal, bool held)
{
        GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FALSE;

        if (terminal->render_held == held)
                return;
        if (held && ghostty_render_state_update(terminal->render_state, terminal->handle) !=
                        GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot capture the frame at a render hold");
                return;
        }
        if (held) {
                XtpGhosttyFrameMeta captured;

                /* Everything the held frame is painted with is fixed here, not only its
                 * cells: settings parsed during the hold must stay hidden too. */
                if (ReadLivePresentation(terminal, &terminal->held_presentation) != 0)
                        terminal->held_presentation.colors_valid = false;
                captured.presentation = terminal->held_presentation;
                /* The capture owes a paint when its cells, cursor or presentation differ
                 * from the last drawn frame; an unchanged frame owes none. */
                if (ghostty_render_state_get(terminal->render_state,
                                             GHOSTTY_RENDER_STATE_DATA_DIRTY,
                                             &dirty) != GHOSTTY_SUCCESS ||
                    ReadCursorMeta(terminal, &captured) != 0 || !terminal->drawn_valid ||
                    !SameFrameMeta(&captured, &terminal->drawn)) {
                        if (dirty == GHOSTTY_RENDER_STATE_DIRTY_FALSE)
                                dirty = GHOSTTY_RENDER_STATE_DIRTY_PARTIAL;
                }
        }
        terminal->render_held = held;
        XtpLog(XTP_LOG_DEBUG, "render", "render hold %s",
               !held                                       ? "ended"
               : dirty != GHOSTTY_RENDER_STATE_DIRTY_FALSE ? "began; captured frame differs"
                                                           : "began; captured frame is unchanged");
        if (terminal->render_hold != NULL)
                terminal->render_hold(terminal->render_hold_closure, held,
                                      held && dirty != GHOSTTY_RENDER_STATE_DIRTY_FALSE);
}

static void
RenderHoldEffect(GhosttyTerminal handle, void *userdata, bool held)
{
        (void)handle;
        SetRenderHeld(userdata, held);
}

static const void *
RenderHoldEffectPointer(void)
{
        GhosttyTerminalRenderHoldFn function = RenderHoldEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
EnquiryEffectPointer(void)
{
        GhosttyTerminalEnquiryFn function = EnquiryEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

int
XtpTerminalSetTerminfoName(XtpTerminal *terminal, const char *name)
{
        GhosttyString value = {.ptr = (const uint8_t *)name,
                               .len = name != NULL ? strlen(name) : 0};

        if (terminal == NULL)
                return -1;
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_TERMINFO_NAME,
                                 name != NULL ? &value : NULL) != GHOSTTY_SUCCESS) {
                /* A rejected name must not leave an earlier one answering TN. */
                (void)ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_TERMINFO_NAME,
                                           NULL);
                XtpLog(XTP_LOG_WARNING, "terminal",
                       "terminfo name rejected bytes=%zu; TN unanswered", value.len);
                return -1;
        }
        XtpLog(XTP_LOG_INFO, "terminal", "terminfo name=%s", name != NULL ? name : "(unset)");
        return 0;
}

int
XtpTerminalSetAnswerback(XtpTerminal *terminal, const char *answerback)
{
        char *copy = NULL;

        if (terminal == NULL)
                return -1;
        if (answerback != NULL && *answerback != '\0') {
                copy = strdup(answerback);
                if (copy == NULL)
                        return -1;
        }
        free(terminal->answerback);
        terminal->answerback = copy;
        terminal->answerback_length = copy != NULL ? strlen(copy) : 0U;
        XtpLog(XTP_LOG_INFO, "terminal", "answerbackString bytes=%zu", terminal->answerback_length);
        return 0;
}

static XtpClipboardTarget
ConvertClipboardLocation(GhosttyClipboardLocation location)
{
        switch (location) {
        case GHOSTTY_CLIPBOARD_LOCATION_SELECTION:
                return XTP_CLIPBOARD_TARGET_SELECT;
        case GHOSTTY_CLIPBOARD_LOCATION_PRIMARY:
                return XTP_CLIPBOARD_TARGET_PRIMARY;
        case GHOSTTY_CLIPBOARD_LOCATION_STANDARD:
        case GHOSTTY_CLIPBOARD_LOCATION_MAX_VALUE:
        default:
                break;
        }
        return XTP_CLIPBOARD_TARGET_CLIPBOARD;
}

static bool
TextMime(GhosttyString mime)
{
        static const char *const exact[] = {"UTF8_STRING", "TEXT", "STRING"};
        size_t index;

        if (mime.len >= 10 && memcmp(mime.ptr, "text/plain", 10) == 0)
                return true;
        for (index = 0; index < sizeof(exact) / sizeof(exact[0]); ++index) {
                if (mime.len == strlen(exact[index]) &&
                    memcmp(mime.ptr, exact[index], mime.len) == 0)
                        return true;
        }
        return false;
}

static void
ClipboardWriteEffect(GhosttyTerminal handle, void *userdata, const GhosttyClipboardWrite *write)
{
        XtpTerminal *terminal = userdata;
        GhosttyClipboardWriteReply reply = {
            .size = sizeof(reply),
            .result = GHOSTTY_CLIPBOARD_WRITE_RESULT_DENIED,
        };
        XtpClipboardResult result = XTP_CLIPBOARD_DENIED;

        (void)handle;
        if (terminal->effects.clipboard_write != NULL) {
                XtpClipboardTarget target = ConvertClipboardLocation(write->location);
                size_t index;

                if (write->contents_len == 0) {
                        result = terminal->effects.clipboard_write(target, NULL, 0, true,
                                                                   terminal->effects.closure);
                } else {
                        result = XTP_CLIPBOARD_UNSUPPORTED;
                        for (index = 0; index < write->contents_len; ++index) {
                                const GhosttyClipboardContent *content = &write->contents[index];

                                if (!TextMime(content->mime))
                                        continue;
                                result = terminal->effects.clipboard_write(
                                    target, content->data.ptr, content->data.len, false,
                                    terminal->effects.closure);
                                break;
                        }
                }
        }
        switch (result) {
        case XTP_CLIPBOARD_SUCCESS:
                reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_SUCCESS;
                break;
        case XTP_CLIPBOARD_UNSUPPORTED:
                reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_UNSUPPORTED;
                break;
        case XTP_CLIPBOARD_UNAVAILABLE:
                reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_BUSY;
                break;
        case XTP_CLIPBOARD_DENIED:
                break;
        }
        write->reply(write, &reply);
}

static void
ClipboardReadEffect(GhosttyTerminal handle, void *userdata, const GhosttyClipboardRead *read)
{
        XtpTerminal *terminal = userdata;
        GhosttyClipboardReadReply reply = {
            .size = sizeof(reply),
            .result = GHOSTTY_CLIPBOARD_READ_RESULT_UNSUPPORTED,
        };
        GhosttyClipboardContent content;
        uint8_t *bytes = NULL;
        size_t length = 0;

        (void)handle;
        if (terminal->effects.clipboard_read != NULL) {
                XtpClipboardResult result =
                    terminal->effects.clipboard_read(ConvertClipboardLocation(read->location),
                                                     &bytes, &length, terminal->effects.closure);

                switch (result) {
                case XTP_CLIPBOARD_SUCCESS:
                        content.mime =
                            (GhosttyString){.ptr = (const uint8_t *)"text/plain", .len = 10};
                        content.data = (GhosttyString){.ptr = bytes, .len = length};
                        reply.contents = &content;
                        reply.contents_len = 1;
                        reply.result = GHOSTTY_CLIPBOARD_READ_RESULT_SUCCESS;
                        break;
                case XTP_CLIPBOARD_DENIED:
                        reply.result = GHOSTTY_CLIPBOARD_READ_RESULT_DENIED;
                        break;
                case XTP_CLIPBOARD_UNAVAILABLE:
                        reply.result = GHOSTTY_CLIPBOARD_READ_RESULT_BUSY;
                        break;
                case XTP_CLIPBOARD_UNSUPPORTED:
                        break;
                }
        }
        read->reply(read, &reply);
        free(bytes);
}

static const void *
ClipboardWriteEffectPointer(void)
{
        GhosttyTerminalClipboardWriteFn function = ClipboardWriteEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
ClipboardReadEffectPointer(void)
{
        GhosttyTerminalClipboardReadFn function = ClipboardReadEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
WritePtyEffectPointer(void)
{
        /*
         * Keep each shim typed so assigning the callback checks its signature.
         * The memcpy then preserves the callback representation expected by
         * Ghostty's option API without a function-to-object-pointer cast.
         */
        GhosttyTerminalWritePtyFn function = WritePtyEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
BellEffectPointer(void)
{
        GhosttyTerminalBellFn function = BellEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
TitleEffectPointer(void)
{
        GhosttyTerminalTitleChangedFn function = TitleEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
SizeEffectPointer(void)
{
        GhosttyTerminalSizeFn function = SizeEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static const void *
XtversionEffectPointer(void)
{
        GhosttyTerminalXtversionFn function = XtversionEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

static void
FreeHandles(XtpTerminal *terminal)
{
        free(terminal->answerback);
        terminal->answerback = NULL;
        FreePromptMarks(terminal);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        ghostty_selection_gesture_event_free(terminal->selection_release);
        ghostty_selection_gesture_event_free(terminal->selection_autoscroll);
        ghostty_selection_gesture_event_free(terminal->selection_drag);
        ghostty_selection_gesture_event_free(terminal->selection_press);
        ghostty_selection_gesture_free(terminal->selection_gesture, terminal->handle);
        XtpCharClassFree(terminal->char_classes);
        ghostty_key_event_free(terminal->key_event);
        ghostty_key_encoder_free(terminal->key_encoder);
        ghostty_mouse_event_free(terminal->mouse_event);
        ghostty_mouse_encoder_free(terminal->mouse_encoder);
        ghostty_render_state_row_cells_free(terminal->cells);
        ghostty_render_state_row_iterator_free(terminal->rows);
        ghostty_render_state_free(terminal->render_state);
        ghostty_terminal_free(terminal->handle);
}

static int
SyncCursorBlinkMode(XtpTerminal *terminal)
{
        GhosttyTerminalModeConfig mode = {
            GHOSTTY_MODE_CURSOR_BLINKING,
            terminal->cursor_blink.blink_requested,
        };

        return ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_MODE, &mode) ==
                       GHOSTTY_SUCCESS
                   ? 0
                   : -1;
}

static void
CursorBlinkBeforeChange(size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;

        if (offset > feed->written) {
                ghostty_terminal_vt_write(feed->terminal->handle, feed->bytes + feed->written,
                                          offset - feed->written);
                feed->written = offset;
        }
}

static void
CursorBlinkWindowOp(unsigned int op, unsigned int parameter_count, const unsigned int *parameters,
                    size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        CursorBlinkBeforeChange(offset, closure);
        if (terminal->effects.title_op != NULL)
                terminal->effects.title_op(
                    (XtpTitleOp)op, parameter_count >= 2U ? parameters[1] : 0U,
                    parameter_count >= 3U ? parameters[2] : 0U, terminal->effects.closure);
}

static bool
ColorOpAllowed(const XtpTerminal *terminal, XtpColorOp op)
{
        return XtpColorOpAllowed(terminal->allow_color_ops, &terminal->color_ops, op);
}

static bool
SameRgb(GhosttyColorRgb left, GhosttyColorRgb right)
{
        return left.r == right.r && left.g == right.g && left.b == right.b;
}

/* TODO(libghostty): replace this filter with a public color-policy hook.
 * A denied reset is spoiled before libghostty parses it. A denied set item of
 * an OSC 10-19 list is withheld and forwarded as a query instead, which keeps
 * the list's successive selectors aligned without ever applying the color;
 * the reply it provokes, and replies to denied queries, are dropped in the
 * PTY write effect. Decisions are made as each item begins. */
static void
SpoilOsc(CursorBlinkFeed *feed, size_t offset, unsigned int selector, XtpColorOp op)
{
        static const uint8_t spoiler[] = "x";

        CursorBlinkBeforeChange(offset, feed);
        ghostty_terminal_vt_write(feed->terminal->handle, spoiler, sizeof(spoiler) - 1U);
        XtpLog(XTP_LOG_INFO, "terminal", "OSC %u denied by Color Ops policy (%s)", selector,
               XtpColorOpName(op));
}

static void
ColorOscHeader(unsigned int selector, size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        terminal->color_list_active = false;
        terminal->color_list_skipping = false;
        terminal->color_list_capturing = false;
        if (selector >= 110U && selector <= 119U) {
                if (!ColorOpAllowed(terminal, XTP_COLOR_OP_SET_COLOR))
                        SpoilOsc(feed, offset, selector, XTP_COLOR_OP_SET_COLOR);
                return;
        }
        if (!((selector >= 10U && selector <= 19U) || selector == 4U || selector == 5U))
                return;
        /* Earlier controls in this feed must answer before the filter is armed. */
        CursorBlinkBeforeChange(offset, feed);
        terminal->color_list_active = true;
        terminal->color_list_any_denied = false;
        terminal->color_list_deny_unknown_index = false;
        terminal->color_list_selector = selector;
        terminal->color_list_items = 0;
        terminal->color_list_drop_selectors = 0;
        terminal->color_list_query_count = 0;
        terminal->color_list_queries_overflow = false;
        terminal->color_list_index_invalid = true;
}

/* Accumulate the palette index item that precedes a query the way libghostty
 * reads it: ignored C0 bytes, an optional leading plus, any number of leading
 * zeros. Anything else leaves the index unknown. */
static void
CaptureIndexText(XtpTerminal *terminal, const uint8_t *bytes, size_t end)
{
        size_t position = terminal->color_list_capture_start;

        for (; position < end; ++position) {
                uint8_t byte = bytes[position];

                if (byte < 0x20U)
                        continue;
                if (byte == '+' && !terminal->color_list_index_digits &&
                    !terminal->color_list_index_invalid &&
                    position == terminal->color_list_capture_start)
                        continue;
                if (byte < '0' || byte > '9' || terminal->color_list_index_value > 100000U) {
                        terminal->color_list_index_invalid = true;
                        continue;
                }
                terminal->color_list_index_digits = true;
                terminal->color_list_index_value =
                    terminal->color_list_index_value * 10U + (unsigned int)(byte - '0');
        }
        terminal->color_list_capture_start = end;
}

static void
RecordPaletteDecision(XtpTerminal *terminal, bool denied)
{
        XtpPaletteQuery *entry;

        if (terminal->color_list_index_invalid || !terminal->color_list_index_digits) {
                if (denied)
                        terminal->color_list_deny_unknown_index = true;
                return;
        }
        if (terminal->color_list_query_count >= XTP_PALETTE_QUERY_LIMIT) {
                terminal->color_list_queries_overflow = true;
                return;
        }
        entry = &terminal->color_list_queries[terminal->color_list_query_count++];
        entry->index = terminal->color_list_index_value;
        entry->denied = denied;
        entry->consumed = false;
}

static void
ColorOscPayload(unsigned int selector, bool query, size_t offset, void *closure)
{
        static const uint8_t as_query[] = "?";
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;
        unsigned int item = terminal->color_list_items++;
        XtpColorOp op;
        bool denied;

        if (!terminal->color_list_active)
                return;
        if (selector >= 10U && selector <= 19U) {
                unsigned int target = selector + item;

                if (target > 19U)
                        return;
                op = query ? XTP_COLOR_OP_GET_COLOR : XTP_COLOR_OP_SET_COLOR;
                denied = !ColorOpAllowed(terminal, op);
                if (!denied)
                        return;
                terminal->color_list_any_denied = true;
                terminal->color_list_drop_selectors |= 1U << (target - 10U);
                if (!query) {
                        CursorBlinkBeforeChange(offset, feed);
                        ghostty_terminal_vt_write(terminal->handle, as_query,
                                                  sizeof(as_query) - 1U);
                        terminal->color_list_skipping = true;
                }
                XtpLog(XTP_LOG_INFO, "terminal", "OSC %u denied by Color Ops policy (%s)", target,
                       XtpColorOpName(op));
                return;
        }
        if (!query) {
                /* An index item: read it for the query that may follow. */
                terminal->color_list_capturing = true;
                terminal->color_list_capture_start = offset;
                terminal->color_list_index_value = 0;
                terminal->color_list_index_digits = false;
                terminal->color_list_index_invalid = false;
                return;
        }
        denied = !ColorOpAllowed(terminal, XTP_COLOR_OP_GET_ANSI_COLOR);
        if (denied)
                terminal->color_list_any_denied = true;
        RecordPaletteDecision(terminal, denied);
        terminal->color_list_index_invalid = true;
        if (denied)
                XtpLog(XTP_LOG_INFO, "terminal", "OSC %u denied by Color Ops policy (%s)", selector,
                       XtpColorOpName(XTP_COLOR_OP_GET_ANSI_COLOR));
}

static void
ColorOscItemEnd(size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        if (terminal->color_list_skipping) {
                feed->written = offset;
                terminal->color_list_skipping = false;
        }
        if (terminal->color_list_capturing) {
                CaptureIndexText(terminal, feed->bytes, offset);
                terminal->color_list_capturing = false;
        }
}

static void
ColorOscEnd(size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        if (!terminal->color_list_active)
                return;
        if (terminal->color_list_skipping) {
                feed->written = offset;
                terminal->color_list_skipping = false;
        }
        terminal->color_list_capturing = false;
        /* libghostty dispatches the list at this byte, so its replies arrive
         * while the per-item decisions are still known. */
        CursorBlinkBeforeChange(offset + 1U, feed);
        terminal->color_list_active = false;
}

static void
FeedOscHeader(unsigned int selector, size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        ColorOscHeader(selector, offset, closure);
        if (selector == 133U) {
                terminal->prompt_mark_first_item = true;
                terminal->prompt_mark_pending = false;
                terminal->command_end_active = true;
                terminal->command_end_first_item = false;
                terminal->command_end_first_done = false;
                terminal->command_end_start = offset + 1U;
                terminal->command_end_bytes = 0;
                terminal->command_end_item_bytes = 0;
        }
        if (selector == 7U) {
                /* Earlier controls in this feed must deliver their own pwd
                 * callbacks before this report's delivery is judged. */
                CursorBlinkBeforeChange(offset, feed);
                terminal->pwd_report_active = true;
                terminal->pwd_report_start = offset + 1U;
                terminal->pwd_report_bytes = 0;
                terminal->pwd_reports_before_report = terminal->pwd_reports_delivered;
        }
}

/* Only an OSC 133 whose first item starts with 'A' begins a prompt; the payload itself
 * stays with libghostty, which marks the row. */
static void
FeedOscPayload(unsigned int selector, bool query, size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        ColorOscPayload(selector, query, offset, closure);
        if (selector == 133U && terminal->prompt_mark_first_item) {
                terminal->prompt_mark_first_item = false;
                terminal->prompt_mark_pending = feed->bytes[offset] == 'A';
                terminal->command_end_first_item = feed->bytes[offset] == 'D';
                terminal->command_end_item_start = offset;
                terminal->command_end_item_bytes = 0;
        }
}

/* libghostty drops nonterminating C0 bytes from an OSC without storing them, so only bytes
 * at or above 0x20 count toward an item's length or the capture limit. */
static size_t
OscPayloadBytes(const uint8_t *bytes, size_t start, size_t end)
{
        size_t count = 0;
        size_t index;

        for (index = start; index < end; ++index)
                count += bytes[index] >= 0x20U;
        return count;
}

/* The ';' closing the first item fixes its length; only a bare "D" counts as completion. */
static void
FeedOscItemEnd(size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        ColorOscItemEnd(offset, closure);
        if (terminal->command_end_active && terminal->command_end_first_item &&
            !terminal->command_end_first_done) {
                terminal->command_end_first_done = true;
                terminal->command_end_item_bytes +=
                    OscPayloadBytes(feed->bytes, terminal->command_end_item_start, offset);
        }
}

/* True only for an OSC 133 whose first item is exactly "D", which ended with BEL or ST rather
 * than an abort, and whose payload (everything after "133;") fit the core's capture so
 * libghostty acted on it. */
static bool
CommandEndAccepted(XtpTerminal *terminal, const CursorBlinkFeed *feed, size_t offset)
{
        uint8_t terminator = feed->bytes[offset];
        size_t total = terminal->command_end_bytes;

        if (!terminal->command_end_active || !terminal->command_end_first_item)
                return false;
        if (!terminal->command_end_first_done) {
                terminal->command_end_first_done = true;
                terminal->command_end_item_bytes +=
                    OscPayloadBytes(feed->bytes, terminal->command_end_item_start, offset);
        }
        total += OscPayloadBytes(feed->bytes, terminal->command_end_start, offset);
        return terminal->command_end_item_bytes == 1U &&
               (terminator == 0x07U || terminator == 0x1bU || terminator == 0x9cU) &&
               total <= XTP_GHOSTTY_OSC_CAPTURE_LIMIT;
}

/* libghostty drops an OSC whose payload overflows its fixed capture buffer
 * without any callback. Dispatching the report here, at its terminator, shows
 * whether the pwd callback fired so a dropped report cannot leave the
 * application holding the previous directory. */
static void
FeedOscEnd(size_t offset, void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        ColorOscEnd(offset, closure);
        terminal->prompt_mark_first_item = false;
        if (terminal->prompt_mark_pending) {
                terminal->prompt_mark_pending = false;
                CursorBlinkBeforeChange(offset + 1U, feed);
                RecordPromptMark(terminal);
        }
        if (terminal->command_end_active) {
                bool accepted = CommandEndAccepted(terminal, feed, offset);

                terminal->command_end_active = false;
                if (accepted) {
                        /* Protocol seen is a property of the shell, not of the row's survival. */
                        terminal->command_end_seen = true;
                        CursorBlinkBeforeChange(offset + 1U, feed);
                        RecordCommandEnd(terminal);
                }
        }
        if (!terminal->pwd_report_active)
                return;
        terminal->pwd_report_active = false;
        if (offset > terminal->pwd_report_start)
                terminal->pwd_report_bytes += offset - terminal->pwd_report_start;
        CursorBlinkBeforeChange(offset + 1U, feed);
        if (terminal->pwd_reports_delivered != terminal->pwd_reports_before_report)
                return;
        XtpLog(XTP_LOG_WARNING, "terminal",
               "working directory report dropped by the core bytes=%zu limit=%u",
               terminal->pwd_report_bytes, XTP_GHOSTTY_OSC_CAPTURE_LIMIT);
        if (terminal->effects.working_directory_dropped != NULL)
                terminal->effects.working_directory_dropped(terminal->pwd_report_bytes,
                                                            terminal->effects.closure);
}

static size_t
ReplyLength(const uint8_t *bytes, size_t length)
{
        size_t index;

        for (index = 2; index < length; ++index) {
                if (bytes[index] == 0x07)
                        return index + 1U;
                if (bytes[index] == 0x1b)
                        return index + 1U < length && bytes[index + 1U] == '\\' ? index + 2U : 0U;
        }
        return 0;
}

static bool
ParseReplyNumber(const uint8_t *reply, size_t length, size_t *index, unsigned int *value)
{
        unsigned int number = 0;
        size_t start = *index;

        while (*index < length && reply[*index] >= '0' && reply[*index] <= '9' && number < 100000U)
                number = number * 10U + (unsigned int)(reply[(*index)++] - '0');
        if (*index == start || *index >= length || reply[*index] != ';')
                return false;
        ++*index;
        *value = number;
        return true;
}

/* Replies carry their selector and palette index, so match decisions there;
 * a denied query whose index could not be read blocks every palette reply. */
static bool
DropColorReply(XtpTerminal *terminal, const uint8_t *reply, size_t length)
{
        unsigned int selector;
        unsigned int index;
        size_t position = 2;

        if (!ParseReplyNumber(reply, length, &position, &selector))
                return terminal->color_list_any_denied;
        if (selector >= 10U && selector <= 19U)
                return (terminal->color_list_drop_selectors & (1U << (selector - 10U))) != 0;
        if (selector != 4U && selector != 5U)
                return false;
        if (!ParseReplyNumber(reply, length, &position, &index) ||
            terminal->color_list_deny_unknown_index ||
            (terminal->color_list_queries_overflow && terminal->color_list_any_denied))
                return true;
        {
                /* Replies for an index come in the order of its queries. */
                unsigned int slot;

                for (slot = 0; slot < terminal->color_list_query_count; ++slot) {
                        XtpPaletteQuery *entry = &terminal->color_list_queries[slot];

                        if (entry->consumed || entry->index != index)
                                continue;
                        entry->consumed = true;
                        return entry->denied;
                }
        }
        return terminal->color_list_any_denied;
}

/* Replies for one OSC list arrive as one write. Forward the permitted ones
 * individually; nothing here allocates, so a denied reply can never slip
 * through on failure. Unparseable output is dropped while a denial is
 * pending. */
static bool
FilterColorReplies(XtpTerminal *terminal, const uint8_t *bytes, size_t length)
{
        size_t offset = 0;
        size_t dropped = 0;

        if (!terminal->color_list_active || !terminal->color_list_any_denied)
                return false;
        while (offset < length) {
                size_t reply =
                    length - offset >= 2 && bytes[offset] == 0x1b && bytes[offset + 1U] == ']'
                        ? ReplyLength(bytes + offset, length - offset)
                        : 0U;

                if (reply == 0) {
                        XtpLog(XTP_LOG_WARNING, "terminal",
                               "dropped unrecognized output while a color denial is pending "
                               "bytes=%zu",
                               length - offset);
                        ++dropped;
                        break;
                }
                if (DropColorReply(terminal, bytes + offset, reply))
                        ++dropped;
                else if (terminal->effects.write_pty != NULL)
                        terminal->effects.write_pty(bytes + offset, reply,
                                                    terminal->effects.closure);
                offset += reply;
        }
        XtpLog(XTP_LOG_INFO, "terminal", "dropped color replies denied by policy count=%zu",
               dropped);
        return true;
}

void
XtpTerminalSetColorOpsPolicy(XtpTerminal *terminal, bool allow_color_ops, const XtpColorOps *ops)
{
        if (terminal == NULL)
                return;
        terminal->allow_color_ops = allow_color_ops;
        if (ops != NULL)
                terminal->color_ops = *ops;
        XtpLog(XTP_LOG_INFO, "terminal",
               "color-ops policy allow=%s SetColor=%s GetColor=%s GetAnsiColor=%s",
               allow_color_ops ? "true" : "false",
               ColorOpAllowed(terminal, XTP_COLOR_OP_SET_COLOR) ? "allowed" : "denied",
               ColorOpAllowed(terminal, XTP_COLOR_OP_GET_COLOR) ? "allowed" : "denied",
               ColorOpAllowed(terminal, XTP_COLOR_OP_GET_ANSI_COLOR) ? "allowed" : "denied");
}

void
XtpTerminalSetAllowColorOps(XtpTerminal *terminal, bool enabled)
{
        XtpTerminalSetColorOpsPolicy(terminal, enabled, NULL);
}

/* The displayed default background: libghostty's effective color, swapped
 * with the foreground while DECSCNM is active. */
static bool
DisplayedBackground(XtpTerminal *terminal, GhosttyColorRgb *background)
{
        GhosttyTerminalModeConfig reverse = {GHOSTTY_MODE_REVERSE_COLORS, false};
        GhosttyTerminalData source;

        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &reverse) !=
            GHOSTTY_SUCCESS)
                reverse.value = false;
        source = reverse.value ? GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND
                               : GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND;
        return ghostty_terminal_get(terminal->handle, source, background) == GHOSTTY_SUCCESS;
}

bool
XtpTerminalBackgroundIsLight(XtpTerminal *terminal)
{
        GhosttyColorRgb background;

        if (terminal == NULL || !DisplayedBackground(terminal, &background))
                return false;
        return ghostty_color_perceived_luminance(&background) > 0.5;
}

static bool
ColorSchemeEffect(GhosttyTerminal handle, void *userdata, GhosttyColorScheme *out_scheme)
{
        XtpTerminal *terminal = userdata;

        (void)handle;
        *out_scheme = XtpTerminalBackgroundIsLight(terminal) ? GHOSTTY_COLOR_SCHEME_LIGHT
                                                             : GHOSTTY_COLOR_SCHEME_DARK;
        XtpLog(XTP_LOG_INFO, "terminal", "color scheme query answered scheme=%s",
               *out_scheme == GHOSTTY_COLOR_SCHEME_LIGHT ? "light" : "dark");
        return true;
}

static const void *
ColorSchemeEffectPointer(void)
{
        GhosttyTerminalColorSchemeFn function = ColorSchemeEffect;
        const void *pointer = NULL;

        _Static_assert(sizeof(function) == sizeof(pointer),
                       "Ghostty callback pointer ABI is unsupported");
        memcpy(&pointer, &function, sizeof(pointer));
        return pointer;
}

/* Mode 2031 asks for unsolicited reports whenever the scheme flips. */
static void
ReportColorSchemeChange(XtpTerminal *terminal)
{
        GhosttyTerminalModeConfig mode = {GHOSTTY_MODE_COLOR_SCHEME_REPORT, false};
        bool light = XtpTerminalBackgroundIsLight(terminal);
        char report[16];
        size_t written = 0;

        if (terminal->scheme_initialized && light == terminal->scheme_light)
                return;
        terminal->scheme_initialized = true;
        terminal->scheme_light = light;
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &mode) !=
                GHOSTTY_SUCCESS ||
            !mode.value || terminal->effects.write_pty == NULL)
                return;
        if (ghostty_color_scheme_report_encode(light ? GHOSTTY_COLOR_SCHEME_LIGHT
                                                     : GHOSTTY_COLOR_SCHEME_DARK,
                                               report, sizeof(report), &written) != GHOSTTY_SUCCESS)
                return;
        terminal->effects.write_pty((const uint8_t *)report, written, terminal->effects.closure);
        XtpLog(XTP_LOG_INFO, "terminal", "color scheme report sent scheme=%s",
               light ? "light" : "dark");
}

static void
CursorBlinkResetEffect(void *closure)
{
        CursorBlinkFeed *feed = closure;
        XtpTerminal *terminal = feed->terminal;

        if (terminal->effects.cursor_blink_reset != NULL)
                terminal->effects.cursor_blink_reset(terminal->effects.closure);
}

XtpTerminal *
XtpTerminalNewWithGraphemeWidth(uint16_t columns, uint16_t rows, uint32_t cell_width,
                                uint32_t cell_height, bool unicode_width)
{
        XtpTerminal *terminal = calloc(1, sizeof(*terminal));
        GhosttyTerminalModeConfig grapheme_mode = {GHOSTTY_MODE_GRAPHEME_CLUSTER, unicode_width};
        size_t unknown_apc_limit = XTP_UNKNOWN_APC_CAPTURE_LIMIT;

        if (terminal == NULL)
                return NULL;
        terminal->bold_colors = true;
        terminal->allow_color_ops = true;
        terminal->allow_mouse_ops = true;
        terminal->allow_tcap_ops = true;
        XtpTcapOpsParse(XTP_TCAP_OPS_DEFAULT_DISALLOWED, &terminal->tcap_ops);
        XtpColorOpsParse(XTP_COLOR_OPS_DEFAULT_DISALLOWED, &terminal->color_ops);
        terminal->geometry_columns = columns;
        terminal->geometry_rows = rows;
        terminal->geometry_cell_width = cell_width;
        terminal->geometry_cell_height = cell_height;

        XtpLog(XTP_LOG_INFO, "terminal",
               "creating backend=libghostty-vt grid=%ux%u cell=%ux%u graphemeWidth=%s", columns,
               rows, cell_width, cell_height, unicode_width ? "unicode" : "legacy");

        if (ghostty_terminal_new(NULL, &terminal->handle, columns, rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_new(NULL, &terminal->render_state) != GHOSTTY_SUCCESS ||
            ghostty_render_state_row_iterator_new(NULL, &terminal->rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_row_cells_new(NULL, &terminal->cells) != GHOSTTY_SUCCESS ||
            ghostty_key_encoder_new(NULL, &terminal->key_encoder) != GHOSTTY_SUCCESS ||
            ghostty_key_event_new(NULL, &terminal->key_event) != GHOSTTY_SUCCESS ||
            ghostty_mouse_encoder_new(NULL, &terminal->mouse_encoder) != GHOSTTY_SUCCESS ||
            ghostty_mouse_event_new(NULL, &terminal->mouse_event) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_new(NULL, &terminal->selection_gesture) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_new(NULL, &terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_PRESS) !=
                GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_new(NULL, &terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_DRAG) !=
                GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_new(
                NULL, &terminal->selection_autoscroll,
                GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_AUTOSCROLL_TICK) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_new(NULL, &terminal->selection_release,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_TYPE_RELEASE) !=
                GHOSTTY_SUCCESS ||
            ghostty_terminal_resize(terminal->handle, columns, rows, cell_width, cell_height) !=
                GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_MODE_DEFAULT,
                                 &grapheme_mode) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_USERDATA, terminal) !=
                GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                                 WritePtyEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_BELL,
                                 BellEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                                 TitleEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SIZE,
                                 SizeEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_XTVERSION,
                                 XtversionEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_CLIPBOARD_WRITE,
                                 ClipboardWriteEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_COLOR_SCHEME,
                                 ColorSchemeEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_ENQUIRY,
                                 EnquiryEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES,
                                 DeviceAttributesEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_PWD_CHANGED,
                                 WorkingDirectoryEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_UNKNOWN_MAX_BYTES,
                                 &unknown_apc_limit) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_UNKNOWN_SEQUENCE,
                                 UnknownSequenceEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_DESKTOP_NOTIFICATION,
                                 DesktopNotificationEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_PROGRESS_REPORT,
                                 ProgressReportEffectPointer()) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_RENDER_HOLD,
                                 RenderHoldEffectPointer()) != GHOSTTY_SUCCESS) {
                FreeHandles(terminal);
                free(terminal);
                return NULL;
        }

        return terminal;
}

void
XtpTerminalFree(XtpTerminal *terminal)
{
        if (terminal != NULL) {
                XtpLog(XTP_LOG_INFO, "terminal", "destroying backend=libghostty-vt");
                FreeHandles(terminal);
                free(terminal);
        }
}

void
XtpTerminalFeed(XtpTerminal *terminal, const uint8_t *bytes, size_t length)
{
        if (terminal != NULL) {
                CursorBlinkFeed feed = {
                    .terminal = terminal,
                    .bytes = bytes,
                };
                XtpCursorBlinkObserverEffects effects = {
                    .before_change = CursorBlinkBeforeChange,
                    .reset = CursorBlinkResetEffect,
                    .window_op = CursorBlinkWindowOp,
                    .osc_header = FeedOscHeader,
                    .osc_payload = FeedOscPayload,
                    .osc_item_end = FeedOscItemEnd,
                    .osc_end = FeedOscEnd,
                    .closure = &feed,
                };

                XtpLog(XTP_LOG_DEBUG, "terminal", "feed bytes=%zu", length);
                XtpCursorBlinkObserverFeed(&terminal->cursor_blink, bytes, length, &effects);
                if (terminal->command_end_active) {
                        terminal->command_end_bytes +=
                            OscPayloadBytes(bytes, terminal->command_end_start, length);
                        terminal->command_end_start = 0;
                        if (terminal->command_end_first_item && !terminal->command_end_first_done) {
                                terminal->command_end_item_bytes += OscPayloadBytes(
                                    bytes, terminal->command_end_item_start, length);
                                terminal->command_end_item_start = 0;
                        }
                }
                if (terminal->pwd_report_active) {
                        if (length > terminal->pwd_report_start)
                                terminal->pwd_report_bytes += length - terminal->pwd_report_start;
                        terminal->pwd_report_start = 0;
                }
                /* A withheld or captured item continues in the next feed. */
                if (terminal->color_list_capturing) {
                        CaptureIndexText(terminal, bytes, length);
                        terminal->color_list_capture_start = 0;
                }
                if (terminal->color_list_skipping)
                        feed.written = length;
                if (feed.written < length)
                        ghostty_terminal_vt_write(terminal->handle, bytes + feed.written,
                                                  length - feed.written);
                if (SyncCursorBlinkMode(terminal) != 0)
                        XtpLog(XTP_LOG_ERROR, "terminal",
                               "cannot synchronize application cursor blink mode");
        }
}

int
XtpTerminalResize(XtpTerminal *terminal, uint16_t columns, uint16_t rows, uint32_t cell_width,
                  uint32_t cell_height)
{
        GhosttyResult result;
        bool synchronized = false;

        if (terminal == NULL)
                return -1;

        XtpLog(XTP_LOG_INFO, "terminal", "resize grid=%ux%u cell=%ux%u", columns, rows, cell_width,
               cell_height);

        /* libghostty clears DEC mode 2026 on resize; a host resize must not end the batch. */
        if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, &synchronized) != 0)
                synchronized = false;
        result = ghostty_terminal_resize(terminal->handle, columns, rows, cell_width, cell_height);
        if (result != GHOSTTY_SUCCESS)
                return -1;
        if (synchronized &&
            XtpTerminalSetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, true) != 0)
                XtpLog(XTP_LOG_ERROR, "terminal",
                       "cannot preserve synchronized output across resize");
        terminal->geometry_columns = columns;
        terminal->geometry_rows = rows;
        terminal->geometry_cell_width = cell_width;
        terminal->geometry_cell_height = cell_height;
        return 0;
}

int
XtpTerminalSetTitle(XtpTerminal *terminal, const char *title, size_t length)
{
        GhosttyString value = {.ptr = (const uint8_t *)title, .len = length};

        if (terminal == NULL)
                return -1;
        return ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_TITLE, &value) ==
                       GHOSTTY_SUCCESS
                   ? 0
                   : -1;
}

int
XtpTerminalSetScrollbackLines(XtpTerminal *terminal, size_t lines)
{
        GhosttyResult result;

        if (terminal == NULL)
                return -1;
        XtpLog(XTP_LOG_INFO, "scrollback", "history limit lines=%zu bytes=%s", lines,
               lines == 0 ? "zero" : "unlimited");
        if (lines == 0) {
                size_t bytes = 0;

                result = ghostty_terminal_set(terminal->handle,
                                              GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES, &bytes);
        } else {
                result = ghostty_terminal_set(terminal->handle,
                                              GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES, NULL);
        }
        if (result != GHOSTTY_SUCCESS)
                return -1;
        return ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES,
                                    &lines) == GHOSTTY_SUCCESS
                   ? 0
                   : -1;
}

int
XtpTerminalSetDefaultCursorShape(XtpTerminal *terminal, XtpCursorShape shape)
{
        GhosttyTerminalCursorStyle style;

        if (terminal == NULL)
                return -1;
        switch (shape) {
        case XTP_CURSOR_SHAPE_UNDERLINE:
                style = GHOSTTY_TERMINAL_CURSOR_STYLE_UNDERLINE;
                break;
        case XTP_CURSOR_SHAPE_BAR:
                style = GHOSTTY_TERMINAL_CURSOR_STYLE_BAR;
                break;
        case XTP_CURSOR_SHAPE_BLOCK:
        case XTP_CURSOR_SHAPE_BLOCK_HOLLOW:
        default:
                style = GHOSTTY_TERMINAL_CURSOR_STYLE_BLOCK;
                break;
        }
        XtpLog(XTP_LOG_INFO, "terminal", "default cursor shape=%s",
               shape == XTP_CURSOR_SHAPE_UNDERLINE ? "underline"
               : shape == XTP_CURSOR_SHAPE_BAR     ? "bar"
                                                   : "block");
        /* libghostty reapplies its default blink with the style; keep the
         * application's mode 12 state authoritative, as the blink default does. */
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_DEFAULT_CURSOR_STYLE,
                                 &style) != GHOSTTY_SUCCESS)
                return -1;
        return SyncCursorBlinkMode(terminal);
}

int
XtpTerminalSetCursorBlinkDefault(XtpTerminal *terminal, bool blinking)
{
        GhosttyResult result;

        if (terminal == NULL)
                return -1;
        XtpLog(XTP_LOG_INFO, "terminal", "cursor blink default=%s", blinking ? "true" : "false");
        /*
         * Retain the configured operand in libghostty for its reset and
         * default-style baseline. Presentation and mode reports use
         * xterm+'s separate raw application state, so restore mode 12 after
         * changing the default.
         */
        result = ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_DEFAULT_CURSOR_BLINK,
                                      &blinking);
        return result == GHOSTTY_SUCCESS && SyncCursorBlinkMode(terminal) == 0 ? 0 : -1;
}

int
XtpTerminalSetCursorBlinkRequestsEnabled(XtpTerminal *terminal, bool enabled)
{
        if (terminal == NULL)
                return -1;
        XtpCursorBlinkObserverSetRequestsEnabled(&terminal->cursor_blink, enabled);
        XtpLog(XTP_LOG_INFO, "terminal", "cursor blink application requests=%s",
               enabled ? "enabled" : "ignored");
        return 0;
}

int
XtpTerminalSetDefaultColors(XtpTerminal *terminal, XtpRgbColor foreground, XtpRgbColor background,
                            XtpRgbColor cursor)
{
        GhosttyColorRgb ghostty_foreground = {foreground.red, foreground.green, foreground.blue};
        GhosttyColorRgb ghostty_background = {background.red, background.green, background.blue};
        GhosttyColorRgb ghostty_cursor = {cursor.red, cursor.green, cursor.blue};

        if (terminal == NULL)
                return -1;
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND,
                                 &ghostty_foreground) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND,
                                 &ghostty_background) != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_COLOR_CURSOR,
                                 &ghostty_cursor) != GHOSTTY_SUCCESS)
                return -1;
        XtpLog(XTP_LOG_INFO, "terminal",
               "default colors foreground=#%02x%02x%02x background=#%02x%02x%02x "
               "cursor=#%02x%02x%02x",
               foreground.red, foreground.green, foreground.blue, background.red, background.green,
               background.blue, cursor.red, cursor.green, cursor.blue);
        return 0;
}

int
XtpTerminalSetAnsiPalette(XtpTerminal *terminal,
                          const XtpRgbColor ansi_palette[XTP_ANSI_PALETTE_SIZE])
{
        GhosttyColorRgb palette[256];
        char summary[512];
        size_t summary_length = 0;
        size_t index;

        if (terminal == NULL || ansi_palette == NULL)
                return -1;
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLOR_PALETTE_DEFAULT,
                                 palette) != GHOSTTY_SUCCESS)
                return -1;
        for (index = 0; index < XTP_ANSI_PALETTE_SIZE; ++index) {
                palette[index] = (GhosttyColorRgb){
                    ansi_palette[index].red, ansi_palette[index].green, ansi_palette[index].blue};
        }
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette) !=
            GHOSTTY_SUCCESS)
                return -1;
        summary[0] = '\0';
        for (index = 0; index < XTP_ANSI_PALETTE_SIZE; ++index) {
                int written = snprintf(summary + summary_length, sizeof(summary) - summary_length,
                                       "%scolor%zu=#%02x%02x%02x", index == 0 ? "" : " ", index,
                                       ansi_palette[index].red, ansi_palette[index].green,
                                       ansi_palette[index].blue);

                if (written < 0 || (size_t)written >= sizeof(summary) - summary_length)
                        break;
                summary_length += (size_t)written;
        }
        XtpLog(XTP_LOG_INFO, "terminal", "configured ANSI palette %s", summary);
        return 0;
}

int
XtpTerminalSetBoldColors(XtpTerminal *terminal, bool enabled)
{
        if (terminal == NULL)
                return -1;
        terminal->bold_colors = enabled;
        return 0;
}

int
XtpTerminalSetCharClass(XtpTerminal *terminal, const char *specification)
{
        XtpCharClassTable *table = NULL;

        if (terminal == NULL)
                return -1;
        if (XtpCharClassParse(specification, &table) != 0)
                goto invalid;
        XtpCharClassFree(terminal->char_classes);
        terminal->char_classes = table;
        XtpLog(XTP_LOG_INFO, "selection", "charClass specification=%s",
               specification != NULL ? specification : "(default)");
        return 0;

invalid:
        XtpLog(XTP_LOG_WARNING, "selection", "invalid charClass specification=%s", specification);
        return -1;
}

int
XtpTerminalGetScrollbar(XtpTerminal *terminal, XtpTerminalScrollbar *scrollbar)
{
        GhosttyTerminalScrollbar state = {0};

        if (terminal == NULL || scrollbar == NULL ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &state) !=
                GHOSTTY_SUCCESS)
                return -1;
        scrollbar->total = state.total;
        scrollbar->offset = state.offset;
        scrollbar->length = state.len;
        return 0;
}

int
XtpTerminalScrollBy(XtpTerminal *terminal, intptr_t rows)
{
        GhosttyTerminalScrollViewport viewport = {
            GHOSTTY_SCROLL_VIEWPORT_DELTA,
            {.delta = rows},
        };

        if (terminal == NULL)
                return -1;
        ghostty_terminal_scroll_viewport(terminal->handle, viewport);
        return 0;
}

int
XtpTerminalScrollTo(XtpTerminal *terminal, uint64_t row)
{
        GhosttyTerminalScrollViewport viewport = {
            GHOSTTY_SCROLL_VIEWPORT_ROW,
            {.row = row > SIZE_MAX ? SIZE_MAX : (size_t)row},
        };

        if (terminal == NULL)
                return -1;
        ghostty_terminal_scroll_viewport(terminal->handle, viewport);
        return 0;
}

int
XtpTerminalScrollToBottom(XtpTerminal *terminal)
{
        GhosttyTerminalScrollViewport viewport = {
            GHOSTTY_SCROLL_VIEWPORT_BOTTOM,
            {0},
        };

        if (terminal == NULL)
                return -1;
        ghostty_terminal_scroll_viewport(terminal->handle, viewport);
        return 0;
}

/* Row count of the primary screen including scrollback; 0 on the alternate screen, which
 * has no history and whose grid references would not describe what is displayed. */
static uint64_t
ScreenRows(XtpTerminal *terminal)
{
        GhosttyTerminalScrollbar state = {0};
        GhosttyTerminalScreen screen = GHOSTTY_TERMINAL_SCREEN_PRIMARY;

        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_ACTIVE_SCREEN, &screen) !=
                GHOSTTY_SUCCESS ||
            screen != GHOSTTY_TERMINAL_SCREEN_PRIMARY ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &state) !=
                GHOSTTY_SUCCESS)
                return 0;
        return state.total;
}

uint64_t
XtpGhosttyScreenRows(XtpTerminal *terminal)
{
        return ScreenRows(terminal);
}

static bool
ScreenRowRef(XtpTerminal *terminal, uint64_t row, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = {0, (uint32_t)row}},
        };

        return row <= UINT32_MAX &&
               ghostty_terminal_grid_ref(terminal->handle, point, ref) == GHOSTTY_SUCCESS;
}

static bool
ScreenRowSemantic(XtpTerminal *terminal, uint64_t row, XtpSemanticRow *state)
{
        GhosttyGridRef ref;
        GhosttyRow data;
        GhosttyRowSemanticPrompt prompt = GHOSTTY_ROW_SEMANTIC_NONE;

        if (!ScreenRowRef(terminal, row, &ref) ||
            ghostty_grid_ref_row(&ref, &data) != GHOSTTY_SUCCESS ||
            ghostty_row_get(data, GHOSTTY_ROW_DATA_SEMANTIC_PROMPT, &prompt) != GHOSTTY_SUCCESS)
                return false;
        if (prompt == GHOSTTY_ROW_SEMANTIC_PROMPT)
                *state = XTP_SEMANTIC_ROW_PROMPT;
        else if (prompt == GHOSTTY_ROW_SEMANTIC_PROMPT_CONTINUATION)
                *state = XTP_SEMANTIC_ROW_PROMPT_CONTINUATION;
        else
                *state = XTP_SEMANTIC_ROW_NONE;
        return true;
}

int
XtpTerminalSemanticRow(XtpTerminal *terminal, uint64_t row, XtpSemanticRow *state)
{
        if (terminal == NULL || state == NULL || row >= ScreenRows(terminal))
                return -1;
        return ScreenRowSemantic(terminal, row, state) ? 0 : -1;
}

static void
RemovePromptMark(XtpTerminal *terminal, size_t index)
{
        ghostty_tracked_grid_ref_free(terminal->prompt_marks[index]);
        memmove(&terminal->prompt_marks[index], &terminal->prompt_marks[index + 1U],
                (terminal->prompt_mark_count - index - 1U) * sizeof(terminal->prompt_marks[0]));
        --terminal->prompt_mark_count;
}

/* Screen row of a mark; a mark whose row is gone is removed and reports false. */
static bool
PromptMarkRow(XtpTerminal *terminal, size_t index, uint64_t *row)
{
        GhosttyPointCoordinate point;

        if (ghostty_tracked_grid_ref_has_value(terminal->prompt_marks[index]) &&
            ghostty_tracked_grid_ref_point(terminal->prompt_marks[index], GHOSTTY_POINT_TAG_SCREEN,
                                           &point) == GHOSTTY_SUCCESS) {
                *row = point.y;
                return true;
        }
        RemovePromptMark(terminal, index);
        return false;
}

/* Pruning takes rows from the top and a reset takes them all, so dead marks form a prefix.
 * Dropping that prefix on every insert keeps the index bounded by the rows the core retains
 * without ever scanning rows. */
static void
CompactPromptMarks(XtpTerminal *terminal)
{
        size_t dead = 0;

        while (dead < terminal->prompt_mark_count &&
               !ghostty_tracked_grid_ref_has_value(terminal->prompt_marks[dead]))
                ghostty_tracked_grid_ref_free(terminal->prompt_marks[dead++]);
        if (dead == 0)
                return;
        memmove(terminal->prompt_marks, terminal->prompt_marks + dead,
                (terminal->prompt_mark_count - dead) * sizeof(terminal->prompt_marks[0]));
        terminal->prompt_mark_count -= dead;
}

/* Index of the first mark below `row`; dead marks met by the bisection are dropped. */
static size_t
PromptMarkUpperBound(XtpTerminal *terminal, uint64_t row)
{
        size_t low = 0;
        size_t high = terminal->prompt_mark_count;

        while (low < high) {
                size_t mid = low + (high - low) / 2U;
                uint64_t mark_row;

                if (!PromptMarkRow(terminal, mid, &mark_row)) {
                        --high;
                        continue;
                }
                if (mark_row > row)
                        high = mid;
                else
                        low = mid + 1U;
        }
        return low;
}

/* Resolves a marked row to its prompt's start: a continuation run belongs to the primary
 * row above it, or to its own top row when that primary row is gone (xterm+ normalizes
 * that case to the top in both directions). False when the row no longer holds a mark. */
static bool
PromptStartOf(XtpTerminal *terminal, uint64_t row, uint64_t *start)
{
        XtpSemanticRow state;
        uint64_t top = row;

        if (!ScreenRowSemantic(terminal, row, &state) || state == XTP_SEMANTIC_ROW_NONE)
                return false;
        while (state == XTP_SEMANTIC_ROW_PROMPT_CONTINUATION && top > 0 &&
               ScreenRowSemantic(terminal, top - 1U, &state) && state != XTP_SEMANTIC_ROW_NONE)
                --top;
        *start = top;
        return true;
}

/* Called once the core has processed an OSC 133 prompt start: the cursor row is the marked
 * row, so a tracked reference records it in row order. */
static void
RecordPromptMark(XtpTerminal *terminal)
{
        uint16_t cursor_y = 0;
        GhosttyPoint point = {GHOSTTY_POINT_TAG_ACTIVE, {.coordinate = {0, 0}}};
        GhosttyGridRef ref;
        GhosttyPointCoordinate coordinate;
        GhosttyTrackedGridRef mark = NULL;
        XtpSemanticRow state;
        uint64_t row;
        uint64_t last_row = 0;
        bool last_valid = false;
        size_t index;

        if (ScreenRows(terminal) == 0 ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_CURSOR_Y, &cursor_y) !=
                GHOSTTY_SUCCESS)
                return;
        CompactPromptMarks(terminal);
        point.value.coordinate.y = cursor_y;
        if (ghostty_terminal_grid_ref(terminal->handle, point, &ref) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &ref, GHOSTTY_POINT_TAG_SCREEN,
                                                 &coordinate) != GHOSTTY_SUCCESS)
                return;
        row = coordinate.y;
        /* An aborted OSC leaves the row unmarked; a redrawn prompt keeps its single mark. */
        if (!ScreenRowSemantic(terminal, row, &state) || state == XTP_SEMANTIC_ROW_NONE)
                return;
        /* One mark per row keeps the index bounded by the rows the core retains, so a redrawn
         * prompt anywhere in the active area never adds a second reference. */
        index = terminal->prompt_mark_count;
        if (index != 0) {
                last_valid = PromptMarkRow(terminal, index - 1U, &last_row);
                if (last_valid && last_row == row)
                        return;
                if (last_valid && last_row > row) {
                        uint64_t previous_row;

                        index = PromptMarkUpperBound(terminal, row);
                        if (index != 0 && PromptMarkRow(terminal, index - 1U, &previous_row) &&
                            previous_row == row)
                                return;
                        if (index > terminal->prompt_mark_count)
                                index = terminal->prompt_mark_count;
                } else {
                        index = terminal->prompt_mark_count;
                }
        }
        if (terminal->prompt_mark_count == terminal->prompt_mark_capacity) {
                size_t capacity =
                    terminal->prompt_mark_capacity != 0 ? terminal->prompt_mark_capacity * 2U : 64U;
                GhosttyTrackedGridRef *marks =
                    realloc(terminal->prompt_marks, capacity * sizeof(*marks));

                if (marks == NULL)
                        return;
                terminal->prompt_marks = marks;
                terminal->prompt_mark_capacity = capacity;
        }
        if (ghostty_terminal_grid_ref_track(terminal->handle, point, &mark) != GHOSTTY_SUCCESS)
                return;
        memmove(&terminal->prompt_marks[index + 1U], &terminal->prompt_marks[index],
                (terminal->prompt_mark_count - index) * sizeof(terminal->prompt_marks[0]));
        terminal->prompt_marks[index] = mark;
        ++terminal->prompt_mark_count;
}

/* OSC 133 D belongs to the newest prompt's command; a tracked reference keeps that prompt's
 * row so completion is known even before the shell prints the next prompt. */
static void
RecordCommandEnd(XtpTerminal *terminal)
{
        uint64_t row;
        GhosttyPoint point = {GHOSTTY_POINT_TAG_SCREEN, {.coordinate = {0, 0}}};
        GhosttyTrackedGridRef mark = NULL;

        if (ScreenRows(terminal) == 0)
                return;
        CompactPromptMarks(terminal);
        if (terminal->prompt_mark_count == 0 ||
            !PromptMarkRow(terminal, terminal->prompt_mark_count - 1U, &row) || row > UINT32_MAX)
                return;
        point.value.coordinate.y = (uint32_t)row;
        if (ghostty_terminal_grid_ref_track(terminal->handle, point, &mark) != GHOSTTY_SUCCESS)
                return;
        ghostty_tracked_grid_ref_free(terminal->completed_prompt);
        terminal->completed_prompt = mark;
}

bool
XtpTerminalCommandEndSeen(XtpTerminal *terminal)
{
        return terminal != NULL && terminal->command_end_seen;
}

int
XtpTerminalLastCompletedPrompt(XtpTerminal *terminal, uint64_t *row)
{
        GhosttyPointCoordinate point;
        uint64_t start;

        if (terminal == NULL || row == NULL || terminal->completed_prompt == NULL ||
            ScreenRows(terminal) == 0 ||
            !ghostty_tracked_grid_ref_has_value(terminal->completed_prompt) ||
            ghostty_tracked_grid_ref_point(terminal->completed_prompt, GHOSTTY_POINT_TAG_SCREEN,
                                           &point) != GHOSTTY_SUCCESS ||
            !PromptStartOf(terminal, point.y, &start))
                return -1;
        *row = start;
        return 0;
}

size_t
XtpTerminalPromptMarks(XtpTerminal *terminal)
{
        return terminal != NULL ? terminal->prompt_mark_count : 0;
}

static void
FreePromptMarks(XtpTerminal *terminal)
{
        while (terminal->prompt_mark_count != 0)
                RemovePromptMark(terminal, terminal->prompt_mark_count - 1U);
        free(terminal->prompt_marks);
        terminal->prompt_marks = NULL;
        terminal->prompt_mark_capacity = 0;
        ghostty_tracked_grid_ref_free(terminal->completed_prompt);
        terminal->completed_prompt = NULL;
}

int
XtpTerminalFindPrompt(XtpTerminal *terminal, uint64_t from, bool forward, uint64_t *row)
{
        uint64_t total;
        uint64_t mark_row;
        uint64_t start;
        size_t index;

        if (terminal == NULL || row == NULL)
                return -1;
        total = ScreenRows(terminal);
        /* Backward searches may start one past the last row to cover the whole screen. */
        if (total == 0 || from > total || (forward && from == total))
                return -1;
        index = PromptMarkUpperBound(terminal, from);
        if (!forward) {
                while (index > 0) {
                        --index;
                        if (!PromptMarkRow(terminal, index, &mark_row) || mark_row >= from)
                                continue;
                        if (!PromptStartOf(terminal, mark_row, &start)) {
                                RemovePromptMark(terminal, index);
                                continue;
                        }
                        if (start < from) {
                                *row = start;
                                return 0;
                        }
                }
                return -1;
        }
        while (index < terminal->prompt_mark_count) {
                if (!PromptMarkRow(terminal, index, &mark_row))
                        continue;
                if (!PromptStartOf(terminal, mark_row, &start)) {
                        RemovePromptMark(terminal, index);
                        continue;
                }
                /* A start at or above `from` is the current prompt's own continuation. */
                if (start > from) {
                        *row = start;
                        return 0;
                }
                ++index;
        }
        return -1;
}

static bool
ScreenCellRef(XtpTerminal *terminal, uint64_t row, uint16_t column, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = {column, (uint32_t)row}},
        };

        return row <= UINT32_MAX &&
               ghostty_terminal_grid_ref(terminal->handle, point, ref) == GHOSTTY_SUCCESS;
}

/* The core picks the prompt nearest above the chosen cell; this confirms no other prompt
 * sits between `prompt_start`'s own rows and the selection, so a prompt missing from the
 * index cannot hand back a later command's output. */
static bool
OutputBelongsToPrompt(XtpTerminal *terminal, uint64_t prompt_start, uint64_t output_row)
{
        uint64_t row = prompt_start + 1U;
        XtpSemanticRow state;

        while (row <= output_row && ScreenRowSemantic(terminal, row, &state) &&
               state == XTP_SEMANTIC_ROW_PROMPT_CONTINUATION)
                ++row;
        for (; row <= output_row; ++row) {
                if (!ScreenRowSemantic(terminal, row, &state) || state != XTP_SEMANTIC_ROW_NONE)
                        return false;
        }
        return true;
}

int
XtpTerminalCommandOutput(XtpTerminal *terminal, uint64_t prompt_start, XtpSemanticSpan *span)
{
        uint64_t total;
        uint64_t end;
        uint64_t next;
        uint64_t row;
        XtpSemanticRow state;

        if (terminal == NULL || span == NULL)
                return -1;
        total = ScreenRows(terminal);
        if (prompt_start >= total || !ScreenRowSemantic(terminal, prompt_start, &state) ||
            state == XTP_SEMANTIC_ROW_NONE)
                return -1;
        end = XtpTerminalFindPrompt(terminal, prompt_start, true, &next) == 0 ? next : total;
        /* Any output-content cell in the block, written or not, lets the core derive the
         * highlight from the prompt itself; it answers no-value when nothing was written. */
        for (row = prompt_start; row < end; ++row) {
                uint16_t column;

                for (column = 0; column < terminal->geometry_columns; ++column) {
                        GhosttyGridRef ref;
                        GhosttyCell cell;
                        GhosttyCellSemanticContent content;
                        GhosttySelection selection = {.size = sizeof(selection)};
                        GhosttyPointCoordinate first;
                        GhosttyPointCoordinate last;

                        if (!ScreenCellRef(terminal, row, column, &ref) ||
                            ghostty_grid_ref_cell(&ref, &cell) != GHOSTTY_SUCCESS ||
                            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_SEMANTIC_CONTENT, &content) !=
                                GHOSTTY_SUCCESS)
                                return -1;
                        if (content != GHOSTTY_CELL_SEMANTIC_OUTPUT)
                                continue;
                        if (ghostty_terminal_select_output(terminal->handle, ref, &selection) !=
                                GHOSTTY_SUCCESS ||
                            ghostty_terminal_point_from_grid_ref(terminal->handle, &selection.start,
                                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                                 &first) != GHOSTTY_SUCCESS ||
                            ghostty_terminal_point_from_grid_ref(terminal->handle, &selection.end,
                                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                                 &last) != GHOSTTY_SUCCESS)
                                return -1;
                        if (last.y < first.y || (last.y == first.y && last.x < first.x)) {
                                GhosttyPointCoordinate swap = first;

                                first = last;
                                last = swap;
                        }
                        if (!OutputBelongsToPrompt(terminal, prompt_start, first.y))
                                return -1;
                        span->start_row = first.y;
                        span->start_column = first.x;
                        span->end_row = last.y;
                        span->end_column = last.x;
                        return 0;
                }
        }
        return -1;
}

int
XtpTerminalSpanText(XtpTerminal *terminal, const XtpSemanticSpan *span, char **text, size_t *length)
{
        GhosttySelection selection = {.size = sizeof(selection)};
        GhosttyTerminalSelectionFormatOptions options = {
            .size = sizeof(options),
            .emit = GHOSTTY_FORMATTER_FORMAT_PLAIN,
            .unwrap = true,
            .trim = true,
        };
        uint8_t *formatted = NULL;
        size_t formatted_length = 0;
        char *copy;

        if (terminal == NULL || span == NULL || text == NULL || length == NULL)
                return -1;
        *text = NULL;
        *length = 0;
        if (ScreenRows(terminal) == 0 ||
            !ScreenCellRef(terminal, span->start_row, span->start_column, &selection.start) ||
            !ScreenCellRef(terminal, span->end_row, span->end_column, &selection.end))
                return -1;
        selection.rectangle = false;
        options.selection = &selection;
        if (ghostty_terminal_selection_format_alloc(terminal->handle, NULL, options, &formatted,
                                                    &formatted_length) != GHOSTTY_SUCCESS)
                return -1;
        copy = malloc(formatted_length + 1U);
        if (copy != NULL) {
                memcpy(copy, formatted, formatted_length);
                copy[formatted_length] = '\0';
        }
        ghostty_free(NULL, formatted, formatted_length);
        if (copy == NULL)
                return -1;
        *text = copy;
        *length = formatted_length;
        return 0;
}

static GhosttyResult
ViewportGridRef(XtpTerminal *terminal, uint16_t column, uint16_t row, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_VIEWPORT,
            {.coordinate = {column, row}},
        };

        return ghostty_terminal_grid_ref(terminal->handle, point, ref);
}

int
XtpTerminalHyperlinkAt(XtpTerminal *terminal, uint16_t column, uint16_t row, uint8_t **uri,
                       size_t *length)
{
        GhosttyGridRef ref;
        GhosttyResult result;
        uint8_t *value;
        size_t required = 0;

        if (terminal == NULL || uri == NULL || length == NULL)
                return -1;
        *uri = NULL;
        *length = 0;
        if (ViewportGridRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS)
                return -1;
        result = ghostty_grid_ref_hyperlink_uri(&ref, NULL, 0, &required);
        if (result == GHOSTTY_SUCCESS && required == 0)
                return 0;
        if (result != GHOSTTY_OUT_OF_SPACE || required == SIZE_MAX)
                return -1;
        value = malloc(required + 1U);
        if (value == NULL)
                return -1;
        result = ghostty_grid_ref_hyperlink_uri(&ref, value, required, &required);
        if (result != GHOSTTY_SUCCESS) {
                free(value);
                return -1;
        }
        value[required] = '\0';
        *uri = value;
        *length = required;
        return 0;
}

int
XtpTerminalEncodePaste(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                       uint8_t **encoded, size_t *encoded_length)
{
        GhosttyTerminalModeConfig config = {GHOSTTY_MODE_BRACKETED_PASTE, false};
        GhosttyResult result;
        char *input;
        char *output;
        size_t required = 0;

        if (terminal == NULL || (bytes == NULL && length != 0) || encoded == NULL ||
            encoded_length == NULL)
                return -1;
        *encoded = NULL;
        *encoded_length = 0;
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &config) !=
            GHOSTTY_SUCCESS)
                return -1;
        input = malloc(length != 0 ? length : 1U);
        if (input == NULL)
                return -1;
        if (length != 0)
                memcpy(input, bytes, length);
        result = ghostty_paste_encode(input, length, config.value, NULL, 0, &required);
        if (result == GHOSTTY_SUCCESS && required == 0) {
                free(input);
                output = malloc(1U);
                if (output == NULL)
                        return -1;
                *encoded = (uint8_t *)output;
                return 0;
        }
        if (result != GHOSTTY_OUT_OF_SPACE) {
                free(input);
                return -1;
        }
        output = malloc(required != 0 ? required : 1U);
        if (output == NULL) {
                free(input);
                return -1;
        }
        result = ghostty_paste_encode(input, length, config.value, output, required, &required);
        free(input);
        if (result != GHOSTTY_SUCCESS) {
                free(output);
                return -1;
        }
        *encoded = (uint8_t *)output;
        *encoded_length = required;
        return 0;
}

void
XtpTerminalSetAllowMouseOps(XtpTerminal *terminal, bool enabled)
{
        if (terminal != NULL) {
                terminal->allow_mouse_ops = enabled;
                ghostty_mouse_encoder_reset(terminal->mouse_encoder);
        }
}

void
XtpTerminalSetTcapOpsPolicy(XtpTerminal *terminal, bool allow, const XtpTcapOps *ops)
{
        if (terminal != NULL) {
                terminal->allow_tcap_ops = allow;
                if (ops != NULL)
                        terminal->tcap_ops = *ops;
        }
}

bool
XtpTerminalMouseTracking(XtpTerminal *terminal)
{
        bool tracking = false;

        if (terminal != NULL && terminal->allow_mouse_ops)
                (void)ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING,
                                           &tracking);
        return tracking;
}

int
XtpTerminalGetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool *enabled)
{
        GhosttyTerminalModeConfig config;

        if (terminal == NULL || enabled == NULL || mode >= XTP_TERMINAL_MODE_COUNT)
                return -1;
        config.mode = ConvertMode(mode);
        config.value = false;
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &config) !=
            GHOSTTY_SUCCESS)
                return -1;
        *enabled = config.value;
        return 0;
}

int
XtpTerminalSetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool enabled)
{
        GhosttyTerminalModeConfig config;

        if (terminal == NULL || mode >= XTP_TERMINAL_MODE_COUNT)
                return -1;
        config.mode = ConvertMode(mode);
        config.value = enabled;
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_MODE, &config) !=
            GHOSTTY_SUCCESS)
                return -1;
        XtpLog(XTP_LOG_INFO, "terminal", "mode=%d enabled=%s", (int)mode,
               enabled ? "true" : "false");
        /* Setting mode 2026 through the option bypasses libghostty's hold report, so the
         * host's own changes -- the timeout release, the resize re-arm -- are mirrored
         * here with the same capture a parsed change gets. */
        if (mode == XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT)
                SetRenderHeld(terminal, enabled);
        return 0;
}

static XtpColor
ConvertColor(GhosttyStyleColor color, const GhosttyRenderStateColors *colors, bool promote_bold)
{
        XtpColor result = {XTP_COLOR_DEFAULT, 0, 0, 0, 0};
        GhosttyColorRgb rgb;

        if (color.tag == GHOSTTY_STYLE_COLOR_PALETTE) {
                uint8_t palette = color.value.palette;

                if (promote_bold && palette < 8U)
                        palette += 8U;
                result.kind = XTP_COLOR_PALETTE;
                result.palette = palette;
                rgb = colors->palette[palette];
                result.red = rgb.r;
                result.green = rgb.g;
                result.blue = rgb.b;
        } else if (color.tag == GHOSTTY_STYLE_COLOR_RGB) {
                result.kind = XTP_COLOR_RGB;
                result.red = color.value.rgb.r;
                result.green = color.value.rgb.g;
                result.blue = color.value.rgb.b;
        }
        return result;
}

static XtpColor
ConvertRgbColor(GhosttyColorRgb color)
{
        XtpColor result = {XTP_COLOR_RGB, 0, color.r, color.g, color.b};

        return result;
}

static XtpRgbColor
RgbFromGhostty(GhosttyColorRgb color)
{
        return (XtpRgbColor){color.r, color.g, color.b};
}

/* Effective defaults, unswapped: the widget applies DECSCNM itself. Returns
 * true when any of them changed since the last frame, which forces a full
 * repaint because libghostty sets no dirty flag for OSC 10/11/12. */
static bool
ApplyEffectiveColors(XtpTerminal *terminal, XtpRenderFrame *frame,
                     const XtpGhosttyPresentation *presentation)
{
        GhosttyColorRgb foreground = presentation->foreground;
        GhosttyColorRgb background = presentation->background;
        GhosttyColorRgb cursor = presentation->cursor;
        bool changed;

        if (!presentation->colors_valid)
                return false;
        frame->foreground = RgbFromGhostty(foreground);
        frame->background = RgbFromGhostty(background);
        frame->cursor = RgbFromGhostty(cursor);
        frame->colors_valid = true;
        changed =
            terminal->colors_initialized && (!SameRgb(foreground, terminal->last_foreground) ||
                                             !SameRgb(background, terminal->last_background) ||
                                             !SameRgb(cursor, terminal->last_cursor));
        if (changed)
                XtpLog(XTP_LOG_INFO, "render",
                       "effective colors changed foreground=#%02x%02x%02x "
                       "background=#%02x%02x%02x cursor=#%02x%02x%02x",
                       foreground.r, foreground.g, foreground.b, background.r, background.g,
                       background.b, cursor.r, cursor.g, cursor.b);
        terminal->colors_initialized = true;
        terminal->last_foreground = foreground;
        terminal->last_background = background;
        terminal->last_cursor = cursor;
        return changed;
}

int
XtpTerminalRender(XtpTerminal *terminal, const XtpRenderer *renderer, void *closure,
                  bool force_full)
{
        GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
        XtpGhosttyPresentation presentation;
        XtpGhosttyFrameMeta meta;
        XtpRenderFrame frame = {0};
        GhosttyRenderStateDirty dirty;
        bool reverse_colors_changed;
        bool colors_changed;
        uint16_t row = 0;
        size_t rendered_cells = 0;
        size_t rendered_graphemes = 0;

        if (terminal == NULL || renderer == NULL)
                return -1;
        /* During a hold the captured frame is drawn, with the presentation captured
         * alongside it; nothing parsed since reaches either. */
        if (!terminal->render_held &&
            ghostty_render_state_update(terminal->render_state, terminal->handle) !=
                GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot update render state");
                return -1;
        }
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_COLORS,
                                     &colors) != GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read render-state colors");
                return -1;
        }
        if (terminal->render_held) {
                presentation = terminal->held_presentation;
        } else if (ReadLivePresentation(terminal, &presentation) != 0) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read reverse-colors mode");
                return -1;
        }
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_COLS,
                                     &frame.columns) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_ROWS,
                                     &frame.rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_DIRTY,
                                     &dirty) != GHOSTTY_SUCCESS ||
            ReadCursorMeta(terminal, &meta) != 0) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read render-state metadata");
                return -1;
        }
        meta.presentation = presentation;
        frame.cursor_blink_requested = presentation.blink_requested;

        frame.reverse_colors = presentation.reverse_colors;
        reverse_colors_changed = terminal->reverse_colors_initialized &&
                                 terminal->reverse_colors != frame.reverse_colors;
        colors_changed = ApplyEffectiveColors(terminal, &frame, &presentation);
        frame.full_repaint = force_full || dirty == GHOSTTY_RENDER_STATE_DIRTY_FULL ||
                             reverse_colors_changed || colors_changed;
        if (reverse_colors_changed)
                XtpLog(XTP_LOG_INFO, "render", "screen reverse changed enabled=%s",
                       frame.reverse_colors ? "true" : "false");
        if (meta.cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE)
                frame.cursor_shape = XTP_CURSOR_SHAPE_UNDERLINE;
        else if (meta.cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR)
                frame.cursor_shape = XTP_CURSOR_SHAPE_BAR;
        else if (meta.cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW)
                frame.cursor_shape = XTP_CURSOR_SHAPE_BLOCK_HOLLOW;
        else
                frame.cursor_shape = XTP_CURSOR_SHAPE_BLOCK;
        frame.cursor_visible = meta.cursor_visible;
        frame.cursor_column = meta.cursor_column;
        frame.cursor_row = meta.cursor_row;

        if (!frame.full_repaint && dirty == GHOSTTY_RENDER_STATE_DIRTY_FALSE)
                XtpLog(XTP_LOG_DEBUG, "render", "frame has no cell damage; checking cursor");

        if (renderer->begin != NULL)
                renderer->begin(&frame, closure);
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,
                                     &terminal->rows) != GHOSTTY_SUCCESS)
                goto render_failed;

        while (ghostty_render_state_row_iterator_next(terminal->rows)) {
                uint16_t column = 0;
                bool row_dirty = false;
                bool clean = false;
                bool row_selected = false;
                bool row_wrapped = false;
                GhosttyRow raw_row;
                GhosttyRenderStateRowSelection selection =
                    GHOSTTY_INIT_SIZED(GhosttyRenderStateRowSelection);

                if (ghostty_render_state_row_get(terminal->rows,
                                                 GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY,
                                                 &row_dirty) != GHOSTTY_SUCCESS)
                        goto render_failed;
                if (!frame.full_repaint && !row_dirty) {
                        ++row;
                        continue;
                }
                row_selected = ghostty_render_state_row_get(terminal->rows,
                                                            GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION,
                                                            &selection) == GHOSTTY_SUCCESS;
                if (ghostty_render_state_row_get(terminal->rows, GHOSTTY_RENDER_STATE_ROW_DATA_RAW,
                                                 &raw_row) != GHOSTTY_SUCCESS ||
                    ghostty_row_get(raw_row, GHOSTTY_ROW_DATA_WRAP, &row_wrapped) !=
                        GHOSTTY_SUCCESS)
                        goto render_failed;
                if (ghostty_render_state_row_get(terminal->rows,
                                                 GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,
                                                 &terminal->cells) != GHOSTTY_SUCCESS)
                        goto render_failed;

                while (ghostty_render_state_row_cells_next(terminal->cells)) {
                        uint8_t local[64];
                        GhosttyBuffer text = {local, sizeof(local), 0};
                        GhosttyStyle style = GHOSTTY_INIT_SIZED(GhosttyStyle);
                        GhosttyCell raw = 0;
                        GhosttyColorRgb background_rgb;
                        GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
                        bool hyperlink = false;
                        XtpRenderCell cell = {0};
                        uint8_t *allocated = NULL;
                        GhosttyResult result;

                        result = ghostty_render_state_row_cells_get(
                            terminal->cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8,
                            &text);
                        if (result == GHOSTTY_OUT_OF_SPACE) {
                                allocated = malloc(text.len);
                                if (allocated == NULL)
                                        goto render_failed;
                                text.ptr = allocated;
                                text.cap = text.len;
                                result = ghostty_render_state_row_cells_get(
                                    terminal->cells,
                                    GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &text);
                        }
                        if (result != GHOSTTY_SUCCESS ||
                            ghostty_render_state_row_cells_get(
                                terminal->cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE,
                                &style) != GHOSTTY_SUCCESS ||
                            ghostty_render_state_row_cells_get(
                                terminal->cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw) !=
                                GHOSTTY_SUCCESS ||
                            ghostty_cell_get(raw, GHOSTTY_CELL_DATA_WIDE, &wide) !=
                                GHOSTTY_SUCCESS ||
                            ghostty_cell_get(raw, GHOSTTY_CELL_DATA_HAS_HYPERLINK, &hyperlink) !=
                                GHOSTTY_SUCCESS) {
                                free(allocated);
                                goto render_failed;
                        }

                        cell.column = column;
                        cell.row = row;
                        cell.utf8 = (const char *)text.ptr;
                        cell.utf8_length = text.len;
                        cell.width = wide == GHOSTTY_CELL_WIDE_WIDE          ? 2U
                                     : wide == GHOSTTY_CELL_WIDE_SPACER_TAIL ? 0U
                                                                             : 1U;
                        cell.foreground = ConvertColor(style.fg_color, &colors,
                                                       style.bold && terminal->bold_colors);
                        cell.background = ConvertColor(style.bg_color, &colors, false);
                        if (cell.background.kind == XTP_COLOR_DEFAULT &&
                            ghostty_render_state_row_cells_get(
                                terminal->cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR,
                                &background_rgb) == GHOSTTY_SUCCESS)
                                cell.background = ConvertRgbColor(background_rgb);
                        cell.bold = style.bold;
                        cell.italic = style.italic;
                        cell.faint = style.faint;
                        cell.inverse = style.inverse;
                        cell.invisible = style.invisible;
                        cell.hyperlink = hyperlink;
                        cell.row_wrapped = row_wrapped;
                        cell.selected = row_selected && column >= selection.start_x &&
                                        column <= selection.end_x;
                        cell.strikethrough = style.strikethrough;
                        cell.overline = style.overline;
                        cell.underline = (XtpUnderline)style.underline;
                        cell.underline_color = ConvertColor(style.underline_color, &colors, false);
                        if (renderer->cell != NULL)
                                renderer->cell(&cell, closure);
                        ++rendered_cells;
                        if (cell.utf8_length != 0)
                                ++rendered_graphemes;
                        free(allocated);
                        ++column;
                }
                (void)ghostty_render_state_row_set(terminal->rows,
                                                   GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);
                ++row;
        }

        if (renderer->end != NULL)
                renderer->end(&frame, closure);
        XtpLog(XTP_LOG_DEBUG, "render",
               "frame mode=%s grid=%ux%u cells=%zu graphemes=%zu cursor=%s@%u,%u shape=%d "
               "blink-requested=%s screen-reverse=%s",
               frame.full_repaint ? "full" : "partial", frame.columns, frame.rows, rendered_cells,
               rendered_graphemes, frame.cursor_visible ? "visible" : "hidden", frame.cursor_column,
               frame.cursor_row, frame.cursor_shape,
               frame.cursor_blink_requested ? "true" : "false",
               frame.reverse_colors ? "true" : "false");
        {
                GhosttyRenderStateDirty clean = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
                (void)ghostty_render_state_set(terminal->render_state,
                                               GHOSTTY_RENDER_STATE_OPTION_DIRTY, &clean);
        }
        terminal->reverse_colors_initialized = true;
        terminal->reverse_colors = frame.reverse_colors;
        /* What is now on screen besides cells, for the next hold to compare with. */
        terminal->drawn = meta;
        terminal->drawn_valid = true;
        ReportColorSchemeChange(terminal);
        return 0;

render_failed:
        if (renderer->abort != NULL)
                renderer->abort(&frame, closure);
        return -1;
}

int
XtpTerminalEncodeKey(XtpTerminal *terminal, const XtpKeyEvent *event, char *buffer, size_t capacity,
                     size_t *written)
{
        GhosttyKeyAction action = GHOSTTY_KEY_ACTION_PRESS;
        GhosttyMods mods;

        if (terminal == NULL || event == NULL || event->key >= XTP_KEY_COUNT)
                return -1;
        mods = ConvertModifiers(event->modifiers);
        switch (event->action) {
        case XTP_KEY_ACTION_PRESS:
                action = GHOSTTY_KEY_ACTION_PRESS;
                break;
        case XTP_KEY_ACTION_REPEAT:
                action = GHOSTTY_KEY_ACTION_REPEAT;
                break;
        case XTP_KEY_ACTION_RELEASE:
                action = GHOSTTY_KEY_ACTION_RELEASE;
                break;
        }

        ghostty_key_encoder_setopt_from_terminal(terminal->key_encoder, terminal->handle);
        ghostty_key_event_set_action(terminal->key_event, action);
        ghostty_key_event_set_key(terminal->key_event, key_map[event->key]);
        ghostty_key_event_set_mods(terminal->key_event, mods);
        ghostty_key_event_set_consumed_mods(terminal->key_event, 0);
        ghostty_key_event_set_composing(terminal->key_event, false);
        ghostty_key_event_set_utf8(terminal->key_event, event->utf8, event->utf8_length);
        ghostty_key_event_set_unshifted_codepoint(terminal->key_event, event->unshifted_codepoint);
        if (ghostty_key_encoder_encode(terminal->key_encoder, terminal->key_event, buffer, capacity,
                                       written) != GHOSTTY_SUCCESS)
                return -1;
        XtpLog(XTP_LOG_DEBUG, "input",
               "encoded key=%d action=%d modifiers=0x%x text-bytes=%zu output-bytes=%zu",
               (int)event->key, (int)event->action, event->modifiers, event->utf8_length,
               written != NULL ? *written : 0U);
        return 0;
}

int
XtpTerminalEncodeFocus(XtpTerminal *terminal, bool focused, char *buffer, size_t capacity,
                       size_t *written)
{
        GhosttyTerminalModeConfig config = {
            .mode = GHOSTTY_MODE_FOCUS_EVENT,
            .value = false,
        };
        GhosttyFocusEvent event;

        if (terminal == NULL || buffer == NULL || written == NULL)
                return -1;
        *written = 0;
        if (!terminal->allow_mouse_ops)
                return 0;
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &config) !=
            GHOSTTY_SUCCESS)
                return -1;
        if (!config.value) {
                XtpLog(XTP_LOG_DEBUG, "input", "encoded focus=%s enabled=false output-bytes=0",
                       focused ? "in" : "out");
                return 0;
        }
        event = focused ? GHOSTTY_FOCUS_GAINED : GHOSTTY_FOCUS_LOST;
        if (ghostty_focus_encode(event, buffer, capacity, written) != GHOSTTY_SUCCESS)
                return -1;
        XtpLog(XTP_LOG_DEBUG, "input", "encoded focus=%s enabled=true output-bytes=%zu",
               focused ? "in" : "out", *written);
        return 0;
}

int
XtpTerminalEncodeMouse(XtpTerminal *terminal, const XtpMouseEvent *event, char *buffer,
                       size_t capacity, size_t *written)
{
        static const GhosttyMouseAction action_map[] = {
            [XTP_MOUSE_ACTION_PRESS] = GHOSTTY_MOUSE_ACTION_PRESS,
            [XTP_MOUSE_ACTION_RELEASE] = GHOSTTY_MOUSE_ACTION_RELEASE,
            [XTP_MOUSE_ACTION_MOTION] = GHOSTTY_MOUSE_ACTION_MOTION,
        };
        GhosttyMouseEncoderSize size;
        bool track_last_cell = true;
        GhosttyResult result;

        if (terminal == NULL || event == NULL || buffer == NULL || written == NULL ||
            event->action > XTP_MOUSE_ACTION_MOTION || event->button >= XTP_MOUSE_BUTTON_COUNT ||
            event->cell_width == 0 || event->cell_height == 0)
                return -1;
        *written = 0;
        if (!terminal->allow_mouse_ops)
                return 0;
        size = (GhosttyMouseEncoderSize){
            .size = sizeof(size),
            .screen_width = event->screen_width,
            .screen_height = event->screen_height,
            .cell_width = event->cell_width,
            .cell_height = event->cell_height,
            .padding_top = event->padding_top,
            .padding_bottom = event->padding_bottom,
            .padding_right = event->padding_right,
            .padding_left = event->padding_left,
        };
        ghostty_mouse_encoder_setopt_from_terminal(terminal->mouse_encoder, terminal->handle);
        ghostty_mouse_encoder_setopt(terminal->mouse_encoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE,
                                     &size);
        ghostty_mouse_encoder_setopt(terminal->mouse_encoder,
                                     GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED,
                                     &event->any_button_pressed);
        ghostty_mouse_encoder_setopt(terminal->mouse_encoder,
                                     GHOSTTY_MOUSE_ENCODER_OPT_TRACK_LAST_CELL, &track_last_cell);
        ghostty_mouse_event_set_action(terminal->mouse_event, action_map[event->action]);
        if (event->button == XTP_MOUSE_BUTTON_NONE)
                ghostty_mouse_event_clear_button(terminal->mouse_event);
        else
                ghostty_mouse_event_set_button(terminal->mouse_event,
                                               mouse_button_map[event->button]);
        ghostty_mouse_event_set_mods(terminal->mouse_event, ConvertModifiers(event->modifiers));
        ghostty_mouse_event_set_position(terminal->mouse_event,
                                         (GhosttyMousePosition){event->x, event->y});
        result = ghostty_mouse_encoder_encode(terminal->mouse_encoder, terminal->mouse_event,
                                              buffer, capacity, written);
        if (result != GHOSTTY_SUCCESS)
                return -1;
        XtpLog(XTP_LOG_DEBUG, "input",
               "encoded mouse action=%d button=%d modifiers=0x%x position=%.1f,%.1f "
               "output-bytes=%zu",
               event->action, event->button, event->modifiers, event->x, event->y, *written);
        return 0;
}

void
XtpTerminalSetRenderHold(XtpTerminal *terminal, XtpTerminalRenderHoldFn hold, void *closure)
{
        if (terminal == NULL)
                return;
        terminal->render_hold = hold;
        terminal->render_hold_closure = hold != NULL ? closure : NULL;
}

bool
XtpTerminalRenderHeld(const XtpTerminal *terminal)
{
        return terminal != NULL && terminal->render_held;
}

void
XtpTerminalSetEffects(XtpTerminal *terminal, const XtpTerminalEffects *effects)
{
        if (terminal == NULL)
                return;
        if (effects == NULL)
                memset(&terminal->effects, 0, sizeof(terminal->effects));
        else
                terminal->effects = *effects;
        /* xterm answers a denied OSC 52 query with silence, so the read
         * callback is only installed when a reply is permitted. */
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_CLIPBOARD_READ,
                                 terminal->effects.clipboard_read != NULL
                                     ? ClipboardReadEffectPointer()
                                     : NULL) != GHOSTTY_SUCCESS)
                XtpLog(XTP_LOG_ERROR, "terminal", "cannot configure clipboard read effect");
        XtpLog(XTP_LOG_INFO, "terminal",
               "effects write-pty=%s bell=%s title=%s clipboard-write=%s clipboard-read=%s",
               terminal->effects.write_pty != NULL ? "on" : "off",
               terminal->effects.bell != NULL ? "on" : "off",
               terminal->effects.title_changed != NULL ? "on" : "off",
               terminal->effects.clipboard_write != NULL ? "on" : "off",
               terminal->effects.clipboard_read != NULL ? "on" : "off");
}

const char *
XtpTerminalBackend(void)
{
        return "libghostty-vt";
}

bool
XtpTerminalBackendIsStub(void)
{
        return false;
}
