#!/bin/sh
#
# Text-emoji artwork acceptance gate.
#
# The original failure was ordinary application output: bare U+1F6E0 rendered
# as tofu while the cursor advanced correctly.  A cursor-position suite cannot
# see that, and monochrome sampled pixels cannot either, because deterministic
# tofu is monochrome ink.  Every positive case here therefore requires a
# non-tofu route, the expected effective font file, the expected presentation,
# visible ink that differs from the measured tofu box, and containment proved
# against a control render rather than by bounds inside an already cropped cell.
# Cursor advance is asserted separately from artwork: an advance mismatch always
# fails, even for a case listed in known_gap.
#
# A case in known_gap must fail in exactly the documented way: the width-1
# advance deferral must be logged, the route must be tofu, and the only
# permitted failures are the direct consequences of that miss.  Any other
# failure -- wrong presentation, an unexpected effective font, a damaged
# neighbor, ink outside the assigned cells -- is an independent regression and
# fails the suite even for a listed case.  An unexpected pass fails too, so a
# repaired renderer cannot stay silently exempted.

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
# The right-edge case samples the last column of the default 80-column grid,
# so the whole window must stay inside the viewable screen area.
xtp_xvfb_screen=1600x900x24
xtp_start_xvfb "$xvfb"
# The fixture runner isolates Fontconfig but not the X resource environment, and a
# fresh Xvfb has no RESOURCE_MANAGER, so Xt falls through to the developer's own
# ~/.Xdefaults.  An inherited faceNameEmojiText would configure the very rescue
# this suite exists to test without, so the terminal runs against an empty home
# with inherited resource files neutralized.  Nothing outside test_dir is touched.
mkdir "$test_dir/empty-home"

modern_mono=NotoEmoji-Regular-3.003.ttf
color_face=Noto-COLRv1.ttf
# U+E000 is unassigned in every fixture face, so it is the width-1 tofu source.
private_use=$(printf '\356\200\200')
unexpected=
passed=0
expected_gaps=0

# Failure categories that are direct consequences of the documented miss.  A
# listed gap may report these and nothing else.
gap_categories="tofu-route effective-file tofu-ink"

# No case is listed: every text-emoji atom below now renders.  The width-1
# advance deferral that used to defer Noto Emoji 3.003 is answered by the span
# fitting policy in font-resolution(7), and the eleven entries that named it
# were removed one at a time as their complete assertions passed.  The machinery
# stays for the next gap: a listed case must fail with the documented evidence
# and nothing else.
known_gap()
{
    case $1 in
    *)
        echo ""
        ;;
    esac
}

expected_cpr()
{
    printf '\033[1;%sR' "$1" | od -An -tx1 -v | tr -d ' \n'
}

ink_field()
{
    printf '%s\n' "$2" | sed -n "s/.* $1=\([0-9][0-9]*\).*/\1/p"
}

ink_class()
{
    printf '%s\n' "$1" | sed -n 's/^class=\([^ ]*\).*/\1/p'
}

ink_hash()
{
    printf '%s\n' "$1" | sed -n 's/.* hash=\([0-9a-f]*\).*/\1/p'
}

bounds_field()
{
    # bounds=x,y,width,height; field 1..4, or empty for bounds=none
    printf '%s\n' "$2" | sed -n "s/.* bounds=\([0-9,]*\)$/\1/p" | cut -d, -f"$1"
}

