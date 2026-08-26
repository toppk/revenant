# Konsole

KDE's terminal emulator, also embedded in Kate, Dolphin, and Yakuake through
the KonsolePart component.

## Identity

- `TERM`: `xterm-256color`.
- `TERM_PROGRAM`: not set.
- `KONSOLE_VERSION`: set in the environment, e.g. `240800`.
- DA1: `CSI ? 62 ; 1 ; 4 c` (VT220 class; `4` indicates Sixel) `?`.
- DA2: `?`
- XTVERSION: `?`

## Documentation

- [Konsole handbook](https://docs.kde.org/stable5/en/konsole/konsole/)
- [Repository](https://invent.kde.org/utilities/konsole)
- [KDE Gear release notes](https://kde.org/announcements/)

## Notable behavior

- Sixel graphics (22.04) and the Kitty graphics protocol (22.04) are both
  supported.
- OSC 8 hyperlinks, underline styles, truecolor, and OSC 133 semantic
  prompts are supported.
- OSC 52 clipboard writes are supported and gated by a profile setting.
- Konsole versions follow KDE Gear numbering (`YY.MM`).

## Version notes

Protocol additions are listed in the KDE Gear announcements for each
release; the Konsole repository changelog is the primary source.

## Probe

```sh
echo "$KONSOLE_VERSION"
tools/query '\033[c'
```
