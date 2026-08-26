# VTE (GNOME Terminal and others)

libvte is the terminal widget behind GNOME Terminal, Ptyxis, Tilix,
xfce4-terminal, Terminator, Black Box, and most GTK-based terminals. Wire
behavior belongs to the library; the host application contributes profiles,
menus, and policy toggles.

## Identity

- `TERM`: `xterm-256color`.
- `TERM_PROGRAM`: not set by the library; hosts may set it.
- `VTE_VERSION`: set in the environment as a numeric version, e.g. `7800`
  for 0.78.
- DA1: `CSI ? 65 ; 1 ; 9 c`.
- DA2: `CSI > 65 ; Pv ; 1 c` where `Pv` is the VTE version number.
- XTVERSION: answered; the exact string form is version-dependent.

## Documentation

- [VTE API reference](https://gnome.pages.gitlab.gnome.org/vte/)
- [Repository and NEWS](https://gitlab.gnome.org/GNOME/vte)
- [Ptyxis](https://gitlab.gnome.org/chergert/ptyxis),
  [GNOME Terminal](https://gitlab.gnome.org/GNOME/gnome-terminal)

## Notable behavior

- Originated OSC 8 hyperlinks (jointly with iTerm2, 2017); the
  [hyperlink specification](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda)
  is hosted by a VTE developer.
- Adds Sixel support (0.70, off by default; enabled in later releases
  depending on build options) and does not implement the Kitty graphics
  protocol.
- Deliberately does not implement several xterm reporting features that
  leak state, and gates OSC 52 clipboard access behind host policy.
- Underline styles (`SGR 4:3`) and underline color (`SGR 58`) are supported.
- Many hosts (notably GNOME Terminal) are on a slower release cadence than
  the library, so a distribution's VTE version determines behavior.

## Version notes

See NEWS in the repository; VTE uses `0.<even>` stable series with
`VTE_VERSION` numbers of the form `MMNN`.

## Probe

```sh
echo "$VTE_VERSION"
tools/query '\033[>0q'   # XTVERSION
tools/query '\033[>c'    # DA2 carries the VTE version
```
