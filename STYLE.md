# Xterm+ C Style Guide

> **Status:** Adopted. C sources and headers are formatted by the repository's
> `.clang-format`. Function names use mixed case; public interfaces retain the
> `Xtp` project prefix.

This is a living guide, not a compatibility contract. The current rules are a
coherent starting point and may be changed when experience shows that another
choice improves readability, safety, or maintainability. Update this document
and `.clang-format` together when a formatting rule changes.

This style deliberately keeps the visual and cultural character of classic
X11 C while avoiding the parts that made old X code difficult to maintain.

The goal is not to reproduce historical X Consortium source mechanically. It
is to write modern, disciplined C that looks as though it belongs in the Xterm
family.

## 1. General character

Xterm+ code should feel:

* explicit rather than clever
* procedural rather than object-oriented
* compact, but not compressed
* strongly organized around structs and functions
* comfortable with small helper functions
* conservative about abstraction
* visually reminiscent of traditional X11 source

Prefer straightforward C over framework-like machinery.

If a reader familiar with Xlib, Xt, or xterm opens a source file, the code
should feel familiar without requiring them to tolerate the worst habits of
1980s C.

---

## 2. Function names

Use mixed-case function names.

```c
OpenTerminal()
CreateScreen()
DrawCursor()
HandleKeyPress()
ResizeWindow()
UpdateSelection()
```

For private helpers, mixed case is still preferred:

```c
static void
DrawCell(Terminal *term, int row, int col)
{
        ...
}
```

Do not use modern snake_case for ordinary functions:

```c
/* Avoid */
draw_cell();
handle_key_press();
resize_terminal();
```

Do not use gratuitous prefixes on every internal function unless namespace
protection is actually needed.

Public interfaces may use a project prefix:

```c
XtpTerminalNew()
XtpTerminalRender()
XtpTerminalFree()
```

Internal code should usually remain simpler.

---

## 3. Type names

Use capitalized mixed-case names for important types.

```c
typedef struct
{
        ...
} Terminal;

typedef struct
{
        ...
} Screen;

typedef struct
{
        ...
} RenderContext;
```

Opaque types are also appropriate:

```c
typedef struct _Terminal Terminal;
typedef struct _Screen Screen;
```

The `_TypeName` struct-tag convention is a particularly good X11 homage.

```c
struct _Terminal
{
        Display *display;
        Window window;
        Screen *screen;
};
```

Avoid suffixing every type with `_t`.

```c
/* Avoid */
terminal_t
screen_t
render_context_t
```

POSIX owns much of the `_t` namespace anyway, and the spelling does not fit the
intended style.

---

## 4. Variables

Locals should generally be lowercase and concise.

```c
Display *dpy;
Terminal *term;
Screen *screen;
Window win;
int row;
int col;
int width;
int height;
```

Traditional abbreviations are welcome when they are obvious in context:

```c
dpy
win
gc
buf
len
ptr
src
dst
```

Do not turn every variable name into a sentence.

```c
/* Too modern/cumbersome */
terminal_rendering_context
current_cursor_column_index
number_of_bytes_remaining
```

But do not imitate ancient C to the point of obscurity:

```c
/* Avoid */
p
q
x1
xx
foo
```

unless the meaning is genuinely local and obvious.

A useful rule is:

> Short names for short-lived values; descriptive names for long-lived state.

---

## 5. Struct fields

Struct members use lowercase names.

```c
struct _Terminal
{
        Display *display;
        Window window;

        int rows;
        int cols;

        int cursorRow;
        int cursorCol;

        Screen *screen;
};
```

Mixed-case members such as `cursorRow` are acceptable and help preserve the X11 feel.

For simple nouns, plain lowercase is preferable:

```c
width
height
font
display
window
```

Avoid elaborate accessor machinery for ordinary C structs.

---

## 6. Constants and macros

Constants use uppercase names.

```c
#define DEFAULT_ROWS    24
#define DEFAULT_COLS    80
#define CURSOR_BLINK_MS 500
```

