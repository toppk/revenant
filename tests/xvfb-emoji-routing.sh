#!/bin/sh
#
# Route identity and width policy for explicitly configured font chains.
#
# This suite grades which role and font a cluster reaches and that policy never
# changes a committed width.  It is not the artwork gate: a `mono` pixel class
# here is not evidence that a glyph was drawn, because deterministic tofu is
# monochrome ink too.  Positive text-emoji artwork lives in
# xvfb-emoji-artwork.sh and candidate discovery in xvfb-font-discovery.sh.  The
# remaining expected-tofu cases are annotated with the mechanism that produces
# them, audited against the debug log.

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
# Every case here pins a serving role, so a developer's own ~/.Xdefaults naming a
# face resource would silently replace one.  The terminal runs against an empty
# home with inherited resource files neutralized; nothing outside test_dir is
# touched.
mkdir "$test_dir/empty-home"

wait_for_terminal()
{
    xtp_wait_for_title "$1" emoji-routing-ready "$2" 300
}

run_case()
{
    universe=$1
    case_name=$2
    probe=$3
    expected_class=$4
    expected_width=$5
    expected_route=$6
    emoji_face=$7
    wide_face=$8
    presentation=$9
    color_glyphs=${10}
    primary_face=${11:-DejaVu Sans Mono:rgba=none}
    grapheme_width=${12:-default}
    expected_file=${13:-}
    log=$test_dir/$case_name.log
    cpr=$test_dir/$case_name.cpr
    done_dir=$test_dir/$case_name.done

    set --
    if test "$emoji_face" != -
    then
        set -- "$@" -fe "$emoji_face"
    else
        set -- "$@" -xrm 'xterm.vt100.faceNameEmoji:'
    fi
    if test "$wide_face" != -
    then
        set -- "$@" -fd "$wide_face"
    else
        set -- "$@" -xrm 'xterm.vt100.faceNameDoublesize:'
    fi
    if test "$grapheme_width" != default
    then
        set -- "$@" -xrm "xterm.vt100.graphemeWidth: $grapheme_width"
    fi

    # The single-quoted program is expanded by the child bash, not this shell.
    # shellcheck disable=SC2016
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$fixture_root/run" "$universe" "$terminal" -debug +sb -geometry 8x4 \
        -fa "$primary_face" -fs 16 "$@" \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #000000' \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm "xterm.vt100.emojiPresentation: $presentation" \
        -xrm "xterm.vt100.colorGlyphs: $color_glyphs" \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H\033[?25l%s\033[6n" "$2"; IFS= read -r -d R reply; printf "%sR" "$reply" >"$1"; if test "$4" = sequence-combining; then printf "\033[2;1HM"; fi; printf "\033]2;emoji-routing-ready\007"; while ! test -d "$3"; do sleep 0.05; done' bash "$cpr" "$probe" "$done_dir" "$case_name" \
        >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    wait_for_terminal "$log" "$case_name"

    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    result=$("$window_ink" "$window" --expose 4 4 $((expected_width * cell_width)) "$cell_height" 0x000000)
    class=$(printf '%s\n' "$result" | sed -n 's/^class=\([^ ]*\).*/\1/p')
    bounds_y=$(printf '%s\n' "$result" | sed -n 's/.* bounds=[0-9][0-9]*,\([0-9][0-9]*\),[0-9][0-9]*,[0-9][0-9]*$/\1/p')
    bounds_height=$(printf '%s\n' "$result" | sed -n 's/.* bounds=[0-9][0-9]*,[0-9][0-9]*,[0-9][0-9]*,\([0-9][0-9]*\)$/\1/p')
    overflow=$("$window_ink" "$window" --expose $((4 + expected_width * cell_width)) 4 "$cell_width" "$cell_height" 0x000000)
    overflow_class=$(printf '%s\n' "$overflow" | sed -n 's/^class=\([^ ]*\).*/\1/p')
    cpr_hex=$(od -An -tx1 -v "$cpr" | tr -d ' \n')
    case $expected_width in
    1) expected_cpr=1b5b313b3252 ;;
    2) expected_cpr=1b5b313b3352 ;;
    4) expected_cpr=1b5b313b3552 ;;
    6) expected_cpr=1b5b313b3752 ;;
    *) echo "unsupported expected width: $expected_width" >&2; exit 2 ;;
    esac

    printf '%-18s class=%-5s cpr=%s %s\n' "$case_name" "$class" "$cpr_hex" "$result"
    if test "$class" != "$expected_class" || test "$cpr_hex" != "$expected_cpr" || \
       test "$overflow_class" != blank || ! grep -F -q -- "font: route $expected_route" "$log"
    then
        echo "$case_name expected class=$expected_class, width=$expected_width, route=$expected_route, and no following-cell ink" >&2
        tail -160 "$log" >&2
        exit 1
    fi
    if test -n "$expected_file" && \
       ! grep -F -q -- "font: route $expected_route" "$log"
    then
        echo "$case_name lost its route line before the effective-file check" >&2
        exit 1
    fi
    if test -n "$expected_file" && \
       ! grep -F -- "font: route $expected_route" "$log" | grep -F -q -- "/$expected_file "
    then
        echo "$case_name expected the atom to be served from $expected_file" >&2
        grep -F -- "font: route $expected_route" "$log" | tail -2 >&2
        exit 1
    fi
    case $case_name in
    seq-*)
        # Containment for the sequence cases is not just the following cell: the
        # band below the atom's whole span must stay blank too.
        below=$("$window_ink" "$window" --expose 4 $((4 + cell_height)) \
            $((expected_width * cell_width)) "$cell_height" 0x000000)
        below_class=$(printf '%s\n' "$below" | sed -n 's/^class=\([^ ]*\).*/\1/p')
        printf '%-18s below-span=%s\n' "$case_name" "$below_class"
        if test "$below_class" != blank
        then
            echo "$case_name painted ink below its span: $below" >&2
            exit 1
        fi
        ;;
    esac
    if test "$case_name" = heart-vs16-unicode
    then
        damage_width=$((cell_width / 2))
        damage_result=$("$window_ink" "$window" --damage-guard 4 4 "$cell_width" "$cell_height" \
            "$damage_width" 0xFF00FF)
        printf '%-18s %s\n' damage-clip "$damage_result"
    fi
    if test "$case_name" = emoji-default && \
       { test -z "$bounds_y" || test -z "$bounds_height" || \
         test "$bounds_y" -eq 0 || test $((bounds_y + bounds_height)) -ge "$cell_height"; }
    then
        echo "emoji-default expected vertically fitted ink with top and bottom margins: $result" >&2
        exit 1
    fi
    if test "$case_name" = sequence-combining
    then
        ascii_result=$("$window_ink" "$window" --expose 4 $((4 + cell_height)) \
            "$cell_width" "$cell_height" 0x000000)
        ascii_class=$(printf '%s\n' "$ascii_result" | sed -n 's/^class=\([^ ]*\).*/\1/p')
        ascii_y=$(printf '%s\n' "$ascii_result" | \
            sed -n 's/.* bounds=[0-9][0-9]*,\([0-9][0-9]*\),[0-9][0-9]*,[0-9][0-9]*$/\1/p')
        ascii_height=$(printf '%s\n' "$ascii_result" | \
            sed -n 's/.* bounds=[0-9][0-9]*,[0-9][0-9]*,[0-9][0-9]*,\([0-9][0-9]*\)$/\1/p')
        printf '%-18s %s\n' xft-face-isolation "$ascii_result"
        if test "$ascii_class" != mono || test -z "$ascii_y" || \
           test -z "$ascii_height" || test "$ascii_y" -eq 0 || \
           test $((ascii_y + ascii_height)) -ge "$cell_height"
        then
            echo "xft-face-isolation expected ordinary text to retain vertical cell margins after shaping" >&2
            exit 1
        fi
    fi
    if test "$case_name" = sequence-family-default
    then
        component=0
        while test "$component" -lt 3
        do
            component_result=$("$window_ink" "$window" --expose \
                $((4 + component * 2 * cell_width)) 4 $((2 * cell_width)) \
                "$cell_height" 0x000000)
            component_class=$(printf '%s\n' "$component_result" | \
                sed -n 's/^class=\([^ ]*\).*/\1/p')
            if test "$component_class" = blank
            then
                echo "sequence-family-default component $component has no ink: $component_result" >&2
                exit 1
            fi
            component=$((component + 1))
        done
    fi
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

