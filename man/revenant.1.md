---
title: REVENANT
section: 1
header: General Commands Manual
date: 2026
---

# NAME

revenant - xterm-compatible X11 terminal emulator with a libghostty core

# SYNOPSIS

**revenant** [*toolkitoption* ...] [*option* ...] [**-e** *command* [*argument* ...]]

**xterm+** [*option* ...]

# DESCRIPTION

**revenant** is a terminal emulator for the X Window System. It preserves the
visible interface of **xterm**: the **XTerm** resource class, the **vt100**
widget, the Ctrl+button popup menus, the bitmap and TrueType font slots, and
the translation vocabulary. An existing xterm configuration in
*~/.Xresources* applies to **revenant** unchanged. Underneath that interface
the terminal engine is **libghostty-vt**, which supplies resize reflow, 24-bit
colour, current mouse and focus reporting, grapheme-aware cell width, and the
kitty keyboard protocol.

Without arguments **revenant** runs the shell named by **SHELL**, or */bin/sh*
if that is unset, and sets **TERM** to **xterm-256color**. Everything after
**-e** is the command to run instead and must come last.

**revenant** is measured against xterm patch 411, which defines the visible
contract, and against the capability floor of a thin libghostty host. Where
the two conflict, xterm decides how a feature is presented and libghostty
decides that the feature exists. The one default difference a user notices
first is keyboard encoding: **revenant** distinguishes Ctrl+I from Tab, Ctrl+M
from Enter, and Ctrl+[ from Escape without an application opting in, because
the engine's encoder does. Applications that assume xterm's exact byte stream
may behave differently.

Single-dash options accept an unambiguous prefix, so **-geo** means
**-geometry**. An option beginning with **+** turns off the behaviour that the
same option with **-** turns on.
Unknown options are rejected before a display is opened.

# OPTIONS

The X Toolkit options **-display**, **-geometry**, **-name**, **-class**,
**-xrm**, **-iconic**, **-fg**, **-bg**, and **-rv** work as in every Xt
program. The remaining options follow xterm's spelling and each sets the
resource named in its entry.

--8<-- "options.md"

# RESOURCES

**revenant** is configured through X resources, as **xterm** is. The
application instance name is **xterm** and its class is **XTerm**; the terminal
widget is **vt100** of class **VT100**. A line in *~/.Xresources* such as

    XTerm*faceName: DejaVu Sans Mono

is loaded with **xrdb**(1) and read when a new window starts. A resource named
on the command line beats a resource file. Run **revenant -report-config** to
print every resource **revenant** resolved, where each value came from, and
whether it is supported, accepted but ignored, or unsupported. Resources that
**xterm** defines but **revenant** does not yet act on are accepted silently so
that a shared configuration file does not fail.

--8<-- "resources.md"

# FONTS

**revenant** has two text renderers. With **renderFont** false it uses X
bitmap fonts through the eight **font** slots, exactly as xterm does. With
**renderFont** true it uses Xft and fontconfig: **faceName** names the primary
face and **faceSize** its point size, and the other menu slots take their sizes
from **faceSize1** through **faceSize7**. The TrueType Fonts entry of the VT
Fonts menu switches renderer at run time.

Under Xft a cluster is drawn by the first face in its role chain that can shape
the whole cluster and has ink for it. Emoji try **faceNameEmoji**, then
**faceNameDoublesize**, then the primary face; wide non-emoji text tries
**faceNameDoublesize** then the primary face; Han characters try **faceNameHan**
first. After the explicit roles, **fallbackFace1** through **fallbackFace16**
are tried in order, and then, if **systemFallback** is true, fontconfig's own
candidates up to the **limitFontsets** budget. A face that cannot draw a whole
ZWJ sequence, keycap, or flag is skipped for that cluster rather than drawing
part of it. Shaping is done by HarfBuzz and never changes the cell width the
terminal engine has committed; a glyph that does not fit is clipped to its
cells.

Point sizes are resolved against the screen's Xft DPI, so a face requested at
10 points has the same physical size it would have in xterm. A **size=** field
in the first **faceName** entry overrides **faceSize**, for compatibility with
xterm.

# POINTER USAGE

Button 1 selects text; a second click within **multiClickTime** selects words
using the **charClass** table and a third selects lines. Button 3 extends the
selection from the nearer end. Button 2 pastes. Dragging past the top or bottom
edge scrolls through saved history while selecting. Meta with Button 1 makes a
rectangular selection.

Selections are published to the X selections named in the binding, by default
the symbolic **SELECT** and **CUT_BUFFER0**. **SELECT** means **PRIMARY** unless
**selectToClipboard** is true, in which case it means **CLIPBOARD**. Pasting
tries the same names in order and requests **UTF8_STRING** before **STRING**.
There is no default binding that pastes **CLIPBOARD** while ordinary selection
stays on **PRIMARY**; add a translation that invokes
**insert-selection(CLIPBOARD)** to get one.

When an application enables mouse reporting, button, motion, and wheel events
inside the text area are encoded for the application instead. Hold Shift to
select or scroll locally while reporting is active. Ctrl with Buttons 1, 2, and
3 always opens the Main Options, VT Options, and VT Fonts menus.

Text carrying an OSC 8 hyperlink, or a visible HTTP(S) URL detected in ordinary
terminal text, is underlined while Shift is held and the pointer is over it.
Shift+Button 1 on such text opens the link with **xdg-open**(1); only **http**
and **https** links are opened.

# MENUS

The three popup menus keep xterm's names and entries. An entry that
**revenant** does not yet implement is shown insensitive rather than removed,
so the menus look as an xterm user expects. Menu labels come from the
app-defaults file and can be overridden like any other resource.

--8<-- "menus.md"

# ACTIONS

Key and button bindings are Xt translations on the **vt100** widget, exactly
as in xterm. The following actions are available to a **translations**
resource. The **-report-config** output checks a user's translation table
against them.

--8<-- "actions.md"

# DEFAULT TRANSLATIONS

--8<-- "translations.md"

# ENVIRONMENT

**DISPLAY**
:   The X server to contact.

**SHELL**
:   The command run when none is given with **-e**.

**TERM**
:   Set to **xterm-256color** in the child's environment.

**XAPPLRESDIR**, **XENVIRONMENT**
:   Consulted by the X Toolkit when loading resources, as for any Xt program.

# FILES

*/etc/X11/app-defaults/XTerm*
:   Class defaults shared with xterm, including menu labels.

*~/.Xresources*
:   The usual place for user resources, loaded with **xrdb**(1).

# SEE ALSO

**xterm**(1), **resize**(1), **xrdb**(1), **xlsfonts**(1), **fc-list**(1),
**fc-match**(1), **xdg-open**(1), **X**(7), **XStandards**(7)

The complete manual, including compatibility ledgers that record every
difference from xterm, is at <https://toppk.github.io/revenant/docs/>.

# BUGS

**revenant** is not yet a complete xterm replacement. Menu entries shown
insensitive, resources reported as accepted but ignored, and the keyboard
encoding difference described above are the known gaps. Report others at
<https://github.com/toppk/revenant/issues>.

# AUTHORS

Kenneth Topp. The xterm-derived compatibility material is covered by the xterm
license; the rest of **revenant** is MIT licensed.