Macros should primarily express constants, conditional compilation, or tiny
operations whose macro nature is useful.

Avoid using macros as disguised functions.

```c
/* Avoid */
#define DRAW_CELL(t,r,c) \
        complicated_expression_with_side_effects(...)
```

Prefer:

```c
static void
DrawCell(Terminal *term, int row, int col)
{
        ...
}
```

Macro magic is one part of historical X code that Xterm+ does not need to preserve.

---

## 7. Function definitions

Use the traditional X-style layout with the return type on its own line.

```c
static void
DrawCursor(Terminal *term)
{
        ...
}
```

```c
static int
ResizeTerminal(Terminal *term, int rows, int cols)
{
        ...
}
```

This convention gives source files a strong classic-X appearance and makes
functions visually easy to scan.

For pointer return types:

```c
static Terminal *
CreateTerminal(Display *dpy)
{
        ...
}
```

Not:

```c
static Terminal*
CreateTerminal(Display *dpy)
```

The `*` visually belongs with the declarator.

---

## 8. Braces

Function braces go on their own lines.

```c
static void
ExposeWindow(Terminal *term)
{
        DrawScreen(term);
}
```

Control-statement braces use the compact X/K&R form selected by
`.clang-format`:

```c
if (term->mapped) {
        DrawScreen(term);
}
```

The vertically expanded form has a stronger historical appearance:

```c
if (term->mapped)
{
        DrawScreen(term);
}
```

For reference, that alternative is:

```c
if (condition)
{
        ...
}
else
{
        ...
}
```

Xterm+ standardizes on the compact form so automated formatting has one
unambiguous result.

---

## 9. Indentation

Use 8-column logical indentation.

Use spaces for leading indentation. Tab stops and logical indentation are
eight columns, as configured in `.clang-format`.

Example:

```c
static void
HandleExpose(Terminal *term, XExposeEvent *event)
{
        if (event->count != 0)
                return;

        if (term->mapped) {
                DrawScreen(term);
                DrawCursor(term);
        }
}
```

The important characteristic is that nesting is visually obvious.

Avoid deeply nested code.

If code reaches four or five levels of indentation, extract a helper or return early.

---

## 10. Early returns

Early returns are encouraged when they make the main path clearer.

```c
static void
HandleKey(Terminal *term, XKeyEvent *event)
{
        if (term == NULL)
                return;

        if (!term->active)
                return;

        ProcessKey(term, event);
}
```

Do not mechanically force every function into one enormous `if` block.

This is one place where modern discipline should improve upon historical style.

---

## 11. Function size

Functions should generally perform one recognizable operation.

Good:

```c
HandleExpose()
DrawScreen()
DrawLine()
DrawCursor()
UpdateSelection()
WritePty()
ReadPty()
```

Prefer a collection of named operations over a giant procedure containing the
entire terminal lifecycle.

At the same time, do not fracture straightforward code into dozens of
microscopic abstraction layers.

The desired unit is a useful procedural operation.

---

## 12. Static functions

Internal helpers should normally be `static`.

```c
static void DrawLine(Terminal *, int);
static void DrawCursor(Terminal *);
static void ClearRegion(Terminal *, int, int, int, int);
```

Keep the external namespace small.

This matches the practical spirit of mature C code and avoids accidental
coupling between translation units.

---

## 13. Forward declarations

A source file may begin with a compact set of private declarations.

```c
static void DrawScreen(Terminal *);
static void DrawLine(Terminal *, int);
static void DrawCursor(Terminal *);
static void HandleExpose(Terminal *, XExposeEvent *);
```

This is intentionally old-school and gives the top of a `.c` file a useful map
of its implementation.

Do not create a private header merely to avoid several static declarations.

---

## 14. Header files

Headers define interfaces, not implementation details.

Typical layout:

```c
#ifndef XTERMPLUS_SCREEN_H
#define XTERMPLUS_SCREEN_H

typedef struct _Screen Screen;
typedef struct _Terminal Terminal;

Screen *CreateScreen(Terminal *);
void DestroyScreen(Screen *);
void ResizeScreen(Screen *, int, int);
void DrawScreen(Screen *);

#endif
```

