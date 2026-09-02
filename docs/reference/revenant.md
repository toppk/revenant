---
man: revenant
section: 1
manual: revenant
description: xterm-compatible X11 terminal emulator with a libghostty core
---

# Revenant reference

This is the reference page for the `revenant` program: the same options,
resources, menus, and actions that the installed `revenant(1)` manual
describes, with links into the pages that explain each subject. The
tables below are generated from the program's own option, action, and menu
tables. Resource names, classes, and literal defaults are checked against the
widget's resource table at build time; the descriptions are maintained by hand.

## Synopsis

```text
revenant [toolkitoption ...] [option ...] [-e command [argument ...]]
```

Without arguments Revenant starts `$SHELL`, falling back to `/bin/sh`, with
`TERM=xterm-256color`. Everything after `-e` is the command and must come
last. Single-dash options accept an unambiguous prefix, and `+opt` turns off
the behaviour that `-opt` turns on.

Start with [first run](../getting-started/first-run.md) if you have never used
xterm, and with [X resources, explained](../configuration/xresources.md) if you
have never configured an X program. The
[keyboard input differences](../compatibility/keyboard-input.md) page explains
the one default that most often surprises an xterm user.

## Options

Most options are shorthand for a resource; the [command-line
options](../configuration/command-line.md) page groups them by subject and the
[feasibility study](../compatibility/command-line-feasibility.md) classifies
every option xterm defines.

--8<-- "options.md"

## Resources

Resources are read through the `XTerm` class and the `vt100` widget, exactly
as in xterm. [Configuring Revenant](../configuration/revenant.md) walks through
them by topic with examples, and [fonts and font
fallback](../configuration/fonts.md) covers the Xft role chains in depth. Run
`revenant -report-config` to see what your own configuration resolved to.

--8<-- "resources.md"

## Menus

Ctrl with Buttons 1, 2, and 3 opens the three menus. Entries Revenant does not
implement yet are shown insensitive; the [popup-menu feasibility
study](../compatibility/menu-feasibility.md) tracks each one.

--8<-- "menus.md"

## Actions

These actions are available to a `translations` resource on the `vt100`
widget. The [default VT bindings](../compatibility/default-bindings.md) audit
compares the shipped translations with xterm's.

--8<-- "actions.md"

## Default translations

--8<-- "translations.md"

## Environment and files

| Name | Meaning |
| --- | --- |
| `DISPLAY` | The X server to contact |
| `SHELL` | The command run when none is given with `-e` |
| `TERM` | Set to `xterm-256color` for the child |
| `/etc/X11/app-defaults/XTerm` | Class defaults shared with xterm, including menu labels |
| `~/.Xresources` | The usual place for user resources, loaded with `xrdb -merge` |
