# X resources, explained

<!-- markdownlint-disable MD013 -->

xterm has no settings dialog and no config file of its own. It is configured
through **X resources** — a mechanism that belongs to the X Window System
itself, is shared by every classic X application, and predates almost every
other configuration system you have used. Once you understand it, xterm+ is
easy to configure. This page is that understanding, from scratch.

If you already know what `xrdb -merge ~/.Xresources` does and why
`XTerm*background` and `xterm.vt100.background` are different, skip ahead to
[Configuring xterm+](xterm-plus.md).

## The one-paragraph version

There is a database of `name: value` lines. Applications look up their
settings in it by asking "what is the value of *foreground* for the *vt100*
widget inside the *xterm* application?". You write lines into a text file
(conventionally `~/.Xresources`), load that file into the database with the
`xrdb` command, and every X application started afterwards sees it. The
clever part is that a line can name one exact widget or a whole family of
widgets, so a single line can restyle every menu in every application.

Everything else on this page is detail on those sentences.

## A first example

Create `~/.Xresources` with these lines:

```xrdb
! xterm+ / xterm settings
XTerm*background:  #1c1c1c
XTerm*foreground:  #d0d0d0
XTerm*faceName:    Hack
XTerm*faceSize:    11
XTerm*saveLines:   10000
```

Load it:

```sh
xrdb -merge ~/.Xresources
```

Start `xterm+` (or `xterm`). You get a dark window with a scalable font and
ten thousand lines of scrollback. That is the whole loop: **edit, `xrdb`,
start the application**.

Note two things already. Lines starting with `!` are comments. And nothing
happened to windows that were already open — resources are read when a
program starts, not continuously.

## Anatomy of a resource line

```text
XTerm*vt100.faceName:   Hack
└─┬─┘│└─┬─┘│└──┬───┘    └─┬┘
  │  │  │  │   │          └ value: everything after the colon, leading spaces stripped
  │  │  │  │   └ resource name: the setting
  │  │  │  └ tight binding: "directly inside"
  │  │  └ widget name
  │  └ loose binding: "anywhere below"
  └ application class
```

The left side is a *path* through the application's widget tree, from the
outside in. The right side is the value. The path components are joined with
either `.` (tight: the next component must be an immediate child) or `*`
(loose: the next component may be anywhere further down).

### Every component has a name and a class

This is the idea that makes X resources powerful, and the one that confuses
newcomers most. Each thing on the path — the application, every widget, and
the setting itself — has **two** identifiers:

| | Instance name | Class name |
| --- | --- | --- |
| The application | `xterm` | `XTerm` |
| The terminal widget | `vt100` | `VT100` |
| The setting | `background` | `Background` |
| A popup menu | `mainMenu` | `SimpleMenu` |
| The scrollbar | `scrollbar` | `Scrollbar` |

Instance names are lower-case and specific: *this* widget. Class names start
with a capital and are general: *any* widget of this kind. You may use either
at any position in the path, and they match different things:

```xrdb
xterm.vt100.background:  black      ! exactly this app, exactly this widget
XTerm*background:        black      ! any app of class XTerm, any widget below it
*background:             black      ! every widget of every X application
XTerm*SimpleMenu*background: gray20 ! every popup menu in xterm-class apps
```

The convention for xterm files is to start with the class `XTerm`, so the
lines also apply to xterm+ and to `uxterm`, which share the class.

!!! tip "Why `XTerm*` and not `xterm*`?"
    The instance name of the application is whatever `argv[0]` or `-name`
    says. `xterm -name work` is instance `work`, class `XTerm`. Lines written
    against the class keep working; lines written against the instance let
    you give a specially-named window different colours. Both are useful,
    which is why both exist.

### Tight versus loose binding

`.` means "immediately inside". `*` means "somewhere inside, at any depth".

```xrdb
XTerm.vt100.font:   fixed     ! only if vt100 is directly under the shell
XTerm*font:         fixed     ! any widget with a font resource, anywhere
```

In practice almost everyone writes `*` almost everywhere, because you rarely
care about the exact tree. Use `.` when a loose line is catching more than
you meant — for example `XTerm*font` also sets the *menu* font, whereas
`XTerm*vt100.font` (or `XTerm.vt100.font`) sets only the terminal font.

### When two lines match: precedence

If several lines could apply to the same setting, Xt picks the most specific,
using rules you can summarise as:

1. A component that names something beats a component that skips it
   (`XTerm.vt100.background` beats `XTerm*background`).
2. An instance name beats a class name at the same position
   (`xterm*background` beats `XTerm*background`).
3. Tight beats loose (`XTerm.vt100.background` beats `XTerm*vt100.background`).
4. These are compared left to right, so the leftmost difference decides.

The practical rule: **if a setting is not taking effect, something more
specific is winning**. `xterm+ -report-config` shows you the resolved value
of every setting and where it came from — use it before guessing.

### Values

The value is everything after the colon with leading whitespace removed.
There are no quotes. Trailing whitespace *is* significant, which is a
classic source of "why is my font name not found". Type conversion is done
by the application: `true`/`false`/`on`/`off`/`yes`/`no` for booleans,
colour names or `#rrggbb` for colours, integers, and for a few resources a
small language of their own (translations, character classes).

A value can span lines by ending a line with a backslash:

```xrdb
XTerm*vt100.translations: #override \n\
    Shift <KeyPress> Insert: insert-selection(SELECT, CUT_BUFFER0) \n\
    Shift <KeyPress> Prior: scroll-back(1,halfpage) \n\
    Shift <KeyPress> Next:  scroll-forw(1,halfpage)
```

