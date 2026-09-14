#!/bin/sh

set -eu

if test "$#" -ne 6
then
    echo "usage: $0 XVFB XTERM_PLUS SEND-KEY READ-SELECTION WINDOW-ALPHA RESIZE" >&2
    exit 2
fi

xvfb=$1
terminal=$2
keys=$3
reader=$4
alpha=$5
resizer=$6
. "$(dirname "$0")/xvfb-test-lib.sh"
xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home" "$test_dir/control"
control=$test_dir/control
# libXt keeps one selection context per atom for the display's lifetime; the case exits normally.
printf 'leak:XtOwnSelection\n' >"$test_dir/lsan-suppressions"
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$test_dir/lsan-suppressions"
export LSAN_OPTIONS

# Rows 5, 25, 45 and 48 hold FIND-ME at column 7; row 60 wraps WRAPPED-NEEDLE onto row 61.
# The raw child records every byte that reaches it, with Kitty release reports on.
cat >"$test_dir/child.sh" <<'CHILD'
control=$1
stty raw -echo
i=0
while test "$i" -lt 60
do
    case $i in
        5|25|45|48) printf 'row %02d FIND-ME here\r\n' "$i" ;;
        *) printf 'row %02d plain text\r\n' "$i" ;;
    esac
    i=$((i + 1))
done
pad=
while test "${#pad}" -lt 36
do
    pad="${pad}x"
done
printf '%sWRAPPED-NEEDLE\r\n' "$pad"
fill=
while test "${#fill}" -lt 40
do
    fill="${fill}a"
done
printf 'last line\r\n'
printf '\033[>2u\033]2;search-ready\007'
cat /dev/tty >"$control/keys" &
reader_pid=$!
while ! test -e "$control/done"
do
    if test -e "$control/output"
    then
        rm "$control/output"
        printf 'more output FIND-ME\r\n'
    fi
    if test -e "$control/fill"
    then
        rm "$control/fill"
        n=0
        while test "$n" -lt 10
        do
            printf '%s\r\n' "$fill"
            n=$((n + 1))
        done
    fi
    if test -e "$control/alt-on"
    then
        rm "$control/alt-on"
        printf '\033[?1049h'
    fi
    if test -e "$control/alt-off"
    then
        rm "$control/alt-off"
        printf '\033[?1049l'
    fi
    sleep 0.05
done
kill "$reader_pid" 2>/dev/null
CHILD

log=$test_dir/search.log
HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
    "$terminal" -debug +sb -fn fixed \
    -xrm 'xterm.vt100.columns: 40' -xrm 'xterm.vt100.rows: 10' \
    -xrm 'xterm.vt100.internalBorder: 2' \
    -xrm 'xterm.vt100.background: #000000' \
    -xrm 'xterm.vt100.foreground: #FFFFFF' \
    -xrm 'xterm.vt100.cursorColor: #FF0000' \
    -xrm 'xterm.vt100.translations: #override <Key>F5: set-font-linedrawing(toggle)' \
    -e sh "$test_dir/child.sh" "$control" >"$test_dir/search.out" 2>"$log" &
terminal_pid=$!
xtp_wait_for_title "$log" search-ready "search scene"
window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
cw=$(sed -n 's/.*VT100 resolved renderer=.* cell=\([0-9][0-9]*\)x[0-9][0-9]* .*/\1/p' "$log" | tail -1)
ch=$(sed -n 's/.*VT100 resolved renderer=.* cell=[0-9][0-9]*x\([0-9][0-9]*\) .*/\1/p' "$log" | tail -1)

red=0xffff0000
white=0xffffffff
black=0xff000000

fail()
{
    echo "$1" >&2
    shift
    printf '%s\n' "$@" >&2
    grep -E 'search:|viewport|selection:|grid changed' "$log" | tail -60 >&2
    dump_row=0
    while test -n "${cw:-}" && test "$dump_row" -lt 10
    do
        echo "viewport row $dump_row column 7: $(cell_pixel "$dump_row" 7 2>/dev/null)" >&2
        dump_row=$((dump_row + 1))
    done
    exit 1
}

count_of()
{
    grep -c -F -- "$1" "$log" || true
}

wait_count()
{
    attempt=0
    while test "$(count_of "$1")" -lt "$2"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 100 || fail "did not log '$1' $2 times"
        sleep 0.05
    done
}

send()
{
    "$keys" "$window" "$@" >/dev/null
}

backspaces()
{
    n=0
    while test "$n" -lt "$1"
    do
        send keysym BackSpace
        n=$((n + 1))
    done
}

