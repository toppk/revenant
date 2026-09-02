#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

if test "$#" -ne 6
then
    echo "usage: $0 XVFB REVENANT XRDB WINDOW-ALPHA TIMEOUT HOLD-COLORS" >&2
    exit 2
fi

xvfb=$1
terminal=$2
xrdb=$3
window_alpha=$4
timeout_program=$5
hold_colors=$6

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"

mkdir "$test_dir/empty-home"
default_reply=$test_dir/default-palette.reply
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null "$timeout_program" 5 "$terminal" -debug \
    -e bash -c '
        stty raw -echo
        exec 3>"$1"
        for index in 1 3 7 12
        do
            printf "\033]4;%d;?\033\\" "$index"
            IFS= read -r -d "\\" reply
            printf "%s\\" "$reply" >&3
        done
    ' bash "$default_reply" >"$test_dir/default-palette.out" 2>"$test_dir/default-palette.log"
then
    echo "revenant failed or timed out during default ANSI-palette query" >&2
    sed -n '1,220p' "$test_dir/default-palette.log" >&2
    exit 1
fi
printf '\033]4;1;rgb:cdcd/0000/0000\033\\\033]4;3;rgb:cdcd/cdcd/0000\033\\\033]4;7;rgb:e5e5/e5e5/e5e5\033\\\033]4;12;rgb:5c5c/5c5c/ffff\033\\' \
    >"$test_dir/default-palette.expected"
if ! cmp -s "$test_dir/default-palette.expected" "$default_reply"
then
    echo "unconfigured ANSI palette does not match xterm defaults" >&2
    od -An -tx1 "$test_dir/default-palette.expected" >&2
    od -An -tx1 "$default_reply" >&2
    sed -n '1,260p' "$test_dir/default-palette.log" >&2
    exit 1
fi

mkdir "$test_dir/precedence-home"
printf 'xterm*color1: #000000\n' >"$test_dir/precedence-home/.Xdefaults"
precedence_reply=$test_dir/palette-precedence.reply
if ! HOME="$test_dir/precedence-home" XENVIRONMENT=/dev/null \
    "$timeout_program" 5 "$terminal" -xrm 'XTerm*color1: #123456' \
    -e bash -c '
        stty raw -echo
        printf "\033]4;1;?\033\\"
        IFS= read -r -d "\\" reply
        printf "%s\\" "$reply" >"$1"
    ' bash "$precedence_reply" >"$test_dir/palette-precedence.out" \
    2>"$test_dir/palette-precedence.log"
then
    echo "revenant failed or timed out during ANSI-palette precedence query" >&2
    sed -n '1,220p' "$test_dir/palette-precedence.log" >&2
    exit 1
fi
printf '\033]4;1;rgb:0000/0000/0000\033\\' >"$test_dir/palette-precedence.expected"
if ! cmp -s "$test_dir/palette-precedence.expected" "$precedence_reply"
then
    echo "ANSI-palette lookup does not preserve normal Xrm name/class precedence" >&2
    od -An -tx1 "$precedence_reply" >&2
    sed -n '1,260p' "$test_dir/palette-precedence.log" >&2
    exit 1
fi

palette_reply=$test_dir/ansi-palette.reply
palette_expected=$test_dir/ansi-palette.expected
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null "$timeout_program" 5 "$terminal" \
    -xrm 'XTerm*color0: #000102' \
    -xrm 'XTerm*color1: #101112' \
    -xrm 'XTerm*color2: #202122' \
    -xrm 'XTerm*color3: #303132' \
    -xrm 'XTerm*color4: #404142' \
    -xrm 'XTerm*color5: #505152' \
    -xrm 'XTerm*color6: #606162' \
    -xrm 'XTerm*color7: #707172' \
    -xrm 'XTerm*color8: #808182' \
    -xrm 'XTerm*color9: #909192' \
    -xrm 'XTerm*color10: #a0a1a2' \
    -xrm 'XTerm*color11: #b0b1b2' \
    -xrm 'XTerm*color12: #c0c1c2' \
    -xrm 'XTerm*color13: #d0d1d2' \
    -xrm 'XTerm*color14: #e0e1e2' \
    -xrm 'XTerm*color15: #f0f1f2' \
    -e bash -c '
        stty raw -echo
        exec 3>"$1"
        for index in $(seq 0 15)
        do
            printf "\033]4;%d;?\033\\" "$index"
            IFS= read -r -d "\\" reply
            printf "%s\\" "$reply" >&3
        done
    ' bash "$palette_reply" >"$test_dir/ansi-palette.out" 2>"$test_dir/ansi-palette.log"
then
    echo "revenant failed or timed out during ANSI-palette class-resource query" >&2
    sed -n '1,220p' "$test_dir/ansi-palette.log" >&2
    exit 1
fi
index=0
while test "$index" -lt 16
do
    red=$((index * 16))
    green=$((red + 1))
    blue=$((red + 2))
    printf '\033]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\033\\' \
        "$index" "$red" "$red" "$green" "$green" "$blue" "$blue"
    index=$((index + 1))