Do not expose structure internals unless callers genuinely need them.

Avoid enormous umbrella headers.

---

## 15. File organization

Prefer traditional subsystem-oriented source files:

```text
main.c
terminal.c
terminal.h
screen.c
screen.h
render.c
render.h
input.c
input.h
pty.c
pty.h
selection.c
selection.h
x11.c
x11.h
```

A filename should usually correspond to a concrete subsystem.

Avoid fashionable directory hierarchies containing one tiny file per abstraction.

---

## 16. Comments

Comments explain intent, invariants, historical weirdness, protocol behavior,
or non-obvious constraints.

```c
/*
 * X may generate several Expose events for one damaged region.
 * Only redraw after the final event in the series.
 */
if (event->count != 0)
        return;
```

Small implementation comments may use:

```c
/* Restore the saved cursor before resizing. */
```

Do not narrate obvious C:

```c
/* Increment row. */
row++;
```

Traditional block comments are preferred over `//` for the core implementation.

This is partly aesthetic and partly a deliberate connection to older X source.

---

## 17. Boolean logic

Prefer direct tests.

```c
if (term->mapped)
        DrawScreen(term);

if (!term->active)
        return;
```

Avoid:

```c
if (term->mapped == True)
```

unless dealing specifically with an API whose `Bool`, `True`, and `False`
vocabulary is useful to retain.

For Xlib interfaces, using Xlib's own types is fine:

```c
Bool mapped;
Atom atom;
Window window;
Pixmap pixmap;
```

Do not rewrite X vocabulary merely to make it look more contemporary.

---

## 18. X11 types remain X11 types

When interacting with Xlib, embrace its native terminology.

```c
Display *dpy;
Window win;
Drawable drawable;
GC gc;
Atom atom;
XEvent event;
XKeyEvent *key;
```

Do not wrap every Xlib object in another project-specific abstraction merely
for stylistic purity.

The X11 interface is already an abstraction.

---

## 19. Error handling

Check errors deliberately, especially at system boundaries.

```c
fd = open(path, O_RDONLY);
if (fd < 0)
{
        Warn("cannot open %s: %s", path, strerror(errno));
        return -1;
}
```

Use a small, consistent error-reporting vocabulary:

```c
Warn()
Fatal()
Trace()
```

or equivalent.

Avoid elaborate exception-like control flow implemented through macros.

---

## 20. Memory ownership

Ownership should be obvious from the interface.

Constructors allocate:

```c
Terminal *CreateTerminal(...);
```

Destructors release:

```c
void DestroyTerminal(Terminal *);
```

If a function borrows a pointer, that should be the natural assumption unless
documented otherwise.

Avoid hidden ownership transfer.

Modern safety matters more than historical authenticity here.

---

## 21. Allocation

Prefer simple allocation patterns.

```c
term = calloc(1, sizeof(*term));
if (term == NULL)
        return NULL;
```

The `sizeof(*term)` form is preferred even though some older code used explicit
type names.

It prevents type drift during refactoring and costs nothing stylistically.

---

## 22. `goto`

`goto` is acceptable for centralized cleanup.

```c
fd = open(path, O_RDONLY);
if (fd < 0)
        goto fail;

buf = malloc(size);
if (buf == NULL)
        goto close_fd;

...

free(buf);
close(fd);
return 0;

close_fd:
        close(fd);
fail:
        return -1;
```

Do not use `goto` for ordinary application control flow.

This preserves an important practical C idiom without recreating spaghetti code.

---

## 23. Global state

Avoid mutable globals.

Historical X software often relied heavily on them; Xterm+ should not.

Prefer explicit context:

```c
struct _Terminal
{
        Display *display;
        Renderer *renderer;
        Pty *pty;
        Screen *screen;
};
```

Pass the relevant object:

```c
DrawScreen(term);
```

