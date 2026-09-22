#ifndef XTERM_PLUS_TERMINAL_H
#define XTERM_PLUS_TERMINAL_H

#include "ansi_palette.h"

#include "color_ops.h"
#include "request_ops.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct XtpTerminal XtpTerminal;

/* Payload bytes retained per unsupported APC before the backend marks it truncated. */
#define XTP_UNKNOWN_APC_CAPTURE_LIMIT 256U

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

typedef struct
{
        uint8_t red;
        uint8_t green;
        uint8_t blue;
} XtpRgbColor;

typedef enum
{
        XTP_CURSOR_SHAPE_BLOCK,
        XTP_CURSOR_SHAPE_UNDERLINE,
        XTP_CURSOR_SHAPE_BAR,
        XTP_CURSOR_SHAPE_BLOCK_HOLLOW,
} XtpCursorShape;

typedef enum
{
        XTP_UNDERLINE_NONE,
        XTP_UNDERLINE_SINGLE,
        XTP_UNDERLINE_DOUBLE,
        XTP_UNDERLINE_CURLY,
        XTP_UNDERLINE_DOTTED,
        XTP_UNDERLINE_DASHED,
} XtpUnderline;

typedef struct
{
        uint16_t columns;
        uint16_t rows;
        bool full_repaint;
        bool reverse_colors;
        bool cursor_visible;
        uint16_t cursor_column;
        uint16_t cursor_row;
        XtpCursorShape cursor_shape;
        bool cursor_blink_requested;
        /* Effective default colors, with OSC 10/11/12 overrides applied. */
        XtpRgbColor foreground;
        XtpRgbColor background;
        XtpRgbColor cursor;
        bool colors_valid;
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
        bool row_wrapped;
        bool selected;
        bool strikethrough;
        bool overline;
        XtpUnderline underline;
        /* SGR 58 underline color; XTP_COLOR_DEFAULT follows the text color. */
        XtpColor underline_color;
} XtpRenderCell;

