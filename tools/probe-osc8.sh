#!/bin/sh
# Human-run explicit and inferred hyperlink interaction probe.

set -eu

single_mode=0

if [ "${1-}" = "--single" ]; then
    single_mode=1
fi

link()
{
    printf '\033]8;;%s\033\\%s\033]8;;\033\\' "$1" "$2"
}

underlined_link()
{
    printf '\033[4m'
    link "$1" "$2"
    printf '\033[24m'
}

styled_link()
{
    printf '\033[%sm' "$1"
    link "$2" "$3"
    printf '\033[24m'
}

styled_auto()
{
    printf '\033[%sm%s\033[24m' "$1" "$2"
}

mixed_underline_link()
{
    printf '\033]8;;%s\033\\plain / \033[4msingle underline\033[24m / plain\033]8;;\033\\' "$1"
}

printf 'Hyperlink probe\n\n'
printf 'Hold Shift: no underline becomes single; single underline becomes double.\n'
printf 'Shift+Button 1: only HTTP(S) targets should open.\n\n'

if [ "$single_mode" -ne 0 ]; then
    printf 'Single target: '
    underlined_link 'https://example.com/already-underlined' 'underlined OSC 8 label'
    printf '\n'
else
    printf 'HTTP:   '
    link 'http://example.com' 'This is an HTTP link'
    printf '\nHTTPS:  '
    link 'https://example.com/path?q=xterm%2B' 'This is an HTTPS link'
    printf '\nInert:  '
    link 'mailto:nobody@example.com' 'mailto target (must not open)'
    printf '\nInert:  '
    link 'file:///tmp/xterm-plus-osc8-probe' 'file target (must not open)'
    printf '\nAutolinks:\n'
    printf '  HTTP:  http://example.com/path. (the sentence period must not open)\n'
    printf '  HTTPS: https://example.com/docs?q=osc8#hover\n'
    printf '  Brackets: see https://example.com/a_(b). (the final period must not open)\n'
    printf '  Not a URL: example.com and mailto:nobody@example.com\n'
    printf '\nAlready underlined by the application (SGR 4):\n'
    printf '  OSC 8: '
    underlined_link 'https://example.com/already-underlined' 'underlined OSC 8 label'
    printf '\n  Auto:  \033[4mhttps://example.com/already-underlined\033[24m\n'
    printf '\nMixed underline state inside one target:\n'
    printf '  OSC 8: '
    mixed_underline_link 'https://example.com/mixed-osc8'
    printf '\n  Auto:  https://example.com/auto-\033[4mmixed\033[24m-underline\n'

    printf '\nUnderline style variants with OSC8 + auto-detection:\n'
    printf '  OSC 8 single: '
    styled_link '4:1' 'https://example.com/underline-style-single' 'single style link'
    printf '\n'
    printf '  Auto single: '
    styled_auto '4:1' 'https://example.com/underline-style-single'
    printf '\n'

    printf '  OSC 8 double: '
    styled_link '4:2' 'https://example.com/underline-style-double' 'double style link'
    printf '\n'
    printf '  Auto double: '
    styled_auto '4:2' 'https://example.com/underline-style-double'
    printf '\n'

    printf '  OSC 8 curly: '
    styled_link '4:3' 'https://example.com/underline-style-curly' 'curly style link'
    printf '\n'
    printf '  Auto curly: '
    styled_auto '4:3' 'https://example.com/underline-style-curly'
    printf '\n'

    printf '  OSC 8 dotted: '
    styled_link '4:4' 'https://example.com/underline-style-dotted' 'dotted style link'
    printf '\n'
    printf '  Auto dotted: '
    styled_auto '4:4' 'https://example.com/underline-style-dotted'
    printf '\n'

    printf '  OSC 8 dashed: '
    styled_link '4:5' 'https://example.com/underline-style-dashed' 'dashed style link'
    printf '\n'
    printf '  Auto dashed: '
    styled_auto '4:5' 'https://example.com/underline-style-dashed'
    printf '\n'
fi
