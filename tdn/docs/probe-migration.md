# Legacy probe migration checklist

The terminal probe is part of TDN. This is a **one-time manual comparison
worksheet** for replacing the 12 Python/shell probe scripts with the standalone
Go probe. Its source remains in `tools/probe/`; feature slugs come from TDN.
All results below start **Pending**. A mapping is not a claim of verified parity
or terminal feature support.

**New feature work uses the Go probe now.** Extend its native cases rather than
adding legacy scripts. The old tools and their `just` recipes remain available
until the maintainer completes this manual review and the gaps below are
resolved. Their removal is a later migration step.

Run commands from the repository root, inside the terminal being checked.
`just probe` builds as needed. Use the same terminal version, font, resources,
Ops permissions and **80×24** geometry for each pair. Start a fresh terminal
between comparisons when a command leaves persistent state. Compare visible
behavior, reply bytes, cleanup and usability; the layouts and navigation need
not match the old scripts.

Record the terminal/version, font and configuration here: **________**.
Change a result to **Pass**, **Difference: …**, or **Blocked: …** and include
enough detail to reproduce it. For a second terminal/configuration, copy this
worksheet. Leave missing replacements unresolved until they are implemented or
explicitly retired.

In the Go probe, **Space/Enter continues; q/Escape exits the test**. Keyboard
capture treats Space/Enter as input; cooked input needs q/Escape then Enter.
The answerback read window treats all bytes as data until its timeout. In the
browser, **F2 Select text** lets you copy a command or slug. Append `--help`
to a new command for its settings. Optional `--output /tmp/probe-result.json`
records evidence; `--assess` separately enables human assessment prompts.

## Colors and rendering

The color samples now use four inspection pages. The old expanded color tour
has additional layouts and controls; the table marks that comparison as partial.

| Old command | New command | What to compare / known difference | Result |
| --- | --- | --- | --- |
| `bash tools/probe-color.sh` | `just probe csi-sgr colors-samples` | SGR attributes, underline styles, 16/256 colors and truecolor forms; inspect all four pages. | Pending |
| `python3 tools/probe-colors.py --mode all` | `just probe sgr-256-colors colors-samples` | Expanded 16/256/truecolor display coverage; redesigned samples, not identical layouts. Also compare old `--mode 16`, `--mode 256` and `--mode true`. | Pending |
| `python3 tools/probe-colors.py --mode tour` | `just probe sgr-256-colors colors-samples` | **Partial:** full tour, palette cycling and presentation flags have no complete replacement; see gaps below. | Pending |
| `python3 tools/probe-colors.py --query` | `just probe osc-4-palette-query colors-palette-query --index 0` | Old queries 0–15 together; new queries one index. Repeat with indices 1–15; compare decoded RGB and silence under GetAnsiColor denial. | Pending |
| `python3 tools/probe-colors.py --list-palettes` | `just probe osc-4-palette-set colors-palette-list` | Same six named palettes and RGB entries. | Pending |
| `python3 tools/probe-colors.py --mode palette --apply-palette --palette verify` | `just probe osc-4-palette-set colors-palette-demo --palette verify` | Inspect installed palette. Old leaves it installed; Go restores queried colors or resets unreadable entries. Repeat for xterm, tango, solarized, gruvbox and nord. | Pending |
| `python3 tools/probe-colors.py --reset-palette` | `just probe osc-104-palette-reset colors-palette-reset` | Reset all indices to configured defaults; persistent request. | Pending |
| No dedicated single-index legacy command | `just probe osc-4-palette-set colors-palette-set --index 1 --color '#ff0000'` | Additional Go case: persistent index write; query index 1 and inspect red samples, then reset. | Pending |
| `python3 tools/probe-colors.py --spawn --mode all --palette verify --program xterm --geometry 80x24` | `just probe osc-4-palette-set colors-palette-spawn --palette verify --program xterm --geometry 80x24` | New terminal, resource colors and samples. Both accept repeatable `--xrm`; the remaining old spawn switches are gaps below. | Pending |
| `bash tools/probe-reverse-video.sh` | `just probe dec-mode-5-decscnm colors-reverse` | SGR inverse, DECSCNM repaint of existing rows, explicit RGB, manual widget Reverse Video and restoration. | Pending |
| `python3 tools/probe-dynamic-colors.py` | `just probe osc-11-default-background-color colors-dynamic-demo` | Default foreground/background/cursor versus explicit RGB, query replies and restoration. | Pending |
| `python3 tools/probe-dynamic-colors.py --query` | `just probe osc-11-default-background-color colors-dynamic-query` | Query all three colors; compare RGB replies with displayed pixels. | Pending |
| `python3 tools/probe-dynamic-colors.py --background '#142850'` | `just probe osc-11-default-background-color colors-dynamic-set --target background --color '#142850'` | Persistent set; repeat with foreground and cursor. Old can set different colors in one invocation; new uses separate invocations. | Pending |
| `python3 tools/probe-dynamic-colors.py --reset all` | `just probe osc-11-default-background-color colors-dynamic-reset --target all` | Configured defaults restored; also compare individual foreground/background/cursor resets. | Pending |
| `python3 tools/probe-dynamic-colors.py --scheme` | `just probe csi-996-n-color-scheme-query colors-scheme` | Decoded light/dark query result. | Pending |
| `python3 tools/probe-sync.py --mode compare` | `just probe dec-mode-2026-synchronized-output rendering-sync --mode compare` | Tearing off versus on. Repeat both with `--mode off` and `--mode on`; timing flags keep their names. | Pending |
| `python3 tools/probe-sync.py --mode on --hold-ms 1500` | `just probe dec-mode-2026-synchronized-output rendering-sync --mode on --hold-ms 1500` | Timeout release during a long hold; resize/expose and exit cleanup. | Pending |
| `bash tools/probe-osc8.sh` | `just probe osc-8-hyperlinks links-demo` | Explicit links versus plain URLs, hover decoration, activation and closing the link. Use the terminal's configured modifier bindings. | Pending |

