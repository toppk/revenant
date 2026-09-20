#!/bin/sh
#
# Automatic fallback discovery: presentation awareness and candidate selection.
#
# Naming a face is not the problem this suite covers; finding one is.  A color
# face that covers the same characters can crowd a usable monochrome face out of
# the candidate inventory, either through FcFontSort coverage trimming or through
# the fixed prefix the inventory stores.  The atom then renders as tofu although
# an installed font covers it.
#
# The first case is the reproduction.  The rest hold the boundaries that repair
# must not move: explicit choices, systemFallback, the zero budget, the CJK
# routes, and an honest tofu when no monochrome face exists at all.

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB XTERM_PLUS WINDOW-INK FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
fixture_root=$4
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"
# The fixture runner isolates Fontconfig but not the X resource environment, and
# every case here asserts which role automatic discovery reaches.  A developer's
# own ~/.Xdefaults naming faceNameEmojiText would silently replace those roles
# with emoji-text, so the terminal runs against an empty home and with inherited
# resource files neutralized.  Nothing outside test_dir is touched.
mkdir "$test_dir/empty-home"

legacy_mono=NotoEmoji-Regular.ttf
modern_mono=NotoEmoji-Regular-3.003.ttf
color_face=Noto-COLRv1.ttf
failures=
checked=0

fail_case()
{
    failures="$failures $1"
    printf 'FAIL  %-22s %s\n' "$1" "$2"
}

ink_class()
{
    printf '%s\n' "$1" | sed -n 's/^class=\([^ ]*\).*/\1/p'
}

note()
{
    checked=$((checked + 1))
    printf 'CASE  %-22s %s\n' "$1" "$2"
}

# start_case NAME UNIVERSE PROBE PRESENTATION EXTRA-RESOURCES...
start_case()
{
    name=$1
    universe=$2
    probe=$3
    presentation=$4
    shift 4
    log=$test_dir/$name.log
    cpr=$test_dir/$name.cpr
    done_dir=$test_dir/$name.done

    # shellcheck disable=SC2016
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$fixture_root/run" "$universe" "$terminal" -debug +sb \
        -fa 'DejaVu Sans Mono:rgba=none' -fs 16 "$@" \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm "xterm.vt100.emojiPresentation: $presentation" \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H\033[?25l%s\033[6n" "$2"; IFS= read -r -d R reply; printf "%sR" "$reply" >"$1"; printf "\033]2;font-discovery-ready\007"; while ! test -d "$3"; do sleep 0.05; done' \
        bash "$cpr" "$probe" "$done_dir" >"$test_dir/$name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" font-discovery-ready "$name" 300
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
}

