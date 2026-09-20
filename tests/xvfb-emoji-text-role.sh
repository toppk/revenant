#!/bin/sh
#
# faceNameEmojiText and fitEmojiText.
#
# The artwork gate in xvfb-emoji-artwork.sh grades the default path with no
# resources set.  This suite grades the two new resources themselves: both chain
# entries, a hostile generic-emoji pattern rule, the fitting opt-out, precedence
# against the text faces that already existed, the fallback budget, atoms with
# different advances and spans in one face, and styled output.
#
# Every case also checks that ink stays inside the atom's cells, because the
# fitting policy is what makes that non-trivial here.

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB XTERM_PLUS WINDOW-INK FONT-KEYS FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
font_keys=$4
fixture_root=$5
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_require_font_fixtures "$fixture_root"
xtp_start_xvfb "$xvfb"

modern_mono=NotoEmoji-Regular-3.003.ttf
color_face=Noto-COLRv1.ttf
tools=🛠
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

ink_hash()
{
    printf '%s\n' "$1" | sed -n 's/.* hash=\([0-9a-f]*\).*/\1/p'
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
    "$fixture_root/run" "$universe" "$terminal" -debug +sb \
        -fa 'DejaVu Sans Mono:rgba=none' -fs 16 "$@" \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm "xterm.vt100.emojiPresentation: $presentation" \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H\033[?25l%s\033[6n" "$2"; IFS= read -r -d R reply; printf "%sR" "$reply" >"$1"; printf "\033]2;emoji-text-ready\007"; while ! test -d "$3"; do sleep 0.05; done' \
        bash "$cpr" "$probe" "$done_dir" >"$test_dir/$name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" emoji-text-ready "$name" 300
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
    printf '%-22s %s\n' "$1" "$(printf '%s' "$route" | sed 's/.*font: //' | cut -c1-96)"
    if ! printf '%s\n' "$route" | grep -F -q " role=$3 "
    then
        fail_case "$1" "expected role=$3"
        return 1
    fi
    if test "$#" -eq 4 && ! printf '%s\n' "$route" | grep -F -q "/$4 "
    then
        fail_case "$1" "expected effective file $4"
        return 1
    fi
    return 0
}

# check_contained NAME CELLS EXPECTED-CLASS
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

note()
{
    checked=$((checked + 1))
    printf 'CASE  %-22s %s\n' "$1" "$2"
}

printf '== both chain entries ==\n'
note entry1 "faceNameEmojiText entry 1 serves the atom"
start_case entry1 routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
check_route entry1 U+1F6E0 emoji-text "$modern_mono" || true
check_contained entry1 1 mono
check_cursor entry1 2
stop_case

note entry2 "an unresolvable entry 1 falls to entry 2 of the same chain"
start_case entry2 routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: XTP No Such Face:foundry=xtp,Noto Emoji'
check_route entry2 U+1F6E0 emoji-text-fallback "$modern_mono" || true
check_contained entry2 1 mono
check_cursor entry2 2
stop_case

printf '\n== a hostile generic-emoji rule ==\n'
# The universe redirects a "Noto Emoji" request to the color family unless the
# request states color=false, which is the shape of the installed generic-emoji
# rules.  The role must still reach the monochrome file.
note alias-configured "a monochrome request survives family redirection"
start_case alias-configured alias-emoji "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
check_route alias-configured U+1F6E0 emoji-text "$modern_mono" || true
check_contained alias-configured 1 mono
check_cursor alias-configured 2
stop_case

# An explicit user constraint is not overridden, even when it is the colour the
# role would not have chosen.  The resolved file proves which face was taken.
note alias-user-color "an explicit color=true in the user's pattern is honored"
start_case alias-user-color alias-emoji "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji:color=true'
resolved=$(grep -F 'resolved Xft role=emoji-text' "$log" | tail -1)
printf '%-22s %s\n' alias-user-color "$(printf '%s' "$resolved" | sed 's/.*font: //' | cut -c1-96)"
if ! printf '%s\n' "$resolved" | grep -F -q "/$color_face "
then
    fail_case alias-user-color "expected the user's color=true to select $color_face"
fi
check_cursor alias-user-color 2
stop_case

printf '\n== the fitting opt-out ==\n'
note fitting-off "fitEmojiText false refuses an oversized discovered face"
start_case fitting-off routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.fitEmojiText: false'
check_route fitting-off U+1F6E0 tofu || true
check_contained fitting-off 1 mono
check_cursor fitting-off 2
if ! grep -E -q -- 'deferred Xft fallback .* width=1 ' "$log"
then
    fail_case fitting-off "expected the advance deferral to be logged"
fi
stop_case

note fitting-off-role "fitEmojiText false also refuses an oversized role face"
start_case fitting-off-role routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji' \
    -xrm 'xterm.vt100.fitEmojiText: false'
check_route fitting-off-role U+1F6E0 tofu || true
check_contained fitting-off-role 1 mono
if ! grep -F -q 'deferred Xft role=emoji-text' "$log"
then
    fail_case fitting-off-role "expected the role deferral to be logged"
fi
stop_case

printf '\n== precedence and budget ==\n'
note precedence-entry2 "entry 2 of faceName keeps precedence over the role"
start_case precedence-entry2 routing-modern "$tools" unicode \
    -fa 'DejaVu Sans Mono:rgba=none,Noto Emoji:color=false' \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
check_route precedence-entry2 U+1F6E0 fallback "$modern_mono" || true
if grep -E -q -- 'font: route .* role=emoji-text' "$log"
then
    fail_case precedence-entry2 "the role served the atom although entry 2 covered it"
fi
check_contained precedence-entry2 1 mono
check_cursor precedence-entry2 2
stop_case

note precedence-named "fallbackFace1 keeps precedence over the role"
start_case precedence-named routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.fallbackFace1: Noto Emoji:color=false' \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
check_route precedence-named U+1F6E0 fallback "$modern_mono" || true
if ! grep -F -q 'source=fallbackFace1' "$log"
then
    fail_case precedence-named "expected fallbackFace1 to be the activated source"
fi
if grep -E -q -- 'font: route .* role=emoji-text' "$log"
then
    fail_case precedence-named "the role served the atom although fallbackFace1 covered it"
fi
check_contained precedence-named 1 mono
check_cursor precedence-named 2
stop_case

# The role is a rescue after the capturing slot's own choices, not a capturing
# role, so a zero fallback budget suppresses both of its entries.
note budget-zero "limitFontsets 0 suppresses both rescue entries"
start_case budget-zero routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji' \
    -xrm 'xterm.vt100.limitFontsets: 0'
check_route budget-zero U+1F6E0 tofu || true
check_cursor budget-zero 2
if grep -E -q -- 'font: (route .* role=emoji-text|resolved Xft role=emoji-text)' "$log"
then
    fail_case budget-zero "the role was loaded or consulted at a zero budget"
fi
stop_case

note budget-zero-entry2 "limitFontsets 0 suppresses the role's entry 2 as well"
start_case budget-zero-entry2 routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: XTP No Such Face:foundry=xtp,Noto Emoji' \
    -xrm 'xterm.vt100.limitFontsets: 0'
check_route budget-zero-entry2 U+1F6E0 tofu || true
check_cursor budget-zero-entry2 2
stop_case

printf '\n== which atoms are fitted ==\n'
# Fitting exists because the advance rule is a one-cell rule: a width-two atom
# bypasses that rule entirely and is served unfitted.  Asserting both halves
# keeps the scope of the policy visible.
note governor-scope "a one-cell atom is fitted and a two-cell atom is not"
start_case governor-scope routing-modern "$(printf '\360\237\233\240\360\237\223\246')" text \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
check_route governor-scope U+1F6E0 emoji-text "$modern_mono" || true
check_route governor-scope U+1F4E6 emoji-text "$modern_mono" || true
one_cell_span=$cell_width
two_cell_span=$((2 * cell_width))
if ! grep -F -q "fitted Xft face span=$one_cell_span " "$log"
then
    fail_case governor-scope "the one-cell atom was not fitted"
fi
if grep -F -q "fitted Xft face span=$two_cell_span " "$log"
then
    fail_case governor-scope "the two-cell atom was fitted although the rule does not apply to it"
fi
first=$(sample_cells 0 1)
second=$(sample_cells 1 2)
printf '%-22s one-cell %s\n' governor-scope "$first"
printf '%-22s two-cell %s\n' governor-scope "$second"
if test "$(ink_class "$first")" != mono || test "$(ink_class "$second")" != mono
then
    fail_case governor-scope "expected monochrome ink for both atoms"
fi
if test "$(ink_class "$(sample_cells 3 1)")" != blank
then
    fail_case governor-scope "ink reached the cell after the two-cell atom"
fi
check_cursor governor-scope 4
stop_case

# Every real glyph in the staged monochrome face has the same 2600-unit advance,
# so two atoms with *different* advances cannot be built from it.  That property
# is not left to a test: the scale is taken from the face's maximum advance, so
# the atom's advance is not an input at all and one instance serves both atoms.
# What is observable, and asserted here, is that reuse.
note instance-reuse "two atoms of one face and span share one fitted instance"
start_case instance-reuse routing-modern "$(printf '\360\237\233\240\342\204\271')" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
opens=$(grep -c -F "fitted Xft face span=$cell_width " "$log" || true)
printf '%-22s fitted-face opens=%s\n' instance-reuse "$opens"
if test "$opens" -ne 1
then
    fail_case instance-reuse "expected exactly one fitted instance, saw $opens"
fi
if ! grep -F -q 'fitted Xft face span=' "$log" || ! grep -F -q ' entry=0 result=opened' "$log"
then
    fail_case instance-reuse "expected the first table entry to hold the fitted face"
fi
hammer=$(ink_hash "$(sample_cells 0 1)")
symbol=$(ink_hash "$(sample_cells 1 1)")
printf '%-22s hammer=%s info=%s\n' instance-reuse "$hammer" "$symbol"
if test -z "$hammer" || test -z "$symbol" || test "$hammer" = "$symbol"
then
    fail_case instance-reuse "expected two distinct glyphs to be drawn"
fi
check_cursor instance-reuse 3
stop_case

printf '\n== a second span for the same face ==\n'
# A font-slot switch changes the cell size inside one universe, so the same
# family is fitted again at a second span.  This is the only way to reach a
# second span here: a width-two atom bypasses the advance rule instead.  The
# first entry must survive, because the route cache still points at it.
note second-span "the same face is fitted again at a new cell size"
start_case second-span routing-modern "$tools" unicode \
    -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
before_span=$cell_width
if ! grep -F -q "fitted Xft face span=$before_span " "$log"
then
    fail_case second-span "the atom was not fitted before the size change"
fi
"$font_keys" "$window" + 1 >/dev/null 2>&1 || true
xtp_wait_for_log "$log" 'select slot=0 ->' second-span-select 200
attempt=0
while test "$(grep -c -F 'fitted Xft face span=' "$log" || true)" -lt 2
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100
    then
        break
    fi
    sleep 0.05
done
cell_width=$(sed -n 's/.*select slot=0 -> [0-9]* old-cell=[0-9]*x[0-9]* new-cell=\([0-9][0-9]*\)x[0-9][0-9]*.*/\1/p' "$log" | tail -1)
cell_height=$(sed -n 's/.*select slot=0 -> [0-9]* old-cell=[0-9]*x[0-9]* new-cell=[0-9][0-9]*x\([0-9][0-9]*\).*/\1/p' "$log" | tail -1)
printf '%-22s span %s then %s\n' second-span "$before_span" "$cell_width"
if test -z "$cell_width" || test "$cell_width" = "$before_span"
then
    fail_case second-span "the cell size did not change, so a second span was not reached"
else
    if ! grep -F -q "fitted Xft face span=$cell_width " "$log"
    then
        fail_case second-span "the face was not fitted again at the new span"
    fi
    if ! grep -F -q "fitted Xft face span=$before_span " "$log"
    then
        fail_case second-span "the first fitted entry was lost"
    fi
    if ! grep -F -q "span=$cell_width max-advance=" "$log" ||
       ! grep -E -q -- "span=$cell_width .* entry=1 result=opened" "$log"
    then
        fail_case second-span "expected the new span to append a second table entry"
    fi
    check_contained second-span 1 mono
fi
stop_case

printf '\n== styled output ==\n'
# A styled candidate may not replace a fitted face with a full-size one.
for style in bold italic
do
    case $style in
    bold) sgr=$(printf '\033[1m') ;;
    *) sgr=$(printf '\033[3m') ;;
    esac
    note "styled-$style" "SGR $style keeps the atom inside its cell"
    start_case "styled-$style" routing-modern "$sgr$tools" unicode
    check_route "styled-$style" U+1F6E0 fallback "$modern_mono" || true
    check_contained "styled-$style" 1 mono
    check_cursor "styled-$style" 2
    stop_case

    note "styled-$style-role" "SGR $style with the role configured"
    start_case "styled-$style-role" routing-modern "$sgr$tools" unicode \
        -xrm 'xterm.vt100.faceNameEmojiText: Noto Emoji'
    check_route "styled-$style-role" U+1F6E0 emoji-text "$modern_mono" || true
    check_contained "styled-$style-role" 1 mono
    check_cursor "styled-$style-role" 2
    stop_case
done

printf '\nemoji-text role: %s cases,%s\n' "$checked" "${failures:- all as expected}"
if test -n "$failures"
then
    echo "failed cases:$failures" >&2
    exit 1
fi