Dynamic-color commands use ST by default in both runners; add `--bel` to both
to compare BEL. Keep permission settings stable during demo cleanup. Compare
allowed and denied behavior separately. Go's default query timeout is 0.5 s;
the old dynamic-color and title scripts use 1 s. Set `--timeout 1` on both if
timing affects the result.

## Clipboard and titles

The old clipboard script defaults to **BEL**, while Go defaults to **ST**.
The commands below add `--bel` to preserve the old wire format. For an ST
comparison, add `--st` to the old command and remove `--bel` from the new one.
Use `--timeout 2` on both to match the old clipboard query deadline.

| Old command | New command | What to compare | Result |
| --- | --- | --- | --- |
| `python3 tools/probe-clipboard.py --query --target p` | `just probe osc-52-read clipboard-query --target primary --bel --timeout 2` | Exact decoded PRIMARY data or silence under GetSelection denial. | Pending |
| `python3 tools/probe-clipboard.py --set 'middle-click this' --target p` | `just probe osc-52-write clipboard-set --target primary --text 'middle-click this' --bel` | Verify with middle-click or `xclip -o -selection primary`; a sent request alone proves nothing. | Pending |
| `python3 tools/probe-clipboard.py --clear --target p` | `just probe osc-52-write clipboard-clear --target primary --bel` | Named selection cleared; other selections remain intact. | Pending |
| `python3 tools/probe-clipboard.py --invalid --target p` | `just probe osc-52-write clipboard-invalid --target primary --bel` | Compare behavior with a known selection already owned. | Pending |
| `python3 tools/probe-titles.py --query --target both` | `just probe csi-21-t-title-report titles-query --target both --timeout 1` | Icon and title replies; repeat with title and icon separately and reports denied. | Pending |
| `python3 tools/probe-titles.py --target both` | `just probe csi-22-t-push-title titles-stack --target both --timeout 1` | Original → A → B → A → original; repeat title/icon separately, normal completion and early exit. | Pending |

Repeat clipboard rows with old `--target c` / new `--target clipboard`, and
old `--target s` / new `--target select`. Repeat under the relevant Window Ops
permissions. Title restoration also depends on working, permitted stack and
Title Ops behavior. Clipboard writes and clears are deliberately persistent.

## Emoji, fonts and keyboard capture

The old emoji script is one complete run. Its sample sections now have separate
case IDs, as well as an all-sections case. Section rows below refer to the
corresponding part of that old run, not to nonexistent old command-line flags.

