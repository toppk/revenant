---
man: revenant-font-fallback-review
section: 7
manual: maintainers
description: emoji fallback design and coverage review
---

# Emoji fallback design and coverage review

**Review date:** 2026-09-19. **Status:** findings and proposed amendment;
no font-routing or packaging changes are implemented by this review.

The trigger was ordinary application output containing bare U+1F6E0:

```rust
eprintln!("🛠 Installed {}-{}", name, version);
```

Revenant rendered tofu; appending VS16 rendered the emoji. The application
string is valid. Requiring applications to add VS16 would conceal a failure
of text-presentation coverage, not repair it.

## Conclusion

This is a gap across product defaults, candidate discovery, glyph fitting,
and acceptance criteria. It is not simply a missing installed font or an
omitted `faceNameEmojiText` resource. Adding Ghostty's monochrome font as an
ordinary numbered fallback still fails this case, even at the maximum
`limitFontWidth: 50`.

Retain the existing capture model, primary metrics authority, atomic clusters,
and backend-owned widths. The maintainer has ruled out bundling fonts. Use
installed fonts with presentation-aware discovery and a separately specified
fitting policy. Repair candidate enumeration independently. Do not silently extend
`faceNameEmoji` to capture text or repurpose the doublesize/CJK role.

## Evidence and scope

Reviewed Revenant source at `67f05c3e84317ba3e9b6e3a78400f6910253d170`,
including the existing staged documentation changes, and the local
`upstream/ghostty` checkout at
`0c2a290d3a3e2a599be3a43435d778a5896667ee`. Source references below name
functions as well as files so line movement does not erase the evidence.

The review rebuilt `build-agent-gcc/revenant`, ran 13 diagnostic launches
under Xvfb, inspected font cmaps with fontTools, compared system Fontconfig
ordering and matching, and reran six existing integration suites. All six
passed: font baseline, emoji routing, user fallback, system fallback, Han,
and wide-slot boundary. Passing those suites does not resolve this incident.

Diagnostic launches used 12-point MonoLisa Custom on the host, or DejaVu Sans
Mono in isolated fixture universes, an explicit color emoji face and CJK wide
face, Unicode presentation, and legacy widths. Fixture runs used an isolated
home and Fontconfig file. These were route/advance observations, not a new
pixel-golden acceptance suite or a test of pristine installed defaults.
Samples included bare/VS15/VS16 U+1F6E0, U+2139, U+1F600+VS15, and
`日あ한`. Logs and launch arguments were retained during the review under
`/tmp/revenant-font-audit/`; the findings below do not depend on retaining
that temporary directory.

### Findings

1. **The monochrome fixture is incomplete for the advertised problem.**
   `tools/stage-font-fixtures` extracts `NotoEmoji-Regular.ttf` from the
   Noto emoji 2.028 archive. Its internal font version is 1.05. Its cmap has
   no U+1F6E0. Ghostty's bundled monochrome asset is version 3.005 and has
   that glyph. The host's version 3.003 has it too. Against the pinned
   Unicode 17 Emoji minus Emoji_Presentation set, excluding the twelve
   keycap bases, the fixture covers **63 of 207** text-default bases;
   Ghostty's and the host's monochrome assets cover **207 of 207**. These
   are scalar cmap counts, not claims about complete sequences or artwork.

2. **Having a font on disk does not make it reachable.**
   `src/vt_font.c`, `VtFontEnsureSystemFallbacks`, asks `FcFontSort` for a
   slot-wide, charset-trimmed list, then stores at most 32 candidates
   (`XTP_XFT_FALLBACK_CAPACITY`). This happens before atom coverage and
   paint-policy validation. The host route logged `count=32`; system
   `fc-match -s` for MonoLisa placed Noto Emoji at position 36 and Noto Sans
   Symbols 2 at 44. Those CLI positions support the diagnosis but are not
   a byte-for-byte dump of Xft's substituted request. A 50-font activation
   budget cannot recover candidates omitted by a 32-entry storage cap.