# Launch one sample, wait for it to settle, and export the observed geometry.
start_sample()
{
    sample=$1
    universe=$2
    probe=$3
    font_size=$4
    presentation=$5
    color_glyphs=$6
    system_fallback=$7
    column=$8
    grapheme_width=${9:-default}
    log=$test_dir/$sample.log
    cpr=$test_dir/$sample.cpr
    done_dir=$test_dir/$sample.done

    set --
    if test "$system_fallback" = false
    then
        set -- "$@" -xrm 'xterm.vt100.systemFallback: false'
    fi
    if test "$grapheme_width" != default
    then
        set -- "$@" -xrm "xterm.vt100.graphemeWidth: $grapheme_width"
    fi
    # The shipped resource defaults are left exactly as they are: no
    # faceNameEmoji, faceNameDoublesize, faceNameHan, numbered fallback or
    # limitFontWidth is set anywhere in this suite.  The reported failure was a
    # default-path failure, so a hand-written chain would not reproduce it.
    # shellcheck disable=SC2016
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$fixture_root/run" "$universe" "$terminal" -debug +sb \
        -fa 'DejaVu Sans Mono:rgba=none' -fs "$font_size" "$@" \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm "xterm.vt100.emojiPresentation: $presentation" \
        -xrm "xterm.vt100.colorGlyphs: $color_glyphs" \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H\033[?25l"; if test "$4" != 1; then printf "\033[1;%sH" "$4"; fi; printf "%s\033[6n" "$2"; IFS= read -r -d R reply; printf "%sR" "$reply" >"$1"; printf "\033]2;emoji-artwork-ready\007"; while ! test -d "$3"; do sleep 0.05; done' \
        bash "$cpr" "$probe" "$done_dir" "$column" \
        >"$test_dir/$sample.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" emoji-artwork-ready "$sample" 300

    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    grid_columns=$(sed -n 's/.*VT100 resolved grid=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    if test -z "$window" || test -z "$cell_width" || test -z "$cell_height" || \
       test -z "$grid_columns"
    then
        echo "$sample did not report a window, cell size and grid" >&2
        sed -n '1,200p' "$log" >&2
        exit 1
    fi
}

stop_sample()
{
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# Sample a cell span: FIRST-CELL CELLS ROW, in cells from the text origin.
sample_cells()
{
    "$window_ink" "$window" --expose $((4 + $1 * cell_width)) \
        $((4 + $3 * cell_height)) $(($2 * cell_width)) "$cell_height" 0x000000
}

# Sample the internal border strip directly above a cell span.
sample_border_above()
{
    "$window_ink" "$window" --expose $((4 + $1 * cell_width)) 0 \
        $(($2 * cell_width)) 4 0x000000
}

# Deterministic tofu for this cell geometry, so a positive case can require
# ink that is not the missing-glyph box.  Recorded per font size and width.
measure_tofu()
{
    font_size=$1
    start_sample "tofu-$font_size" base "$private_use" "$font_size" unicode true false 1
    tofu_one=$(sample_cells 0 1 0)
    stop_sample
    start_sample "tofu-wide-$font_size" base 日 "$font_size" unicode true false 1
    tofu_two=$(sample_cells 0 2 0)
    stop_sample
    tofu_one_hash=$(ink_hash "$tofu_one")
    tofu_two_hash=$(ink_hash "$tofu_two")
    if test -z "$tofu_one_hash" || test -z "$tofu_two_hash" || \
       test "$(ink_class "$tofu_one")" != mono || test "$(ink_class "$tofu_two")" != mono
    then
        echo "tofu reference at size $font_size did not render monochrome boxes" >&2
        printf 'width1: %s\nwidth2: %s\n' "$tofu_one" "$tofu_two" >&2
        exit 1
    fi
    printf '%-20s size=%s width1=%s width2=%s\n' tofu-reference "$font_size" \
        "$tofu_one_hash" "$tofu_two_hash"
}

fail_artwork()
{
    artwork_failures="$artwork_failures
$1|$2"
}

print_failures()
{
    old_ifs=$IFS
    IFS='
'
    for failure in $artwork_failures
    do
        printf '  %s: %s\n' "${failure%%|*}" "${failure#*|}"
    done
    IFS=$old_ifs
}

# Categories reported that are not direct consequences of the documented miss.
unrelated_failures()
{
    categories=
    old_ifs=$IFS
    IFS='
'
    for failure in $artwork_failures
    do
        category=${failure%%|*}
        case " $gap_categories " in
        *" $category "*) ;;
        *) categories="$categories $category" ;;
        esac
    done
    IFS=$old_ifs
    printf '%s' "$categories"
}

