#!/bin/sh
# Human-run OSC 8 rendering and xterm+ interaction probe.

set -eu

link()
{
    printf '\033]8;;%s\033\\%s\033]8;;\033\\' "$1" "$2"
}

printf 'OSC 8 hyperlink probe\n\n'
printf 'Hold Shift: linked labels should underline in xterm+.\n'
printf 'Shift+Button 1: only HTTP(S) targets should open.\n\n'

printf 'HTTP:   '
link 'http://example.com' 'This is an HTTP link'
printf '\nHTTPS:  '
link 'https://example.com/path?q=xterm%2B' 'This is an HTTPS link'
printf '\nInert:  '
link 'mailto:nobody@example.com' 'mailto target (must not open)'
printf '\nInert:  '
link 'file:///tmp/xterm-plus-osc8-probe' 'file target (must not open)'
printf '\nPlain:  http://example.com (not OSC 8; must not underline or open)\n'
