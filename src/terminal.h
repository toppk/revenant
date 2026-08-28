#ifndef XTERM_PLUS_TERMINAL_H
#define XTERM_PLUS_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct XtpTerminal XtpTerminal;

typedef enum
{
        XTP_COLOR_DEFAULT,
        XTP_COLOR_PALETTE,
        XTP_COLOR_RGB,
} XtpColorKind;

typedef struct
{
        XtpColorKind kind;
        uint8_t palette;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
} XtpColor;

typedef enum
{
        XTP_CURSOR_SHAPE_BLOCK,
        XTP_CURSOR_SHAPE_UNDERLINE,
        XTP_CURSOR_SHAPE_BAR,
        XTP_CURSOR_SHAPE_BLOCK_HOLLOW,
} XtpCursorShape;

typedef struct
{
        uint16_t columns;
        uint16_t rows;
        bool full_repaint;
        bool cursor_visible;
        uint16_t cursor_column;
        uint16_t cursor_row;
        XtpCursorShape cursor_shape;
        bool cursor_blinking;
} XtpRenderFrame;

typedef struct
{
        uint16_t column;
        uint16_t row;
        const char *utf8;
        size_t utf8_length;
        /* Zero marks a wide-cell continuation; otherwise the occupied columns. */
        uint8_t width;
        XtpColor foreground;
        XtpColor background;
        bool bold;
        bool italic;
        bool faint;
        bool inverse;
        bool invisible;
        bool hyperlink;
        bool selected;
        bool strikethrough;
        bool overline;
        int underline;
} XtpRenderCell;

typedef struct
{
        void (*begin)(const XtpRenderFrame *frame, void *closure);
        void (*cell)(const XtpRenderCell *cell, void *closure);
        void (*end)(const XtpRenderFrame *frame, void *closure);
} XtpRenderer;

typedef enum
{
        XTP_KEY_UNIDENTIFIED,
        XTP_KEY_BACKQUOTE,
        XTP_KEY_BACKSLASH,
        XTP_KEY_BRACKET_LEFT,
        XTP_KEY_BRACKET_RIGHT,
        XTP_KEY_COMMA,
        XTP_KEY_0,
        XTP_KEY_1,
        XTP_KEY_2,
        XTP_KEY_3,
        XTP_KEY_4,
        XTP_KEY_5,
        XTP_KEY_6,
        XTP_KEY_7,
        XTP_KEY_8,
        XTP_KEY_9,
        XTP_KEY_EQUAL,
        XTP_KEY_A,
        XTP_KEY_B,
        XTP_KEY_C,
        XTP_KEY_D,
        XTP_KEY_E,
        XTP_KEY_F,
        XTP_KEY_G,
        XTP_KEY_H,
        XTP_KEY_I,
        XTP_KEY_J,
        XTP_KEY_K,
        XTP_KEY_L,
        XTP_KEY_M,
        XTP_KEY_N,
        XTP_KEY_O,
        XTP_KEY_P,
        XTP_KEY_Q,
        XTP_KEY_R,
        XTP_KEY_S,
        XTP_KEY_T,
        XTP_KEY_U,
        XTP_KEY_V,
        XTP_KEY_W,
        XTP_KEY_X,
        XTP_KEY_Y,
        XTP_KEY_Z,
        XTP_KEY_MINUS,
        XTP_KEY_PERIOD,
        XTP_KEY_QUOTE,
        XTP_KEY_SEMICOLON,
        XTP_KEY_SLASH,
        XTP_KEY_ALT_LEFT,
        XTP_KEY_ALT_RIGHT,
        XTP_KEY_BACKSPACE,
        XTP_KEY_CAPS_LOCK,
        XTP_KEY_CONTROL_LEFT,
        XTP_KEY_CONTROL_RIGHT,
        XTP_KEY_ENTER,
        XTP_KEY_META_LEFT,
        XTP_KEY_META_RIGHT,
        XTP_KEY_SHIFT_LEFT,
        XTP_KEY_SHIFT_RIGHT,
        XTP_KEY_SPACE,
        XTP_KEY_TAB,
        XTP_KEY_DELETE,
        XTP_KEY_END,
        XTP_KEY_HOME,
        XTP_KEY_INSERT,
        XTP_KEY_PAGE_DOWN,
        XTP_KEY_PAGE_UP,
        XTP_KEY_ARROW_DOWN,
        XTP_KEY_ARROW_LEFT,
        XTP_KEY_ARROW_RIGHT,
        XTP_KEY_ARROW_UP,
        XTP_KEY_NUM_LOCK,
        XTP_KEY_NUMPAD_0,
        XTP_KEY_NUMPAD_1,
        XTP_KEY_NUMPAD_2,
        XTP_KEY_NUMPAD_3,
        XTP_KEY_NUMPAD_4,
        XTP_KEY_NUMPAD_5,
        XTP_KEY_NUMPAD_6,
        XTP_KEY_NUMPAD_7,
        XTP_KEY_NUMPAD_8,
        XTP_KEY_NUMPAD_9,
        XTP_KEY_NUMPAD_ADD,
        XTP_KEY_NUMPAD_DECIMAL,
        XTP_KEY_NUMPAD_DIVIDE,
        XTP_KEY_NUMPAD_ENTER,
        XTP_KEY_NUMPAD_MULTIPLY,
        XTP_KEY_NUMPAD_SUBTRACT,
        XTP_KEY_ESCAPE,
        XTP_KEY_F1,
        XTP_KEY_F2,
        XTP_KEY_F3,
        XTP_KEY_F4,
        XTP_KEY_F5,
        XTP_KEY_F6,
        XTP_KEY_F7,
        XTP_KEY_F8,
        XTP_KEY_F9,
        XTP_KEY_F10,
        XTP_KEY_F11,
        XTP_KEY_F12,
        XTP_KEY_COUNT,
} XtpKey;

