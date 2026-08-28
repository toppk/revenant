#!/bin/sh

set -eu

if test "$#" -ne 5
then
    echo "usage: $0 XVFB XTERM-PLUS COMPOSITOR-OWNER WINDOW-ALPHA DRAG-SLIDER" >&2
    exit 2
fi

xvfb=$1
terminal=$2
compositor_owner=$3
window_alpha=$4
drag_slider=$5
test_dir=$(mktemp -d)
xvfb_pid=
compositor_pid=
terminal_pid=

cleanup()
{
    if test -n "$terminal_pid"
    then
        kill "$terminal_pid" 2>/dev/null || true
        wait "$terminal_pid" 2>/dev/null || true
    fi
    if test -n "$compositor_pid"
    then
        kill "$compositor_pid" 2>/dev/null || true
        wait "$compositor_pid" 2>/dev/null || true
    fi
    if test -n "$xvfb_pid"
    then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$test_dir"
}
trap cleanup EXIT HUP INT TERM

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
    while ! grep -q 'shell: realized window=' "$log" 2>/dev/null
    do
        attempt=$((attempt + 1))
        if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
        then
            echo "xterm+ did not become ready for $description" >&2
            sed -n '1,220p' "$log" >&2
            exit 1
        fi
        sleep 0.05
    done
}

fallback_log=$test_dir/fallback.log
"$terminal" -debug -xrm 'XTerm*backgroundOpacity: 0.5' \
    -e sh -c 'printf "opaque fallback\r\n"; sleep 20' \
    >"$test_dir/fallback.out" 2>"$fallback_log" &
terminal_pid=$!
wait_for_terminal "$fallback_log" 'opaque fallback'
if ! grep -q 'compositor is unavailable; using opaque default visual' "$fallback_log" || \
   ! grep -q 'depth=24 argb=false background-alpha=65535' "$fallback_log"
then
    echo "xterm+ did not fall back to the opaque default visual" >&2
    sed -n '1,220p' "$fallback_log" >&2
    exit 1
fi
kill "$terminal_pid"
wait "$terminal_pid" 2>/dev/null || true
terminal_pid=

"$compositor_owner" >"$test_dir/compositor.out" 2>"$test_dir/compositor.log" &
compositor_pid=$!
attempt=0
while ! grep -q '^ready$' "$test_dir/compositor.out" 2>/dev/null
do
    attempt=$((attempt + 1))
    if test "$attempt" -ge 100 || ! kill -0 "$compositor_pid" 2>/dev/null
    then
        echo "compositor selection owner did not become ready" >&2
        sed -n '1,80p' "$test_dir/compositor.log" >&2
        exit 1
    fi
    sleep 0.05
done

run_argb_case()
{
    renderer=$1
    log=$test_dir/$renderer.log
    if test "$renderer" = true
    then
        renderer_name=xft
    else
        renderer_name=xlib-bitmap
    fi

    "$terminal" -debug -sb -fa monospace \
        -xrm 'XTerm*backgroundOpacity: 0.5' \
        -xrm "xterm.vt100.renderFont: $renderer" \
        -e sh -c 'printf "transparent background\r\n"; sleep 20' \
        >"$test_dir/$renderer.out" 2>"$log" &
    terminal_pid=$!
    wait_for_terminal "$log" "$renderer ARGB visual"
    if ! grep -q 'depth=32 argb=true background-alpha=32768' "$log" || \
       ! grep -q 'effective-alpha=32768 visual-alpha=true depth=32' "$log" || \
       ! grep -q "active renderer=$renderer_name" "$log"
    then
        echo "xterm+ did not select the requested ARGB visual for $renderer" >&2
        sed -n '1,240p' "$log" >&2
        exit 1
    fi
    window=$(sed -n 's/.*shell: realized window=\(0x[0-9a-fA-F]*\).*/\1/p' "$log" | tail -1)
    alpha=$("$window_alpha" "$window" --expose)
    if test "$alpha" -lt 32000 || test "$alpha" -gt 33500
    then
        echo "$renderer redraw changed background alpha to $alpha" >&2
        sed -n '1,260p' "$log" >&2
        exit 1
    fi
    if test "$renderer" = true
    then
        "$drag_slider" open "$window" >/dev/null
        attempt=0
        while ! grep -q 'opacity slider geometry' "$log"
        do
            attempt=$((attempt + 1))
            if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
            then
                echo "Main Options menu did not report the opacity slider geometry" >&2
                sed -n '1,280p' "$log" >&2
                exit 1
            fi
            sleep 0.05
        done
        slider_x=$(sed -n 's/.*opacity slider geometry x=\([0-9]*\) .*/\1/p' "$log" | tail -1)
        slider_y=$(sed -n 's/.*opacity slider geometry .* y=\([0-9]*\) .*/\1/p' "$log" | tail -1)
        slider_w=$(sed -n 's/.*opacity slider geometry .* width=\([0-9]*\) .*/\1/p' "$log" | tail -1)
        slider_h=$(sed -n 's/.*opacity slider geometry .* height=\([0-9]*\).*/\1/p' "$log" | tail -1)
        "$drag_slider" drag "$window" $((slider_x + slider_w * 3 / 4)) $((slider_y + slider_h / 2)) >/dev/null
        attempt=0
        while ! grep -q 'background opacity changed percent=' "$log"
        do
            attempt=$((attempt + 1))
            if test "$attempt" -ge 100 || ! kill -0 "$terminal_pid" 2>/dev/null
            then
                echo "Main Options opacity slider did not update the terminal" >&2
                sed -n '1,280p' "$log" >&2
                exit 1
            fi
            sleep 0.05
        done
        percent=$(sed -n 's/.*background opacity changed percent=\([0-9][0-9]*\).*/\1/p' "$log" | tail -1)
        alpha=$("$window_alpha" "$window" --expose)
        expected_alpha=$((percent * 65535 / 100))
        alpha_difference=$((alpha - expected_alpha))
        if test "$alpha_difference" -lt 0
        then
            alpha_difference=$((-alpha_difference))
        fi
        if test "$alpha_difference" -gt 500
        then
            echo "slider requested $percent% but window alpha is $alpha" >&2
            sed -n '1,300p' "$log" >&2
            exit 1
        fi
    fi
    kill "$terminal_pid"
    wait "$terminal_pid" 2>/dev/null || true
    terminal_pid=
}

run_argb_case false
run_argb_case true
