---
man: revenant-theme-setter
section: 1
manual: revenant
description: preview and set Xresources themes with verified Revenant reload targets
---

# Theme setter utility

Run the PySide interface from the checkout:

```sh
uv run tools/theme-setter-utility.py
```

The script embeds its PySide6 dependency for uv. CLI operations also work with
`python3 tools/theme-setter-utility.py`; they do not import Qt. Keep the sibling
`theme_resources.py` module alongside the utility. It needs X11/Xwayland's
`DISPLAY`, libX11 and `xrdb` to change the resource database.

Use the **Catalog** dropdown to choose **Mine**, **Ghostty**, **iTerm2**,
**terminal.love**, or **Favorites**, then search and preview the palettes.
Ghostty and iTerm2 show the same cached iTerm2 Color Schemes collection pinned
by Ghostty; these are not independent downloads. **Browse catalog online**
opens the selected web catalog in your browser. Download/export a Ghostty theme
or a complete six-digit RGB Xresources palette, then use **Open theme file…**.
Imports go into **Mine**, or into **terminal.love** when that catalog is selected.
The utility does not scrape online catalogs or import native `.itermcolors` files.

**Mine → Current** is a live view of the X server's resource colors for
`xterm.vt100` and the selected resource class. It resolves Xrm specificity and
named colors; unspecified colors use explicit xterm defaults. This is the server
configuration, not an inspection of a running terminal's command-line overrides,
reverse-video setting, or colors changed through OSC sequences.

On first use of the GUI or an Xresources CLI operation, the utility saves that
palette as **Mine → Stock**, once. Stock survives X server restarts and is separate
from the exact per-session resource database backup. Deleting Stock does not
cause it to be captured again.

**☆ Favorite** adds a saved palette to Favorites; **★ Unfavorite** removes it.
**Save selection to Mine…** saves a named copy, including Current. In **Edit
palette**, Mine themes have editable hex fields and clickable color swatches.
**Save edits** persists the palette; **Delete from Mine** removes it, including
its favorite and default references. Current itself is dynamic and cannot be
edited or deleted. Selecting another palette discards unsaved edits.

**Set as default** chooses the selected palette for `--load-default`. For Current,
it saves a fixed copy called **Mine → Current default**. Edits to a saved default
Mine theme take effect the next time that default is loaded. Deleting the default
clears the choice. If no default is selected, the CLI reports an error without
changing resources.

```sh
uv run tools/theme-setter-utility.py --load-default
```

This merges the saved default's nineteen colors into XRDB, without a GUI,
without editing startup files, and without sending signals.

Personal palettes, imports, favorites, the default choice and interface
preferences live in `$XDG_STATE_HOME/theme-setter/catalog.json` (normally
`~/.local/state/theme-setter/catalog.json`) and survive X server restarts.

The custom title bar shows **themesetter — ACTIVE_THEME** on the left, with
an icon toggle for the light/dark interface, a burger menu, minimize,
maximize/restore, and close controls on the right. Drag the title bar to move
the window, double-click it to maximize/restore, or drag the edge/corner grips
to resize. The menu includes import/fetch, loading the saved default, refreshing
the active palette, and opening the log file.
The palette filter independently selects light or dark terminal themes, using
the background's luminance rather than its name. Ctrl+plus / Ctrl+minus zoom the
interface and preview; Ctrl+= and Ctrl+numpad plus/minus also work. Style and font
size are saved. The preview shows normal shell output, directory listings,
diagnostics, a diff, syntax colors, the sixteen theme colors and the complete
256-color indexed palette. Indexed colors 16–255 use the standard fixed cube and
grayscale.

Applying a theme writes exactly nineteen resources: `XTerm*foreground`,
`XTerm*background`, `XTerm*cursorColor`, and `XTerm*color0` through
`XTerm*color15`. The **Resource class** field changes the `XTerm` prefix.
A missing imported cursor color defaults to that theme's foreground.
The **Theme Xresources** tab shows the exact generated settings.

The utility does not edit `.Xdefaults` or `.Xresources`. GUI operations can reload
one selected, verified Revenant window; **XRDB only** sends no signal. CLI
operations, including `--load-default`, still send no signals. Other running
terminals retain their appearance. New terminals use the updated server resources
subject to their startup files and command-line settings. Review the **Theme Xresources** tab to copy a palette into your startup
file for persistence.

## Feedback and troubleshooting

The title bar identifies the palette matching the live XRDB colors; it does not
claim that running terminals have reloaded it. A message confirms each Apply
with the selected name and time. Operation failures appear in that message area
and are logged without opening a blocking error dialog.

The GUI writes flushed logs to stdout and to
`$XDG_STATE_HOME/theme-setter/theme-setter.log` (normally
`~/.local/state/theme-setter/theme-setter.log`), rotating at 1 MiB with two backups.
Startup, explicit actions and exceptions are recorded. Each normal Apply emits
one summary line with the selected theme, effective active palette, target and
result. Low-level XRDB/HUP details and individual overridden-color values are
debug-only.
Selection itself is not logged. Catalog navigation, dragging and resizing are
debug-only.
CLI logs go to stderr so `show THEME > theme.xrdb` still produces a usable file.
Ctrl+C in the launching terminal closes the GUI. Use `--debug` for more details.

## Live preview while browsing

**Live preview: apply selected palette to XRDB** is an opt-in checkbox. With the
theme list focused, Up/Down selects and applies palettes; rapid selections are
debounced for 180 ms. Current is a read-only meta entry and is not reapplied.
Live preview always merges colors and disables **Replace entire database** so
browsing preserves fonts, bindings and unrelated settings. Turning preview off
keeps the last applied colors; use Restore original if you want to undo them.
It starts disabled each time the utility opens and does not change your saved
default choice.

