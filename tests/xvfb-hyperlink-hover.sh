#!/bin/sh

set -eu

if test "$#" -ne 4
then
    echo "usage: $0 XVFB XTERM-PLUS SEND-HOVER TEST-LIB" >&2
    exit 2
fi

xvfb=$1
terminal=$2
hover=$3
test_lib=$4

. "$test_lib"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"

wait_for_count()
{
    log=$1
    pattern=$2
    wanted=$3
    description=$4
    attempt=0
    while test "$(grep -F -c -- "$pattern" "$log" 2>/dev/null || true)" -lt "$wanted"
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not log $description" >&2
            sed -n '1,260p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

log=$test_dir/xterm.log
"$terminal" -debug +sb -xrm 'XTerm*columns: 48' -xrm 'XTerm*rows: 8' \
    -e sh -c 'printf "\033]8;;https://same.example\033\\ONE\033]8;;\033\\  \033]8;;https://same.example\033\\TWO\033]8;;\033\\\r\n\033[4:1m\033]8;;https://single.example\033\\SINGLE\033]8;;\033\\\033[24m\r\nplain https://inferred.example/a\r\n"; sleep 20' \
    >"$test_dir/xterm.out" 2>"$log" &
terminal_pid=$!

xtp_wait_for_log "$log" 'shell: realized window=' 'hyperlink hover test'
xtp_wait_for_log "$log" 'render: frame' 'hyperlink hover output'
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cell=$(sed -n 's/.*config: VT100 resolved renderer=.* cell=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' "$log" | tail -1)
cell_width=${cell%x*}
cell_height=${cell#*x}
row0_y=$((2 + cell_height / 2))
row1_y=$((2 + cell_height + cell_height / 2))
row2_y=$((2 + 2 * cell_height + cell_height / 2))
same_first_x=$((2 + cell_width / 2))
same_second_x=$((2 + 5 * cell_width + cell_width / 2))
single_x=$((2 + cell_width / 2))
inferred_x=$((2 + 8 * cell_width + cell_width / 2))

"$hover" "$window" "$same_first_x" "$row0_y" shift
wait_for_count "$log" 'hyperlink: hover bytes=20 preview="https://same.example"' 1 'first repeated OSC 8 URI hover'
wait_for_count "$log" 'hyperlink: cursor hand' 1 'hand cursor for OSC 8 URI'
"$hover" "$window" "$same_first_x" "$row0_y" plain
wait_for_count "$log" 'hyperlink: hover cleared' 1 'OSC 8 hover clear'
wait_for_count "$log" 'hyperlink: cursor default' 1 'default cursor after OSC 8 hover'

"$hover" "$window" "$same_second_x" "$row0_y" shift
wait_for_count "$log" 'hyperlink: hover bytes=20 preview="https://same.example"' 2 'second repeated OSC 8 URI hover'
wait_for_count "$log" 'hyperlink: cursor hand' 2 'hand cursor for second OSC 8 URI'
"$hover" "$window" "$same_second_x" "$row0_y" plain
wait_for_count "$log" 'hyperlink: hover cleared' 2 'second OSC 8 hover clear'
wait_for_count "$log" 'hyperlink: cursor default' 2 'default cursor after second OSC 8 hover'

"$hover" "$window" "$single_x" "$row1_y" shift
wait_for_count "$log" 'hyperlink: hover bytes=22 preview="https://single.example"' 1 'single-underlined OSC 8 URI hover'
"$hover" "$window" "$single_x" "$row1_y" plain
wait_for_count "$log" 'hyperlink: hover cleared' 3 'single-underlined OSC 8 hover clear'

"$hover" "$window" "$inferred_x" "$row2_y" shift
wait_for_count "$log" 'hyperlink: hover inferred bytes=26 preview="https://inferred.example/a"' 1 'inferred URI hover'
wait_for_count "$log" 'hyperlink: cursor hand' 4 'hand cursor for inferred URI'
"$hover" "$window" "$inferred_x" "$row2_y" plain
wait_for_count "$log" 'hyperlink: hover cleared' 4 'inferred URI hover clear'
wait_for_count "$log" 'hyperlink: cursor default' 4 'default cursor after inferred URI hover'
