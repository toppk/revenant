# Configuring Revenant

This page assumes you have read [X resources, explained](xresources.md), or
already know the `edit → xrdb -merge → restart` loop. Everything below is a
resource line you put in `~/.Xresources`; each also has a command-line
equivalent where xterm defines one (see [Command-line options](command-line.md)).

Revenant uses application class `XTerm`, instance `xterm`, and a terminal
widget named `vt100` of class `VT100` — deliberately the same as xterm. An
existing xterm configuration applies to Revenant unchanged. Where Revenant does
not yet implement a resource, it still accepts the line; run
`revenant -report-config` to see which lines are *supported*, *accepted but
ignored*, or *unsupported*.

## A complete starter file

```xrdb
! ~/.Xresources — xterm / Revenant
XTerm*renderFont:      true
XTerm*faceName:        Hack
XTerm*faceSize:        11
XTerm*background:      #1c1c1c
XTerm*backgroundOpacity: 0.90
XTerm*foreground:      #d0d0d0
XTerm*cursorColor:     #ffaf00
XTerm*saveLines:       10000
XTerm*scrollBar:       true
XTerm*rightScrollBar:  true
XTerm*scrollTtyOutput: false
XTerm*scrollKey:       true
XTerm*internalBorder:  4
XTerm*title:           Revenant
```

Load it with `xrdb -merge ~/.Xresources` and start a new Revenant.

## Fonts

This section is the short overview. The dedicated
[Fonts and font fallback](fonts.md) guide explains the two-entry xterm grammar,
role selection, additional fonts, shaping/width invariants, diagnostics, and
which Revision-5 resolver resources are implemented in the current build.

xterm has a font *menu* with ten slots, and the resources fill the first
eight. Understanding the slots explains most of the font resources.

| Menu entry | Resource (bitmap) | Resource (Xft size) | xterm default |
| --- | --- | --- | --- |
| Default | `font` | `faceSize` | `fixed` / 8pt |
| Unreadable | `font1` | `faceSize1` | `nil2` |
| Tiny | `font2` | `faceSize2` | `5x7` |
| Small | `font3` | `faceSize3` | `6x10` |
| Medium | `font4` | `faceSize4` | `7x13` |
| Large | `font5` | `faceSize5` | `9x15` |
| Huge | `font6` | `faceSize6` | `10x20` |
| Enormous | `font7` | `faceSize7` | *(unset)* |
| Escape Sequence | — | — | set by a program at runtime |
| Selection | — | — | font name taken from the selection |

Shift+keypad + and − step through the *configured* slots in size order, not
menu order. `-report-config` prints the actual order it computed.

### Two renderers

`renderFont` picks which of Revenant's two text paths is used:

```xrdb
XTerm*renderFont: true      ! Xft/fontconfig: faceName + faceSize apply
XTerm*renderFont: false     ! Xlib bitmap fonts: font, font1 … font7 apply
```

Bitmap mode is a supported compatibility path, not a deprecated fallback. The
Revision 5 role, shaping, emoji, and general-fallback expansion applies only to
`renderFont: true`; an existing xterm bitmap-font configuration may continue
to use `renderFont: false` and the traditional bitmap resources unchanged.

With Xft, name the face the fontconfig way and give sizes in points:

```xrdb
XTerm*faceName:  DejaVu Sans Mono
XTerm*faceSize:  10.5
XTerm*faceSize3: 8
XTerm*faceSize5: 14
```

Wide text and emoji can use separate faces without changing the grid width
already chosen by the terminal engine:

```xrdb
XTerm*faceNameDoublesize: Noto Sans Mono CJK JP
XTerm*faceNameEmoji:      Noto Color Emoji
XTerm*emojiPresentation:  unicode  ! unicode, text, or emoji
XTerm*colorGlyphs:        true
XTerm*graphemeWidth:      legacy   ! legacy or unicode
```