done >"$palette_expected"
if ! cmp -s "$palette_expected" "$palette_reply"
then
    echo "configured ANSI palette returned an unexpected reply" >&2
    od -An -tx1 "$palette_expected" >&2
    od -An -tx1 "$palette_reply" >&2
    exit 1
fi

printf 'XTerm*color2: #123456\n' | "$xrdb" -nocpp -load -
if ! "$xrdb" -query | grep -F -q 'XTerm*color2:'
then
    echo "xrdb did not install the ANSI-palette class resource" >&2
    "$xrdb" -query >&2
    exit 1
fi
server_reply=$test_dir/server-palette.reply
if ! XENVIRONMENT=/dev/null "$timeout_program" 5 "$terminal" -debug \
    -e bash -c '
        stty raw -echo
        printf "\033]4;2;?\033\\"
        IFS= read -r -d "\\" reply
        printf "%s\\" "$reply" >"$1"
    ' bash "$server_reply" >"$test_dir/server-palette.out" 2>"$test_dir/server-palette.log"
then
    echo "revenant failed or timed out during ANSI-palette server-resource query" >&2
    sed -n '1,220p' "$test_dir/server-palette.log" >&2
    exit 1
fi
printf '\033]4;2;rgb:1212/3434/5656\033\\' >"$test_dir/server-palette.expected"
if ! cmp -s "$test_dir/server-palette.expected" "$server_reply"
then
    echo "XTerm*color2 from RESOURCE_MANAGER did not apply" >&2
    od -An -tx1 "$server_reply" >&2
    sed -n '1,260p' "$test_dir/server-palette.log" >&2
    exit 1
fi

pixel_log=$test_dir/palette-pixel.log
XENVIRONMENT=/dev/null "$terminal" -debug +sb -fa monospace \
    -xrm 'XTerm*color1: #ff8000' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.internalBorder: 2' \
    -xrm 'xterm.vt100.renderFont: true' \
    -e sh -c 'printf "\033[2J\033[H\033[41m    \033[0m\033]2;palette-pixel-ready\007"; sleep 20' \
    >"$test_dir/palette-pixel.out" 2>"$pixel_log" &
terminal_pid=$!
xtp_wait_for_title "$pixel_log" palette-pixel-ready 'ANSI-palette painted pixels'
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$pixel_log" | tail -1)
cell_width=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$pixel_log" | tail -1)
cell_height=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$pixel_log" | tail -1)
pixel=$("$window_alpha" "$window" --expose --argb \
    $((2 + cell_width / 2)) $((2 + cell_height / 2)))
if test "$pixel" != 0xffff8000
then
    echo "SGR color1 painted $pixel instead of configured 0xffff8000" >&2
    sed -n '1,300p' "$pixel_log" >&2
    exit 1
fi

kill "$terminal_pid"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
xtp_xvfb_test_cleanup
xtp_xvfb_test_init
xtp_xvfb_screen=1024x768x8
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

"$hold_colors" 184 >"$test_dir/held-colors.log" 2>&1 &
aux_pid=$!
attempt=0
while ! grep -q '^held_colors=' "$test_dir/held-colors.log" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$aux_pid" 2>/dev/null
    then
        echo "color-cell holder did not constrain the PseudoColor colormap" >&2
        sed -n '1,120p' "$test_dir/held-colors.log" >&2
        exit 1
    fi
    sleep 0.05
done

constrained_reply=$test_dir/constrained-palette.reply
if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null "$timeout_program" 5 "$terminal" \
    -xrm 'XTerm*backgroundOpacity: 1' \
    -xrm 'XTerm*color1: red3' \
    -xrm 'XTerm*color2: green3' \
    -xrm 'XTerm*color15: white' \
    -e bash -c '
        stty raw -echo
        exec 3>"$1"
        for index in 1 2 15
        do
            printf "\033]4;%d;?\033\\" "$index"
            IFS= read -r -d "\\" reply
            printf "%s\\" "$reply" >&3
        done
    ' bash "$constrained_reply" \
    >"$test_dir/constrained-palette.out" 2>"$test_dir/constrained-palette.log"
then
    echo "revenant failed during exhausted-PseudoColor palette query" >&2
    sed -n '1,240p' "$test_dir/constrained-palette.log" >&2
    exit 1
fi
printf '\033]4;1;rgb:cdcd/0000/0000\033\\\033]4;2;rgb:0000/cdcd/0000\033\\\033]4;15;rgb:ffff/ffff/ffff\033\\' \
    >"$test_dir/constrained-palette.expected"
if ! cmp -s "$test_dir/constrained-palette.expected" "$constrained_reply"
then
    echo "ANSI colors changed under exhausted PseudoColor colormap" >&2
    cat "$test_dir/held-colors.log" >&2
    od -An -tx1 "$test_dir/constrained-palette.expected" >&2
    od -An -tx1 "$constrained_reply" >&2
    sed -n '1,240p' "$test_dir/constrained-palette.log" >&2
    exit 1
fi
if grep -q 'Cannot convert string.*to type Pixel' "$test_dir/constrained-palette.log"
then
    echo "ANSI palette still attempted Xt Pixel allocation" >&2
    sed -n '1,240p' "$test_dir/constrained-palette.log" >&2
    exit 1
fi

echo "ANSI palette resources, queries, and painted pixels passed"
