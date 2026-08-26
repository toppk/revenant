# Terminal capability probes

Small, inspectable POSIX shell programs for exercising terminal protocols by
name. They are capability probes rather than xterm+ unit tests: run them
inside any emulator to observe its behavior, capture their bytes for a parser
test, or use them while filling in a compatibility table in the
[Terminal Developers Network](../docs/index.md).

<!-- markdownlint-disable MD013 -->

| Tool | Purpose |
| --- | --- |
| `sendcsi` | Named CSI cursor-style, blink, and visibility controls |
| `testfocus` | Enable focus reporting and verify one focus-out/focus-in pair |
| `sendosc` | Named OSC controls: title, hyperlink, clipboard, cwd, notifications, colors, shell-integration marks |
| `query` | Send a query and print the terminal's reply with control bytes visible; named queries for DA, XTVERSION, DECRQM, XTGETTCAP, DECRQSS, colors, kitty keyboard |
| `sgr-sampler` | Print attributes, underline styles, 16/256/truecolor ramps in both `;` and `:` forms |

<!-- markdownlint-enable MD013 -->

Every tool writes only the requested bytes to standard output (`query` also
reads standard input), so output can be captured with `od -c` or redirected
into a parser test. `query` restores terminal settings on exit and times out
rather than blocking when a terminal does not answer.

Examples:

```sh
tools/query version da1
tools/query mode 2026
tools/query tcap RGB
tools/query '\033[?2027$p'
tools/testfocus
tools/sendosc link https://example.com "example"
tools/sgr-sampler
```

When recording an observation in a TDN compatibility table, name the
emulator, its version (`query version` where supported), the date, and the
tool invocation.