3. **Charset trimming discards meaningful alternatives.**
   In the isolated `routing` universe, system `fc-match -s` omits Noto Emoji;
   `fc-match -a` includes it. The color font's scalar coverage can make the
   monochrome face look redundant, although text policy later rejects its
   color-only glyphs. This independently affects U+2139 and forced-text
   grinning face. Trimming by cmap cannot establish equivalence of ink,
   presentation, sequence shaping, UVS coverage, or supported font format.
   Turning trimming off alone is insufficient: the 32-candidate cap would
   still discard later useful faces, often after admitting more styles.

4. **A family name is not a verified monochrome asset.**
   On the host, `/usr/bin/fc-match 'Noto Emoji'` selects Twemoji. The actual
   named-fallback trial then reports rejected outline ink and tofu.
   `Noto Emoji:color=false` selects the installed monochrome Noto font.
   The unqualified family name is therefore not a dependable monochrome request.
   This follows installed policy: `45-generic.conf` associates Noto Emoji
   with generic `emoji` and language `und-zsye`; `60-generic.conf` appends
   `color=true` when color is unspecified. The personal
   `~/.config/fontconfig/fonts.conf` prefers Twemoji for generic `emoji`.
   `fc-pattern -c -d` confirms the resulting color preference; explicitly
   setting `color=false` prevents that default. This is not a corrupt RPM
   or Fontconfig randomly ignoring the name.
   With the personal XDG configuration excluded, the same unqualified
   request selects Noto Color Emoji instead; the constrained request still
   selects monochrome Noto Emoji. The system color preference is independent
   of the personal choice of Twemoji.
   Also, PATH resolves `fc-match` to a Linuxbrew executable, while Revenant
   links `/lib64/libfontconfig.so.1`; the two tools return different
   candidate orderings. Diagnostics must identify the actual library and
   effective file, not assume a shell's `fc-match` reproduces the renderer.

5. **Even the correct modern monochrome font fails the width governor.**
   With the host's actual monochrome font selected using `:color=false`,
   U+1F6E0 has a normalized advance of 24.161 pixels in an 11-pixel cell.
   Ghostty's bundled font, tested inside an isolated universe as an explicit
   numbered fallback, has advance 20.134 in a 10-pixel cell. Both fail at
   limits 10 and 50. By comparison, Noto Sans Symbols 2 has advance 13.533
   in an 11-pixel cell and succeeds at 50 in the earlier reproduction.
   Raising the global limit was a font-specific workaround, not a solution
   for Noto Emoji. The existing fixture's U+2139 also fails at 50
   (18.369 pixels in a 10-pixel cell).

6. **The tests deliberately accept some of these misses.**
   `tests/xvfb-emoji-routing.sh` expects `role=tofu` for `info-text`,
   `policy-text-grin`, and `policy-text-color-wide`. Commit `2b4d1ba`
   changed those expectations from `primary` to `tofu` while retaining
   the `mono` pixel class. Deterministic tofu makes a missing glyph visible
   and monochrome, so `mono` is not evidence of successful text rendering.
   This history does not prove the earlier `primary` result was recognizable
   artwork: it may have been a font's `.notdef`. It does show that these
   cases became explicit missing-glyph acceptance, without a companion
   positive test requiring usable monochrome emoji fallback.

7. **The interactive probe tests a different contract.**
   `tools/probe-emoji.py` explicitly never grades artwork. The Go migration
   preserves that scope: `tools/probe/width.go` derives verdicts from CPR
   and mode 2027, prints "Artwork is not graded", and records width findings.
   `tools/probe/cases.go` associates these cases with width-related TDN IDs.
   Optional `--assess` records a human judgment after a run; it is not a
   mandatory per-sample coverage gate. The corpus includes text-default BMP
   symbols and VS15, but no hammer-and-wrench case and no required
   supplementary-plane text-default fallback success. A correct-width tofu
   passes the mechanical check. The probe is honest about this limitation;
   treating it as rendering acceptance was the error.

8. **The architecture specifies selection but not baseline supply.**
   Revision 5 principle 4 explicitly forbids a new default fallback source.
   The role contract has a color/emoji-presentation slot, a wide-text slot,
   and a Han override; text-default pictographs normally use primary or
   wide fallback. That classification is valid, but nothing guarantees a
   usable text pictograph face will be supplied, reached, or fitted there.
   The practical configuration names a color font and CJK font, but no
   dependable monochrome emoji baseline. Packaging declares renderer
   libraries, not a color-plus-monochrome asset baseline.

