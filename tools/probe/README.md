# Terminal probe

This probe is part of TDN. The [one-time migration checklist](../../tdn/docs/probe-migration.md)
maps old scripts to feature commands for manual comparison before legacy removal.

A standalone Go terminal exerciser with commands and a keyboard/mouse TUI. Linux,
Go 1.23 or newer, standard library only; no Python runtime or external Go
modules are needed to run it. Embedded JSON carries the original font/emoji
samples and named palettes. Build it separately from the terminal:

```sh
just probe emoji                                # emoji sections
just probe emoji flags                          # flags only
just probe emoji all                            # all emoji samples
just probe                                      # curated feature browser
just probe features                             # flat slug browser
just probe osc-8-hyperlinks                      # feature scenarios
just probe osc-52-write clipboard-set --target primary --text 'middle-click this'
just probe dec-mode-2027-grapheme-clusters emoji-mode-2027
just probe dec-mode-2027-grapheme-clusters emoji-flags
just probe dec-mode-2027-grapheme-clusters emoji-all
just probe osc-52-write --help                   # list its scenarios
just probe osc-52-write clipboard-set --help     # scenario settings
just probe list                                 # feature slugs and locations
```

`just probe` builds `build-probe/probe` before running it. Go's build cache
reuses unchanged work, so no manual rebuild is needed after source edits.
Arguments pass through as separate shell arguments, preserving quoted spaces.
Copy that binary to another Linux machine of the same architecture to compare
terminals; no source tree is needed. A non-Linux build supports help/list but
reports that interactive tty handling is unavailable.

No command opens the full-screen feature browser. A TDN slug opens its scenario
menu; a slug followed by a case ID runs that scenario directly. A
highlighted list and detail pane show the selected case and its expected
behavior, policy, TDN references and cleanup. Controls work throughout the
browser, case actions and settings:

- Up/Down selects; Space, Right or Enter opens; Left or Escape goes back.
- Click a row to open it; the mouse wheel scrolls the selection.
- Type a number (including multi-digit numbers) or name, then Enter to open.
- Home/End and Page Up/Page Down navigate longer lists; `0` then Enter or `q`
  goes back, or exits at the top level.
- Open **Settings** to change named values such as the selection target, text,
  timeout or width mode. Choose enum values from a list, toggle booleans, or
  edit text and numbers. Repeatable capabilities/resources have their own list.
  Select **Done** to apply, **Reset defaults** to reset, or **Cancel** to discard.
  Invalid values leave the setting unchanged and display the reason.
- **Help** opens a scrollable reference, with keyboard and wheel navigation.

Assessment prompts are off by default. Start `just probe --assess` (or
`just probe text emoji --assess`) to ask for a human judgment after each run.
JSON recording with `--output` is independent of that flag; measured widths
and decoded replies remain available during ordinary testing.

The retained `text emoji` submenu offers **all**, **mode-2027**, **single**,
**variation-selectors**, **skin-tones**, **zwj**, **flags**, **boundaries**, and
**capacity**. Each section reuses the original samples. Mode 2027 runs a short
ZWJ/flag comparison with the mode off and on; other cases use the selected
legacy/cluster/both width mode. `text emoji` opens the submenu; use
`text emoji all` to run the former complete suite.

The **Procedural glyph rendering** cases present a visual catalog of characters
that are useful to draw from cell geometry instead of a font. This renderer
behavior has no universal capability name. xterm calls its subset *built-in
line-drawing characters* and controls it with `forceBoxChars`; Ghostty calls its
implementation a *sprite face*; foot documents
`box-drawings-uses-font-glyphs`, whose default false value selects generated
drawings.

The catalog is grouped by purpose. Each group starts with constructions that
use the characters as intended: complete boxes and junctions, continuous block
bars, Braille plots, Powerline segments, or tiled mosaics. Compact code-point
sheets follow for inspecting individual glyphs. The catalog contains no baked-in
terminal support claims; record the observed result for the terminal being
tested. The union includes Unicode Box Drawing, Block Elements, Braille,
Symbols for Legacy Computing and its supplement, DEC Special Graphics,
Powerline, geometric pieces, and a private branch-symbol range.

