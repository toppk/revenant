#include "terminal.h"

#include "char_class.h"
#include "cursor_blink.h"
#include "diagnostics.h"

#include <ghostty/vt.h>

#include <stdlib.h>
#include <string.h>

struct XtpTerminal
{
        GhosttyTerminal handle;
        GhosttyRenderState render_state;
        GhosttyRenderStateRowIterator rows;
        GhosttyRenderStateRowCells cells;
        GhosttyKeyEncoder key_encoder;
        GhosttyKeyEvent keyEvent;
        GhosttyMouseEncoder mouse_encoder;
        GhosttyMouseEvent mouse_event;
        GhosttySelectionGesture selection_gesture;
        GhosttySelectionGestureEvent selection_press;
        GhosttySelectionGestureEvent selection_drag;
        GhosttySelectionGestureEvent selection_autoscroll;
        GhosttySelectionGestureEvent selection_release;
        GhosttyTrackedGridRef selection_extend_start;
        GhosttyTrackedGridRef selection_extend_end;
        GhosttySelectionGestureBehavior selection_extend_behavior;
        bool selection_extend_left;
        bool selection_extend_rectangle;
        bool reverse_colors_initialized;
        bool reverse_colors;
        XtpCursorBlinkObserver cursor_blink;
        XtpCharClassTable *char_classes;
        XtpTerminalEffects effects;
};

typedef struct
{
        XtpTerminal *terminal;
        const uint8_t *bytes;
        size_t written;
} CursorBlinkFeed;

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
        case XTP_TERMINAL_MODE_COUNT:
                break;
        }
        return ghostty_mode_new(0, false);
}