9. **Diagnostics hide distinctions necessary to investigate this failure.**
   Width deferral is encoded as the generic `shape` miss in
   `FallbackRangeWithCluster`. Route misses do not identify each rejected
   candidate's file. The 32-entry candidate cutoff is not reported as a
   distinct exhaustion reason; `truncated` currently records policy
   truncation when system fallback is disabled. A final tofu result cannot
   distinguish missing coverage, lost candidates, paint rejection, and
   advance rejection without additional debug/source investigation.

### Reproduction and asset identity

Use system Fontconfig tools linked to the same library as Revenant:

```sh
/usr/bin/fc-list ':charset=1f6e0' family file
/usr/bin/fc-match -f '%{family} %{file}\n' 'Noto Emoji'
/usr/bin/fc-match -f '%{family} %{file}\n' 'Noto Emoji:color=false'
font-fixtures-stage/run routing /usr/bin/fc-match -s 'DejaVu Sans Mono'
font-fixtures-stage/run routing /usr/bin/fc-match -a 'DejaVu Sans Mono'
```

For a live diagnostic, launch with `-debug`, primary MonoLisa Custom at
12 points, `-fe 'Noto Color Emoji'`, and a CJK doublesize face. Compare
`fallbackFace1: Noto Emoji`, `Noto Emoji:color=false`, and
`Noto Sans Symbols 2`, at width limits 10 and 50, while printing:

```sh
printf '\U0001F6E0 Installed demo-1.0\n\U0001F6E0\uFE0E\n\U0001F6E0\uFE0F\n'
```

Use an isolated home, explicit Fontconfig universe, and explicit resources
for repeatable tests. Do not call a configured diagnostic a default-install
test. The exact monochrome files compared have SHA-256:

```text
fixture NotoEmoji-Regular.ttf:
415dc6290378574135b64c808dc640c1df7531973290c4970c51fdeb849cb0c5
upstream/ghostty/src/font/res/NotoEmoji-Regular.ttf:
cd07743a4c93d8da929afdd28c1d368f11da478a1093146a13610081c2a58440
```

## What Ghostty provides

The local checkout's `src/font/embedded.zig` declares separate color and
monochrome Noto assets. `SharedGridSet.zig` registers both as fallback faces
on Linux, without a user family-name lookup; macOS normally uses Apple Color
Emoji. `CodepointResolver.zig` consults its existing collection before system
discovery. `discovery.zig` disables Fontconfig charset trimming. The FreeType
render path applies glyph constraints and transforms outlines to fitted
dimensions; it is not Revenant's inherited fallback advance rejection rule.

Ghostty also has an eventual any-presentation collection search. That is a
separate policy choice, not a prerequisite for supplying monochrome emoji.
Do not copy it accidentally: Revenant promises that explicit text and disabled
color paint do not leak color. Importing `libghostty-vt` does not import
Ghostty's frontend font collection, assets, or rendering policies.

## How the gap survived

The evidence supports a process failure across three separately successful
work streams. Format tests proved the renderer could paint selected files;
width probes proved cursor arithmetic; resolver tests proved contractual
priority, budgets, and deterministic misses. None owned the end-to-end
requirement that common application text render recognizably with the
delivered default font supply.

The fixtures contained a monochrome font, so inventory alone suggested that
case was represented. Its release archive/version was not tied to an explicit
coverage requirement. The general resolver work then made misses deterministic
and updated expected routes to tofu. The documentation describes these
mechanisms accurately in many places, but its broad Unicode/emoji coverage
language does not expose this default-availability gap. This explains what
the artifacts demonstrate; it does not reconstruct undocumented reviewer
intent or prove every earlier version had the same visible bug.

## Revised direction: installed fonts, no bundling

The original review proposed packaged fallback assets. The maintainer rejected
bundling; that proposal and its `builtinEmojiFallback` resource are withdrawn.
The later roadmap decision permits evaluating vendored fonts in the future,
provided users can replace or disable both faces. Immediate work still uses
installed fonts; that future option does not restore the withdrawn API.
The remaining design work concerns installed-font discovery and fitting.
The following is a direction for an amendment, not implemented behavior or
a shipped resource API. The follow-up proposal names `faceNameEmojiText`
as the targeted monochrome fallback resource.

