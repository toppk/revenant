# Page conventions

TDN pages share one vocabulary so that a reader can compare a cursor-style
page with a graphics page without relearning the notation. This page is the
contract that every feature page follows.

## Notation

Sequences are written with named control bytes and spaces between tokens.
The spaces are notation, never bytes, unless written as `SP`.

| Token | Bytes | Meaning |
| --- | --- | --- |
| `ESC` | `0x1b` | Escape |
| `CSI` | `ESC [` or `0x9b` | Control Sequence Introducer |
| `OSC` | `ESC ]` or `0x9d` | Operating System Command |
| `DCS` | `ESC P` or `0x90` | Device Control String |
| `APC` | `ESC _` or `0x9f` | Application Program Command |
| `ST` | `ESC \` or `0x9c` | String Terminator |
| `BEL` | `0x07` | Bell; accepted as an OSC terminator by most emulators |
| `SP` | `0x20` | A literal space byte used as an intermediate |
| `Ps` | | One numeric parameter |
| `Pm` | | Several numeric parameters separated by `;` |
| `Pt` | | Free-form text parameter |

The seven-bit forms `ESC [`, `ESC ]`, `ESC P`, `ESC \` are the interoperable
choice. The single-byte C1 forms collide with UTF-8 continuation bytes and are
disabled or ignored by many modern emulators.

Examples in shell use `printf` with octal escapes so they run in any POSIX
shell:

```sh
printf '\033[2 q'          # CSI 2 SP q
printf '\033]0;title\033\\' # OSC 0 ; title ST
```

## Status vocabulary

Every feature carries exactly one status:

| Status | Meaning |
| --- | --- |
| **Standard** | Defined by ECMA-48, ISO 6429, or an ITU/ANSI equivalent |
| **DEC** | Defined in a DEC VT-series manual; a de facto standard |
| **xterm** | Introduced by xterm and documented in *XTerm Control Sequences* |
| **Extension** | Introduced by another emulator, documented by that emulator, adopted by at least one other |
| **Vendor-private** | Documented by one emulator and not adopted elsewhere |
| **Convention** | Widely implemented, but no primary document defines it |
| **Disputed** | Primary sources disagree; the page explains the split |

"Extension" is not a value judgment. The Kitty keyboard protocol and OSC 8
hyperlinks are extensions that many emulators now treat as baseline.

## Compatibility tables

A compatibility cell records what a source says, not what is fashionable.

| Cell | Meaning |
| --- | --- |
| **Yes** | Documented as supported, with the documenting version where known |
| **Partial** | Supported with a documented limitation; the note says which |
| **No** | Documented as unsupported, refused, or removed |
| **Observed** | Not documented, but measured with a named probe and version |
| `?` | Not checked; a contribution target, not a claim |

A cell should link or footnote its source. Observations name the emulator
version, the date, and the probe used. Observations never overrule an
implementation's own documentation on intent; they record outcomes.

Terminals are listed in a fixed order on every table so columns line up
across pages: xterm, VTE, Konsole, kitty, WezTerm, Ghostty, foot, Alacritty,
Contour, mintty, PuTTY, Windows Terminal, Apple Terminal, iTerm2, xterm.js,
tmux. A page may omit columns that are irrelevant, but must not reorder them.

## Feature page template

```markdown
# Feature name

Status: one of the values above.

One-paragraph summary: what the feature does and who uses it.

## Syntax
## Behavior
## Compatibility
## Probe
## Sources
```

Optional sections, placed before **Compatibility**: **History**,
**Parameters**, **Responses**, **Pitfalls**.

## Terminology

- **Application**: the program writing to the PTY; a shell, editor, or TUI.
- **Emulator**: the program that owns the PTY master and renders cells.
- **Multiplexer**: tmux, screen, or zellij; an emulator on one side and an
  application on the other, which is why they appear in compatibility tables.
- **Report**: bytes the emulator writes to PTY input in reply to a query.
- **Mode**: a persistent switch set with `h` and reset with `l`.
- **Cell**: one column of one row; wide characters occupy two cells.

See the [Glossary](glossary.md) for the full list.