record()
{
    sample=$1
    detail=$2
    gap=$(known_gap "$sample")
    if test -z "$gap"
    then
        if test -n "$artwork_failures"
        then
            unexpected="$unexpected $sample"
            printf 'FAIL  %-20s %s\n' "$sample" "$detail"
            print_failures
        else
            passed=$((passed + 1))
            printf 'PASS  %-20s %s\n' "$sample" "$detail"
        fi
        return
    fi

    # A repaired renderer is diagnosed first: the expected-failure evidence
    # below is only meaningful for a case that actually failed.
    if test -z "$artwork_failures"
    then
        unexpected="$unexpected $sample"
        printf 'XPASS %-20s remove the stale known_gap entry\n' "$sample"
        return
    fi

    # A listed gap must fail in the documented way and in no other way.
    unrelated=$(unrelated_failures)
    missing=
    if ! grep -E -q -- 'deferred Xft fallback .* width=1 ' "$test_dir/$sample.log"
    then
        missing="$missing no-width-1-advance-deferral-logged"
    fi
    case "$artwork_failures" in
    *"tofu-route|"*) ;;
    *) missing="$missing route-is-not-tofu" ;;
    esac
    if test -n "$unrelated" || test -n "$missing"
    then
        unexpected="$unexpected $sample"
        printf 'FAIL  %-20s listed gap failed in an undocumented way\n' "$sample"
        test -z "$unrelated" || printf '  independent regressions:%s\n' "$unrelated"
        test -z "$missing" || printf '  expected evidence absent:%s\n' "$missing"
        print_failures
    else
        expected_gaps=$((expected_gaps + 1))
        printf 'XFAIL %-20s %s\n' "$sample" "$gap"
        print_failures
    fi
}

# Advance is the separately owned contract and is never excused by known_gap.
check_advance()
{
    actual=$(od -An -tx1 -v "$test_dir/$1.cpr" | tr -d ' \n')
    want=$(expected_cpr "$2")
    printf '%-20s advance column=%s\n' "$1.cpr" "$2"
    if test "$actual" != "$want"
    then
        echo "$1 reported cursor $actual, expected column $2 ($want)" >&2
        exit 1
    fi
}

# Route identity: the role, the presentation, and the effective file, because a
# family name is a preference and not an identity.
check_route()
{
    base=$1
    expected_presentation=$2
    expected_file=$3

    route=$(grep -F "route base=$base " "$log" | tail -1)
    if test -z "$route"
    then
        fail_artwork no-route "nothing logged for $base"
        return
    fi
    printf '%-20s %s\n' "$sample.route" "$(printf '%s' "$route" | sed 's/.*font: //')"
    if printf '%s\n' "$route" | grep -F -q ' role=tofu'
    then
        fail_artwork tofu-route "$(printf '%s' "$route" | sed 's/.*font: //')"
    fi
    if ! printf '%s\n' "$route" | grep -F -q " presentation=$expected_presentation "
    then
        fail_artwork presentation "expected presentation=$expected_presentation"
    fi
    if ! printf '%s\n' "$route" | grep -F -q "/$expected_file "
    then
        fail_artwork effective-file "expected effective file $expected_file"
    fi
}