#define XTP_MOD_SHIFT (1U << 0)
#define XTP_MOD_CONTROL (1U << 1)
#define XTP_MOD_ALT (1U << 2)
#define XTP_MOD_SUPER (1U << 3)
#define XTP_MOD_CAPS_LOCK (1U << 4)
#define XTP_MOD_NUM_LOCK (1U << 5)

typedef enum
{
        XTP_KEY_ACTION_PRESS,
        XTP_KEY_ACTION_REPEAT,
        XTP_KEY_ACTION_RELEASE,
} XtpKeyAction;

typedef struct
{
        XtpKeyAction action;
        XtpKey key;
        unsigned int modifiers;
        const char *utf8;
        size_t utf8_length;
        uint32_t unshifted_codepoint;
} XtpKeyEvent;

typedef enum
{
        XTP_MOUSE_ACTION_PRESS,
        XTP_MOUSE_ACTION_RELEASE,
        XTP_MOUSE_ACTION_MOTION,
} XtpMouseAction;

typedef enum
{
        XTP_MOUSE_BUTTON_NONE,
        XTP_MOUSE_BUTTON_LEFT,
        XTP_MOUSE_BUTTON_MIDDLE,
        XTP_MOUSE_BUTTON_RIGHT,
        XTP_MOUSE_BUTTON_FOUR,
        XTP_MOUSE_BUTTON_FIVE,
        XTP_MOUSE_BUTTON_SIX,
        XTP_MOUSE_BUTTON_SEVEN,
        XTP_MOUSE_BUTTON_EIGHT,
        XTP_MOUSE_BUTTON_NINE,
        XTP_MOUSE_BUTTON_TEN,
        XTP_MOUSE_BUTTON_ELEVEN,
        XTP_MOUSE_BUTTON_COUNT,
} XtpMouseButton;

typedef struct
{
        XtpMouseAction action;
        XtpMouseButton button;
        unsigned int modifiers;
        float x;
        float y;
        uint32_t screen_width;
        uint32_t screen_height;
        uint32_t cell_width;
        uint32_t cell_height;
        uint32_t padding_top;
        uint32_t padding_bottom;
        uint32_t padding_left;
        uint32_t padding_right;
        bool any_button_pressed;
} XtpMouseEvent;

typedef struct
{
        void (*write_pty)(const uint8_t *bytes, size_t length, void *closure);
        void (*bell)(void *closure);
        void (*title_changed)(const char *title, size_t length, void *closure);
        void *closure;
} XtpTerminalEffects;

typedef struct
{
        uint64_t total;
        uint64_t offset;
        uint64_t length;
} XtpTerminalScrollbar;

typedef enum
{
        XTP_TERMINAL_MODE_BACKARROW_KEY,
        XTP_TERMINAL_MODE_NUMLOCK_KEYPAD,
        XTP_TERMINAL_MODE_ALT_SENDS_ESCAPE,
        XTP_TERMINAL_MODE_META_SENDS_ESCAPE,
        XTP_TERMINAL_MODE_AUTOWRAP,
        XTP_TERMINAL_MODE_REVERSE_WRAP,
        XTP_TERMINAL_MODE_AUTOLINEFEED,
        XTP_TERMINAL_MODE_APPLICATION_CURSOR,
        XTP_TERMINAL_MODE_APPLICATION_KEYPAD,
        XTP_TERMINAL_MODE_ALLOW_132,
        XTP_TERMINAL_MODE_COUNT,
} XtpTerminalMode;

