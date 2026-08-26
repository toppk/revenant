# Windows Terminal and ConPTY

Microsoft's terminal for Windows 10 and 11, and the console host
(`conhost.exe` / OpenConsole) that provides the pseudo-console every Windows
terminal uses to run console applications.

## Identity

- `TERM`: not set by Windows Terminal. WSL distributions set
  `xterm-256color`; Cygwin/MSYS2 shells set their own.
- `TERM_PROGRAM`: not set.
- `WT_SESSION`: a GUID identifying the Windows Terminal session; its
  presence is the usual detection heuristic. `WT_PROFILE_ID` is also set.
- DA1: `CSI ? 61 ; 4 ; 6 ; 7 ; 14 ; 21 ; 22 ; 23 ; 24 ; 28 ; 32 ; 42 c`
  `?` (VT100-level class with feature flags; check the current build).
- DA2: `CSI > 0 ; 10 ; 1 c` `?`.
- XTVERSION: `?`

## ConPTY

Console applications on Windows do not write to a PTY. They call the console
API or write VT to a console handle owned by the console host. ConPTY renders
that into an internal screen buffer and then *re-emits* the buffer state as a
VT stream to the attached terminal. Consequences:

- sequences the console host does not model never reach the terminal;
- attributes and cursor movement may be re-serialized in a different form;
- input from the terminal is decoded into console input records and then
  re-encoded for applications that read VT, which is why the
  `win32-input-mode` protocol (`CSI ? 9001 h`) exists;
- the console host and the terminal have separate version numbers, and both
  matter.

WSL, Cygwin, and MSYS2 programs run through the same path when launched from
Windows Terminal. ConPTY passthrough mode (a later addition) reduces the
rewriting for applications that opt in; check the build.

## Documentation

- [Windows Terminal docs](https://learn.microsoft.com/windows/terminal/)
- [Console virtual terminal sequences](https://learn.microsoft.com/windows/console/console-virtual-terminal-sequences)
- [Repository](https://github.com/microsoft/terminal)
- [Release notes](https://github.com/microsoft/terminal/releases)

## Notable behavior

- Supports truecolor, DECSCUSR, mouse modes including SGR, bracketed paste,
  alternate screen, OSC 8, OSC 52 writes, and OSC 9;4 progress (its own
  extension, adopted by ConEmu-style conventions).
- Sixel graphics arrived in 1.22.
- Originated `win32-input-mode` and OSC 9;9 (current working directory).
- Does not implement the Kitty keyboard protocol or the Kitty graphics
  protocol.
- The legacy console host without VT processing enabled still exists on
  older systems; `ENABLE_VIRTUAL_TERMINAL_PROCESSING` must be set by
  applications targeting it.

## Version notes

`1.xx` numbering with Preview and Stable channels; the release notes list
VT additions.

## Probe

```sh
wt -v
tools/query '\033[c'
```