# Every route logged for one base, for a row of atoms that share a base -- the tag
# sequences below all start at U+1F3F4, so the last line alone would grade one of
# them.  Duplicate lines from repeated renders are harmless here: the assertion is
# that no logged route for this base was split, tofu, or served from another file.
check_every_route()
{
    base=$1
    expected_presentation=$2
    expected_file=$3

    routes=$(grep -F "route base=$base " "$log" || true)
    if test -z "$routes"
    then
        fail_artwork no-route "nothing logged for $base"
        return
    fi
    printf '%-20s %s\n' "$sample.routes" \
        "$(printf '%s\n' "$routes" | sed 's/.*font: //' | sort -u | wc -l) distinct"
    for pattern in " role=tofu" " glyphs=1 "
    do
        case $pattern in
        " role=tofu")
            if printf '%s\n' "$routes" | grep -F -q -- "$pattern"
            then
                fail_artwork tofu-route "a $base route is tofu"
            fi
            ;;
        *)
            if printf '%s\n' "$routes" | grep -F -v -q -- "$pattern"
            then
                fail_artwork shaping "a $base route did not shape one glyph"
                printf '%s\n' "$routes" | grep -F -v -- "$pattern" | sed 's/.*font: //'
            fi
            ;;
        esac
    done
    if printf '%s\n' "$routes" | grep -F -v -q -- " presentation=$expected_presentation "
    then
        fail_artwork presentation "expected presentation=$expected_presentation"
    fi
    if printf '%s\n' "$routes" | grep -F -v -q -- "/$expected_file "
    then
        fail_artwork effective-file "expected effective file $expected_file"
    fi
}

# One drawn atom: visible ink that is not the tofu box, of the expected paint
# class, with margins inside its own cells.  Containment across the cell
# boundary is established separately, by check_surroundings.
check_cell_artwork()
{
    label=$1
    first_cell=$2
    cells=$3
    row=$4
    expected_class=$5

    result=$(sample_cells "$first_cell" "$cells" "$row")
    class=$(ink_class "$result")
    ink=$(ink_field ink "$result")
    hash=$(ink_hash "$result")
    bounds_x=$(bounds_field 1 "$result")
    bounds_y=$(bounds_field 2 "$result")
    bounds_width=$(bounds_field 3 "$result")
    bounds_height=$(bounds_field 4 "$result")
    span=$((cells * cell_width))
    printf '%-20s %s\n' "$label" "$result"

    if test "$class" != "$expected_class"
    then
        fail_artwork class "$label expected pixel class $expected_class, got $class"
    fi
    if test -z "$ink" || test "$ink" -eq 0
    then
        fail_artwork no-ink "$label has no ink in its assigned cells"
        return
    fi
    case $cells in
    1) tofu_hash=$tofu_one_hash ;;
    *) tofu_hash=$tofu_two_hash ;;
    esac
    if test "$hash" = "$tofu_hash"
    then
        fail_artwork tofu-ink "$label ink is pixel-identical to the tofu box"
    fi
    # A recognizable glyph covers a useful part of its cells.  The floor is
    # deliberately low: it rejects a hairline or a stray dot, and is not a
    # legibility judgment, which stays with the human probe assessment.
    if test "$ink" -lt $((span * cell_height / 40))
    then
        fail_artwork ink-floor "$label ink=$ink is below $((span * cell_height / 40)) for ${span}x$cell_height"
    fi
    if test "$bounds_y" -eq 0 || test $((bounds_y + bounds_height)) -ge "$cell_height"
    then
        fail_artwork margins "$label ink touches a cell edge: bounds=$bounds_x,$bounds_y,$bounds_width,$bounds_height"
    fi
}

# Containment and neighbor preservation.  Everything outside the atom's own
# cells is compared with the control render of the same line, so overwritten or
# tofu-replaced neighbor text cannot pass as "nonblank".  Blank regions are
# required to be genuinely blank, which is an exact statement about ink.
control_hash=
capture_control()
{
    control_hash="$control_hash $1=$(ink_hash "$(sample_cells "$2" "$3" "$4")")"
}

control_of()
{
    printf '%s' "$control_hash" | tr ' ' '\n' | sed -n "s/^$1=//p"
}

check_matches_control()
{
    label=$1
    expected=$(control_of "$1")
    result=$(sample_cells "$2" "$3" "$4")
    printf '%-20s %s %s\n' "$sample" "$label" "$result"
    if test -z "$expected"
    then
        echo "$sample has no control hash for $label" >&2
        exit 1
    fi
    if test "$(ink_hash "$result")" != "$expected"
    then
        fail_artwork neighbor "$label differs from the control render: $result"
    fi
}

