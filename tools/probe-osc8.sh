#!/bin/sh
# Human-run explicit and inferred hyperlink interaction probe.

set -eu

link()
{
    printf '\033]8;;%s\033\\%s\033]8;;\033\\' "$1" "$2"
}

printf 'Hyperlink probe\n\n'
printf 'Hold Shift: linked labels and plain HTTP(S) URLs should underline in xterm+.\n'
printf 'Shift+Button 1: only HTTP(S) targets should open.\n\n'

printf 'HTTP:   '
link 'http://example.com' 'This is an HTTP link'
printf '\nHTTPS:  '
link 'https://example.com/path?q=xterm%2B' 'This is an HTTPS link'
printf '\nInert:  '
link 'mailto:nobody@example.com' 'mailto target (must not open)'
printf '\nInert:  '
link 'file:///tmp/xterm-plus-osc8-probe' 'file target (must not open)'
printf '\nPlain:  http://example.com/path. (the sentence period must not open)\n'
