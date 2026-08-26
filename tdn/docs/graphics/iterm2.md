# iTerm2 inline images

Status: Extension. Defined by iTerm2 under its OSC 1337 namespace; adopted by
WezTerm, mintty, and the xterm.js image addon.

The protocol sends a whole image file, in any format the emulator can decode,
and displays it in a rectangle of cells at the cursor. It has no image ids and
no delete command; the image lives and dies with the cells it occupies.

## Syntax

```text
OSC 1337 ; File = [arguments] : base64-data BEL
```

`ST` is accepted as the terminator by most implementations; iTerm2's own
documentation uses `BEL`. Arguments are `key=value` pairs separated by `;`.

<!-- markdownlint-disable MD013 -->

| Argument | Values | Meaning |
| --- | --- | --- |
| `name` | base64 | File name; used for downloads, optional for inline |
| `size` | bytes | Length of the decoded file; optional but recommended |
| `inline` | `0` or `1` | 1 displays the image; 0 offers it as a download (iTerm2 only) |
| `width`, `height` | `N` cells, `Npx` pixels, `N%` of the pane, `auto` | Display size; default `auto` keeps the native size clamped to the pane |
| `preserveAspectRatio` | `0` or `1` | Default 1; when 0 the image is stretched to `width`×`height` |
| `doNotMoveCursor` | `0` or `1` | When 1 the cursor stays where it was (WezTerm and iTerm2) |

<!-- markdownlint-enable MD013 -->

## Multipart transfers

For files larger than the emulator's OSC buffer, iTerm2 3.5+ accepts:

```text
OSC 1337 ; MultipartFile = arguments BEL
OSC 1337 ; FilePart = base64-chunk BEL      (repeated)
OSC 1337 ; FileEnd BEL
```

Arguments match `File=`. Adoption outside iTerm2: `?`.

## Behavior

The image occupies `width`×`height` cells beginning at the cursor. The cursor
moves to the row below the image, column 1, unless `doNotMoveCursor=1`. The
image scrolls with the rows it sits on and is removed when those rows are
erased. On the alternate screen it is discarded when the screen is left.

## Compatibility

<!-- markdownlint-disable MD013 -->

| Emulator | Support | Notes |
| --- | --- | --- |
| xterm | ? | |
| VTE | ? | |
| Konsole | Yes, 22.04+[^konsole] | |
| kitty | ? | |
| WezTerm | Yes[^wez] | |
| Ghostty | ? | |
| foot | ? | |
| Alacritty | ? | |
| Contour | ? | |
| mintty | Yes[^mintty] | |
| PuTTY | ? | |
| Windows Terminal | ? | |
| Apple Terminal | ? | |
| iTerm2 | Yes[^iterm] | Reference; multipart since 3.5 |
| xterm.js | Yes, addon[^xjs] | `@xterm/addon-image`; also used by the VS Code terminal |
| tmux | ? | |

<!-- markdownlint-enable MD013 -->

[^konsole]: [Konsole 22.04 announcement](https://kde.org/announcements/gear/22.04.0/).
[^wez]: [WezTerm imgcat](https://wezterm.org/imgcat.html).
[^mintty]: [mintty wiki, Control Sequences](https://github.com/mintty/mintty/wiki/CtrlSeqs).
[^iterm]: [iTerm2 inline images](https://iterm2.com/documentation-images.html).
[^xjs]: [@xterm/addon-image](https://github.com/jerch/xterm-addon-image).

## Probe

A 1×1 red PNG, displayed 4 cells wide:

```sh
printf '\033]1337;File=inline=1;width=4;height=2;preserveAspectRatio=0:iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==\a\n'
```

Any file:

```sh
python3 -c '
import base64,sys
d=open(sys.argv[1],"rb").read()
sys.stdout.write("\033]1337;File=inline=1;size=%d:%s\a\n"
                 % (len(d), base64.b64encode(d).decode()))' photo.png
```

## Sources

- [iTerm2 inline images](https://iterm2.com/documentation-images.html)
- [iTerm2 proprietary escape codes](https://iterm2.com/documentation-escape-codes.html)
- [WezTerm imgcat](https://wezterm.org/imgcat.html)
