#!/bin/sh

set -eu

terminal=$1
program=$(basename "$terminal")
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

run()
{
    expected=$1
    shift
    set +e
    DISPLAY= "$terminal" "$@" >"$test_dir/out" 2>"$test_dir/err"
    status=$?
    set -e
    if test "$status" -ne "$expected"
    then
        echo "unexpected status $status (wanted $expected): $*" >&2
        sed -n '1,80p' "$test_dir/out" >&2
        sed -n '1,80p' "$test_dir/err" >&2
        exit 1
    fi
}

run 0 -version
grep -E -q "^$program [^[:space:]]+\$" "$test_dir/out"
test ! -s "$test_dir/err"
cp "$test_dir/out" "$test_dir/version"

run 0 --version
cmp "$test_dir/version" "$test_dir/out"
test ! -s "$test_dir/err"

run 0 -help
grep -q "^$program .* usage:\$" "$test_dir/out"
grep -q "^    $program \[-options \.\.\.\] \[-e command args\]\$" "$test_dir/out"
grep -q '^    -version ' "$test_dir/out"
grep -q '^    -help ' "$test_dir/out"
grep -q '^    -name string ' "$test_dir/out"
grep -q '^    -class string ' "$test_dir/out"
grep -q '^    -/+pc ' "$test_dir/out"
grep -q '^    -e command args \.\.\. ' "$test_dir/out"
test ! -s "$test_dir/err"

run 0 -h
grep -q "^$program .* usage:\$" "$test_dir/out"

run 0 -v
cmp "$test_dir/version" "$test_dir/out"

run 0 -geo 80x24 -version
cmp "$test_dir/version" "$test_dir/out"

run 0 -clas CustomTerm -nam custom -sele 1000 -version
cmp "$test_dir/version" "$test_dir/out"

run 0 -pc +pc -version
cmp "$test_dir/version" "$test_dir/out"

run 1 -fo value
grep -q "^$program: bad command line option \"-fo\"\$" "$test_dir/err"

run 1 -bo value
grep -q "^$program: bad command line option \"-bo\"\$" "$test_dir/err"

run 1 -bogus
test ! -s "$test_dir/out"
grep -q "^$program: bad command line option \"-bogus\"\$" "$test_dir/err"
grep -q "^usage:  $program " "$test_dir/err"
grep -q "^Type $program -help for a full description\.\$" "$test_dir/err"

run 1 -e
grep -q "^$program: bad command line option \"-e\"\$" "$test_dir/err"

run 1 -class
grep -q "^$program: option -class requires a value\$" "$test_dir/err"

run 1 -name
grep -q "^$program: option -name requires a value\$" "$test_dir/err"

run 1 -log
grep -q "^$program: option -log requires a value\$" "$test_dir/err"

run 1 -help -bogus
grep -q "^$program .* usage:\$" "$test_dir/out"
grep -q "^$program: bad command line option \"-bogus\"\$" "$test_dir/err"

run 1 -bogus -help
test ! -s "$test_dir/out"
grep -q "^$program: bad command line option \"-bogus\"\$" "$test_dir/err"

ln -s "$terminal" "$test_dir/xterm+"
set +e
DISPLAY= "$test_dir/xterm+" -bogus >"$test_dir/out" 2>"$test_dir/err"
status=$?
set -e
test "$status" -eq 1
grep -q '^xterm+: bad command line option "-bogus"$' "$test_dir/err"
grep -q '^usage:  xterm+ ' "$test_dir/err"

DISPLAY= "$test_dir/xterm+" -help >"$test_dir/out" 2>"$test_dir/err"
grep -q "^$program .* usage:\$" "$test_dir/out"
grep -q '^    xterm+ \[-options \.\.\.\] \[-e command args\]$' "$test_dir/out"
test ! -s "$test_dir/err"

echo "command-line help, version, abbreviation, and error handling passed"