stop_case()
{
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

sample_cells()
{
    "$window_ink" "$window" --expose $((4 + $1 * cell_width)) 4 $(($2 * cell_width)) \
        "$cell_height" 0x000000
}

# check_route NAME BASE EXPECTED-ROLE [EXPECTED-FILE]
check_route()
{
    route=$(grep -F "route base=$2 " "$log" | tail -1)
    printf '%-22s %s\n' "$1" "$(printf '%s' "$route" | sed 's/.*font: //' | cut -c1-92)"
    if ! printf '%s\n' "$route" | grep -F -q " role=$3 "
    then
        fail_case "$1" "expected role=$3"
        return
    fi
    if test "$#" -eq 4 && ! printf '%s\n' "$route" | grep -F -q "/$4 "
    then
        fail_case "$1" "expected effective file $4"
    fi
}

check_contained()
{
    result=$(sample_cells 0 "$2")
    following=$(sample_cells "$2" 1)
    printf '%-22s %s | following %s\n' "$1" "$result" "$(ink_class "$following")"
    if test "$(ink_class "$result")" != "$3"
    then
        fail_case "$1" "expected pixel class $3, got $(ink_class "$result")"
    elif test "$(ink_class "$following")" != blank
    then
        fail_case "$1" "ink reached the following cell: $following"
    fi
}

check_cursor()
{
    actual=$(od -An -tx1 -v "$test_dir/$1.cpr" | tr -d ' \n')
    want=$(printf '\033[1;%sR' "$2" | od -An -tx1 -v | tr -d ' \n')
    if test "$actual" != "$want"
    then
        fail_case "$1" "cursor reported $actual, expected column $2"
    fi
}

printf '== the remaining discovery miss ==\n'
# The reported configuration: an explicit color emoji face and CJK wide face,
# with a monochrome face installed in the same universe that covers U+2139.
# Discovery must reach that monochrome face.
note trimmed-mono "a monochrome face survives a color face covering the same base"
start_case trimmed-mono routing ℹ unicode \
    -fe 'Noto Color Emoji' -fd 'Noto Sans Mono CJK JP'
check_route trimmed-mono U+2139 fallback "$legacy_mono"
check_contained trimmed-mono 1 mono
check_cursor trimmed-mono 2
# The recovered candidate must be identifiable as such in the diagnostics.
if ! grep -F -q 'source=monochrome' "$log"
then
    fail_case trimmed-mono "expected the activated candidate to be named as recovered"
fi
if ! grep -E -q -- 'discovered monochrome Xft candidate .* scanned=[0-9]+/[0-9]+ reserve=1/' "$log"
then
    fail_case trimmed-mono "expected the per-atom scan to report the candidate it took"
fi
stop_case

# Thirty-six filler faces carry the primary family name and sort ahead of the
# monochrome emoji face, putting it past the ordinary inventory bound of 32.  A
# pass that took a blind prefix, of either sort, would miss it; the per-atom scan
# skips the fillers because none of them covers the atom.
note crowded-inventory "the needed face is reached past the inventory bound"
start_case crowded-inventory crowded 🛠 unicode
check_route crowded-inventory U+1F6E0 fallback "$modern_mono"
check_contained crowded-inventory 1 mono
check_cursor crowded-inventory 2
scanned=$(sed -n 's/.*discovered monochrome Xft candidate .* scanned=\([0-9]*\)\/\([0-9]*\) .*/\1 \2/p' "$log" | tail -1)
printf '%-22s scanned %s (ordinary bound 32)\n' crowded-inventory "$scanned"
# shellcheck disable=SC2086
set -- $scanned
if test "$#" -ne 2 || test "$1" -le 32
then
    fail_case crowded-inventory "expected the scan to pass the ordinary bound, got '$scanned'"
fi
if ! grep -F -q 'source=monochrome' "$log"
then
    fail_case crowded-inventory "expected the recovered candidate to serve the atom"
fi
if grep -F -q 'XtpCrowdFiller' "$log"
then
    fail_case crowded-inventory "a filler face was selected"
fi
stop_case

# One session, two atoms.  Nothing covers U+E100, so its search must scan the
# whole sort and append nothing; the emoji that follows must still find its face.
# Reversed, the emoji's search must not have consumed anything the other atom
# needed either.  A shared consuming cursor would fail one of these two.
note order-unsupported-first "an unsupported atom does not spend the search"
start_case order-unsupported-first crowded "$(printf '\356\204\200\360\237\233\240')" unicode
check_route order-unsupported-first U+E100 tofu
check_route order-unsupported-first U+1F6E0 fallback "$modern_mono"
first=$(sample_cells 0 1)
second=$(sample_cells 1 1)
printf '%-22s unsupported %s\n' order-unsupported-first "$first"
printf '%-22s emoji %s\n' order-unsupported-first "$second"
if test "$(ink_class "$first")" != mono || test "$(ink_class "$second")" != mono
then
    fail_case order-unsupported-first "expected both cells to carry monochrome ink"
fi
if test "$(ink_class "$(sample_cells 2 1)")" != blank
then
    fail_case order-unsupported-first "ink reached the cell after the emoji"
fi
check_cursor order-unsupported-first 3
if ! grep -F -q 'source=monochrome' "$log"
then
    fail_case order-unsupported-first "expected the emoji to be served by a discovered candidate"
fi
stop_case

note order-emoji-first "the emoji's search leaves the reserve usable"
start_case order-emoji-first crowded "$(printf '\360\237\233\240\356\204\200')" unicode
check_route order-emoji-first U+1F6E0 fallback "$modern_mono"
check_route order-emoji-first U+E100 tofu
first=$(sample_cells 0 1)
second=$(sample_cells 1 1)
printf '%-22s emoji %s\n' order-emoji-first "$first"
printf '%-22s unsupported %s\n' order-emoji-first "$second"
if test "$(ink_class "$first")" != mono || test "$(ink_class "$second")" != mono
then
    fail_case order-emoji-first "expected both cells to carry monochrome ink"
fi
check_cursor order-emoji-first 3
if ! grep -E -q -- 'discovered monochrome Xft candidate .* reserve=1/' "$log"
then
    fail_case order-emoji-first "expected exactly one reserve entry to be spent"
fi
stop_case

printf '\n== environments that must keep working ==\n'
note clean-defaults "no face resources at all, modern monochrome installed"
start_case clean-defaults routing-modern 🛠 unicode
check_route clean-defaults U+1F6E0 fallback "$modern_mono"
check_contained clean-defaults 1 mono
check_cursor clean-defaults 2
stop_case

# This universe states a color preference on any request that does not say
# otherwise, including the ones automatic discovery makes from the primary
# family.  Verified with the fixture's own Fontconfig: the color face then sorts
# ahead of the primary family itself.  The consequence, asserted below, is that
# the color face is tried for this text atom and refused for lack of outline ink
# before the monochrome face serves it.  A color preference must not decide
# presentation.
note alias-color "a color-preferring rule on the discovery request is survivable"
start_case alias-color alias-emoji 🛠 unicode
check_route alias-color U+1F6E0 fallback "$modern_mono"
check_contained alias-color 1 mono
check_cursor alias-color 2
if ! grep -F -q 'outline fallback rejected' "$log"
then
    fail_case alias-color "expected the preferred color face to be tried and refused"
fi
if grep -E -q -- "route base=U\+1F6E0 .*/$color_face " "$log"
then
    fail_case alias-color "the color face served a text-presentation atom"
fi
stop_case

note mono-absent "no monochrome face exists, so tofu is the honest answer"
start_case mono-absent cbdt 🛠 unicode
check_route mono-absent U+1F6E0 tofu
check_contained mono-absent 1 mono
check_cursor mono-absent 2
stop_case

printf '\n== color emoji with nothing configured ==\n'
# An unnamed emoji role used to seed no candidates at all, so an emoji-presentation
# atom reached tofu with a usable color face installed.  Its seed now comes from
# the primary face with a stated color preference.
note emoji-discovery "a color face is discovered with no emoji resource set"
start_case emoji-discovery routing-modern '🛠️' unicode
check_route emoji-discovery U+1F6E0 emoji-fallback "$color_face"
check_contained emoji-discovery 1 color
check_cursor emoji-discovery 2
if ! grep -F -q 'prepared unnamed Xft emoji discovery' "$log"
then
    fail_case emoji-discovery "expected the unnamed emoji role to be seeded"
fi
stop_case

note emoji-discovery-off "systemFallback false suppresses the emoji seed too"
start_case emoji-discovery-off routing-modern '🛠️' unicode \
    -xrm 'xterm.vt100.systemFallback: false'
check_route emoji-discovery-off U+1F6E0 tofu
check_cursor emoji-discovery-off 2
if grep -F -q 'prepared unnamed Xft emoji discovery' "$log"
then
    fail_case emoji-discovery-off "the seed was prepared although discovery is disabled"
fi
stop_case

note emoji-discovery-budget "limitFontsets 0 suppresses the emoji seed too"
start_case emoji-discovery-budget routing-modern '🛠️' unicode \
    -xrm 'xterm.vt100.limitFontsets: 0'
check_route emoji-discovery-budget U+1F6E0 tofu
check_cursor emoji-discovery-budget 2
stop_case

# Declining color must not be answered by leaking color, and must not quietly
# substitute a monochrome face for an atom that asked for emoji presentation.
note emoji-color-declined "colorGlyphs false refuses the discovered color face"
start_case emoji-color-declined routing-modern '🛠️' unicode \
    -xrm 'xterm.vt100.colorGlyphs: false'
check_route emoji-color-declined U+1F6E0 tofu
check_contained emoji-color-declined 1 mono
check_cursor emoji-color-declined 2
stop_case

# No color face exists here.  Emoji presentation is not satisfied by monochrome
# artwork, so this is deterministic tofu rather than a silent presentation swap.
# Presentation orders the candidates and governs the paint; it does not make a
# monochrome face ineligible.  With no color face installed, a one-cell atom is
# refused by the advance rule and ends in tofu, while a two-cell atom clears that
# rule and is served in monochrome.  Both are asserted rather than one being
# stated as a general rule.
note emoji-absent-narrow "no color face and one cell: the advance rule refuses"
start_case emoji-absent-narrow mono-modern '🛠️' unicode
check_route emoji-absent-narrow U+1F6E0 tofu
check_cursor emoji-absent-narrow 2
stop_case

note emoji-absent-wide "no color face and two cells: monochrome serves"
start_case emoji-absent-wide mono-modern 📦 unicode
check_route emoji-absent-wide U+1F4E6 emoji-fallback "$modern_mono"
check_contained emoji-absent-wide 2 mono
check_cursor emoji-absent-wide 3
stop_case

note emoji-absent-vs16-wide "the same under the mode-2027 width regime"
start_case emoji-absent-vs16-wide mono-modern '🛠️' unicode \
    -xrm 'xterm.vt100.graphemeWidth: unicode'
check_route emoji-absent-vs16-wide U+1F6E0 emoji-fallback "$modern_mono"
check_contained emoji-absent-vs16-wide 2 mono
check_cursor emoji-absent-vs16-wide 3
stop_case

# Automatic color discovery must not outrank an explicit choice that can serve
# the atom.  Each of these has a usable face configured and color discovery
# available at the same time.
note emoji-primary-wins "a primary face covering the atom outranks discovery"
start_case emoji-primary-wins routing-modern '❤️' unicode
check_route emoji-primary-wins U+2764 primary DejaVuSansMono.ttf
check_contained emoji-primary-wins 1 mono
check_cursor emoji-primary-wins 2
stop_case

# A two-cell atom, so the explicit monochrome face clears the advance rule and is
# genuinely usable; a one-cell atom would be refused by that rule before
# precedence could be observed at all.
note emoji-entry2-wins "primary entry 2 outranks discovery"
start_case emoji-entry2-wins routing-modern 📦 unicode \
    -fa 'DejaVu Sans Mono:rgba=none,Noto Emoji:color=false'
check_route emoji-entry2-wins U+1F4E6 fallback "$modern_mono"
check_contained emoji-entry2-wins 2 mono
check_cursor emoji-entry2-wins 3
stop_case

note emoji-named-wins "fallbackFace1 outranks discovery"
start_case emoji-named-wins routing-modern 📦 unicode \
    -xrm 'xterm.vt100.fallbackFace1: Noto Emoji:color=false'
check_route emoji-named-wins U+1F4E6 fallback "$modern_mono"
if ! grep -F -q 'source=fallbackFace1' "$log"
then
    fail_case emoji-named-wins "expected fallbackFace1 to be the activated source"
fi
check_contained emoji-named-wins 2 mono
check_cursor emoji-named-wins 3
stop_case

note emoji-explicit-wins "an explicit emoji face outranks discovery"
start_case emoji-explicit-wins routing-modern '🛠️' unicode -fe 'Noto Color Emoji'
check_route emoji-explicit-wins U+1F6E0 emoji "$color_face"
check_contained emoji-explicit-wins 1 color
check_cursor emoji-explicit-wins 2
if grep -F -q 'prepared unnamed Xft emoji discovery' "$log"
then
    fail_case emoji-explicit-wins "the unnamed seed was prepared although the role is named"
fi
stop_case

printf '\n== boundaries repair must not move ==\n'
note explicit-override "an explicit monochrome choice still outranks discovery"
start_case explicit-override routing-modern 🛠 unicode \
    -xrm 'xterm.vt100.fallbackFace1: Noto Emoji:color=false'
check_route explicit-override U+1F6E0 fallback "$modern_mono"
if ! grep -F -q 'source=fallbackFace1' "$log"
then
    fail_case explicit-override "expected fallbackFace1 to be the activated source"
fi
check_contained explicit-override 1 mono
check_cursor explicit-override 2
stop_case

note system-fallback-off "systemFallback false suppresses discovery"
start_case system-fallback-off routing-modern 🛠 unicode \
    -xrm 'xterm.vt100.systemFallback: false'
check_route system-fallback-off U+1F6E0 tofu
check_cursor system-fallback-off 2
stop_case

note budget-zero "limitFontsets 0 permits nothing beyond entry 1"
start_case budget-zero routing-modern 🛠 unicode \
    -xrm 'xterm.vt100.limitFontsets: 0'
check_route budget-zero U+1F6E0 tofu
check_cursor budget-zero 2
stop_case

# A monochrome emoji candidate must not capture CJK, and the Han and wide routes
# must keep serving it.
note cjk-boundary "Han keeps its own route beside emoji discovery"
start_case cjk-boundary cjk-emoji 日 unicode \
    -fd 'Noto Sans Mono CJK JP' -xrm 'xterm.vt100.faceNameHan: Noto Sans Mono CJK JP'
check_route cjk-boundary U+65E5 han NotoSansMonoCJKjp-Regular.otf
check_contained cjk-boundary 2 mono
check_cursor cjk-boundary 3
stop_case

note cjk-wide-boundary "the wide route keeps serving CJK when no Han face is set"
start_case cjk-wide-boundary cjk-emoji 日 unicode -fd 'Noto Sans Mono CJK JP'
check_route cjk-wide-boundary U+65E5 doublesize NotoSansMonoCJKjp-Regular.otf
check_contained cjk-wide-boundary 2 mono
check_cursor cjk-wide-boundary 3
stop_case

printf '\nfont discovery: %s cases,%s\n' "$checked" "${failures:- all as expected}"
if test -n "$failures"
then
    echo "failed cases:$failures" >&2
    exit 1
fi