VS15 always requests text presentation and VS16 always requests emoji
presentation. Without a selector, `emojiPresentation` chooses Unicode's
default, forces text, or forces emoji. Emoji presentation tries
`faceNameEmoji`, then `faceNameDoublesize`, then the primary face; wide
non-emoji text tries `faceNameDoublesize` and then the primary face. If those
explicit roles miss, Revenant walks a bounded fontconfig fallback chain for
the active font slot and SGR weight/slant. A face is
skipped when the complete grapheme cannot be shaped by that face or its
selected ink path is empty. Fallback is atomic: a partially covered ZWJ or
modifier sequence, keycap, regional-indicator flag, or terminated subdivision
tag flag is retried in the next role as a whole. Validation shaping preserves
default-ignorables so an unsupported tag sequence cannot masquerade as its
generic black-flag base; accepted drawing runs remove unresolved controls.
Text presentation never permits a color paint: a color-capable doublesize or
primary face is usable only when it has a genuine outline for that glyph.

`colorGlyphs: false` declines CBDT, sbix, COLR, and SVG color paint. A genuine
outline base is drawn in the foreground; an empty or degenerate base falls
through to the next configured face. If the primary face itself has no
outline, the cell remains blank rather than leaking color through Xft.
Revenant uses Cairo 1.18 for color formats that Xft does not render, including
COLRv1 and SVGinOT. The delegate caches its Xlib surface and scaled role fonts,
fits from the selected face's own extents, and uses the same cell, damage, and
cursor clips as the Xft path.

HarfBuzz shapes the complete grapheme bytes supplied by `libghostty-vt`, so
keycaps, skin-tone modifiers, ZWJ emoji, regional-indicator and tag flags, and
ordinary combining clusters can resolve to positioned glyph runs.
`graphemeWidth: legacy` is the default: it leaves DEC private mode 2027 off so
wcwidth-based applications retain xterm-compatible cursor arithmetic.
`graphemeWidth: unicode` enables mode 2027 initially and after reset for
cluster-aware environments. Applications can still negotiate either state
explicitly with DECSET/DECRST 2027. Libghostty remains responsible for cluster
boundaries and terminal width; rendering never changes the chosen width.

Compatible adjacent non-emoji cells are shaped together when they resolve to
the same face. This lets an Indic conjunct split across backend graphemes and
mark-heavy runs use contextual substitution and positioning. The painter clips
the result to the combined cells committed by libghostty; shaping never changes
cursor arithmetic, history, selection, or the grid.

SGR italic and bold-italic select separate fontconfig faces. If a requested
style is unavailable, rendering falls back through the configured role and
normal face without synthesizing a slant. The Xlib bitmap path remains
unchanged and continues to use its historical synthetic-bold behavior.

Revenant resolves those point sizes using the active X screen's Xft defaults,
including its DPI. This keeps the initial font and every font-menu slot at the
same physical scale as xterm on displays whose DPI is not 96.

For xterm compatibility, a `size=` field in the first `faceName` item takes
precedence over the separate `faceSize` resource. Revenant removes that field
from the fontconfig pattern and uses it as the Default menu size, so the other
font-menu entries can still derive their own sizes. Prefer setting the size in
one place to avoid this intentionally surprising precedence rule.

If you leave `faceSize`*N* unset, Revenant derives sizes from the bitmap slot
proportions the way xterm does. `fc-match 'DejaVu Sans Mono'` shows what
fontconfig will actually pick; `-report-config` prints the same match.

With bitmap fonts, use X logical font descriptions or aliases from
`xlsfonts`:

```xrdb
XTerm*font:  -misc-fixed-medium-r-normal--13-120-75-75-c-70-iso10646-1
XTerm*font5: -misc-fixed-medium-r-normal--18-120-100-100-c-90-iso10646-1
```

The `TrueType Fonts` entry in the VT Fonts menu toggles `renderFont` at
runtime, and the Xt action `set-render-font(toggle)` does the same from a
translation.

!!! note "Font resolution"
    The [font guide](fonts.md) is the maintained source for Xft role chains,
    shaping, fallback, diagnostics, and current limitations. Traditional
    `renderFont: false` bitmap configurations remain supported independently.

## Colours

```xrdb
XTerm*background:  #1c1c1c    ! -bg
XTerm*foreground:  #d0d0d0    ! -fg
XTerm*cursorColor: #ffaf00    ! -cr
```

