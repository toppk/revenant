#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$script_dir/xvfb-test-lib.sh"

xvfb=$1
terminal=$2
program_name=$3
version=$4

xtp_xvfb_test_init
xtp_start_xvfb "$xvfb"
mkdir "$test_dir/empty-home"

# DA2's firmware number is derived here from Meson's version, not from the
# terminal: major * 10000 + minor * 100 + patch, minor and patch capped at 99,
# the total capped at 65535.
firmware_for()
{
    major=${1%%.*}
    rest=${1#"$major"}
    rest=${rest#.}
    minor=${rest%%.*}
    rest=${rest#"$minor"}
    rest=${rest#.}
    patch=$(printf '%s' "$rest" | sed 's/[^0-9].*$//')
    minor=$(printf '%s' "$minor" | sed 's/[^0-9].*$//')
    minor=${minor:-0}
    patch=${patch:-0}
    test "$minor" -le 99 || minor=99
    test "$patch" -le 99 || patch=99
    value=$((${major:-0} * 10000 + minor * 100 + patch))
    test "$value" -le 65535 || value=65535
    printf '%s' "$value"
}

for sample in '0.7.0-dev 700' '7.0.0 65535' '0.100.0 9900' '1.250.3 19903' '0.6.1 601'
do
    if test "$(firmware_for "${sample% *}")" != "${sample#* }"
    then
        echo "firmware oracle gives $(firmware_for "${sample% *}") for ${sample% *}" >&2
        exit 1
    fi
done
firmware=$(firmware_for "$version")

da1=$(printf '\033[?62;6;21;22c')
da2=$(printf '\033[>1;%s;0c' "$firmware")
da3=$(printf '\033P!|00000000\033\\')
xtversion=$(printf '\033P>|%s(%s)\033\\' "$program_name" "$version")
dsr=$(printf '\033[0n')

# The child copies the raw bytes that arrive within two seconds to REPLY-FILE.
run_case()
{
    case_name=$1
    sender=$2
    reader="stty raw -echo; $sender; IFS= read -r -t 2 -N 256 reply || true; printf '%s' \"\$reply\" >\"\$1\""
    log=$test_dir/$case_name.log
    reply=$test_dir/$case_name.reply
    if ! HOME="$test_dir/empty-home" XENVIRONMENT=/dev/null XFILESEARCHPATH=/dev/null \
        "$terminal" -debug -e bash -c "$reader" bash "$reply" \
        >"$test_dir/$case_name.out" 2>"$log"
    then
        echo "xterm+ failed during the $case_name device-attributes case" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    fi
    if ! cmp -s "$test_dir/$case_name.expected" "$reply"
    then
        echo "$case_name device-attributes replies differ" >&2
        echo "expected:" >&2
        od -An -c "$test_dir/$case_name.expected" >&2
        echo "actual:" >&2
        od -An -c "$reply" >&2
        sed -n '1,120p' "$log" >&2
        exit 1
    fi
}

# One write: an operating-status query, all three DA forms, then XTVERSION.
printf '%s%s%s%s%s' "$dsr" "$da1" "$da2" "$da3" "$xtversion" >"$test_dir/one-write.expected"
run_case one-write "printf '\\033[5n\\033[c\\033[>c\\033[=c\\033[>q'"

# Split writes: each query arrives in fragments with pauses between them.
printf '%s%s%s%s' "$da1" "$da2" "$da3" "$da1" >"$test_dir/split.expected"
run_case split "printf '\\033['; sleep 0.2; printf 'c\\033[>'; sleep 0.2; printf 'c'; sleep 0.2; printf '\\033[='; sleep 0.2; printf 'c\\033[0c'"

echo "device attributes reply exactly through the PTY, whole and split"
