#!/bin/sh
# A compact, screenshot-friendly terminal demo.

set -eu

TerminalIdentity()
{
    local_saved=
    local_reply=
    local_prefix=
    local_suffix=

    if [ ! -t 0 ]; then
        return
    fi
    local_saved=$(stty -g) || return
    trap 'stty "$local_saved"' EXIT HUP INT QUIT TERM
    stty raw -echo min 0 time 2 || return
    printf '\033[>0q\033[c' > /dev/tty
    local_reply=$(dd bs=4096 2>/dev/null; printf x)
    local_reply=${local_reply%x}
    stty "$local_saved"
    trap - EXIT HUP INT QUIT TERM
    local_prefix=$(printf '\033P>|')
    local_suffix=$(printf '\033\\')
    # The XTVERSION payload is terminal-defined; keep it verbatim and remove
    # only the standard DCS transport wrapper.
    case "$local_reply" in
    *"$local_prefix"*)
        local_reply=${local_reply#*"$local_prefix"}
        printf '%s' "${local_reply%%"$local_suffix"*}"
        ;;
    esac
}

terminal_identity=$(TerminalIdentity)

if [ -z "$terminal_identity" ]; then
    terminal_name=${TERM_PROGRAM:-terminal}
    terminal_version=${TERM_PROGRAM_VERSION:-}
    if [ -n "$terminal_version" ]; then
        terminal_identity="$terminal_name $terminal_version"
    else
        terminal_identity=$terminal_name
    fi
fi
# Keep the screenshot layout within an 80-column terminal even when a host
# returns an unusually long XTVERSION payload.
terminal_identity=$(printf '%.32s' "$terminal_identity")

EmojiMode2027()
{
    local_saved=
    local_reply=

    if [ ! -t 0 ]; then
        return
    fi
    local_saved=$(stty -g) || return
    trap 'stty "$local_saved"' EXIT HUP INT QUIT TERM
    stty raw -echo min 0 time 2 || return
    printf '\033[?2027$p' > /dev/tty
    local_reply=$(dd bs=4096 2>/dev/null; printf x)
    local_reply=${local_reply%x}
    stty "$local_saved"
    trap - EXIT HUP INT QUIT TERM
    case "$local_reply" in
    *"$(printf '\033[?2027;1$y')"*) printf 'set' ;;
    *"$(printf '\033[?2027;2$y')"*) printf 'reset' ;;
    *"$(printf '\033[?2027;3$y')"*) printf 'permanent-set' ;;
    *"$(printf '\033[?2027;4$y')"*) printf 'permanent-reset' ;;
    esac
}

emoji_mode=$(EmojiMode2027)
emoji_mode_changed=false
case "$emoji_mode" in
reset)
    # Mode 2027 makes flags and ZWJ emoji one grapheme rather than a run of
    # independently laid-out code points.  Restore the reported baseline.
    printf '\033[?2027h'
    emoji_mode_changed=true
    ;;
esac

link()
{
    printf '\033]8;;%s\033\\%s\033]8;;\033\\' "$1" "$2"
}

style_link()
{
    printf '\033[%sm' "$1"
    link "$2" "$3"
    printf '\033[0m'
}

printf '\033[38;2;126;231;135m'
printf '  ✦  %s  ·  small terminal comforts\033[0m\n\n' "$terminal_identity"

printf '  \033[38;2;255;203;107m🇺🇸\033[0m  '
style_link '4:3;38;2;126;231;135' 'https://toppk.github.io/revenant/docs' 'curly docs'
printf '  ·  '
style_link '4:4;38;2;124;189;255' 'https://github.com/toppk/revenant/releases' 'dotted releases'
printf '  ·  '
style_link '4:5;38;2;255;143;190' 'https://github.com/toppk/revenant/issues' 'dashed ideas'
printf '\n\n'

printf '  \033[38;2;124;189;255m👨‍💻\033[0m  plain: '
printf '\033[4:1;38;2;124;189;255mhttps://example.com/links\033[0m\n'

printf '  \033[38;2;255;143;190m🌱\033[0m  '
style_link '4:1;38;2;255;203;107' 'https://example.com/already-styled' 'an already-underlined OSC 8 label'
printf '\n\n'

printf '  \033[38;2;176;130;255m✧\033[0m  Shift-hover: hand cursor; underline changes shape.\n'
printf '     none → single  ·  single → double  ·  ◌ ◌ ◌\n'
printf '\033[0m'

if [ "$emoji_mode_changed" = true ]; then
    printf '\033[?2027l'
fi