```sh
just probe text-box-drawing text-procedural-union  # every group
just probe text-box-drawing text-procedural-box    # U+2500-U+257F
just probe text-block-drawing text-procedural-block
just probe text-braille-drawing text-procedural-braille
just probe text-powerline-drawing text-procedural-powerline
```

Mouse navigation requires working, permitted SGR mouse reports; keyboard and
number navigation remain available when mouse reporting is disabled.

At 80×24, dense color and width samples use inspection pages. A finished
visual stage returns to the browser without asking for another inspection;
query-only output still waits once. The test ID is printed above visual pages.

To select text **directly on the menu**, press **F2** or click **F2 Select text**
in the bottom blue bar. The menu stays visible and stops repainting, with mouse reporting
disabled. Drag to select the command, slug, or any other text and use the
terminal's usual copy action. F2, Space/Enter, or q/Escape resumes the menu.
This also works in Help and settings editors; it uses no OSC 52 permission.

Underlined breadcrumb segments are clickable: select an ancestor to jump there,
including Home. Long labels shorten to keep every level reachable. **Test
location** releases menu mouse capture and prints the full path, case ID and
command as selectable terminal text for a bug report; Space/Enter or q/Escape returns to the case.

The layout adapts to resize; very small terminals show a resize message.
Before a case runs, the browser leaves its alternate screen and relinquishes
mouse reporting and cursor control. After inspection, it resumes browsing.
For repeated testing, use the equivalent command printed above the case.

Menus do not repaint or read input while a case runs. Most visual cases wait
for Space/Enter; `--delay SECONDS` advances automatically and `--no-pause` skips pauses.
Use **Space/Enter to continue** an inspection and **q/Escape to exit the test**. In a browser
session q returns to the case menu after cleanup; a direct command exits with
status zero and a `stopped` evidence outcome. Stopping skips assessment prompts.
q also works during timed drawing, delays and framed queries. It is recognized
as a whole key, so letters/spaces inside replies are never navigation.

Input-capture exceptions are stated on screen: Space is captured as test data;
raw/Kitty capture reserves q/Escape to exit, while cooked input requires q or Escape then Enter.
The answerback read window treats all bytes as data until its short timeout.
During framed queries, a lone ESC is held until the reply deadline; use q or
Ctrl+C for immediate exit. In menus the Escape ambiguity delay is at least the
configured query timeout (normally 0.5 seconds), allowing slower split replies.
Text-setting editors keep Space and q as text and use Enter to save / Escape to
cancel. Ctrl+C normally exits the process. During raw/Kitty keyboard capture it
is recorded as input and capture continues; q/Escape ends the case early, or
the `--seconds` deadline ends it normally. Both paths run cleanup.
SIGINT/SIGTERM/SIGHUP restore tty settings and run registered cleanup actions.
After the first process signal, normal signal handling resumes so a second
signal can terminate a blocked cleanup. Unexpected panics also run all registered
case cleanups before propagating.

## Feature identity and navigation

TDN's feature slug is the entry point; a separate stable case ID identifies a
scenario. One feature can have many cases, and a case may exercise several
features. Review its stated scope when interpreting results.

Breadcrumbs are curated in `tdn/data/probe-navigation.json`. For example,
`osc-8-hyperlinks` lives under **Window and desktop / Hyperlinks**. Moving a
feature in that tree does not rename its command. Slugs retain the control and
purpose where available; specification provenance is separate metadata.

`tdn/data/features.yaml` owns the titles, sequences and specification references.
Run `just update-probe-registry` after editing the registry or navigation to
refresh the embedded metadata. This development step needs Python and PyYAML;
ordinary `just probe` builds and runs using Go alone. TDN tests detect stale
snapshots; Go tests reject unmapped features, unused navigation entries and
duplicate case IDs. Adding navigation does not create a compatibility claim.