### Preserve the invariants

Keep primary entry 1 as the sole metrics authority; commit widths in the
backend before selection; route full atoms; preserve exact Han IVS checks;
keep normal-canonical style selection, generation invalidation, transactional
reload, bounded reporting, and the bitmap/Xft separation. Keep Han, kana,
Hangul, and wide punctuation on their existing semantic routes.

Do not widen bare U+1F6E0, force all emoji into color, make `faceNameEmoji`
capture text by default, or put monochrome emoji into `faceNameDoublesize`.
Do not silently raise or reinterpret the inherited `limitFontWidth` resource.

### Request and validate the intended presentation

Use the system's installed fonts. The Fedora package already present is
`google-noto-emoji-fonts-20250623-4.fc44.noarch`; its file is
`/usr/share/fonts/google-noto-emoji-fonts/NotoEmoji-Regular.ttf`, internal
version 3.003. It is the same monochrome family as Ghostty's 3.005 asset,
but not byte-identical. Both cover U+1F6E0 and all 207 text-default bases in
the census. Our old fixture is version 1.05, not the installed RPM font.

For automatic monochrome discovery, communicate that requirement before
Fontconfig substitution (for example `color=false`), then validate actual
whole-atom ink and effective file/index. Do not change the user's global
emoji preference or force every primary/wide request to monochrome: those
roles can serve multiple presentations. A family request is a preference,
not an exact-file guarantee. Preserve the existing allowance for genuine
monochrome outlines in fonts that also contain color tables; a non-color
search must not become the only possible search for usable text ink.

The follow-up API proposal is an explicit `faceNameEmojiText`
monochrome-emoji fallback resource, backed by presentation-aware discovery.
It offers a stable user choice, but must not recapture successful primary text, consume numbered
fallback resources implicitly, or alter Han/wide boundaries. Adding a
resource alone does not repair discovery or fitting.

### Controls, budget, fitting, and reporting

Keep `systemFallback: false` as the switch disabling unnamed installed-font
discovery. Keep `limitFontsets: 0` disabling fallback and preserve the
positive glyph-bearing activation budget. An explicit new resource, if one
is adopted, needs a specified position relative to entry 2, numbered fonts,
and emoji/doublesize rescue; it must not introduce a hidden second budget.

A separate fitting amendment is necessary. The installed monochrome font's
normalized advance is 24.161 pixels in an 11-pixel cell, so it still fails
after the matching problem is fixed. Evaluate uniform shrink-to-span for
emoji-bearing atoms under a new, explicit policy with an opt-out. Preserve
baseline/centering, glyph positions, outline transforms, and damage/cursor
clips. Clipping away half the symbol is not successful fitting. Never alter
backend widths or silently reinterpret inherited `limitFontWidth`.

Define whether fitting applies to explicit and automatic candidates, and at
what priority, before implementing it. Preserve every existing successful
explicit choice. Exclude ordinary ASCII keycap bases unless they form an
actual sequence; keep arbitrary CJK and other scripts outside emoji-specific
fitting. Retain exact IVS checks and no color leakage under VS15/forced text.

Today the emoji route exhausts doublesize fallback, including its system
candidates, before emoji-slot named/system fallback. Inserting a new retry
inside `AllFallbacksWithCluster` could preempt later user choices. Specify
and test the complete order rather than adding opportunistic retries.

Report effective file/version, requested and effective presentation, paint
mode, and fitting scale. Add distinct width-rejection and discovery-exhaustion
reasons and enough bounded candidate identity to explain a miss. Decide
NDJSON schema compatibility before extending rung/miss enums. Cache and
reload tests must cover any new policy, including failed replacement loads.

### Repair system discovery independently

Retain role-seeded preferences but discover candidates relevant to the
atom's coverage, without discarding alternatives solely by scalar cmap
redundancy. Iterate or page candidates and cache successful faces within a
bounded resource model. Separate enumeration/work limits from the inherited
glyph-bearing activation budget. Report an explicit bound when work stops;
do not report exhaustive absence when a cap ended the search.