typedef struct
{
        void (*begin)(const XtpRenderFrame *frame, void *closure);
        void (*cell)(const XtpRenderCell *cell, void *closure);
        void (*end)(const XtpRenderFrame *frame, void *closure);
        void (*abort)(const XtpRenderFrame *frame, void *closure);
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
        XTP_KEY_CONTEXT_MENU,
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
        XTP_KEY_HELP,
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
        XTP_KEY_NUMPAD_EQUAL,
        XTP_KEY_NUMPAD_MULTIPLY,
        XTP_KEY_NUMPAD_SEPARATOR,
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
        XTP_KEY_F13,
        XTP_KEY_F14,
        XTP_KEY_F15,
        XTP_KEY_F16,
        XTP_KEY_F17,
        XTP_KEY_F18,
        XTP_KEY_F19,
        XTP_KEY_F20,
        XTP_KEY_F21,
        XTP_KEY_F22,
        XTP_KEY_F23,
        XTP_KEY_F24,
        XTP_KEY_F25,
        XTP_KEY_PRINT_SCREEN,
        XTP_KEY_SCROLL_LOCK,
        XTP_KEY_PAUSE,
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

typedef enum
{
        XTP_CLIPBOARD_TARGET_CLIPBOARD,
        XTP_CLIPBOARD_TARGET_SELECT,
        XTP_CLIPBOARD_TARGET_PRIMARY,
} XtpClipboardTarget;

typedef enum
{
        XTP_CLIPBOARD_SUCCESS,
        XTP_CLIPBOARD_DENIED,
        XTP_CLIPBOARD_UNSUPPORTED,
        XTP_CLIPBOARD_UNAVAILABLE,
} XtpClipboardResult;

typedef enum
{
        XTP_TITLE_OP_REPORT_ICON = 20,
        XTP_TITLE_OP_REPORT_WINDOW = 21,
        XTP_TITLE_OP_PUSH = 22,
        XTP_TITLE_OP_POP = 23,
} XtpTitleOp;

typedef enum
{
        XTP_TITLE_TARGET_BOTH = 0,
        XTP_TITLE_TARGET_ICON = 1,
        XTP_TITLE_TARGET_WINDOW = 2,
} XtpTitleTarget;

typedef enum
{
        XTP_PROGRESS_REMOVE,
        XTP_PROGRESS_SET,
        XTP_PROGRESS_ERROR,
        XTP_PROGRESS_INDETERMINATE,
        XTP_PROGRESS_PAUSE,
} XtpProgressState;

typedef struct
{
        void (*write_pty)(const uint8_t *bytes, size_t length, void *closure);
        void (*bell)(void *closure);
        void (*title_changed)(const char *title, size_t length, void *closure);
        void (*cursor_blink_reset)(void *closure);
        /* OSC 52 set; clear=true carries no text. */
        XtpClipboardResult (*clipboard_write)(XtpClipboardTarget target, const uint8_t *bytes,
                                              size_t length, bool clear, void *closure);
        /* OSC 52 query; a NULL effect leaves the query unanswered. On success the
         * caller frees *bytes. */
        XtpClipboardResult (*clipboard_read)(XtpClipboardTarget target, uint8_t **bytes,
                                             size_t *length, void *closure);
        /* XTWINOPS 20-23 with the raw second and third parameters (0 when absent). */
        void (*title_op)(XtpTitleOp op, unsigned int target, unsigned int slot, void *closure);
        /* May CSI 14/16/18 t be answered? NULL answers every one. */
        bool (*size_report_allowed)(unsigned int op, void *closure);
        /* OSC 7/9/1337 working-directory report as the application sent it, borrowed
         * for the call; zero length means the application cleared it. */
        void (*working_directory_changed)(const uint8_t *bytes, size_t length, void *closure);
        /* The core discarded a complete OSC 7 without reporting it, typically
         * because the payload exceeded its capture buffer; the earlier directory
         * is no longer trustworthy. `length` counts the payload bytes seen. */
        void (*working_directory_dropped)(size_t length, void *closure);
        /* A completed APC the core does not implement, borrowed; truncated marks a capture cut. */
        void (*unknown_apc)(const uint8_t *bytes, size_t length, bool truncated, void *closure);
        /* OSC 9 / OSC 777 notification; both strings are borrowed and the title may be empty. */
        void (*notification)(const uint8_t *title, size_t title_length, const uint8_t *body,
                             size_t body_length, void *closure);
        /* OSC 9;4 progress; libghostty also reports removal on RIS. Percent is 0-100 or -1. */
        void (*progress)(XtpProgressState state, int percent, void *closure);
        void *closure;
} XtpTerminalEffects;

const char *XtpClipboardTargetName(XtpClipboardTarget target);

/* xterm's Color Ops policy: allow overrides the list; the list names SetColor
 * (OSC 10-19 sets, 110-119 resets), GetColor (their queries), and
 * GetAnsiColor (OSC 4/5 queries). Ordinary palette writes stay ungated. */
void XtpTerminalSetColorOpsPolicy(XtpTerminal *terminal, bool allow_color_ops,
                                  const XtpColorOps *ops);
void XtpTerminalSetAllowColorOps(XtpTerminal *terminal, bool enabled);
/* Input-report permission; named disallowedMouseOps exceptions remain pending. */
void XtpTerminalSetAllowMouseOps(XtpTerminal *terminal, bool enabled);
/* XTGETTCAP is already answered by the core; XTSETTCAP is not implemented. */
void XtpTerminalSetTcapOpsPolicy(XtpTerminal *terminal, bool allow, const XtpTcapOps *ops);
/* Light or dark, from the displayed default background's perceived luminance. */
bool XtpTerminalBackgroundIsLight(XtpTerminal *terminal);

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
        XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT,
        /* libghostty's own mode 12; the observer holds the application operand. */
        XTP_TERMINAL_MODE_CURSOR_BLINKING,
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

/* Selection mutations distinguish failure, no visible change, and change. */
typedef enum
{
        XTP_SELECTION_ERROR = -1,
        XTP_SELECTION_UNCHANGED = 0,
        XTP_SELECTION_CHANGED = 1,
} XtpSelectionResult;

XtpTerminal *XtpTerminalNewWithGraphemeWidth(uint16_t columns, uint16_t rows, uint32_t cell_width,
                                             uint32_t cell_height, bool unicode_width);
void XtpTerminalFree(XtpTerminal *terminal);
void XtpTerminalFeed(XtpTerminal *terminal, const uint8_t *bytes, size_t length);
int XtpTerminalFeedOutput(XtpTerminal *terminal, const uint8_t *bytes, size_t length,
                          bool scroll_tty_output);
/* Keeps the viewport on the same text while output arrives, as a search needs. */
int XtpTerminalFeedOutputPinned(XtpTerminal *terminal, const uint8_t *bytes, size_t length);
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
/* The shape DECSCUSR 0 and a full reset return to; blink stays separate. */
int XtpTerminalSetDefaultCursorShape(XtpTerminal *terminal, XtpCursorShape shape);
/* xterm's startup rule: cursorUnderLine beats cursorBar beats the block. */
XtpCursorShape XtpTerminalStartupCursorShape(bool underline, bool bar);
int XtpTerminalSetCursorBlinkRequestsEnabled(XtpTerminal *terminal, bool enabled);
int XtpTerminalSetDefaultColors(XtpTerminal *terminal, XtpRgbColor foreground,
                                XtpRgbColor background, XtpRgbColor cursor);
int XtpTerminalSetAnsiPalette(XtpTerminal *terminal,
                              const XtpRgbColor palette[XTP_ANSI_PALETTE_SIZE]);
int XtpTerminalSetBoldColors(XtpTerminal *terminal, bool enabled);
int XtpTerminalSetCharClass(XtpTerminal *terminal, const char *specification);
int XtpTerminalSetTitle(XtpTerminal *terminal, const char *title, size_t length);
/* The name XTGETTCAP "TN" reports; it must match the child's TERM. Names over
 * 128 bytes are rejected by the core and leave TN unanswered. */
int XtpTerminalSetTerminfoName(XtpTerminal *terminal, const char *name);
/* xterm's answerbackString, sent verbatim for ENQ; empty or NULL stays silent. */
int XtpTerminalSetAnswerback(XtpTerminal *terminal, const char *answerback);
int XtpTerminalGetScrollbar(XtpTerminal *terminal, XtpTerminalScrollbar *scrollbar);
int XtpTerminalScrollBy(XtpTerminal *terminal, intptr_t rows);
int XtpTerminalScrollTo(XtpTerminal *terminal, uint64_t row);
int XtpTerminalScrollToBottom(XtpTerminal *terminal);

typedef enum
{
        XTP_SEMANTIC_ROW_NONE,
        XTP_SEMANTIC_ROW_PROMPT,
        XTP_SEMANTIC_ROW_PROMPT_CONTINUATION,
} XtpSemanticRow;

/* An inclusive cell range in screen coordinates: row 0 is the top of the scrollback. */
typedef struct
{
        uint64_t start_row;
        uint16_t start_column;
        uint64_t end_row;
        uint16_t end_column;
} XtpSemanticSpan;

/* OSC 133 prompt state of a screen row; -1 when the row does not exist or the alternate
 * screen is active. Each call resolves the row from the top of the screen. */
int XtpTerminalSemanticRow(XtpTerminal *terminal, uint64_t row, XtpSemanticRow *state);
/* Start row of the nearest prompt strictly above (or below) `from`, found through an index
 * of OSC 133 marks rather than a row scan. A continuation run resolves to its primary row
 * or, when that row is gone, to the run's top row. -1 when there is none. */
int XtpTerminalFindPrompt(XtpTerminal *terminal, uint64_t from, bool forward, uint64_t *row);
/* Start row of the prompt whose command last reported completion with a well-formed OSC 133 D;
 * -1 before any D, when that prompt was pruned or reset away, or on the alternate screen. */
int XtpTerminalLastCompletedPrompt(XtpTerminal *terminal, uint64_t *row);
/* Whether any well-formed OSC 133 D has ever been accepted, so callers can tell a shell that
 * never reports completion from a completed prompt that is no longer available. */
bool XtpTerminalCommandEndSeen(XtpTerminal *terminal);
/* Prompt marks the backend currently stores, dead ones included; never compacts. Diagnostics. */
size_t XtpTerminalPromptMarks(XtpTerminal *terminal);
/* The core's command-output selection for the prompt starting at `prompt_start`: from the
 * first written output cell (a written space counts) to the last written one, cell-exact,
 * ending before the next prompt row. -1 when the command wrote nothing. */
int XtpTerminalCommandOutput(XtpTerminal *terminal, uint64_t prompt_start, XtpSemanticSpan *span);
/* Plain text of a span with soft wraps joined and trailing blanks trimmed, NUL-terminated;
 * the caller frees *text. */
int XtpTerminalSpanText(XtpTerminal *terminal, const XtpSemanticSpan *span, char **text,
                        size_t *length);
/* Literal primary-screen search; HANDOFF.md describes the matching and invalidation rules. */
typedef struct XtpTerminalSearch XtpTerminalSearch;

typedef enum
{
        XTP_SEARCH_IDLE,
        XTP_SEARCH_RUNNING,
        XTP_SEARCH_COMPLETE,
        /* The alternate screen is active; the search resumes when it ends. */
        XTP_SEARCH_UNAVAILABLE,
        /* A scan allocation or core call failed; matches already found stay usable. */
        XTP_SEARCH_ERROR,
} XtpSearchState;

/* Matches kept per query, newest first; more set the truncated flag. */
#define XTP_SEARCH_MATCH_LIMIT 4096U
/* Rows of one soft-wrapped line searched together; longer lines are split. */
#define XTP_SEARCH_LINE_ROW_LIMIT 256U

/* NULL on the stub backend or allocation failure; free it before the terminal. */
XtpTerminalSearch *XtpTerminalSearchNew(XtpTerminal *terminal);
void XtpTerminalSearchFree(XtpTerminalSearch *search);
/* Restarts and reads the active screen at once; -1 on bad UTF-8, no memory or alt screen. */
int XtpTerminalSearchSetQuery(XtpTerminalSearch *search, const char *utf8, size_t length);
/* Scans older rows: at most row_budget plus one wrapped line's rest; 0 does nothing. */
XtpSearchState XtpTerminalSearchStep(XtpTerminalSearch *search, size_t row_budget);
XtpSearchState XtpTerminalSearchState(XtpTerminalSearch *search);
/* Stops the scan and drops the query and its matches. */
void XtpTerminalSearchCancel(XtpTerminalSearch *search);
/* Live matches after dropping evicted ones. */
size_t XtpTerminalSearchMatches(XtpTerminalSearch *search, bool *truncated);
/* Rows examined since the query was set; diagnostics. */
uint64_t XtpTerminalSearchRowsScanned(XtpTerminalSearch *search);
/* Nearest match starting after or before a screen cell, wrapping; changed matches are dropped. */
int XtpTerminalSearchNavigate(XtpTerminalSearch *search, uint64_t row, uint16_t column,
                              bool forward, XtpSemanticSpan *match, bool *wrapped);
/* Live matches touching screen rows first..last, oldest first; returns the count written. */
size_t XtpTerminalSearchVisible(XtpTerminalSearch *search, uint64_t first_row, uint64_t last_row,
                                XtpSemanticSpan *spans, size_t capacity);

/* A primary-screen cell followed through scrolling, eviction and reflow. */
typedef struct XtpTerminalCellMark XtpTerminalCellMark;

/* NULL on the stub backend, the alternate screen, a cell off the screen or no memory. */
XtpTerminalCellMark *XtpTerminalMarkCell(XtpTerminal *terminal, uint64_t row, uint16_t column);
/* -1 once the marked cell was evicted. */
int XtpTerminalMarkPosition(const XtpTerminalCellMark *mark, uint64_t *row, uint16_t *column);
void XtpTerminalMarkFree(XtpTerminalCellMark *mark);
XtpSelectionResult XtpTerminalSelectionStart(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                             double surface_x, double surface_y, uint64_t time_ns,
                                             XtpSelectionUnit unit, bool repeat);
XtpSelectionResult XtpTerminalSelectionExtend(XtpTerminal *terminal, uint16_t column, uint16_t row,
                                              double surface_x, double surface_y, uint32_t columns,
                                              uint32_t cell_width, uint32_t padding_left,
                                              uint32_t screen_height, bool rectangle);
int XtpTerminalSelectionGetAutoscroll(XtpTerminal *terminal, XtpSelectionAutoscroll *direction);
XtpSelectionResult XtpTerminalSelectionAutoscrollTick(XtpTerminal *terminal, uint16_t column,
                                                      uint16_t row, double surface_x,
                                                      double surface_y, uint32_t columns,
                                                      uint32_t cell_width, uint32_t padding_left,
                                                      uint32_t screen_height, bool rectangle);
void XtpTerminalSelectionEnd(XtpTerminal *terminal, uint16_t column, uint16_t row, bool valid);
XtpSelectionResult XtpTerminalSelectionExtendStart(XtpTerminal *terminal, uint16_t column,
                                                   uint16_t row, XtpSelectionUnit unit);
XtpSelectionResult XtpTerminalSelectionExtendActive(XtpTerminal *terminal, uint16_t column,
                                                    uint16_t row, bool rectangle);
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
/*
 * Synchronized output (DEC mode 2026) as a render hold. When a hold begins, the
 * backend captures the frame the application finished before it, at that exact
 * parser position, and XtpTerminalRender keeps drawing that frame until the hold
 * ends; then it resumes drawing live state. HELD reports each change; when a hold
 * begins, CHANGED says whether the captured frame differs from the last one drawn,
 * which is when a paint is owed to show it. The callback runs inside
 * XtpTerminalFeedOutput, so it must not feed, render or change modes of the
 * terminal -- recording the change for the next paint is what it is for.
 * Renderer-facing rather than an application effect, so it is registered by whatever
 * draws the terminal; NULL unregisters. The stub backend never holds.
 */
typedef void (*XtpTerminalRenderHoldFn)(void *closure, bool held, bool changed);
void XtpTerminalSetRenderHold(XtpTerminal *terminal, XtpTerminalRenderHoldFn hold, void *closure);
bool XtpTerminalRenderHeld(const XtpTerminal *terminal);
const char *XtpTerminalBackend(void);
bool XtpTerminalBackendIsStub(void);

#endif