Commands such as `text emoji flags` remain aliases for the same handlers.
`just probe cases` opens the former command-oriented hierarchy.
`features --json` lists feature metadata; `list --json` retains the case catalog
with stable `case_id`, canonical `command`, old `path`, and `feature_ids`.

## Evidence

```sh
just probe csi-21-t-title-report titles-query --target both \
  --terminal xterm --terminal-version '411' \
  --configuration 'Allow Window Ops enabled' \
  --output /tmp/title-results.json
```

The output is an array of schema-version-1 case records, also usable for a
whole menu session. It contains the stable `case_id`, selected `entry_feature_id` for feature entry
points, legacy case path, all exercised TDN IDs, supplied assessed
version/configuration, environment, arguments, exact query/input bytes as
base64, width findings, and optional tester observations. A successful process
exit leaves the overall result `unassessed` unless a human observation was
provided. Width findings have their own narrower contract verdicts. No reply
can mean denial, unsupported behavior, empty data, or timeout.

Reply capture is bounded to 64 KiB per query and the evidence trace to 1 MiB
per case, with a truncation flag. Output files are replaced atomically with
private permissions. Results are evidence for review; this program does not
edit `tdn/terminals/*.yaml` or infer broad feature support.

## State and migration

Query operations do not change the tested setting. Explicit clipboard/color/
font set or reset commands persist. Demos restore queried state where possible;
unreadable colors reset to configured defaults, unqueryable modes use stated
baseline defaults, and title cleanup requests matching stack pops. No cleanup
can force a restoration that the terminal's permission policy denies. Shell
fixtures close synthetic command markers and report the probe's cwd; they do
not execute the displayed shell commands.

The old `probe-*.py`/shell commands and their `just` entries remain available as
comparison oracles. This is a new command interface, not a flag-compatible
wrapper. Native cases cover all existing probe families and share the exact
font/emoji sample sets and six palette definitions. The color display is a
new layout; the original color-tour UI and its full xterm spawn flag surface
remain in `probe-colors.py`. Native `colors palette spawn` supplies palette
resources and accepts extra `--xrm` values. Graphics still covers static inline
RGBA only, matching the existing fixture's scope.

## Maintenance

New features use this Go probe now. Add or extend a native case; do not add a
parallel Python/shell implementation. Keep the legacy tools and their recipes
until the maintainer completes the TDN migration checklist and resolves its
gaps. Retirement must also replace tests that import the old sample data.

For each new case, register its feature ID in `tdn/data/features.yaml`, add its
curated location to `tdn/data/probe-navigation.json` when needed, and run
`just update-probe-registry`. Declare a stable case ID, feature IDs, settings,
expected behavior, policy and cleanup in `cases.go`. Exercise completion and
early exit at 80×24, and add relevant decoder/PTY checks. Run `just test-probe`
and `just check-tdn` before dispatching the feature for manual review.

`cases.go` is the shared command/menu catalog. Keep its feature IDs aligned
with TDN; several cases can exercise subsets of one feature. Case code uses
one `Session` for terminal I/O, deadlines, byte capture and cleanup. Measurements
must use quiet queries: printing a CPR reply during measurement moves the
cursor and invalidates the result. Keyboard framing retains split events;
reported text is escaped before display.

Standard CPR and a modified F3 key can have identical bytes. Do not type while
width measurements run; the probe cannot distinguish those bytes reliably.
Termios restoration uses immediate TCSETS rather than a potentially blocking
TCSETSW drain, so exiting cannot hang waiting for terminal output to drain.
Emoji sections carry stable IDs in the embedded data; sample order does not
select the named section. `go test` checks the binary's internal catalog;
`just check-tdn` additionally verifies freshness against the source registry.

```sh
just test-probe
```

Go tests cover decoders, framing, width verdicts, option validation and evidence.
The Python test harness uses PTYs to exercise all native cases, interruption,
tty restoration, menus and JSON. It also compares embedded sample/palette data
with the original probes. Python is a test dependency only. Actual pixel,
window-manager and human-input acceptance remains a separate terminal test.