static void
WritePtyEffect(GhosttyTerminal handle, void *userdata, const uint8_t *bytes, size_t length)
{
        XtpTerminal *terminal = userdata;
        uint8_t rewritten[10];
        const uint8_t *output = bytes;

        (void)handle;
        if (length == 9U && memcmp(bytes, "\033[?12;", 6) == 0 && bytes[7] == '$' &&
            bytes[8] == 'y') {
                memcpy(rewritten, bytes, length);
                rewritten[6] = terminal->cursor_blink.blink_requested ? '1' : '2';
                output = rewritten;
        } else if (length == 10U && memcmp(bytes, "\033P1$r", 5) == 0 && bytes[6] == ' ' &&
                   bytes[7] == 'q' && bytes[8] == 0x1bU && bytes[9] == '\\' && bytes[5] >= '1' &&
                   bytes[5] <= '6') {
                memcpy(rewritten, bytes, length);
                if (terminal->cursor_blink.blink_requested) {
                        if ((rewritten[5] & 1U) == 0U)
                                --rewritten[5];
                } else if ((rewritten[5] & 1U) != 0U) {
                        ++rewritten[5];
                }
                output = rewritten;
        }
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
        if (terminal->effects.bell != NULL)
                XtpLog(XTP_LOG_INFO, "terminal", "BEL effect");
        if (terminal->effects.bell != NULL)
                terminal->effects.bell(terminal->effects.closure);
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

static const void *
WritePtyEffectPointer(void)
{
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

static void
FreeHandles(XtpTerminal *terminal)
{
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        ghostty_selection_gesture_event_free(terminal->selection_release);
        ghostty_selection_gesture_event_free(terminal->selection_autoscroll);
        ghostty_selection_gesture_event_free(terminal->selection_drag);
        ghostty_selection_gesture_event_free(terminal->selection_press);
        ghostty_selection_gesture_free(terminal->selection_gesture, terminal->handle);
        XtpCharClassFree(terminal->char_classes);
        ghostty_key_event_free(terminal->keyEvent);
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

        if (terminal == NULL)
                return NULL;

        XtpLog(XTP_LOG_INFO, "terminal",
               "creating backend=libghostty-vt grid=%ux%u cell=%ux%u graphemeWidth=%s", columns,
               rows, cell_width, cell_height, unicode_width ? "unicode" : "legacy");

        if (ghostty_terminal_new(NULL, &terminal->handle, columns, rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_new(NULL, &terminal->render_state) != GHOSTTY_SUCCESS ||
            ghostty_render_state_row_iterator_new(NULL, &terminal->rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_row_cells_new(NULL, &terminal->cells) != GHOSTTY_SUCCESS ||
            ghostty_key_encoder_new(NULL, &terminal->key_encoder) != GHOSTTY_SUCCESS ||
            ghostty_key_event_new(NULL, &terminal->keyEvent) != GHOSTTY_SUCCESS ||
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
                                 TitleEffectPointer()) != GHOSTTY_SUCCESS) {
                FreeHandles(terminal);
                free(terminal);
                return NULL;
        }

        return terminal;
}

XtpTerminal *
XtpTerminalNew(uint16_t columns, uint16_t rows, uint32_t cell_width, uint32_t cell_height)
{
        return XtpTerminalNewWithGraphemeWidth(columns, rows, cell_width, cell_height, false);
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
        if (terminal != NULL)
                XtpLog(XTP_LOG_DEBUG, "terminal", "feed bytes=%zu", length);
        if (terminal != NULL) {
                CursorBlinkFeed feed = {
                    .terminal = terminal,
                    .bytes = bytes,
                };
                XtpCursorBlinkObserverEffects effects = {
                    .before_change = CursorBlinkBeforeChange,
                    .reset = CursorBlinkResetEffect,
                    .closure = &feed,
                };

                XtpCursorBlinkObserverFeed(&terminal->cursor_blink, bytes, length, &effects);
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
        if (terminal == NULL)
                return -1;

        XtpLog(XTP_LOG_INFO, "terminal", "resize grid=%ux%u cell=%ux%u", columns, rows, cell_width,
               cell_height);

        return ghostty_terminal_resize(terminal->handle, columns, rows, cell_width, cell_height) ==
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
XtpTerminalSetCursorBlinkDefault(XtpTerminal *terminal, bool blinking)
{
        GhosttyResult result;

        if (terminal == NULL)
                return -1;
        XtpLog(XTP_LOG_INFO, "terminal", "cursor blink default=%s", blinking ? "true" : "false");
        /*
         * Retain the configured operand in libghostty for its reset and
         * default-style baseline. Presentation and mode reports use
         * Revenant's separate raw application state, so restore mode 12 after
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

static GhosttyResult
SelectionRef(XtpTerminal *terminal, uint16_t column, uint16_t row, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_VIEWPORT,
            {.coordinate = {column, row}},
        };

        return ghostty_terminal_grid_ref(terminal->handle, point, ref);
}

static int
InstallGestureSelection(XtpTerminal *terminal, GhosttyResult result, GhosttySelection *selection)
{
        if (result == GHOSTTY_NO_VALUE)
                return 0;
        if (result != GHOSTTY_SUCCESS ||
            ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, selection) !=
                GHOSTTY_SUCCESS)
                return -1;
        return 1;
}

static GhosttyResult
GridRefCharacterClass(XtpTerminal *terminal, const GhosttyGridRef *ref, int *character_class)
{
        GhosttyCell cell;
        uint32_t codepoint = 0;

        if (ghostty_grid_ref_cell(ref, &cell) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        *character_class = XtpCharClassOf(terminal->char_classes, codepoint);
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
ScreenSelectionRef(XtpTerminal *terminal, GhosttyPointCoordinate coordinate, GhosttyGridRef *ref)
{
        GhosttyPoint point = {
            GHOSTTY_POINT_TAG_SCREEN,
            {.coordinate = coordinate},
        };

        return ghostty_terminal_grid_ref(terminal->handle, point, ref);
}

static bool
GridRefRowWrapped(const GhosttyGridRef *ref)
{
        GhosttyRow row;
        bool wrapped = false;

        return ghostty_grid_ref_row(ref, &row) == GHOSTTY_SUCCESS &&
               ghostty_row_get(row, GHOSTTY_ROW_DATA_WRAP, &wrapped) == GHOSTTY_SUCCESS && wrapped;
}

static bool
GridRefDrawn(const GhosttyGridRef *ref)
{
        GhosttyCell cell;
        GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
        uint32_t codepoint = 0;

        if (ghostty_grid_ref_cell(ref, &cell) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint) != GHOSTTY_SUCCESS ||
            ghostty_cell_get(cell, GHOSTTY_CELL_DATA_WIDE, &wide) != GHOSTTY_SUCCESS)
                return false;
        return codepoint != 0 || wide != GHOSTTY_CELL_WIDE_NARROW;
}

static int
LastDrawnColumn(XtpTerminal *terminal, uint32_t screen_row, uint16_t columns)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate point;
        int column;

        point.y = screen_row;
        for (column = (int)columns - 1; column >= 0; --column) {
                point.x = (uint16_t)column;
                if (ScreenSelectionRef(terminal, point, &ref) != GHOSTTY_SUCCESS)
                        return -1;
                if (GridRefDrawn(&ref))
                        return column;
        }
        return -1;
}

static GhosttyResult
UndrawnSuffixSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyPointCoordinate point;
        GhosttyPointCoordinate first;
        GhosttyPointCoordinate last;
        uint16_t columns = 0;
        int last_drawn;

        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0)
                return GHOSTTY_INVALID_VALUE;
        last_drawn = LastDrawnColumn(terminal, point.y, columns);
        if ((int)point.x <= last_drawn)
                return GHOSTTY_NO_VALUE;
        first.x = (uint16_t)(last_drawn + 1);
        first.y = point.y;
        last.x = columns - 1U;
        last.y = point.y;
        if (ScreenSelectionRef(terminal, first, &selection->start) != GHOSTTY_SUCCESS ||
            ScreenSelectionRef(terminal, last, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static bool
PointBefore(GhosttyPointCoordinate left, GhosttyPointCoordinate right)
{
        return left.y < right.y || (left.y == right.y && left.x < right.x);
}

static bool
PointInUndrawnSuffix(XtpTerminal *terminal, GhosttyPointCoordinate point, uint16_t columns)
{
        return (int)point.x > LastDrawnColumn(terminal, point.y, columns);
}

static bool
AdvanceWordPoint(XtpTerminal *terminal, GhosttyPointCoordinate *point, uint16_t columns,
                 int direction)
{
        GhosttyGridRef edge = GHOSTTY_INIT_SIZED(GhosttyGridRef);

        if (direction < 0) {
                if (point->x != 0) {
                        --point->x;
                        return true;
                }
                if (point->y == 0) {
                        return false;
                }
                {
                        GhosttyPointCoordinate previous = {columns - 1U, point->y - 1U};

                        if (ScreenSelectionRef(terminal, previous, &edge) != GHOSTTY_SUCCESS ||
                            !GridRefRowWrapped(&edge))
                                return false;
                }
                --point->y;
                point->x = columns - 1U;
                return true;
        }
        if ((uint16_t)(point->x + 1U) < columns) {
                ++point->x;
                return true;
        }
        if (ScreenSelectionRef(terminal, *point, &edge) != GHOSTTY_SUCCESS ||
            !GridRefRowWrapped(&edge))
                return false;
        ++point->y;
        point->x = 0;
        return true;
}

static GhosttyResult
CharacterClassSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyPointCoordinate origin;
        GhosttyPointCoordinate first;
        GhosttyPointCoordinate last;
        GhosttyPointCoordinate probe_point;
        GhosttyGridRef probe = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        uint16_t columns = 0;
        int wanted_class;
        int probe_class;

        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &origin) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0 ||
            GridRefCharacterClass(terminal, &target, &wanted_class) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointInUndrawnSuffix(terminal, origin, columns))
                return GHOSTTY_NO_VALUE;
        first = origin;
        probe_point = first;
        while (AdvanceWordPoint(terminal, &probe_point, columns, -1)) {
                if (PointInUndrawnSuffix(terminal, probe_point, columns) ||
                    ScreenSelectionRef(terminal, probe_point, &probe) != GHOSTTY_SUCCESS ||
                    GridRefCharacterClass(terminal, &probe, &probe_class) != GHOSTTY_SUCCESS ||
                    probe_class != wanted_class)
                        break;
                first = probe_point;
        }
        last = origin;
        probe_point = last;
        while (AdvanceWordPoint(terminal, &probe_point, columns, 1)) {
                if (PointInUndrawnSuffix(terminal, probe_point, columns) ||
                    ScreenSelectionRef(terminal, probe_point, &probe) != GHOSTTY_SUCCESS ||
                    GridRefCharacterClass(terminal, &probe, &probe_class) != GHOSTTY_SUCCESS ||
                    probe_class != wanted_class)
                        break;
                last = probe_point;
        }
        if (ScreenSelectionRef(terminal, first, &selection->start) != GHOSTTY_SUCCESS ||
            ScreenSelectionRef(terminal, last, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
CharacterClassDragSelection(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                            GhosttySelection *selection)
{
        GhosttySelection anchor_word = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection target_word = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;
        GhosttyResult anchor_result;
        GhosttyResult target_result;

        anchor_result = CharacterClassSelection(terminal, anchor, &anchor_word);
        if (anchor_result == GHOSTTY_NO_VALUE)
                return GHOSTTY_NO_VALUE;
        target_result = CharacterClassSelection(terminal, target, &target_word);
        if (target_result == GHOSTTY_NO_VALUE)
                target_result = UndrawnSuffixSelection(terminal, target, &target_word);
        if (anchor_result != GHOSTTY_SUCCESS || target_result != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point)) {
                selection->start = target_word.start;
                selection->end = anchor_word.end;
        } else {
                selection->start = anchor_word.start;
                selection->end = target_word.end;
        }
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
ExpandCellSelectionForUndrawn(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                              GhosttySelection *selection)
{
        GhosttySelection undrawn = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection ordered = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;
        GhosttyResult result = UndrawnSuffixSelection(terminal, target, &undrawn);

        if (result == GHOSTTY_NO_VALUE)
                return GHOSTTY_SUCCESS;
        if (result != GHOSTTY_SUCCESS ||
            ghostty_terminal_selection_ordered(terminal->handle, selection,
                                               GHOSTTY_SELECTION_ORDER_FORWARD,
                                               &ordered) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point))
                ordered.start = undrawn.start;
        else
                ordered.end = undrawn.end;
        *selection = ordered;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
LineSelection(XtpTerminal *terminal, GhosttyGridRef target, GhosttySelection *selection)
{
        GhosttyTerminalSelectLineOptions options =
            GHOSTTY_INIT_SIZED(GhosttyTerminalSelectLineOptions);
        GhosttyPointCoordinate point;
        GhosttyGridRef drawn = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        uint16_t columns = 0;
        int last_drawn;
        GhosttyResult result;

        options.ref = target;
        result = ghostty_terminal_select_line(terminal->handle, &options, selection);
        if (result != GHOSTTY_NO_VALUE)
                return result;
        if (ghostty_terminal_point_from_grid_ref(
                terminal->handle, &target, GHOSTTY_POINT_TAG_SCREEN, &point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            columns == 0)
                return GHOSTTY_INVALID_VALUE;
        last_drawn = LastDrawnColumn(terminal, point.y, columns);
        if (last_drawn >= 0) {
                point.x = (uint16_t)last_drawn;
                if (ScreenSelectionRef(terminal, point, &drawn) != GHOSTTY_SUCCESS)
                        return GHOSTTY_INVALID_VALUE;
                options.ref = drawn;
                result = ghostty_terminal_select_line(terminal->handle, &options, selection);
                if (result != GHOSTTY_NO_VALUE)
                        return result;
        }
        point.x = 0;
        if (ScreenSelectionRef(terminal, point, &selection->start) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        point.x = columns - 1U;
        if (ScreenSelectionRef(terminal, point, &selection->end) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static GhosttyResult
LineDragSelection(XtpTerminal *terminal, GhosttyGridRef anchor, GhosttyGridRef target,
                  GhosttySelection *selection)
{
        GhosttySelection anchor_line = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection target_line = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate anchor_point;
        GhosttyPointCoordinate target_point;

        if (LineSelection(terminal, anchor, &anchor_line) != GHOSTTY_SUCCESS ||
            LineSelection(terminal, target, &target_line) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &anchor,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &anchor_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS)
                return GHOSTTY_INVALID_VALUE;
        if (PointBefore(target_point, anchor_point)) {
                selection->start = target_line.start;
                selection->end = anchor_line.end;
        } else {
                selection->start = anchor_line.start;
                selection->end = target_line.end;
        }
        selection->rectangle = false;
        return GHOSTTY_SUCCESS;
}

static int
InstallDragSelection(XtpTerminal *terminal, GhosttyResult result, GhosttyGridRef target,
                     bool rectangle, GhosttySelection *selection)
{
        GhosttyGridRef anchor = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelectionGestureBehavior behavior = GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;

        if (ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_BEHAVIOR,
                                          &behavior) == GHOSTTY_SUCCESS &&
            ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_ANCHOR,
                                          &anchor) == GHOSTTY_SUCCESS) {
                if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD)
                        result = CharacterClassDragSelection(terminal, anchor, target, selection);
                else if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE)
                        result = LineDragSelection(terminal, anchor, target, selection);
                else if (!rectangle && result == GHOSTTY_SUCCESS)
                        result = ExpandCellSelectionForUndrawn(terminal, anchor, target, selection);
        }
        return InstallGestureSelection(terminal, result, selection);
}

int
XtpTerminalSelectionStart(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                          double surface_y, uint64_t time_ns, XtpSelectionUnit unit, bool repeat)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelectionGestureBehavior behavior;
        GhosttySelectionGestureBehaviors behaviors;
        GhosttySurfacePosition position = {surface_x, surface_y};
        const uint64_t repeat_interval_ns = UINT64_MAX;
        const double repeat_distance = 1.0e100;
        GhosttyResult result;

        if (terminal == NULL || unit > XTP_SELECTION_LINE)
                return -1;
        behavior = unit == XTP_SELECTION_WORD   ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD
                   : unit == XTP_SELECTION_LINE ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE
                                                : GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        behaviors.single_click = behavior;
        behaviors.double_click = behavior;
        behaviors.triple_click = behavior;
        if (!repeat)
                ghostty_selection_gesture_reset(terminal->selection_gesture, terminal->handle);
        if (ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, NULL) !=
                GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF,
                                                &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_TIME_NS,
                                                &time_ns) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(
                terminal->selection_press, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_INTERVAL_NS,
                &repeat_interval_ns) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REPEAT_DISTANCE,
                                                &repeat_distance) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_press,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_BEHAVIORS,
                                                &behaviors) != GHOSTTY_SUCCESS)
                return -1;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_press, &selection);
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD) {
                GhosttyGridRef anchor = GHOSTTY_INIT_SIZED(GhosttyGridRef);

                if (ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                                  GHOSTTY_SELECTION_GESTURE_DATA_ANCHOR,
                                                  &anchor) == GHOSTTY_SUCCESS)
                        result = CharacterClassDragSelection(terminal, anchor, ref, &selection);
                else
                        result = CharacterClassSelection(terminal, ref, &selection);
                return InstallGestureSelection(terminal, result, &selection);
        }
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE) {
                result = LineSelection(terminal, ref, &selection);
                return InstallGestureSelection(terminal, result, &selection);
        }
        return InstallGestureSelection(terminal, result, &selection);
}

int
XtpTerminalSelectionExtend(XtpTerminal *terminal, uint16_t column, uint16_t row, double surface_x,
                           double surface_y, uint32_t columns, uint32_t cell_width,
                           uint32_t padding_left, uint32_t screen_height, bool rectangle)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySurfacePosition position = {surface_x, surface_y};
        GhosttySelectionGestureGeometry geometry = {
            columns,
            cell_width,
            padding_left,
            screen_height,
        };
        GhosttyResult result;

        if (terminal == NULL || SelectionRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF,
                                                &ref) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY,
                                                &geometry) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_drag,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_RECTANGLE,
                                                &rectangle) != GHOSTTY_SUCCESS)
                return -1;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_drag, &selection);
        return InstallDragSelection(terminal, result, ref, rectangle, &selection);
}

int
XtpTerminalSelectionGetAutoscroll(XtpTerminal *terminal, XtpSelectionAutoscroll *direction)
{
        GhosttySelectionGestureAutoscroll ghostty_direction;

        if (terminal == NULL || direction == NULL ||
            ghostty_selection_gesture_get(terminal->selection_gesture, terminal->handle,
                                          GHOSTTY_SELECTION_GESTURE_DATA_AUTOSCROLL,
                                          &ghostty_direction) != GHOSTTY_SUCCESS)
                return -1;
        if (ghostty_direction == GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_UP)
                *direction = XTP_SELECTION_AUTOSCROLL_UP;
        else if (ghostty_direction == GHOSTTY_SELECTION_GESTURE_AUTOSCROLL_DOWN)
                *direction = XTP_SELECTION_AUTOSCROLL_DOWN;
        else
                *direction = XTP_SELECTION_AUTOSCROLL_NONE;
        return 0;
}

int
XtpTerminalSelectionAutoscrollTick(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                   double surface_x, double surface_y, uint32_t columns,
                                   uint32_t cell_width, uint32_t padding_left,
                                   uint32_t screen_height, bool rectangle)
{
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate viewport = {column, row};
        GhosttySurfacePosition position = {surface_x, surface_y};
        GhosttySelectionGestureGeometry geometry = {
            columns,
            cell_width,
            padding_left,
            screen_height,
        };
        GhosttyResult result;

        if (terminal == NULL ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_VIEWPORT,
                                                &viewport) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_POSITION,
                                                &position) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_GEOMETRY,
                                                &geometry) != GHOSTTY_SUCCESS ||
            ghostty_selection_gesture_event_set(terminal->selection_autoscroll,
                                                GHOSTTY_SELECTION_GESTURE_EVENT_OPT_RECTANGLE,
                                                &rectangle) != GHOSTTY_SUCCESS)
                return -1;
        result = ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                                 terminal->selection_autoscroll, &selection);
        if (result == GHOSTTY_NO_VALUE)
                return 0;
        if (result != GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS)
                return -1;
        return InstallDragSelection(terminal, result, target, rectangle, &selection);
}

void
XtpTerminalSelectionEnd(XtpTerminal *terminal, uint16_t column, uint16_t row, bool valid)
{
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);

        if (terminal == NULL)
                return;
        if (valid && SelectionRef(terminal, column, row, &ref) == GHOSTTY_SUCCESS)
                (void)ghostty_selection_gesture_event_set(
                    terminal->selection_release, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, &ref);
        else
                (void)ghostty_selection_gesture_event_set(
                    terminal->selection_release, GHOSTTY_SELECTION_GESTURE_EVENT_OPT_REF, NULL);
        (void)ghostty_selection_gesture_event(terminal->selection_gesture, terminal->handle,
                                              terminal->selection_release, NULL);
}

static uint64_t
SelectionCoordinate(GhosttyPointCoordinate point, uint16_t columns)
{
        return (uint64_t)point.y * columns + point.x;
}

static GhosttyResult
SelectionForBehavior(XtpTerminal *terminal, GhosttyGridRef target,
                     GhosttySelectionGestureBehavior behavior, GhosttySelection *selection)
{
        GhosttyResult result;

        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD) {
                result = CharacterClassSelection(terminal, target, selection);
                if (result == GHOSTTY_NO_VALUE)
                        result = UndrawnSuffixSelection(terminal, target, selection);
                return result;
        }
        if (behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE) {
                return LineSelection(terminal, target, selection);
        }
        result = UndrawnSuffixSelection(terminal, target, selection);
        if (result == GHOSTTY_NO_VALUE) {
                selection->start = target;
                selection->end = target;
                selection->rectangle = false;
                return GHOSTTY_SUCCESS;
        }
        return result;
}

static GhosttyResult
TrackSelectionPoint(XtpTerminal *terminal, GhosttyPointCoordinate coordinate,
                    GhosttyTrackedGridRef *tracked)
{
        GhosttyPoint point;

        point.tag = GHOSTTY_POINT_TAG_SCREEN;
        point.value.coordinate = coordinate;
        return ghostty_terminal_grid_ref_track(terminal->handle, point, tracked);
}

int
XtpTerminalSelectionExtendStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                XtpSelectionUnit unit)
{
        GhosttySelection current = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection ordered = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyPointCoordinate start_point;
        GhosttyPointCoordinate end_point;
        GhosttyPointCoordinate target_point;
        uint16_t columns = 0;
        uint64_t start_distance;
        uint64_t end_distance;
        uint64_t target_coordinate;
        uint64_t endpoint_coordinate;
        GhosttySelectionGestureBehavior behavior;

        if (terminal == NULL || unit > XTP_SELECTION_LINE ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_SELECTION, &current) !=
                GHOSTTY_SUCCESS ||
            ghostty_terminal_selection_ordered(terminal->handle, &current,
                                               GHOSTTY_SELECTION_ORDER_FORWARD,
                                               &ordered) != GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &ordered.start,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &start_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &ordered.end,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &end_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS)
                return 0;
        behavior = unit == XTP_SELECTION_WORD   ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_WORD
                   : unit == XTP_SELECTION_LINE ? GHOSTTY_SELECTION_GESTURE_BEHAVIOR_LINE
                                                : GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        if (current.rectangle)
                behavior = GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL;
        target_coordinate = SelectionCoordinate(target_point, columns);
        endpoint_coordinate = SelectionCoordinate(start_point, columns);
        start_distance = target_coordinate > endpoint_coordinate
                             ? target_coordinate - endpoint_coordinate
                             : endpoint_coordinate - target_coordinate;
        endpoint_coordinate = SelectionCoordinate(end_point, columns);
        end_distance = target_coordinate > endpoint_coordinate
                           ? target_coordinate - endpoint_coordinate
                           : endpoint_coordinate - target_coordinate;
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        terminal->selection_extend_start = NULL;
        terminal->selection_extend_end = NULL;
        if (TrackSelectionPoint(terminal, start_point, &terminal->selection_extend_start) !=
                GHOSTTY_SUCCESS ||
            TrackSelectionPoint(terminal, end_point, &terminal->selection_extend_end) !=
                GHOSTTY_SUCCESS) {
                XtpTerminalSelectionExtendEnd(terminal);
                return -1;
        }
        terminal->selection_extend_left =
            start_distance < end_distance ||
            target_coordinate < SelectionCoordinate(start_point, columns);
        terminal->selection_extend_rectangle = current.rectangle;
        terminal->selection_extend_behavior = behavior;
        return XtpTerminalSelectionExtendActive(terminal, column, row, current.rectangle);
}

int
XtpTerminalSelectionExtendActive(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                 bool rectangle)
{
        GhosttyGridRef original_start = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyGridRef original_end = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttyGridRef target = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        GhosttySelection target_selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
        GhosttyPointCoordinate start_point;
        GhosttyPointCoordinate end_point;
        GhosttyPointCoordinate target_point;
        uint16_t columns = 0;
        uint64_t target_coordinate;
        uint64_t start_coordinate;
        uint64_t end_coordinate;
        uint64_t unit_offset;

        if (terminal == NULL || terminal->selection_extend_start == NULL ||
            terminal->selection_extend_end == NULL ||
            ghostty_tracked_grid_ref_snapshot(terminal->selection_extend_start, &original_start) !=
                GHOSTTY_SUCCESS ||
            ghostty_tracked_grid_ref_snapshot(terminal->selection_extend_end, &original_end) !=
                GHOSTTY_SUCCESS ||
            SelectionRef(terminal, column, row, &target) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &original_start,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &start_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &original_end,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &end_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_point_from_grid_ref(terminal->handle, &target,
                                                 GHOSTTY_POINT_TAG_SCREEN,
                                                 &target_point) != GHOSTTY_SUCCESS ||
            ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_COLS, &columns) !=
                GHOSTTY_SUCCESS ||
            SelectionForBehavior(terminal, target, terminal->selection_extend_behavior,
                                 &target_selection) != GHOSTTY_SUCCESS)
                return -1;
        start_coordinate = SelectionCoordinate(start_point, columns);
        end_coordinate = SelectionCoordinate(end_point, columns);
        target_coordinate = SelectionCoordinate(target_point, columns);
        unit_offset = terminal->selection_extend_behavior == GHOSTTY_SELECTION_GESTURE_BEHAVIOR_CELL
                          ? 0U
                          : 1U;
        if (terminal->selection_extend_left && target_coordinate + unit_offset > end_coordinate)
                terminal->selection_extend_left = false;
        else if (!terminal->selection_extend_left && target_coordinate < start_coordinate)
                terminal->selection_extend_left = true;

        if (terminal->selection_extend_left) {
                selection.start = original_end;
                selection.end = target_selection.start;
        } else {
                selection.start = original_start;
                selection.end = target_selection.end;
        }
        selection.rectangle = terminal->selection_extend_rectangle && rectangle;
        return ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, &selection) ==
                       GHOSTTY_SUCCESS
                   ? 1
                   : -1;
}

void
XtpTerminalSelectionExtendEnd(XtpTerminal *terminal)
{
        if (terminal == NULL)
                return;
        ghostty_tracked_grid_ref_free(terminal->selection_extend_end);
        ghostty_tracked_grid_ref_free(terminal->selection_extend_start);
        terminal->selection_extend_start = NULL;
        terminal->selection_extend_end = NULL;
}

void
XtpTerminalSelectionClear(XtpTerminal *terminal)
{
        if (terminal == NULL)
                return;
        XtpTerminalSelectionExtendEnd(terminal);
        ghostty_selection_gesture_reset(terminal->selection_gesture, terminal->handle);
        (void)ghostty_terminal_set(terminal->handle, GHOSTTY_TERMINAL_OPT_SELECTION, NULL);
}

int
XtpTerminalSelectionText(XtpTerminal *terminal, uint8_t **bytes, size_t *length)
{
        GhosttyTerminalSelectionFormatOptions options =
            GHOSTTY_INIT_SIZED(GhosttyTerminalSelectionFormatOptions);
        uint8_t *ghostty_bytes = NULL;
        uint8_t *copy;
        size_t ghostty_length = 0;

        if (terminal == NULL || bytes == NULL || length == NULL)
                return -1;
        *bytes = NULL;
        *length = 0;
        options.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
        options.unwrap = true;
        options.trim = true;
        if (ghostty_terminal_selection_format_alloc(terminal->handle, NULL, options, &ghostty_bytes,
                                                    &ghostty_length) != GHOSTTY_SUCCESS)
                return -1;
        copy = malloc(ghostty_length != 0 ? ghostty_length : 1U);
        if (copy == NULL) {
                ghostty_free(NULL, ghostty_bytes, ghostty_length);
                return -1;
        }
        if (ghostty_length != 0)
                memcpy(copy, ghostty_bytes, ghostty_length);
        ghostty_free(NULL, ghostty_bytes, ghostty_length);
        *bytes = copy;
        *length = ghostty_length;
        return 0;
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
        if (SelectionRef(terminal, column, row, &ref) != GHOSTTY_SUCCESS)
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

bool
XtpTerminalMouseTracking(XtpTerminal *terminal)
{
        bool tracking = false;

        if (terminal != NULL)
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
        return 0;
}

static XtpColor
ConvertColor(GhosttyStyleColor color, const GhosttyRenderStateColors *colors)
{
        XtpColor result = {XTP_COLOR_DEFAULT, 0, 0, 0, 0};
        GhosttyColorRgb rgb;

        if (color.tag == GHOSTTY_STYLE_COLOR_PALETTE) {
                result.kind = XTP_COLOR_PALETTE;
                result.palette = color.value.palette;
                rgb = colors->palette[color.value.palette];
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

int
XtpTerminalRender(XtpTerminal *terminal, const XtpRenderer *renderer, void *closure,
                  bool force_full)
{
        GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
        GhosttyTerminalModeConfig reverse_colors = {GHOSTTY_MODE_REVERSE_COLORS, false};
        XtpRenderFrame frame = {0};
        GhosttyRenderStateDirty dirty;
        GhosttyRenderStateCursorVisualStyle cursor_style;
        bool reverse_colors_changed;
        uint16_t row = 0;
        bool cursor_in_viewport = false;
        bool cursor_wide_tail = false;
        size_t rendered_cells = 0;
        size_t rendered_graphemes = 0;

        if (terminal == NULL || renderer == NULL)
                return -1;
        if (ghostty_render_state_update(terminal->render_state, terminal->handle) !=
            GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot update render state");
                return -1;
        }
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_COLORS,
                                     &colors) != GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read render-state colors");
                return -1;
        }
        if (ghostty_terminal_get(terminal->handle, GHOSTTY_TERMINAL_DATA_MODE, &reverse_colors) !=
            GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read reverse-colors mode");
                return -1;
        }
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_COLS,
                                     &frame.columns) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_ROWS,
                                     &frame.rows) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_DIRTY,
                                     &dirty) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,
                                     &frame.cursor_visible) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE,
                                     &cursor_style) != GHOSTTY_SUCCESS ||
            ghostty_render_state_get(terminal->render_state,
                                     GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,
                                     &cursor_in_viewport) != GHOSTTY_SUCCESS) {
                XtpLog(XTP_LOG_ERROR, "render", "cannot read render-state metadata");
                return -1;
        }
        frame.cursor_blink_requested = terminal->cursor_blink.blink_requested;

        frame.reverse_colors = reverse_colors.value;
        reverse_colors_changed = terminal->reverse_colors_initialized &&
                                 terminal->reverse_colors != frame.reverse_colors;
        frame.full_repaint =
            force_full || dirty == GHOSTTY_RENDER_STATE_DIRTY_FULL || reverse_colors_changed;
        if (reverse_colors_changed)
                XtpLog(XTP_LOG_INFO, "render", "screen reverse changed enabled=%s",
                       frame.reverse_colors ? "true" : "false");
        if (cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE)
                frame.cursor_shape = XTP_CURSOR_SHAPE_UNDERLINE;
        else if (cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR)
                frame.cursor_shape = XTP_CURSOR_SHAPE_BAR;
        else if (cursor_style == GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW)
                frame.cursor_shape = XTP_CURSOR_SHAPE_BLOCK_HOLLOW;
        else
                frame.cursor_shape = XTP_CURSOR_SHAPE_BLOCK;
        frame.cursor_visible = frame.cursor_visible && cursor_in_viewport;
        if (frame.cursor_visible) {
                if (ghostty_render_state_get(terminal->render_state,
                                             GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,
                                             &frame.cursor_column) != GHOSTTY_SUCCESS ||
                    ghostty_render_state_get(terminal->render_state,
                                             GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,
                                             &frame.cursor_row) != GHOSTTY_SUCCESS ||
                    ghostty_render_state_get(terminal->render_state,
                                             GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_WIDE_TAIL,
                                             &cursor_wide_tail) != GHOSTTY_SUCCESS)
                        return -1;
                if (cursor_wide_tail && frame.cursor_column != 0)
                        --frame.cursor_column;
        }

        if (!frame.full_repaint && dirty == GHOSTTY_RENDER_STATE_DIRTY_FALSE)
                XtpLog(XTP_LOG_DEBUG, "render", "frame has no cell damage; checking cursor");

        if (renderer->begin != NULL)
                renderer->begin(&frame, closure);
        if (ghostty_render_state_get(terminal->render_state, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,
                                     &terminal->rows) != GHOSTTY_SUCCESS)
                return -1;

        while (ghostty_render_state_row_iterator_next(terminal->rows)) {
                uint16_t column = 0;
                bool row_dirty = false;
                bool clean = false;
                bool row_selected = false;
                GhosttyRenderStateRowSelection selection =
                    GHOSTTY_INIT_SIZED(GhosttyRenderStateRowSelection);

                if (ghostty_render_state_row_get(terminal->rows,
                                                 GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY,
                                                 &row_dirty) != GHOSTTY_SUCCESS)
                        return -1;
                if (!frame.full_repaint && !row_dirty) {
                        ++row;
                        continue;
                }
                row_selected = ghostty_render_state_row_get(terminal->rows,
                                                            GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION,
                                                            &selection) == GHOSTTY_SUCCESS;
                if (ghostty_render_state_row_get(terminal->rows,
                                                 GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,
                                                 &terminal->cells) != GHOSTTY_SUCCESS)
                        return -1;

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
                                        return -1;
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
                                return -1;
                        }

                        cell.column = column;
                        cell.row = row;
                        cell.utf8 = (const char *)text.ptr;
                        cell.utf8_length = text.len;
                        cell.width = wide == GHOSTTY_CELL_WIDE_WIDE          ? 2U
                                     : wide == GHOSTTY_CELL_WIDE_SPACER_TAIL ? 0U
                                                                             : 1U;
                        cell.foreground = ConvertColor(style.fg_color, &colors);
                        cell.background = ConvertColor(style.bg_color, &colors);
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
                        cell.selected = row_selected && column >= selection.start_x &&
                                        column <= selection.end_x;
                        cell.strikethrough = style.strikethrough;
                        cell.overline = style.overline;
                        cell.underline = style.underline;
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
        return 0;
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
        ghostty_key_event_set_action(terminal->keyEvent, action);
        ghostty_key_event_set_key(terminal->keyEvent, key_map[event->key]);
        ghostty_key_event_set_mods(terminal->keyEvent, mods);
        ghostty_key_event_set_consumed_mods(terminal->keyEvent, 0);
        ghostty_key_event_set_composing(terminal->keyEvent, false);
        ghostty_key_event_set_utf8(terminal->keyEvent, event->utf8, event->utf8_length);
        ghostty_key_event_set_unshifted_codepoint(terminal->keyEvent, event->unshifted_codepoint);
        if (ghostty_key_encoder_encode(terminal->key_encoder, terminal->keyEvent, buffer, capacity,
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
XtpTerminalSetEffects(XtpTerminal *terminal, const XtpTerminalEffects *effects)
{
        if (terminal == NULL)
                return;
        if (effects == NULL)
                memset(&terminal->effects, 0, sizeof(terminal->effects));
        else
                terminal->effects = *effects;
        XtpLog(XTP_LOG_INFO, "terminal", "effects write-pty=%s bell=%s title=%s",
               terminal->effects.write_pty != NULL ? "on" : "off",
               terminal->effects.bell != NULL ? "on" : "off",
               terminal->effects.title_changed != NULL ? "on" : "off");
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