Programs that emit 256-colour and 24-bit colour sequences are rendered
directly. The sixteen `color0` … `color15` resources configure the ANSI
palette, using xterm's compiled defaults when no override is present. OSC 4
can still override individual entries at runtime, and OSC 104 restores the
configured resource value. `colorBD`, `colorUL`, `pointerColor`, and related
resources remain accepted but not yet applied.

Reverse video is available through the `reverseVideo` resource, the `-rv` and
`+rv` command-line forms, and the runtime toggle in the VT Options menu. It
swaps the configured default foreground and background without conflating that
widget policy with application-selected DECSCNM or per-cell SGR 7 inverse.
Applications can toggle DECSCNM with `CSI ? 5 h` and `CSI ? 5 l`; it repaints
the complete screen and composes with SGR 7, so applying both inversions restores
normal cell polarity. Background opacity remains attached to the effective
screen background, while explicit cell backgrounds and reversed ink stay
opaque.

### Compositor-backed background opacity

Set `backgroundOpacity` to a value from `0.0` (fully transparent) through
`1.0` (opaque, the default):

```xrdb
XTerm*backgroundOpacity: 0.85
```

Revenant uses a 32-bit ARGB window only when an X compositor owns the screen's
standard compositor selection and the X server offers a suitable visual. If
either condition is missing, it starts normally with the opaque default
visual and logs the fallback. The visual is chosen when Revenant starts, so
changing the resource requires a new window.

Alpha applies to the default terminal background and scrollbar trough.
Foreground text, explicit SGR cell backgrounds, selections, the cursor,
scrollbar thumb, and Athena popup menus remain opaque for legibility. Both the
Xft and bitmap-font renderers preserve the same policy across redraws.

This is real compositor-backed transparency. Revenant does not copy the root
pixmap or implement urxvt-style `transparent`, `inheritPixmap`, tint, or shade
pseudo-transparency resources.

The Main Options menu contains one Athena-native **Opacity** slider immediately
below **SVG Screen Dump** for live changes to the current window. It adjusts
this same background alpha; it does not set whole-window opacity and does not
fade text. Because an X11 window's visual cannot be changed after creation, the
slider is active when the window started on the ARGB path and insensitive after
an opaque-visual fallback.
Once on the ARGB path, moving the slider to 100% and back remains available
for the life of the window.

## Scrollback, selection, and the scrollbar

```xrdb
XTerm*saveLines:       10000  ! -sl N     lines of history kept by libghostty
XTerm*scrollBar:       true   ! -sb / +sb show the Athena scrollbar
XTerm*rightScrollBar:  true   ! -rightbar / -leftbar
XTerm*scrollTtyOutput: false  ! -si / +si  do not jump to the live bottom
XTerm*scrollKey:       true   ! -sk / +sk  jump to the bottom when you type
XTerm*multiClickTime:  250    ! -mc N       multi-click interval in milliseconds
XTerm*charClass:       47:48  ! -cc ranges  make slash join alphanumeric words
XTerm*scrollBarBorder: 1
XTerm*scrollbar.width: 12     ! ordinary Athena child resources also work
XTerm*scrollbar.thickness: 12
```

The scrollbar is a real Xaw `Scrollbar` widget, so anything you could set on
it in xterm — width, thickness, foreground, background, border — works
through the `scrollbar` path. The wheel scrolls five lines per tick;
Shift+Page Up/Down scroll by half a page.

`saveLines` is the active libghostty line constraint. Revenant removes
libghostty's independent default byte constraint when `saveLines` is positive;
otherwise that byte limit can truncate a large configured history long before
the requested row count. A zero value disables history. libghostty removes
history in whole internal pages, so a positive limit is approximate within one
page rather than an exact xterm-style row allocation.

With `scrollTtyOutput: false`, new output preserves the viewport's distance
from the live bottom rather than freezing an absolute history row. For
example, a view of rows 700–724 becomes 701–725 when one new row arrives. A
true value jumps directly to the newest screen, matching xterm's behavior.

