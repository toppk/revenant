#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
sender=$3

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home" "$test_dir/work dir"
long=$(printf 'w%.0s' $(seq 1 110))

# Three OSC 133 command blocks with Unicode, a line that wraps at 80 columns,
# and shell-looking text that must stay data, then a live prompt. The shell
# also reports the work directory with OSC 7 so the helper runs there.
uri_dir=$(printf '%s' "$test_dir/work dir" | sed 's/ /%20/g')
script=$test_dir/child.sh
cat >"$script" <<EOF
printf '\\033]7;%s\\033\\\\' 'file://localhost$uri_dir'
n=1
while test \$n -le 3
do
    printf '\\033]133;A\\033\\\\\$ \\033]133;B\\033\\\\command-%s\\r\\n\\033]133;C\\033\\\\' "\$n"
    printf 'COMMAND-%s-BEGIN\\r\\n' "\$n"
    printf 'output with Unicode: café 界; literal shell text: \$(do-not-execute)\\r\\n'
    printf 'wrapped: %s\\r\\n' '$long'
    printf 'COMMAND-%s-END\\r\\n' "\$n"
    printf '\\033]133;D;0\\033\\\\'
    n=\$((n + 1))
done
printf '\\033]2;pipe-ready\\007'
while ! test -e "$test_dir/step-live"; do sleep 0.05; done
printf '\\033]133;A\\033\\\\\$ \\033]133;B\\033\\\\'
printf '\\033]2;live-ready\\007'
while ! test -e "$test_dir/step-done"; do sleep 0.05; done
EOF

start_terminal()
{
    case_name=$1
    shift
    log=$test_dir/$case_name.log
    rm -f "$test_dir/step-done" "$test_dir/step-live"
    HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug +sb -fn fixed -geometry 80x24 "$@" -e sh "$script" >"$test_dir/$case_name.out" 2>"$log" &
    terminal_pid=$!
    xtp_wait_for_title "$log" pipe-ready "$case_name fixture"
    xtp_wait_for_log "$log" "working directory set path=$test_dir/work dir" "$case_name directory"
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
}

expect_count()
{
    actual=$(grep -c -F -- "$1" "$log" || true)
    if test "$actual" -ne "$2"
    then
        echo "$3: expected $2 occurrences of '$1', found $actual" >&2
        exit 1
    fi
}

finish_terminal()
{
    touch "$test_dir/step-live" "$test_dir/step-done"
    if ! wait "$terminal_pid"
    then
        terminal_pid=
        echo "xterm+ failed while exiting from the $1 case" >&2
        sed -n '1,200p' "$log" >&2
        exit 1
    fi
    terminal_pid=
}

# The helper captures its stdin and its working directory. The first press comes
# before the shell prints its next prompt, so completion must come from OSC 133 D;
# the second press, with the live prompt on screen, must pick the same command.
start_terminal capture -xrm "XTerm*pipeCommandOutput: pwd > '$test_dir/capture-dir'; cat > '$test_dir/capture'"
printf 'COMMAND-3-BEGIN\noutput with Unicode: café 界; literal shell text: $(do-not-execute)\nwrapped: %s\nCOMMAND-3-END' "$long" >"$test_dir/expected"
check_capture()
{
    "$sender" "$window" ctrl-shift-g >/dev/null
    attempt=0
    while test "$(grep -c -F 'command exited pid=' "$log" || true)" -lt "$1"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || { echo "$2: helper $1 did not exit" >&2; sed -n '1,200p' "$log" >&2; exit 1; }
        sleep 0.05
    done
    if ! cmp -s "$test_dir/expected" "$test_dir/capture"
    then
        echo "$2: captured output differs from COMMAND-3-BEGIN..COMMAND-3-END" >&2
        diff -u "$test_dir/expected" "$test_dir/capture" >&2 || true
        sed -n '1,200p' "$log" >&2
        exit 1
    fi
    test "$(cat "$test_dir/capture-dir")" = "$test_dir/work dir" ||
        { echo "$2: helper ran in $(cat "$test_dir/capture-dir"), not the reported directory" >&2; exit 1; }
    rm -f "$test_dir/capture" "$test_dir/capture-dir"
}
check_capture 1 "before the next prompt"
touch "$test_dir/step-live"
xtp_wait_for_title "$log" live-ready "live prompt"
check_capture 2 "with the live prompt"
expect_count "command exited pid=" 2 "both helpers reaped"
expect_count "no OSC 133 D seen" 0 "completion must come from OSC 133 D"
finish_terminal capture
test ! -e "$test_dir/do-not-execute" || { echo "shell-looking output was executed" >&2; exit 1; }

# Unset resource: the action only logs and starts nothing.
start_terminal unset
"$sender" "$window" ctrl-shift-g >/dev/null
xtp_wait_for_log "$log" "pipeCommandOutput is unset; nothing to run" "unset resource"
finish_terminal unset
grep -q -F "pipe: spawned pid=" "$log" && { echo "unset resource still spawned a helper" >&2; exit 1; }

# Closed pipe: the helper exits without reading while more than a pipe buffer
# of output is pending, and the terminal keeps going.
rm -f "$test_dir/step-done"
cat >"$test_dir/big.sh" <<EOF
printf '\\033]133;A\\033\\\\\$ \\033]133;B\\033\\\\big\\r\\n\\033]133;C\\033\\\\'
i=0
while test \$i -lt 1200; do printf 'line %04d %s\\r\\n' "\$i" '$long'; i=\$((i + 1)); done
printf '\\033]133;D;0\\033\\\\\\033]133;A\\033\\\\\$ \\033]133;B\\033\\\\'
printf '\\033]2;pipe-ready\\007'
while ! test -e "$test_dir/step-done"; do sleep 0.05; done
EOF
rm -f "$test_dir/step-live"
log=$test_dir/closed.log
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug +sb -fn fixed -geometry 80x24 -sl 6000 -xrm 'XTerm*pipeCommandOutput: exec true' \
    -e sh "$test_dir/big.sh" >"$test_dir/closed.out" 2>"$log" &
terminal_pid=$!
xtp_wait_for_title "$log" pipe-ready "closed-pipe fixture"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
"$sender" "$window" ctrl-shift-g >/dev/null
xtp_wait_for_log "$log" "output pipe closed early written=" "closed pipe"
xtp_wait_for_log "$log" "command exited pid=" "closed-pipe helper exit"
finish_terminal closed-pipe

# Teardown: the helper shell exits at once, leaving a background child that
# ignores SIGTERM in its process group; the child must be gone afterwards.
start_terminal teardown -xrm "XTerm*pipeCommandOutput: trap '' TERM; sleep 30 & echo \$! > '$test_dir/helper-pid'"
"$sender" "$window" ctrl-shift-g >/dev/null
xtp_wait_for_log "$log" "command exited pid=" "teardown leader exit"
attempt=0
while ! test -s "$test_dir/helper-pid"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || { echo "teardown helper did not record its child" >&2; exit 1; }
    sleep 0.05
done
finish_terminal teardown
grep -q -F "command killed group=" "$log" || { echo "teardown did not kill the helper group" >&2; exit 1; }
sleep 0.2
if kill -0 "$(cat "$test_dir/helper-pid")" 2>/dev/null
then
    echo "the helper's child survived teardown" >&2
    exit 1
fi

echo "pipe-command-output sends exactly the last command's output to the configured helper in the reported directory"