Test a usable font beyond 32 irrelevant candidates, color-before-monochrome
coverage overlap, same-family styles, whole-sequence composition, and IVS.
Do not simply increase 32 or remove trimming while keeping the same
unfiltered fixed prefix. Changes to inherited normal-style selection need
T0 replay and a documented compatibility decision; the existing principle
of free missing-glyph candidates is undermined by the current prefix cap.

### Installed-font requirements and fixtures

Document distribution packages for color and monochrome coverage and validate
actual resolved files. Do not bundle fonts, fetch them at runtime, modify
global Fontconfig policy, or depend on files inside an upstream checkout.
A clean machine without suitable fonts must report the missing prerequisite
honestly; no default complete-coverage claim is possible without font supply.

Keep the old fixture as an explicitly incomplete adversary. Add a modern
monochrome test asset matching a documented package/upstream release, with
hash, license, and coverage checks. Test fixtures are not shipped runtime
assets. Test installed packages with the documented external font prerequisites
and separately test absent fonts and hostile aliases. CJK availability and
regional glyph choices remain separate documented requirements.

## Acceptance and probe changes

Keep width and rendering judgments separate. Add a dedicated rendering
scenario with recognizable-glyph expectations, per-sample human assessment,
and font/configuration evidence, while keeping the existing CPR probe's
scope honest. Add bare/VS15/VS16 hammer-and-wrench plus text-default symbols
outside the primary font, forced-text emoji-default bases, color-disabled
cases, and the original application-style line. A width pass must not become
a glyph-coverage pass or a broad TDN emoji-support claim.

The automated matrix must require non-tofu route identity **and** visible
ink/presentation/bounds, plus independent CPR. Use genuinely absent glyphs
or deliberately disabled rescue to retain negative tofu tests. Do not
replace current expected-tofu cases with a `mono`-only positive assertion.

Required gates for the amendment:

| Area | Required observations |
| --- | --- |
| Default product | Clean home with documented external fonts; original install line renders without hand-written fallback chains |
| Presentation | Bare, VS15, VS16, forced text/emoji, color on/off; identifiable glyph, correct paint and unchanged width |
| Fonts | Current monochrome coverage census, old incomplete font, absent installed font, alias to color font |
| Discovery | More than 32 irrelevant fonts; overlapping color/mono coverage; useful later candidate remains reachable |
| Priority | Explicit primary, emoji, wide, Han, entry 2 and numbered fallbacks retain successful choices |
| Controls | New fitting policy on/off, system discovery on/off, zero/one/exhausted budgets |
| Geometry | Narrow/wide bases, both mode-2027 regimes, multiple primary fonts, sizes and DPI; no clipped symbols or neighbor ink |
| CJK | Han override, kana, Hangul, shared punctuation and exact IVS; no emoji source capture or unwanted recapture |
| Lifecycle | Normal/bold/italic family stability, cache eviction, menu sizes, successful and failed reloads |
| Packaging | Installed artifacts work with documented external fonts and diagnose their absence; no bundled runtime fonts |

## Work sequence

1. Adopt the installed-font prerequisites and amend the discovery/fitting
   contract, including opt-outs, budget, scope, reporting, and compatibility.
2. Add the modern fixture, coverage census, and positive/negative acceptance
   cases before changing the resolver. Preserve existing historical fixtures.
   Done: see the [emoji artwork gate](emoji-artwork-gate.md). It confirms
   finding 5 for the one-cell case and adds one qualification: the advance rule
   in `XtpFontFallbackAdvanceFits` applies only when the committed width is 1, so
   a width-2 text atom bypasses it and the same 3.003 file then renders real
   monochrome artwork. The font and its coverage are therefore usable; the
   missing pieces are one-cell acceptance and a fitting policy.
3. Fix candidate discovery with bounded, observable behavior and T0 review.
4. Implement the chosen presentation-aware discovery and scoped fitting
   policy, with its priority and control matrix.
5. Add the rendering probe, update documentation/support evidence, and gate
   installed release artifacts. Run the full compiler/backend/sanitizer matrix.

The six existing passing suites and the 13 diagnostic runs establish the
gap and constrain the proposal. They are not acceptance evidence for the
unimplemented amendment. This review intentionally makes no blanket claim
of complete Unicode 17 sequence coverage or universal CJK font availability.