Resize reflow can also change the number of display rows represented by saved
history. A viewport that is not at the live bottom remains a historical view;
its last visible row may resemble the current prompt without containing the
current editable input. Scroll to the bottom before treating that as lost
terminal data. Reflow must preserve the underlying text independently of the
viewport position.

If an application enables mouse reporting, buttons, motion, modifiers, and
the wheel are encoded in its requested terminal protocol instead. Hold Shift
to select or scroll locally while reporting is active. Ctrl+button 1, 2, and 3
continue to open the Main Options, VT Options, and VT Fonts menus.

## Word selection classes

Double-click selection follows xterm's character-class model. Letters,
digits, and underscore share class 48 by default; punctuation characters such
as `~`, `/`, `.`, and `-` each form separate classes. Override classes with
`charClass` or `-cc` using xterm's `low[-high][:class]` syntax. For example,
this app-defaults example makes common URL and path punctuation part of class
48:

```xrdb
XTerm*charClass: 33:48,35:48,37-38:48,43-47:48,58:48,61:48,63-64:48,95:48,126:48
```

An undrawn cell is not a space. Double-clicking beyond the last written cell
does not create a word selection. When an existing character or word selection
is extended into that area, the row's undrawn suffix is selected as one
past-end region; triple-click line selection also works on a completely
untouched row.

While Button 1 is selecting, or Button 3 is extending an existing selection,
drag beyond the top or bottom edge to scroll through saved history. The fixed
endpoint remains attached to its original terminal row rather than to a
viewport coordinate, including across multiple pages of scrollback.

Selection uses xterm's named action policy: `SELECT` means X11 `PRIMARY` by
default and `CLIPBOARD` when `selectToClipboard` is true. Button 2 and
Shift+Insert try `SELECT`, then `CUT_BUFFER0`; explicit atom and cut-buffer
arguments are also honored. The [copy and paste guide](../usage/copy-paste.md)
explains X11's multiple selections, action ordering, and legacy cut buffers.

## Window

```xrdb
XTerm*geometry:       100x30+50+50  ! -geometry; size is in characters
XTerm*internalBorder: 4             ! -b N, padding between text and edge
XTerm*title:          work           ! -title / -T
XTerm*iconName:       work           ! -n
XTerm*iconGeometry:   +0+0
```

Font changes and window-manager resizes both keep the terminal's rows and
columns coherent with the kernel PTY size, and the primary screen reflows.
Each grid change invalidates the last-painted frame before Expose handling, so
cached cells from the previous grid are never repainted with new geometry.

Readline 8.3 has a known wrapped-prompt regression: after a prompt with two or
more nonprinting runs wraps, widening the terminal can leave the cursor inside
the prompt. It affects OSC 133 and ordinary SGR prompt regions alike. Bash
development commit `1e9f5e10b2` fixes Readline's stale per-line invisible-byte
metadata; Revenant requires no workaround. See the
[Readline 8.3 resize regression](../reference/bash-readline-resize.md) for the
source diagnosis, exact cursor-offset proof, and fixed-build validation.

## Cursor

```xrdb
XTerm*cursorColor:     #ffaf00
XTerm*alwaysHighlight: true   ! -ah / +ah: filled block even when unfocused
XTerm*cursorBlink:     false  ! false, true, always, or never
XTerm*cursorBlinkXOR:  true
XTerm*cursorOnTime:    600
XTerm*cursorOffTime:   300
```