run_unicode_case()
{
    if test "$#" -eq 10
    then
        run_case "$@" 'DejaVu Sans Mono:rgba=none' unicode
    elif test "$#" -eq 11
    then
        run_case "$@" unicode
    else
        echo "run_unicode_case expected 10 or 11 arguments, got $#" >&2
        exit 2
    fi
}

run_cursor_clip_case()
{
    log=$test_dir/cursor-clip.log
    done_dir=$test_dir/cursor-clip.done

    # A width-1 color glyph is deliberately the sharp case: without the cursor
    # clip, the cursor repaint sizes the square emoji to the cell height and
    # overwrites the following cell.
    # shellcheck disable=SC2016
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$fixture_root/run" routing "$terminal" -debug +sb -geometry 8x4 \
        -fa 'DejaVu Sans Mono:rgba=none' -fs 16 \
        -fe 'Noto Color Emoji' -fd 'Noto Sans Mono CJK JP' \
        -xrm 'xterm.vt100.internalBorder: 4' \
        -xrm 'xterm.vt100.background: #000000' \
        -xrm 'xterm.vt100.foreground: #FFFFFF' \
        -xrm 'xterm.vt100.cursorColor: #FFFFFF' \
        -xrm 'xterm.vt100.alwaysHighlight: true' \
        -xrm 'xterm.vt100.renderFont: true' \
        -xrm 'xterm.vt100.emojiPresentation: emoji' \
        -xrm 'xterm.vt100.colorGlyphs: true' \
        -e bash -c 'stty raw -echo; printf "\033[2J\033[H%s\033[D" "$1"; printf "\033]2;emoji-routing-ready\007"; while ! test -d "$2"; do sleep 0.05; done' bash '❤' "$done_dir" \
        >"$test_dir/cursor-clip.out" 2>"$log" &
    terminal_pid=$!
    wait_for_terminal "$log" cursor-clip

    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
    cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)
    overflow=$("$window_ink" "$window" --expose $((4 + cell_width)) 4 "$cell_width" "$cell_height" 0x000000)
    overflow_class=$(printf '%s\n' "$overflow" | sed -n 's/^class=\([^ ]*\).*/\1/p')
    printf '%-18s following-cell=%s %s\n' cursor-clip "$overflow_class" "$overflow"
    if test "$overflow_class" != blank || \
       ! grep -F -q -- 'font: route base=U+2764 width=1 presentation=emoji role=emoji' "$log"
    then
        echo 'cursor-clip expected a routed width-1 color glyph and no following-cell ink' >&2
        sed -n '1,320p' "$log" >&2
        exit 1
    fi
    mkdir "$done_dir"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

# Unicode defaults and explicit selectors route independently of width.
bold_grin=$(printf '\033[1m😀')
bold_cjk=$(printf '\033[1m日')
combining_acute=$(printf 'e\314\201')
run_case routing emoji-default 😀 color 2 \
    'base=U+1F600 width=2 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing emoji-bold "$bold_grin" color 2 \
    'base=U+1F600 width=2 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing cjk-wide 日 mono 2 \
    'base=U+65E5 width=2 presentation=none role=doublesize' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing cjk-bold "$bold_cjk" mono 2 \
    'base=U+65E5 width=2 presentation=none role=doublesize' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing heart-text ❤ mono 1 \
    'base=U+2764 width=1 presentation=text role=primary' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_unicode_case routing heart-vs16-unicode ❤️ color 2 \
    'base=U+2764 width=2 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing heart-vs16-default ❤️ color 1 \
    'base=U+2764 width=1 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' default
run_case routing heart-vs15 '❤︎' mono 1 \
    'base=U+2764 width=1 presentation=text role=primary' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
# This case used to expect tofu: the coverage-trimmed candidate sort dropped this
# universe's monochrome face as redundant once the color face covered U+2139, so
# the alternative was never activated.  The presentation-aware discovery pass now
# recovers it, and the span fitting policy makes its oversized glyph fit, so the
# atom is served from Noto Emoji 1.05.  Route identity is the assertion here;
# artwork lives in xvfb-emoji-artwork.sh and discovery in xvfb-font-discovery.sh.
run_case routing info-text ℹ mono 1 \
    'base=U+2139 width=1 presentation=text role=fallback' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' default NotoEmoji-Regular.ttf
run_unicode_case routing info-vs16-unicode ℹ️ color 2 \
    'base=U+2139 width=2 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true

# HarfBuzz receives each complete backend grapheme. These sequences must
# resolve to one positioned glyph without changing libghostty's cell width.
run_unicode_case routing sequence-keycap 1️⃣ color 2 \
    'base=U+0031 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing keycap-default 1️⃣ color 1 \
    'base=U+0031 width=1 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' default
run_unicode_case routing sequence-tone 👋🏽 color 2 \
    'base=U+1F44B width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_unicode_case routing sequence-zwj 👩‍💻 color 2 \
    'base=U+1F469 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
# Current Noto deliberately paints family sequences with an achromatic gray
# COLRv1 palette. "mono" here describes sampled pixels, not the font format.
run_unicode_case routing sequence-family 👨‍👩‍👧 mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_case routing sequence-family-default 👨‍👩‍👧 color 6 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_unicode_case routing sequence-flag 🇺🇸 color 2 \
    'base=U+1F1FA width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_unicode_case routing sequence-tag-flag 🏴󠁧󠁢󠁳󠁣󠁴󠁿 color 2 \
    'base=U+1F3F4 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
# Twitter's SVG font has the black-flag base but not the Scotland ligature.
# Preserved tag components must reject it before drawing can erase them.
run_unicode_case atomic-tag sequence-tag-atomic-fallback 🏴󠁧󠁢󠁳󠁣󠁴󠁿 color 2 \
    'base=U+1F3F4 width=2 presentation=emoji role=doublesize glyphs=1' \
    'Twitter Color Emoji' 'Noto Color Emoji' unicode true
run_unicode_case routing adjacent-flags 🇺🇸🇯🇵 color 4 \
    'base=U+1F1FA width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
run_unicode_case routing sequence-combining "$combining_acute" mono 1 \
    'base=U+0065 width=1 presentation=none role=primary glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true

# A partial match in the preferred emoji face must not split the cluster.
run_unicode_case routing sequence-atomic-fallback ❤️‍🔥 color 2 \
    'base=U+2764 width=2 presentation=emoji role=doublesize glyphs=1' \
    'DejaVu Sans Mono' 'Noto Color Emoji' unicode true
# Noto Emoji has both component glyphs but no woman-technologist ligature. It
# must also fall through atomically, rather than squeezing both into one cell.
run_unicode_case routing sequence-ligature-fallback 👩‍💻 color 2 \
    'base=U+1F469 width=2 presentation=emoji role=doublesize glyphs=1' \
    'Noto Emoji' 'Noto Color Emoji' unicode true

# Styled whole-sequence selection.  The generated styled family maps both
# components and the joiner in all three real faces, and only Regular carries the
# `liga` rule that joins them, so a bold or italic request has a real, covering
# candidate that still cannot shape the complete atom.  Production accepts a styled
# candidate only when it shapes the whole cluster to one glyph
# (`requires_composition` in src/font_router.c), which these cases grade in both
# places style selection happens: the configured role and the fallback candidate
# list.  Every case runs in mode 2027, because the complete grapheme is the atom
# under test; the effective file is the discriminator, since the route line reports
# the *requested* bold/italic attributes either way.
zwj_man_laptop=$(printf '\U0001F468\u200D\U0001F4BB')
man=$(printf '\U0001F468')
bulb=$(printf '\U0001F4A1')
bold=$(printf '\033[1m')
italic=$(printf '\033[3m')
styled_family='XTP Styled Emoji'
partial_family='XTP Partial Sequence'
# The complete sequence, served by the one face that can shape it.
run_case styled-emoji seq-styled-normal "$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$styled_family" 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
# Bold and italic requests: the real styled faces of that family cover every
# component, so only complete-sequence acceptance can decline them.  The complete
# result must survive, which is the effective file staying Regular.
run_case styled-emoji seq-styled-bold "$bold$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$styled_family" 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
run_case styled-emoji seq-styled-italic "$italic$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$styled_family" 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
# The controls that make those two mean something: the same request style on one
# component of the same sequence is served by the styled face itself, so the
# declines above are about the complete atom and not about coverage or realness.
run_case styled-emoji seq-component-bold "$bold$man" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$styled_family" 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Bold.ttf
run_case styled-emoji seq-component-italic "$italic$man" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$styled_family" 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Italic.ttf
# A preferred role face that covers both components and cannot shape the sequence:
# the atom must fall through whole to the later candidate, under every style.
run_case styled-emoji seq-partial-normal "$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=doublesize glyphs=1' \
    "$partial_family" "$styled_family" unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
run_case styled-emoji seq-partial-bold "$bold$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=doublesize glyphs=1' \
    "$partial_family" "$styled_family" unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
run_case styled-emoji seq-partial-italic "$italic$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=doublesize glyphs=1' \
    "$partial_family" "$styled_family" unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
# Why that role face was refused: it serves one component of the same sequence.
run_case styled-emoji seq-partial-component "$man" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji glyphs=1' \
    "$partial_family" "$styled_family" unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpPartialSequence-Regular.ttf
# The same decline in the other place style selection happens: with no wide role
# the atom is served by a fallback rung, where styled candidates come from the
# same-family candidate list rather than from a configured role.
run_case styled-emoji seq-fallback-bold "$bold$zwj_man_laptop" mono 2 \
    'base=U+1F468 width=2 presentation=emoji role=emoji-fallback glyphs=1' \
    "$partial_family" - unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Regular.ttf
# That rung's own control: U+1F4A1 is carried by the styled family alone, so this
# request reaches the same fallback candidate list -- and there the bold candidate is
# accepted.  Without this, retaining Regular above would also pass if no usable bold
# candidate existed on that rung at all.
run_case styled-emoji seq-fallback-component-bold "$bold$bulb" mono 2 \
    'base=U+1F4A1 width=2 presentation=emoji role=emoji-fallback glyphs=1' \
    "$partial_family" - unicode true \
    'DejaVu Sans Mono:rgba=none' unicode XtpStyledEmoji-Bold.ttf
# Real-font sequences under a style request, for the types the generated fixture
# does not cover.  These do not reach the styled-candidate branch -- neither Noto
# family ships a real bold -- so they grade only that a style request leaves a
# composed atom whole, with its width and containment intact.
run_case routing sequence-keycap-bold "$bold"1️⃣ color 2 \
    'base=U+0031 width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode
run_case routing sequence-flag-italic "$italic"🇺🇸 color 2 \
    'base=U+1F1FA width=2 presentation=emoji role=emoji glyphs=1' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true \
    'DejaVu Sans Mono:rgba=none' unicode

# An emoji-face miss falls through to doublesize; policy never changes width.
run_case routing emoji-fallthrough 🫨 color 2 \
	'base=U+1FAE8 width=2 presentation=emoji role=doublesize' \
	'Noto Emoji' 'Noto Color Emoji' unicode true
run_case legacy-routing legacy-cbdt-fallthrough 🫨 color 2 \
	'base=U+1FAE8 width=2 presentation=emoji role=doublesize' \
	'Noto Color Emoji' OpenMoji unicode true
run_case routing policy-emoji-heart ❤ color 1 \
    'base=U+2764 width=1 presentation=emoji role=emoji' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' emoji true
# Forced text presentation still refuses the color face's color-only glyph, and
# the recovered monochrome candidate now serves the atom through the wide slot.
# These two continue to establish that forced text never leaks color and never
# changes the committed width; they now also pin the serving role.
run_case routing policy-text-grin 😀 mono 2 \
    'base=U+1F600 width=2 presentation=text role=doublesize-fallback' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' text true \
    'DejaVu Sans Mono:rgba=none' default NotoEmoji-Regular.ttf
run_case routing policy-text-color-wide 😀 mono 2 \
    'base=U+1F600 width=2 presentation=text role=doublesize-fallback' \
    'Noto Color Emoji' 'Noto Color Emoji' text true \
    'DejaVu Sans Mono:rgba=none' default NotoEmoji-Regular.ttf

# Declining color uses genuine outlines; rejected empty/bitmap-only bases
# exhaust the role chain and render deterministic tofu.
run_case colrv0 no-color-colrv0 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=tofu' OpenMoji - unicode false
run_case svginot no-color-svg 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=emoji' 'Twitter Color Emoji' - unicode false
run_case colrv1 no-color-colrv1 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=tofu' 'Noto Color Emoji' - unicode false
run_case sbix no-color-sbix 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=tofu' 'XTP Synthetic sbix' - unicode false
run_case cbdt no-color-primary 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=tofu' - - unicode false 'Noto Color Emoji'
run_cursor_clip_case