With a verified **Reload target** selected, each successfully applied palette
also sends HUP to that terminal. With **XRDB only**, browsing changes only the
server database.

## Targeted live terminal preview

Start a newly built Revenant, then run the utility from its shell:

```sh
./build/revenant -e bash
# In that new terminal, from this checkout:
uv run tools/theme-setter-utility.py
```

The **Reload target** dropdown defaults to the launching terminal's `WINDOWID`
when eligible. Check its title, version, PID and hexadecimal window ID, enable
**Live preview**, focus the palette list and use Up/Down. **Refresh targets**
rescans available windows. Choosing **XRDB only** disables signal delivery.

Only same-user, local windows advertising `_REVENANT_RELOAD_SIGHUP_V1` are
eligible. The utility uses `_NET_WM_PID`, `WM_CLIENT_MACHINE` and the running
version, pins the selected Linux process with a pidfd, rechecks the window's
capability/identity, then sends HUP through that descriptor. A closed or changed
target is rejected. It does not use `pkill`, signal every terminal, or infer
support from the installed executable's version. It needs `xprop` for discovery
and Linux pidfd support for signaling. Old releases remain ineligible.

Apply, live preview, loading the saved default in the GUI, restoring original
resources and resetting to explicit xterm colors reload the chosen target.
Overridden theme writes produce a warning and send no HUP. Blank database sends
no HUP because terminal reload retains omitted settings. The GUI confirms the
PID/window signaled after an application.

## Xrm precedence and conflicting color keys

X resources are case-sensitive. Revenant normally uses the application name
`xterm` and class `XTerm`; `Xterm` is a third, distinct spelling. An instance-name
binding such as `xterm*background` can win over the `XTerm*background` class key
written by this utility. Tight widget paths and screen-specific properties can
also take precedence. Both keys can exist in XRDB even when only one affects the
terminal.

The utility shows a warning for alternative winning color keys and case variants.
**Resource conflicts** lists the exact winning key and its source property for
each color, using Xrm's own matching rules. After applying a theme, it compares
all nineteen effective colors with the requested palette. Overridden colors
produce a visible warning and log entries with requested/effective values and
the winning keys; the active title continues to reflect the effective palette.
The same checks apply to CLI theme writes and loading the saved default.

Resolve the listed color keys in your live database and startup resources if you
want themes to use consistent `XTerm*` keys. The utility preserves those existing
keys in merge mode; it does not silently delete them or edit `.Xdefaults`.
Replacing the entire database also clears unrelated fonts and bindings.

## Apply and reset

**Apply selected theme** merges the color resources, preserving unrelated
resources such as fonts and translations. More-specific resource keys and
command-line settings may take precedence. **Replace entire database when
applying** removes the server resource databases first; this also removes
unrelated settings from those databases.

The reset actions are distinct:

- **Restore original:** restore the exact server resources saved before the first
  change, including screen-specific properties and absent versus empty values.
- **Blank database:** remove `RESOURCE_MANAGER` and `SCREEN_RESOURCES` from all
  screens. Applications can still load their own startup defaults/files.
- **Reset to xterm default colors:** replace the database with explicit normal
  xterm patch-411 foreground/background, cursor and sixteen ANSI color values.
  These are colors only, not a copy of every xterm resource default.

The xterm default palette is built into `tools/theme_resources.py`; it needs
no saved defaults file. The original live database is saved as a session-specific
`<id>.json` snapshot under `$XDG_STATE_HOME/theme-setter/` (normally
`~/.local/state/theme-setter/`) before the first modification. It survives utility
restarts and is scoped to the current X server session. A new X server gets a
new original. Restore does not consume the saved copy. Failed `xrdb` application
restores the preceding database.

## CLI

```sh
python3 tools/theme-setter-utility.py list
python3 tools/theme-setter-utility.py show 'Catppuccin Mocha'
python3 tools/theme-setter-utility.py set 'Catppuccin Mocha'
python3 tools/theme-setter-utility.py reset original
python3 tools/theme-setter-utility.py reset blank
python3 tools/theme-setter-utility.py reset xterm
python3 tools/theme-setter-utility.py status
```

Add `--replace` after `set THEME` to replace rather than merge. Global options
`--display DISPLAY`, `--themes-dir DIRECTORY`, `--state-dir DIRECTORY` and
`--class CLASS` go before the subcommand. The default resource class is `XTerm`.
Use `--class Revenant` for terminals launched with `-class Revenant`.

`capture-original` explicitly saves a baseline without applying a theme.
`capture-original --replace` makes the current database the new original; prior
snapshot files remain in the state directory.

The default collection is the checkout's ignored `.cache/ghostty-themes/`.
**Fetch Ghostty collection**, or the `fetch` subcommand, downloads the exact
collection named in `upstream/ghostty/build.zig.zon`; fetching therefore needs
that local source checkout and curl. `--themes-dir` can instead select themes
already installed by Ghostty. Fetching and previewing do not alter Xresources.

## Signals and older terminals

The pinned xterm source installs no USR1/USR2 handlers. Released Revenant through
0.8.1 likewise has no handlers for these signals, so their default action is
process termination.

The new startup policy reserves USR1 and USR2 with caught no-op handlers.
HUP requests configuration reload once the event loop is ready; before setup
and during teardown it is a no-op. Caught handlers reset to defaults on exec,
so child commands retain normal HUP and user-signal behavior. Exit and
job-control signals keep their existing behavior.

This change protects newly built terminals, not older processes already running.
The utility only signals selected windows advertising the reload capability;
older windows remain ineligible even after an executable upgrade.