check_blank()
{
    label=$1
    result=$(sample_cells "$2" "$3" "$4")
    printf '%-20s %s %s\n' "$sample" "$label" "$result"
    if test "$(ink_class "$result")" != blank
    then
        fail_artwork neighbor "$label should be blank: $result"
    fi
}

# Vertical containment, measured outside the atom's cells: the border strip
# above and the whole row below must carry no ink.
check_vertical_containment()
{
    first_cell=$1
    cells=$2
    above=$(sample_border_above "$first_cell" "$cells")
    below=$(sample_cells "$first_cell" "$cells" 1)
    printf '%-20s border-above %s\n' "$sample" "$above"
    printf '%-20s row-below %s\n' "$sample" "$below"
    if test "$(ink_class "$above")" != blank
    then
        fail_artwork border "ink reached the internal border above: $above"
    fi
    if test "$(ink_class "$below")" != blank
    then
        fail_artwork below "ink reached the row below: $below"
    fi
}

# A negative case: the glyph is genuinely unavailable, so tofu is correct and
# the pixels must match the measured tofu box exactly.
negative_case()
{
    sample=$1
    universe=$2
    probe=$3
    base=$4
    cells=$5
    advance=$6
    system_fallback=$7
    artwork_failures=

    start_sample "$sample" "$universe" "$probe" 16 unicode true "$system_fallback" 1
    check_advance "$sample" "$advance"
    route=$(grep -F "route base=$base " "$log" | tail -1)
    result=$(sample_cells 0 "$cells" 0)
    printf '%-20s %s\n' "$sample" "$result"
    case $cells in
    1) tofu_hash=$tofu_one_hash ;;
    *) tofu_hash=$tofu_two_hash ;;
    esac
    if ! printf '%s\n' "$route" | grep -F -q ' role=tofu'
    then
        fail_artwork tofu-route "expected a deliberate tofu route: $route"
    fi
    if test "$(ink_hash "$result")" != "$tofu_hash"
    then
        fail_artwork tofu-ink "expected the deterministic tofu box, got $result"
    fi
    stop_sample
    record "$sample" "deliberate tofu preserved"
}

printf '== tofu references ==\n'
measure_tofu 16
tofu16_one=$tofu_one_hash
tofu16_two=$tofu_two_hash

printf '\n== the reported failure ==\n'
# The original application line, byte for byte, with no variation selector and
# no configured fallback chain.  Cell 0 must show a hammer and wrench, and the
# nineteen cells of surrounding text must be pixel-identical to the same line
# with a space in place of the symbol.
artwork_failures=
control_hash=
start_sample install-line-control routing-modern '  Installed demo-1.0' 16 unicode true true 1
capture_control trailing-text 1 19 0
stop_sample
start_sample install-line routing-modern '🛠 Installed demo-1.0' 16 unicode true true 1
check_advance install-line 21
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork install-line 0 1 0 mono
check_matches_control trailing-text 1 19 0
check_blank after-text 20 1 0
check_vertical_containment 0 1
stop_sample
record install-line "bare U+1F6E0 in application output"

printf '\n== presentation ==\n'
artwork_failures=
start_sample tools-bare routing-modern 🛠 16 unicode true true 1
check_advance tools-bare 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-bare 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-bare "bare base takes text presentation"

artwork_failures=
start_sample tools-vs15 routing-modern '🛠︎' 16 unicode true true 1
check_advance tools-vs15 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-vs15 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-vs15 "VS15 forces text presentation"

artwork_failures=
start_sample tools-vs16 routing-modern '🛠️' 16 unicode true true 1
check_advance tools-vs16 2
check_route U+1F6E0 emoji "$color_face"
check_cell_artwork tools-vs16 0 1 0 color
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-vs16 "VS16 selects color artwork in the legacy width regime"