rather than relying on:

```c
CurrentTerminal
GlobalDisplay
TheScreen
```

Read-only tables and true process-global constants are fine.

---

## 24. Callbacks

Callbacks should have clear, concrete names.

```c
HandleExpose()
HandleConfigure()
HandleKeyPress()
HandleButtonPress()
HandleSelection()
```

Avoid generic callback names such as:

```c
callback1()
eventHandler()
doThing()
```

When adapting to an external API's callback signature, keep the public
callback thin and hand off immediately:

```c
static void
ExposeCallback(Widget w, XtPointer closure, XEvent *event, Boolean *cont)
{
        Terminal *term = closure;

        HandleExpose(term, &event->xexpose);
}
```

---

## 25. Rendering code

Rendering should remain direct.

Good:

```c
BeginFrame(renderer);

for (row = first; row <= last; row++)
        DrawLine(term, row);

DrawCursor(term);

EndFrame(renderer);
```

Avoid constructing an elaborate scene graph unless the renderer genuinely
requires one.

Xterm+ is a terminal, not a GUI toolkit.

---

## 26. libghostty boundary

The libghostty renderer should be treated as a subsystem rather than allowed
to reshape the entire program.

For example:

```c
Renderer *CreateRenderer(Terminal *);
void DestroyRenderer(Renderer *);
void ResizeRenderer(Renderer *, int, int);
void DrawCells(Renderer *, const Cell *, int);
void PresentRenderer(Renderer *);
```

The surrounding program should continue to look like a conventional terminal
implementation.

Modern rendering internals do not require modernizing every naming convention
above them.

---

## 27. Avoid faux-retro C

The following historical practices are not part of the homage:

```c
foo(a, b)
int a;
char *b;
{
        ...
}
```

Do not use K&R function definitions.

Do not omit prototypes.

Do not depend on implicit `int`.

Do not cast away compiler warnings.

Do not abuse preprocessor conditionals.

Do not deliberately use unsafe string functions.

Retro appearance must not mean retro correctness.

---

## 28. Compiler discipline

Compile as modern C and keep warnings clean.

For example:

```text
-Wall
-Wextra
-Wpedantic
-Wshadow
-Wformat=2
-Wstrict-prototypes
-Wmissing-prototypes
```

The exact warning set may vary, but warning-free compilation is part of the style.

The code may look like X11.

The compiler discipline should look like 2026.

---

## 29. Example

A representative Xterm+ function should look something like this:

```c
static void
HandleConfigure(Terminal *term, XConfigureEvent *event)
{
        int rows;
        int cols;

        if (event->width == term->width &&
            event->height == term->height)
                return;

        term->width = event->width;
        term->height = event->height;

        cols = term->width / term->cellWidth;
        rows = term->height / term->cellHeight;

        if (cols == term->cols && rows == term->rows)
                return;

        ResizeScreen(term->screen, rows, cols);
        ResizeRenderer(term->renderer, term->width, term->height);

        term->rows = rows;
        term->cols = cols;

        DrawScreen(term);
}
```

It is recognizable as traditional C.

It has the vertical rhythm of X-era source.

It uses concrete names and simple control flow.

But ownership, prototypes, encapsulation, and error handling remain modern.

---

## 30. The rule of thumb

When choosing between historical authenticity and maintainability:

**Keep the typography and vocabulary. Modernize the engineering.**

Preserve:

* mixed-case function names
* capitalized types
* lowercase concise locals
* traditional C declarations
* return type above function name
* strong vertical formatting
* X11 vocabulary
* subsystem-oriented `.c` files
* simple procedural interfaces
* block comments
* explicit code

Discard:

* implicit declarations
* unsafe C
* unnecessary globals
* macro mazes
* obscure one-letter identifiers
* accidental ownership
* K&R syntax
* gigantic functions
* portability hacks whose original platforms disappeared decades ago

Xterm+ should look like software the X11 hackers might have written if they
had today's compilers, sanitizers, libraries, and forty more years of
experience.