typedef enum
{
        XTP_SELECTION_CELL,
        XTP_SELECTION_WORD,
        XTP_SELECTION_LINE,
} XtpSelectionUnit;

typedef enum
{
        XTP_SELECTION_AUTOSCROLL_NONE,
        XTP_SELECTION_AUTOSCROLL_UP,
        XTP_SELECTION_AUTOSCROLL_DOWN,
} XtpSelectionAutoscroll;

XtpTerminal *XtpTerminalNew(uint16_t columns, uint16_t rows, uint32_t cell_width,
                            uint32_t cell_height);
void XtpTerminalFree(XtpTerminal *terminal);
void XtpTerminalFeed(XtpTerminal *terminal, const uint8_t *bytes, size_t length);
int XtpTerminalFeedOutput(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                          bool scroll_tty_output);
int XtpTerminalResize(XtpTerminal *terminal, uint16_t columns, uint16_t rows, uint32_t cell_width,
                      uint32_t cell_height);
int XtpTerminalRender(XtpTerminal *terminal, const XtpRenderer *renderer, void *closure,
                      bool force_full);
int XtpTerminalEncodeKey(XtpTerminal *terminal, const XtpKeyEvent *event, char *buffer,
                         size_t capacity, size_t *written);
int XtpTerminalEncodeFocus(XtpTerminal *terminal, bool focused, char *buffer, size_t capacity,
                           size_t *written);
int XtpTerminalEncodeMouse(XtpTerminal *terminal, const XtpMouseEvent *event, char *buffer,
                           size_t capacity, size_t *written);
int XtpTerminalSetScrollbackLines(XtpTerminal *terminal, size_t lines);
int XtpTerminalSetCursorBlinkDefault(XtpTerminal *terminal, bool blinking);
int XtpTerminalSetCharClass(XtpTerminal *terminal, const char *specification);
int XtpTerminalGetScrollbar(XtpTerminal *terminal, XtpTerminalScrollbar *scrollbar);
int XtpTerminalScrollBy(XtpTerminal *terminal, intptr_t rows);
int XtpTerminalScrollTo(XtpTerminal *terminal, uint64_t row);
int XtpTerminalScrollToBottom(XtpTerminal *terminal);
int XtpTerminalSelectionStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                              double surface_x, double surface_y, uint64_t time_ns,
                              XtpSelectionUnit unit, bool repeat);
int XtpTerminalSelectionExtend(XtpTerminal *terminal, uint16_t column, uint16_t row,
                               double surface_x, double surface_y, uint32_t columns,
                               uint32_t cell_width, uint32_t padding_left, uint32_t screen_height,
                               bool rectangle);
int XtpTerminalSelectionGetAutoscroll(XtpTerminal *terminal, XtpSelectionAutoscroll *direction);
int XtpTerminalSelectionAutoscrollTick(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                       double surface_x, double surface_y, uint32_t columns,
                                       uint32_t cell_width, uint32_t padding_left,
                                       uint32_t screen_height, bool rectangle);
void XtpTerminalSelectionEnd(XtpTerminal *terminal, uint16_t column, uint16_t row, bool valid);
int XtpTerminalSelectionExtendStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                    XtpSelectionUnit unit);
int XtpTerminalSelectionExtendActive(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                     bool rectangle);
void XtpTerminalSelectionExtendEnd(XtpTerminal *terminal);
void XtpTerminalSelectionClear(XtpTerminal *terminal);
int XtpTerminalSelectionText(XtpTerminal *terminal, uint8_t **bytes, size_t *length);
int XtpTerminalHyperlinkAt(XtpTerminal *terminal, uint16_t column, uint16_t row, uint8_t **uri,
                           size_t *length);
int XtpTerminalEncodePaste(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                           uint8_t **encoded, size_t *encoded_length);
bool XtpTerminalMouseTracking(XtpTerminal *terminal);
int XtpTerminalGetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool *enabled);
int XtpTerminalSetMode(XtpTerminal *terminal, XtpTerminalMode mode, bool enabled);
void XtpTerminalSetEffects(XtpTerminal *terminal, const XtpTerminalEffects *effects);
const char *XtpTerminalBackend(void);
bool XtpTerminalBackendIsStub(void);

#endif