artwork_failures=
start_sample tools-vs16-unicode routing-modern '🛠️' 16 unicode true true 1 unicode
check_advance tools-vs16-unicode 3
check_route U+1F6E0 emoji "$color_face"
check_cell_artwork tools-vs16-unicode 0 2 0 color
check_blank following-cell 2 1 0
check_vertical_containment 0 2
stop_sample
record tools-vs16-unicode "VS16 selects color artwork in the mode-2027 regime"

artwork_failures=
start_sample package-default routing-modern 📦 16 unicode true true 1
check_advance package-default 3
check_route U+1F4E6 emoji "$color_face"
check_cell_artwork package-default 0 2 0 color
check_blank following-cell 2 1 0
check_vertical_containment 0 2
stop_sample
record package-default "emoji-default base needs no selector"

artwork_failures=
start_sample package-text routing-modern 📦 16 text true true 1
check_advance package-text 3
check_route U+1F4E6 text "$modern_mono"
check_cell_artwork package-text 0 2 0 mono
check_blank following-cell 2 1 0
check_vertical_containment 0 2
stop_sample
record package-text "forced text presentation of an emoji-default base"

printf '\n== color disabled and monochrome-only supply ==\n'
artwork_failures=
start_sample tools-no-color routing-modern 🛠 16 unicode false true 1
check_advance tools-no-color 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-no-color 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-no-color "colorGlyphs false still needs outline ink"

artwork_failures=
start_sample tools-mono-only mono-modern 🛠 16 unicode true true 1
check_advance tools-mono-only 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-mono-only 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-mono-only "no color face is installed at all"

printf '\n== fitting, neighbors and geometry ==\n'
# Both symbols are graded in full: the second one is an atom, not a "nonblank
# neighbor", and an adjacent pair is exactly where fitting can steal a cell.
artwork_failures=
start_sample tools-adjacent routing-modern 🛠🛠 16 unicode true true 1
check_advance tools-adjacent 3
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-adjacent.first 0 1 0 mono
check_cell_artwork tools-adjacent.second 1 1 0 mono
check_blank following-cell 2 1 0
check_vertical_containment 0 2
stop_sample
record tools-adjacent "adjacent symbols keep one cell each"

artwork_failures=
start_sample tools-spaced routing-modern '🛠 🛠' 16 unicode true true 1
check_advance tools-spaced 4
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-spaced.first 0 1 0 mono
check_cell_artwork tools-spaced.second 2 1 0 mono
check_blank gap-cell 1 1 0
check_blank following-cell 3 1 0
check_vertical_containment 0 3
stop_sample
record tools-spaced "a spaced symbol may not claim the gap silently"

# Bracket neighbors are compared with the control render, so replacing them
# with tofu, or painting over them, cannot pass.
artwork_failures=
control_hash=
start_sample tools-neighbors-control routing-modern '[ ]' 16 unicode true true 1
capture_control left-bracket 0 1 0
capture_control right-bracket 2 1 0
stop_sample
start_sample tools-neighbors routing-modern '[🛠]' 16 unicode true true 1
check_advance tools-neighbors 4
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-neighbors 1 1 0 mono
check_matches_control left-bracket 0 1 0
check_matches_control right-bracket 2 1 0
check_blank following-cell 3 1 0
check_vertical_containment 1 1
stop_sample
record tools-neighbors "occupied neighbors keep their own ink"

printf '\n== font sizes ==\n'
measure_tofu 12
artwork_failures=
start_sample tools-size-12 routing-modern 🛠 12 unicode true true 1
check_advance tools-size-12 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-size-12 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-size-12 "12 point"

measure_tofu 32
artwork_failures=
start_sample tools-size-32 routing-modern 🛠 32 unicode true true 1
check_advance tools-size-32 2
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-size-32 0 1 0 mono
check_blank following-cell 1 1 0
check_vertical_containment 0 1
stop_sample
record tools-size-32 "32 point"