The `\n` inside the value is the *translation table's* line separator; the
trailing `\` is the *resource file's* line continuation. Both are needed.

## Where resources come from

An application does not read `~/.Xresources`. That surprises everyone. It
reads several sources, merges them, and the file you edited is only in there
because `xrdb` copied it somewhere the application does look. From lowest to
highest priority:

| Priority | Source | What it is |
| --- | --- | --- |
| 1 | Compiled-in fallback | Defaults built into the program |
| 2 | System app-defaults | `/usr/share/X11/app-defaults/XTerm` — shipped by the distribution; this is where the menu labels and default font slots live |
| 3 | User app-defaults | A file named `XTerm` under `$XAPPLRESDIR` or on `$XUSERFILESEARCHPATH` |
| 4 | **The server** `RESOURCE_MANAGER` property, *or* `~/.Xdefaults` if that property is absent | What `xrdb` loads. This is where your `~/.Xresources` ends up |
| 5 | `$XENVIRONMENT`, or `~/.Xdefaults-<hostname>` | Per-host overrides |
| 6 | Command line | `-xrm 'XTerm*background: black'`, or the short options such as `-bg black` which are just aliases for resources |

Two consequences worth internalising:

- **`xrdb` stores the text *in the X server*, as a property on the root
  window.** That is why it survives when you delete the file, why it is
  shared by every client of that display including ones running remotely
  over `ssh -X`, and why it is gone after you log out. Look at it with
  `xrdb -query`.
- **`~/.Xdefaults` is only read when nothing has been loaded with `xrdb`.**
  On most desktops something loads `xrdb` at login, so `~/.Xdefaults` is
  silently ignored. Use `~/.Xresources` plus `xrdb`, and forget `.Xdefaults`
  exists.

### Getting `~/.Xresources` loaded at login

Most display managers and desktop sessions already run
`xrdb -merge ~/.Xresources` when you log in. If yours does not, add it to
`~/.xinitrc`, `~/.xsession`, or `~/.xprofile`:

```sh
[ -r "$HOME/.Xresources" ] && xrdb -merge "$HOME/.Xresources"
```

## Working with `xrdb`

```sh
xrdb -merge ~/.Xresources   # add/override; keeps other loaded resources
xrdb -load  ~/.Xresources   # replace everything with this file
xrdb -query                 # show what is loaded right now
xrdb -remove                # empty the database (everything, not just yours)
```

Use `-merge` day to day. `-load` is right when you have removed lines and
want them gone, since merging cannot delete.

`xrdb` runs your file through the C preprocessor before loading it. That
gives you `#include`, `#define`, and `#ifdef`, and it means `#` starts a
preprocessor directive rather than a comment — use `!` for comments.

```xrdb
#include "/home/me/.config/x/colors-dark"
#define FONT Hack
XTerm*faceName: FONT
```

Paths in `#include` must be absolute; `~` is not expanded. If the
preprocessor is not available or you want to bypass it, `xrdb -nocpp`.

## Finding out what a program actually saw

Because there are six sources and a precedence algorithm, the important
skill is *inspection*, not memorising rules.

- `xrdb -query` — what is in the server database.
- `appres XTerm xterm` — the merged view of everything that would apply to
  an application with that class and instance name.
- `editres` — interactive widget-tree browser, if you want to see the real
  paths.
- `xterm+ -report-config` — xterm+'s own report: every resource it knows,
  the effective value, whether it came from the command line, resources, or
  the compiled default, and whether xterm+ supports it yet. This is the one
  to reach for first; see [Diagnostics](../reference/diagnostics.md).

## A tour of the xterm widget tree

Knowing the shape of the tree tells you which paths make sense:

```text
xterm  (XTerm)              the application shell; title, iconName, geometry live here
└── vt100  (VT100)          the terminal: fonts, colours, saveLines, scrollBar, translations
    ├── scrollbar  (Scrollbar)   the Athena scrollbar: width, thickness, colours
    ├── mainMenu   (SimpleMenu)  Ctrl+button 1 popup
    ├── vtMenu     (SimpleMenu)  Ctrl+button 2 popup
    └── fontMenu   (SimpleMenu)  Ctrl+button 3 popup
        └── fontdefault, font1 … (SmeBSB)   the menu entries
```

So `XTerm*vt100.saveLines` and `XTerm*saveLines` name the same thing;
`XTerm*scrollbar.width` restyles the scrollbar; `XTerm*fontMenu*font1*Label`
renames a menu entry; and `XTerm*title` sets the window title because
`title` is a resource of the shell at the top.

## Common mistakes

| Symptom | Cause |
| --- | --- |
| Edited the file, nothing changed | Did not run `xrdb -merge`, or the window was already open |
| Works in one window, not after `-name` | Lines written against instance `xterm` instead of class `XTerm` |
| Font "not found" though `fc-list` shows it | Trailing whitespace after the value, or a `#` comment that cpp swallowed |
| Menu font changed when you meant the terminal font | `XTerm*font` is loose; use `XTerm*vt100.font` |
| Colour works in xterm, ignored in xterm+ | The resource is accepted but not applied yet; `-report-config` labels it *accepted but ignored* |
| `~/.Xdefaults` ignored | Something loaded `xrdb` at login, so the file is never consulted |

## Further reading

- `man xrdb`, `man appres`, `man editres`
- The X Toolkit Intrinsics manual, chapter "Resource Management", for the
  full precedence algorithm
- xterm's own manual page, section RESOURCES, for the complete list of
  xterm resources — xterm+ inventories all of them in
  `compat/xterm-410-resources.tsv` and reports its support for each

<!-- markdownlint-enable MD013 -->