| Old command / section | New command | What to compare | Result |
| --- | --- | --- | --- |
| `python3 tools/probe-emoji.py` | `just probe dec-mode-2027-grapheme-clusters emoji-all` | All original samples, measured advances, classification, glyphs and mode restoration. | Pending |
| `python3 tools/probe-emoji.py` — mode off/on comparison | `just probe dec-mode-2027-grapheme-clusters emoji-mode-2027` | Additional short ZWJ/flag comparison; does not replace the full suite. | Pending |
| `python3 tools/probe-emoji.py` — single emoji | `just probe dec-mode-2027-grapheme-clusters emoji-single` | Single-codepoint emoji widths and glyphs. | Pending |
| `python3 tools/probe-emoji.py` — variation selectors | `just probe text-vs16-width emoji-variation-selectors` | Text/emoji presentation and VS16 width under both regimes. | Pending |
| `python3 tools/probe-emoji.py` — skin tones | `just probe dec-mode-2027-grapheme-clusters emoji-skin-tones` | Modifier sequences. | Pending |
| `python3 tools/probe-emoji.py` — ZWJ | `just probe dec-mode-2027-grapheme-clusters emoji-zwj` | Joined sequences and cell advance. | Pending |
| `python3 tools/probe-emoji.py` — flags | `just probe dec-mode-2027-grapheme-clusters emoji-flags` | Regional indicators and tag flags. | Pending |
| `python3 tools/probe-emoji.py` — boundaries | `just probe dec-mode-2027-grapheme-clusters emoji-boundaries` | Sequence boundaries and wrapping classifications. | Pending |
| `python3 tools/probe-emoji.py` — capacity | `just probe dec-mode-2027-grapheme-clusters emoji-capacity` | Long clusters and capacity limits. | Pending |
| `python3 tools/probe-fonts.py` | `just probe dec-mode-2027-grapheme-clusters text-fonts` | Original font/shaping samples, mark placement and width results. | Pending |
| `python3 tools/probe-keymodes.py` — cooked stage | `just probe csi-modify-other-keys input-keyboard-cooked` | Line discipline and encoded input. New stages are separate commands. | Pending |
| `python3 tools/probe-keymodes.py --raw-only` | `just probe csi-modify-other-keys input-keyboard-raw` | Raw key bytes, modifiers and restoration of tty settings. | Pending |
| `python3 tools/probe-keymodes.py --kitty-only` | `just probe csi-u-kitty-keyboard input-keyboard-kitty` | Kitty key events and decoding; protocol stack restored on exit. | Pending |

Both width runners accept `--regime legacy`, `--regime cluster` or
`--regime both` (the default), plus `--no-pause`. The short `emoji-mode-2027`
case always compares off/on and has no regime setting. Go keyboard capture is
bounded by `--seconds` (default 20); q/Escape is reserved for navigation.
Its cooked/raw feature entry identifies the keyboard family, not a claim that
the case exhaustively tests modifyOtherKeys.

## Former feature dispatcher

These rows cover every subcommand of `tools/probe-features.py`, splitting font
query/set and cursor inspection/style cycling into separate checks.