tofu_one_hash=$tofu16_one
tofu_two_hash=$tofu16_two

printf '\n== right edge ==\n'
artwork_failures=
start_sample tools-right-edge routing-modern 🛠 16 unicode true true 999
check_route U+1F6E0 text "$modern_mono"
check_cell_artwork tools-right-edge $((grid_columns - 1)) 1 0 mono
# A width-1 symbol in the last column must not wrap onto the next row.
check_vertical_containment $((grid_columns - 1)) 1
check_advance tools-right-edge "$grid_columns"
stop_sample
record tools-right-edge "last column placement"

printf '\n== newer bases and tag sequences ==\n'
# Chosen from tools/emoji-coverage-audit.py, which inventories the pinned
# emoji-data.txt (Unicode 17.0) against the staged faces and the test corpus.  Each
# row is one terminal launch covering several Unicode entries: the audit found 33
# bases newer than E15.0 that a staged face covers and no suite touched, and three
# of the four tag sequences it measures unexercised.  These rows sample that space
# rather than enumerate it.
# POSIX printf does not decode Unicode escapes (Ubuntu /bin/sh is dash).
newer_bases=$(python3 -c 'print("\U0001FA75\U0001FADF\U0001FA8A\U0001FACD", end="")')
# E15.0 and E16.0, which the modern monochrome face covers.
newer_text_bases=$(python3 -c 'print("\U0001FA75\U0001FADF", end="")')
# E17.0, which it does not: see the font-gap case below.
newest_bases=$(python3 -c 'print("\U0001FA8A\U0001FACD", end="")')
tag_eng=$(python3 -c 'print("\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007F", end="")')
tag_wls=$(python3 -c 'print("\U0001F3F4\U000E0067\U000E0062\U000E0077\U000E006C\U000E0073\U000E007F", end="")')
# Syntactically valid and outside the RGI set, so it separates "the font has this
# ligature" from "the renderer split the atom".
tag_usca=$(python3 -c 'print("\U0001F3F4\U000E0075\U000E0073\U000E0063\U000E0061\U000E007F", end="")')

artwork_failures=
start_sample newer-bases-row routing-modern "$newer_bases" 16 unicode true true 1 unicode
check_advance newer-bases-row 9
check_route U+1FA75 emoji "$color_face"
check_route U+1FADF emoji "$color_face"
check_route U+1FA8A emoji "$color_face"
check_route U+1FACD emoji "$color_face"
check_cell_artwork newer-bases-row.e15 0 2 0 color
check_cell_artwork newer-bases-row.e16 2 2 0 color
check_cell_artwork newer-bases-row.e17a 4 2 0 color
check_cell_artwork newer-bases-row.e17b 6 2 0 color
check_blank following-cell 8 1 0
check_vertical_containment 0 8
stop_sample
record newer-bases-row "E15.0, E16.0 and E17.0 bases in mode 2027"

artwork_failures=
start_sample newer-bases-text routing-modern "$newer_text_bases" 16 text true true 1 unicode
check_advance newer-bases-text 5
check_route U+1FA75 text "$modern_mono"
check_route U+1FADF text "$modern_mono"
check_cell_artwork newer-bases-text.e15 0 2 0 mono
check_cell_artwork newer-bases-text.e16 2 2 0 mono
check_blank following-cell 4 1 0
check_vertical_containment 0 4
stop_sample
record newer-bases-text "forced text presentation of E15.0 and E16.0 bases"

