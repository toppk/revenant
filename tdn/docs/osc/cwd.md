# Working directory

Status: Convention (Apple Terminal origin).

A shell tells the emulator where it is so that new tabs open in the same
directory, tab titles show the path, and file links resolve. Three
incompatible OSCs do this.

## Syntax

### OSC 7

```text
OSC 7 ; file://hostname/path ST
```

The value is a `file:` URI. `hostname` should be the machine's hostname so
the emulator can tell a local directory from one seen over SSH; an empty
host means localhost. The path is percent-encoded: spaces become `%20`,
non-ASCII bytes are encoded per byte.

Apple Terminal introduced OSC 7 (and OSC 6 for the current document). VTE
adopted it in 0.34, and most emulators since.

### OSC 1337 CurrentDir

```text
OSC 1337 ; CurrentDir=/absolute/path ST
```

iTerm2's form. The path is plain text, not a URI, and has no host.

### OSC 9;9

```text
OSC 9 ; 9 ; C:\absolute\path ST
```

ConEmu's form, adopted by Windows Terminal. The path is a Windows path
without encoding. It shares the OSC 9 selector with notifications and
progress; see [Notifications](notifications.md).

## Behavior

- The emulator stores the path per pane. Use is emulator-defined: inherit
  in new tabs, show in the tab title, or resolve relative file links.
- A path from a remote host is still stored. Emulators that check the
  hostname in OSC 7 refuse to `cd` into it locally; emulators that ignore
  the hostname silently open a non-existent directory.
- Nothing is reported back; there is no query form.
- tmux does not forward OSC 7 without passthrough, but tracks
  `pane_current_path` itself from the process table on platforms that
  support it.

### Shell snippets

bash:

```sh
__tdn_osc7() {
  printf '\033]7;file://%s%s\033\\' "$HOSTNAME" "$(printf '%s' "$PWD" | sed 's/%/%25/g; s/ /%20/g')"
}
PROMPT_COMMAND="__tdn_osc7${PROMPT_COMMAND:+; $PROMPT_COMMAND}"
```

zsh:

```zsh
autoload -Uz add-zsh-hook
__tdn_osc7() { printf '\033]7;file://%s%s\033\\' "$HOST" "${PWD// /%20}" }
add-zsh-hook chpwd __tdn_osc7; __tdn_osc7
```

fish:

```fish
function __tdn_osc7 --on-variable PWD
    printf '\033]7;file://%s%s\033\\' (hostname) (string escape --style=url $PWD | string replace -a '%2F' '/')
end
```

Most emulators ship their own snippets that also emit OSC 133; prefer those
where they exist.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Feature | xterm | VTE | Konsole | kitty | WezTerm | Ghostty | foot | Alacritty | Contour | mintty | PuTTY | Windows Terminal | Apple Terminal | iTerm2 | xterm.js | tmux |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OSC 7 | ? | Yes (0.34) | Yes | Yes | Yes | Yes | Yes | ? | ? | Yes | ? | ? | Yes | Yes | ? | ? |
| OSC 1337 CurrentDir | ? | ? | ? | ? | Yes | ? | ? | ? | ? | ? | ? | ? | ? | Yes | ? | ? |
| OSC 9;9 | ? | ? | ? | ? | ? | ? | ? | ? | ? | Yes | ? | Yes | ? | ? | ? | ? |

<!-- markdownlint-enable MD013 -->

## Probe

```sh
tools/sendosc cwd            # current directory
tools/sendosc cwd /tmp
printf '\033]7;file://%s/tmp\033\\' "$(hostname)"
```

Open a new tab or window afterwards and check its directory.

## Sources

- [VTE: OSC 7 support (0.34)](https://gitlab.gnome.org/GNOME/vte/-/blob/master/NEWS)
- [iTerm2 proprietary escape codes, CurrentDir](https://iterm2.com/documentation-escape-codes.html)
- [Windows Terminal: tab duplication with OSC 9;9](https://learn.microsoft.com/en-us/windows/terminal/tutorials/new-tab-same-directory)
- [ConEmu ANSI escape codes, OSC 9;9](https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC)
- [kitty: shell integration](https://sw.kovidgoyal.net/kitty/shell-integration/)
- [WezTerm: shell integration](https://wezterm.org/shell-integration.html)
- [Ghostty: shell integration](https://ghostty.org/docs/features/shell-integration)
- [foot: shell integration](https://codeberg.org/dnkl/foot/src/branch/master/README.md#shell-integration)