| Old command | New command | What to compare | Result |
| --- | --- | --- | --- |
| `python3 tools/probe-features.py answerback` | `just probe c0-enq-answerback identity-answerback` | Configured bytes verbatim and empty/default silence. | Pending |
| `python3 tools/probe-features.py tcap --cap TN --cap Co` | `just probe dcs-xtgettcap identity-tcap --cap TN --cap Co` | Decoded names/values; TN versus child TERM, GetTcap denial. | Pending |
| `python3 tools/probe-features.py mouse --mode 1000` | `just probe dec-mode-1000-mouse input-mouse --mode 1000` | Buttons, wheel, focus and live Mouse Ops toggle; repeat modes 9, 1002 and 1003. | Pending |
| `python3 tools/probe-features.py font` | `just probe osc-50-font-query font-query` | Reply bytes or permitted silence; Font Ops policy. | Pending |
| `python3 tools/probe-features.py font --font fixed` | `just probe osc-50-font-set font-set --font fixed` | Explicit persistent font request; restore font manually afterward. | Pending |
| `python3 tools/probe-features.py underline` | `just probe sgr-58-underline-color text-underline` | Independent underline ink, styles, SGR 59, inverse and default text color. | Pending |
| `python3 tools/probe-features.py pointer` | `just probe osc-22-pointer-shape input-pointer` | Pointer shape over grid and default restored on exit. | Pending |
| `python3 tools/probe-features.py cursor` | `just probe csi-decscusr text-cursor` | Startup cursor appearance and query replies. | Pending |
| `python3 tools/probe-features.py cursor --styles` | `just probe csi-decscusr text-cursor --styles` | Explicit cursor styles and return to configured default. | Pending |
| `python3 tools/probe-features.py identity` | `just probe csi-da1 identity-reports` | DA1, DA2 and XTVERSION; Go additionally queries DA3. | Pending |
| `python3 tools/probe-features.py unknown` | `just probe diagnostics-unknown-apc diagnostics-unknown` | Unsupported APC diagnostics; no reply required. | Pending |
| `python3 tools/probe-features.py cwd --cwd /tmp` | `just probe osc-7-working-directory shell-cwd --cwd /tmp` | Reported directory in terminal diagnostics; also try an existing path with spaces/Unicode. | Pending |
| `python3 tools/probe-features.py prompts` | `just probe osc-133-prompt-marks shell-prompts` | Synthetic prompt markers and manual prompt navigation. | Pending |
| `python3 tools/probe-features.py pipe` | `just probe ui-pipe-command-output shell-pipe` | Synthetic output boundaries; manually invoke the terminal's pipe action. | Pending |
| `python3 tools/probe-features.py notify` | `just probe osc-9-notification notifications-urgency` | Focus away before delivery, urgency/notification, focus to clear. | Pending |
| `python3 tools/probe-features.py progress` | `just probe osc-9-4-progress notifications-progress` | Progress states and clear on exit. | Pending |
| `python3 tools/probe-features.py glyphs` | `just probe text-box-drawing text-glyphs` | Box, block, braille and powerline joining. | Pending |
| `python3 tools/probe-features.py copy` | `just probe selection-copy-feedback selection-copy` | Manually select/copy; feedback and exact pasted text. | Pending |
| `python3 tools/probe-features.py search` | `just probe search-scrollback-literal selection-search` | Repeated, Unicode and wrapped matches, navigation and viewport restoration. | Pending |
| `python3 tools/probe-features.py graphics` | `just probe apc-kitty-static-images graphics-static` | Four-color inline image, clipping/scroll/resize, deletion of only the probe's image. | Pending |

Fixtures for terminal UI actions require those actions to be implemented and
invoked manually. A silent request or printed fixture does not establish
support. Inspect the case's Help for permission and cleanup details.

## Gaps and retirement decisions

| Legacy surface | Current replacement / decision needed | Result |
| --- | --- | --- |
| `probe-colors.py --mode tour` / `--all-palettes` | No full tour or automatic all-palette sequence. Run individual palette demos; decide which remaining tour presentations to port or retire. | Pending |
| `probe-colors.py --apply-palette --palette verify` | No persistent whole-palette install command. Go's palette demo restores on exit; single-index set persists. Do not treat demo as an exact replacement. | Pending |
| `probe-colors.py --width`, `--quiet`, `--ascii`, `--wait`, `--no-clear`, `--compact` | New layout uses terminal geometry and shared inspection controls; no flag-for-flag parity. Decide which old display modes still matter. | Pending |
| `probe-colors.py` spawn `--exec-flag`, `--resource-class`, `--direct-color`, `--term-name`, `--font`, `--font-size`, `--hold`, `--dry-run` | Go has program, geometry, palette and repeatable `--xrm`, with fixed XTerm resource class and `-e`. Resource settings can use `--xrm`; other options need a port or retirement decision. | Pending |
| `tools/_width_probe.py` | Shared helper, not a standalone probe. Go width measurement and embedded samples replace runtime use; compare behavior through emoji/font rows above. | Pending |

The Markdown Unicode sampler `tools/probe-scripts.md` is reference material,
not one of the executable scripts. Automated `tests/` drivers are also outside
this manual migration. Neither should be removed as a side effect of retiring
the 12 scripts.

Before removal, resolve every difference and gap, then update old `just`
recipes, documentation and any tests importing legacy helpers/data. Preserve
the final comparison notes with the migration commit. Ongoing terminal support
assessments belong in the [per-terminal database](registry.md), separately from
this runner-migration worksheet.