# A font gap, recorded as one: NotoEmoji 3.003 predates E17.0, so forced text
# presentation has nothing to serve these two bases with and tofu is the correct
# result.  The advance is asserted anyway, because that contract is the renderer's
# and does not depend on any font.  Replacing the monochrome fixture with a face
# that covers E17.0 should turn this case into a positive one.
artwork_failures=
start_sample newest-bases-text-gap routing-modern "$newest_bases" 16 text true true 1 unicode
check_advance newest-bases-text-gap 5
for base in U+1FA8A U+1FACD
do
    route=$(grep -F "route base=$base " "$log" | tail -1)
    printf '%-20s %s\n' "$sample.route" "$(printf '%s' "$route" | sed 's/.*font: //')"
    if ! printf '%s\n' "$route" | grep -F -q -- ' role=tofu'
    then
        fail_artwork tofu-route "$base is served after all: remove this font-gap case"
    fi
done
first=$(sample_cells 0 2 0)
second=$(sample_cells 2 2 0)
printf '%-20s %s\n' "$sample.first" "$first"
printf '%-20s %s\n' "$sample.second" "$second"
if test "$(ink_hash "$first")" != "$tofu_two_hash" || \
   test "$(ink_hash "$second")" != "$tofu_two_hash"
then
    fail_artwork tofu-ink "expected the deterministic width-2 tofu box in both cells"
fi
check_vertical_containment 0 4
stop_sample
record newest-bases-text-gap "E17.0 bases have no monochrome supply in this universe"

artwork_failures=
start_sample tag-flags-row routing-modern "$tag_eng$tag_wls$tag_usca" 16 unicode true true 1 unicode
check_advance tag-flags-row 7
# All three atoms share the base, so every logged route is graded, not the last.
check_every_route U+1F3F4 emoji "$color_face"
check_cell_artwork tag-flags-row.eng 0 2 0 color
check_cell_artwork tag-flags-row.wls 2 2 0 color
check_cell_artwork tag-flags-row.usca 4 2 0 color
check_blank following-cell 6 1 0
check_vertical_containment 0 6
# The tag payload has to reach the font: two different subdivisions may not paint
# the same artwork.  Nothing here judges which flag is which, or its legibility.
eng_hash=$(ink_hash "$(sample_cells 0 2 0)")
wls_hash=$(ink_hash "$(sample_cells 2 2 0)")
printf '%-20s eng=%s wls=%s\n' tag-flags-row "$eng_hash" "$wls_hash"
if test "$eng_hash" = "$wls_hash"
then
    fail_artwork neighbor "the England and Wales sequences painted identical artwork"
fi
stop_sample
record tag-flags-row "RGI England and Wales plus one valid non-RGI tag sequence"

# Legacy segmentation, stated rather than assumed: this backend hands the complete
# tag sequence to the renderer in the legacy regime too, measured here, so the same
# whole-atom expectations apply.  Nothing joined is required of legacy for atom
# kinds where it supplies separate atoms.
artwork_failures=
start_sample tag-flags-legacy routing-modern "$tag_eng$tag_wls$tag_usca" 16 unicode true true 1
check_advance tag-flags-legacy 7
check_every_route U+1F3F4 emoji "$color_face"
check_cell_artwork tag-flags-legacy.eng 0 2 0 color
check_cell_artwork tag-flags-legacy.wls 2 2 0 color
check_cell_artwork tag-flags-legacy.usca 4 2 0 color
check_blank following-cell 6 1 0
check_vertical_containment 0 6
stop_sample
record tag-flags-legacy "the legacy regime supplies the same complete tag atoms"

printf '\n== retained negative cases ==\n'
# The 1.05 monochrome fixture genuinely lacks U+1F6E0, so tofu is the correct
# result there.  This keeps the old incomplete face useful instead of retiring
# it, and proves the positive gate above is not simply accepting any ink.
negative_case legacy-mono-absent mono 🛠 U+1F6E0 1 2 true
negative_case pua-absent routing-modern "$private_use" U+E000 1 2 false

printf '\nemoji artwork gate: %s passed, %s expected gaps,%s\n' \
    "$passed" "$expected_gaps" "${unexpected:- 0 unexpected}"
if test -n "$unexpected"
then
    echo "unexpected results:$unexpected" >&2
    exit 1
fi