# The top-left pixel of a cell is glyph-free in the fixed font: it shows the cell background.
cell_pixel()
{
    "$alpha" "$window" --sample --argb $((2 + $2 * cw)) $((2 + $1 * ch))
}

wait_pixel()
{
    attempt=0
    while test "$(cell_pixel "$1" "$2")" != "$3"
    do
        attempt=$((attempt + 1))
        test "$attempt" -lt 40 || fail "$4: expected $3" "pixel=$(cell_pixel "$1" "$2")"
        sleep 0.05
    done
}

# Ctrl+Shift+F opens the overlay; a wrapped match is highlighted on both of its rows.
send keysym f ctrl-shift
xtp_wait_for_log "$log" 'search: opened overlay=' 'search open'
grep -q 'search: opened overlay=0x0 ' "$log" && fail "the overlay has no window"
send text WRAPPED-NEEDLE
xtp_wait_for_log "$log" 'search: active start=60,36 end=61,9 reason=nearest' 'wrapped match'
# The bottom viewport shows screen rows 54-63, so the match covers viewport rows 6 and 7.
wait_pixel 6 36 "$red" "active match on its first row"
wait_pixel 7 9 "$red" "active match on its continuation row"
wait_pixel 7 10 "$black" "cell after the match"
wait_pixel 6 35 "$black" "cell before the match"

# Emptying the query clears every highlight.
backspaces 14
wait_count 'search: status query-bytes=0 matches=0 ' 2
wait_pixel 6 36 "$black" "highlight after the query emptied"

# The nearest match above the starting viewport becomes active and is scrolled into view;
# other visible matches look selected.
send text FIND-ME
xtp_wait_for_log "$log" 'search: active start=48,7 end=48,13 reason=nearest' 'nearest match'
xtp_wait_for_log "$log" 'viewport row=43 ' 'scroll to the nearest match'
wait_pixel 5 7 "$red" "active match"
wait_pixel 2 7 "$white" "another visible match"
wait_pixel 2 0 "$black" "plain cell"
send keysym Up
xtp_wait_for_log "$log" 'search: active start=45,7 end=45,13 reason=previous wrapped=false' 'previous match'
wait_pixel 2 7 "$red" "previous match made active"
wait_pixel 5 7 "$white" "former active match"
send keysym Up
xtp_wait_for_log "$log" 'search: active start=25,7 end=25,13 reason=previous wrapped=false' 'older match'
xtp_wait_for_log "$log" 'viewport row=20 ' 'scroll to the older match'
send keysym Up
xtp_wait_for_log "$log" 'search: active start=5,7 end=5,13 reason=previous wrapped=false' 'oldest match'
xtp_wait_for_log "$log" 'viewport row=0 ' 'scroll to the oldest match'
send keysym Up
xtp_wait_for_log "$log" 'search: active start=48,7 end=48,13 reason=previous wrapped=true' 'wrap to the newest'
send keysym Down
xtp_wait_for_log "$log" 'search: active start=5,7 end=5,13 reason=next wrapped=true' 'wrap to the oldest'

# Missing text reports no matches.
backspaces 7
send text NOPE
xtp_wait_for_log "$log" 'search: status query-bytes=4 matches=0 ' 'missing text'

# Output during a search neither moves the viewport nor joins the results.
backspaces 4
send text FIND-ME
wait_count 'search: active start=48,7 end=48,13 reason=nearest' 2
wait_pixel 5 7 "$red" "active match before output"
viewport_moves=$(count_of 'scrollback: viewport')
: >"$control/output"
xtp_wait_for_log "$log" 'more output FIND-ME' 'output during the search'
sleep 0.3
test "$(count_of 'scrollback: viewport')" = "$viewport_moves" || fail "output moved the viewport"
wait_pixel 5 7 "$red" "active match after output"
send keysym Down
wait_count 'search: active start=5,7 end=5,13 reason=next wrapped=true' 2

# A resize reflows the history; the overlay moves with the window and the active match survives.
placed=$(count_of 'search: overlay placed')
"$resizer" "$window" --grid 40 60 10 100 >/dev/null
xtp_wait_for_log "$log" 'VT100 grid changed' 'resize during the search'
wait_count 'search: overlay placed' $((placed + 1))
send keysym Up
wait_count 'search: active start=48,7 end=48,13 reason=previous wrapped=true' 2

