#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB REVENANT WINDOW-INK FIXTURE-ROOT" >&2
    exit 2
fi

xvfb=$1
terminal=$2
window_ink=$3
fixture_root=$4
test_dir=$(mktemp -d)
xvfb_pid=
terminal_pid=

cleanup()
{
    if test -n "$terminal_pid"
    then
        kill "$terminal_pid" 2>/dev/null || true
        wait "$terminal_pid" 2>/dev/null || true
    fi
    if test -n "$xvfb_pid"
    then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$test_dir"
}
trap cleanup EXIT HUP INT TERM

if ! test -x "$fixture_root/run"
then
    echo "SKIP: stage fixtures with tools/stage-font-fixtures first"
    exit 77
fi

"$xvfb" -displayfd 3 -screen 0 1024x768x24 -nolisten unix -listen tcp -ac \
    3>"$test_dir/display" >"$test_dir/xvfb.log" 2>&1 &
xvfb_pid=$!

attempt=0
while ! test -s "$test_dir/display"
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$xvfb_pid" 2>/dev/null
    then
        echo "Xvfb did not become ready" >&2
        sed -n '1,80p' "$test_dir/xvfb.log" >&2
        exit 1
    fi
    sleep 0.05
done
DISPLAY=127.0.0.1:$(sed -n '1p' "$test_dir/display")
export DISPLAY

wait_for_terminal()
{
    log=$1
    description=$2
    attempt=0
    while ! grep -F -q -- 'emoji-routing-ready' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "revenant did not become ready for $description" >&2
            sed -n '1,300p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
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
        sed -n '1,320p' "$log" >&2
        exit 1
    fi
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
run_case routing info-text ℹ mono 1 \
    'base=U+2139 width=1 presentation=text role=primary' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' unicode true
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
run_case routing policy-text-grin 😀 mono 2 \
    'base=U+1F600 width=2 presentation=text role=primary' \
    'Noto Color Emoji' 'Noto Sans Mono CJK JP' text true
run_case routing policy-text-color-wide 😀 mono 2 \
    'base=U+1F600 width=2 presentation=text role=primary' \
    'Noto Color Emoji' 'Noto Color Emoji' text true

# Declining color uses genuine outlines and rejects empty outline bases.
run_case colrv0 no-color-colrv0 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=primary' OpenMoji - unicode false
run_case svginot no-color-svg 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=emoji' 'Twitter Color Emoji' - unicode false
run_case colrv1 no-color-colrv1 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=primary' 'Noto Color Emoji' - unicode false
run_case sbix no-color-sbix 😀 mono 2 \
    'base=U+1F600 width=2 presentation=emoji role=primary' 'Revenant Synthetic sbix' - unicode false
run_case cbdt no-color-primary 😀 blank 2 \
    'base=U+1F600 width=2 presentation=emoji role=primary' - - unicode false 'Noto Color Emoji'
run_cursor_clip_case