Applications can select block, underline, and bar cursors with DECSCUSR. The
blinking variants and DEC private mode 12 blink using the configured on/off
times. Revenant retains xterm's separate configured and application blink
states. The application state starts false; DECSCUSR 0/1/3/5 and DECSET 12
set it, while steady DECSCUSR styles and DECRST 12 clear it. DEC private mode
save/restore (`CSI ? 12 s` / `CSI ? 12 r`) preserves and restores it. Both RIS
and the DECSTR soft reset clear it and restore the startup `cursorBlink` policy
if a runtime resource or menu action changed that policy.
`cursorBlinkXOR: true` combines the configured and application states with
XOR; `false` uses OR. `always` and `never` remain absolute and ignore both
combining rules without suppressing application shape or visibility changes.
While either forced policy is active, blink controls do not update the stored
application state; returning to `false` or `true` resumes the prior state. This
matches xterm's `SettableCursorBlink` behavior.
The command-line forms are `-bc`, `+bc`, `-bcn milliseconds`, and `-bcf
milliseconds`. A focused block is filled and an unfocused block is an outline.
See the [TDN cursor-controls
reference](https://toppk.github.io/revenant/tdn/csi/cursor/) for the wire
protocol and compatibility notes. Wiring `cursorUnderLine` and `cursorBar` as
startup shape resources remains roadmap work.

In table form, ordinary `cursorBlink` values are the configured operand;
`always` and `never` bypass the expression:

<!-- markdownlint-disable MD013 -->

| `cursorBlink` | Configured state | Effective policy |
| --- | --- | --- |
| `false` | Steady | Combine with application state using XOR or OR |
| `true` | Blinking | Combine with application state using XOR or OR |
| `always` | Blinking | Always blink; application blink changes are ignored |
| `never` | Steady | Never blink; application blink changes are ignored |

<!-- markdownlint-enable MD013 -->

The defaults, `cursorBlink: false` and `cursorBlinkXOR: true`, match xterm.

## Keyboard and modes

The main and VT menus expose the DEC private modes xterm does — backarrow
key, NumLock keypad, Alt sends Escape, autowrap, reverse wrap, auto
linefeed, application cursor keys, application keypad. Each is read back
from the terminal engine when the menu opens, so the checkmark is the truth
even if a program changed the mode with an escape sequence. Their resource
forms (`backarrowKey`, `altSendsEscape`, `autoWrap`, …) are inventoried but
not yet applied at startup.

## Translations

Key and button bindings are Xt translations on the `vt100` widget, exactly
as in xterm. `-report-config` parses your `VT100.translations` value, prints
it back in reusable syntax, and checks every action it invokes against the
114 actions xterm registers:

```xrdb
XTerm*vt100.translations: #override \n\
    Shift <KeyPress> Insert: insert-selection(SELECT, CUT_BUFFER0) \n\
    Shift <KeyPress> Prior:  scroll-back(1,halfpage) \n\
    Shift <KeyPress> Next:   scroll-forw(1,halfpage) \n\
    <Btn4Down>:              scroll-back(5,line)     \n\
    <Btn5Down>:              scroll-forw(5,line)
```

Actions currently implemented include `insert-selection`, `select-end`,
`set-select`, `scroll-back`, `scroll-forw`, `larger-vt-font`,
`smaller-vt-font`, `set-render-font`, and the popup actions. The report labels
each action *supported* or *unsupported*.
The [default VT bindings audit](../compatibility/default-bindings.md) accounts
for every patch-411 group and records the remaining action-level gaps.

## Menus

Menu labels and menu fonts come from the app-defaults file and can be
overridden like any other resource:

```xrdb
XTerm*SimpleMenu*font:        -adobe-helvetica-bold-r-normal--*-120-*
XTerm*SimpleMenu*background:  gray20
XTerm*SimpleMenu*foreground:  white
XTerm*fontMenu*font3*Label:   Small (6x10)
```

Menus are created under the `menuLocale` resource (default `C`) so bitmap
menu fonts work on UTF-8 desktops without a `Missing charsets` warning, the
same trick xterm uses.

## Diagnostics from the resource system

```xrdb
XTerm*logLevel: warning
```

is the default and prints only warnings and errors. Set it to `info` for
startup and lifecycle records or `debug` for per-key, PTY, and frame details.
The older `XTerm*debug: true` resource remains a compatibility alias when
`logLevel` is unset. See [Diagnostics](../reference/diagnostics.md).

## Checking your configuration

```sh
revenant -report-config                 # everything, annotated, coloured
revenant -report-config | grep -i face  # just the font lines
revenant -fa Hack -fs 12 -report-config # what would these options resolve to?
```

The report is in `.Xresources` syntax, so lines can be pasted back into your
file. Command-line values, X resources, compiled defaults, and unset
resources are coloured differently; every entry says whether Revenant supports
it.