# Escape closes the search and returns to the bottom where it started; no key reached the child.
send keysym Escape
xtp_wait_for_log "$log" 'search: closed reason=escape restore-viewport=true' 'escape'
xtp_wait_for_log "$log" 'scrollback: viewport bottom offset=' 'viewport restored'
test ! -s "$control/keys" || fail "search keys reached the application" "$(od -An -c "$control/keys")"

# Enter copies the active match to PRIMARY with its soft wrap joined, and closes the search.
send keysym f ctrl-shift
wait_count 'search: opened overlay=' 2
send text WRAPPED-NEEDLE
wait_count 'search: status query-bytes=14 matches=1 ' 1
send keysym Return
xtp_wait_for_log "$log" 'search: closed reason=copied restore-viewport=false' 'copy'
grep -q 'selection: publish source=SEARCH selection=PRIMARY bytes=14 owned=true' "$log" ||
    fail "the match was not published to PRIMARY"
value=$("$reader" PRIMARY)
test "$value" = WRAPPED-NEEDLE || fail "PRIMARY holds the wrong text" "$(printf '%s' "$value" | od -c)"
test ! -s "$control/keys" || fail "search keys reached the application" "$(od -An -c "$control/keys")"

# Opened from the top of history with a match in the active screen below, the search
# waits for the scan instead of wrapping to that newer match; translated actions stay silent.
n=0
while test "$n" -lt 20
do
    send keysym Prior shift
    n=$((n + 1))
done
xtp_wait_for_log "$log" 'offset=0 length=10 ' 'scroll to the top of history'
send keysym f ctrl-shift
wait_count 'search: opened overlay=' 3
grep -q 'search: opened overlay=.* viewport-at-bottom=false' "$log" || fail "search did not start from history"
send text FIND-ME
wait_count 'search: active start=5,7 end=5,13 reason=nearest wrapped=false' 1
wait_count 'search: status query-bytes=7 matches=5 truncated=false state=complete' 1
test "$(count_of 'reason=nearest wrapped=true')" = 0 || fail "the search wrapped below the starting viewport"
send keysym F5
send keysym g ctrl-shift
send keysym Up ctrl-shift
sleep 0.5
grep -q 'procedural glyphs font-first -> forced' "$log" && fail "a translated action ran during the search"
grep -q 'action pipe-command-output' "$log" && fail "Ctrl+Shift+G ran pipe-command-output during the search"
send keysym Escape
wait_count 'search: closed reason=escape restore-viewport=true' 2

# Opened and typed on the alternate screen, the search waits and recovers when the application returns.
: >"$control/alt-on"
xtp_wait_for_log "$log" '[?1049h' 'alternate screen'
send keysym f ctrl-shift
wait_count 'search: opened overlay=' 4
send text FIND-ME
xtp_wait_for_log "$log" 'search: status query-bytes=7 matches=0 truncated=false state=idle unavailable=true' 'unavailable search'
recovered=$(count_of 'search: status query-bytes=7 matches=5 truncated=false state=complete unavailable=false')
: >"$control/alt-off"
xtp_wait_for_log "$log" '[?1049l' 'primary screen'
wait_count 'search: status query-bytes=7 matches=5 truncated=false state=complete unavailable=false' $((recovered + 1))
send keysym Escape
wait_count 'search: closed reason=escape restore-viewport=true' 3
test ! -s "$control/keys" || fail "search keys reached the application" "$(od -An -c "$control/keys")"

# A screen of single-character matches, far more than 256, is highlighted to the last row.
: >"$control/fill"
xtp_wait_for_log "$log" 'aaaaaaaaaaaaaaaaaaaa' 'screen of matches'
sleep 0.3
send keysym f ctrl-shift
wait_count 'search: opened overlay=' 5
send text a
wait_pixel 8 38 "$white" "a late match among more than 256 visible matches"
wait_pixel 0 0 "$white" "an early match among more than 256 visible matches"
wait_pixel 8 39 "$red" "the active match after a screen of matches"
send keysym Escape
wait_count 'search: closed reason=escape restore-viewport=true' 4
test ! -s "$control/keys" || fail "search keys reached the application" "$(od -An -c "$control/keys")"

# Once the search is closed, keys go to the application again.
send text a
attempt=0
while ! test -s "$control/keys"
do
    attempt=$((attempt + 1))
    test "$attempt" -lt 100 || fail "keys did not reach the application after the search closed"
    sleep 0.05
done

send keysym F5
xtp_wait_for_log "$log" 'procedural glyphs font-first -> forced' 'translation after the search closed'

: >"$control/done"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=
echo "search overlay finds, highlights, navigates, copies and restores the viewport while owning its keys"
