# Readline 8.3 wrapped-prompt resize regression

Status: identified upstream and fixed in Bash development. The observations
below were made on 2026-08-26 with Bash 5.3.9 and Readline 8.3. The fault is in
Readline's prompt metadata refresh after `SIGWINCH`, not in xterm+, xterm, or
libghostty reflow.

The upstream fix is Bash commit
[`1e9f5e10b2a6f81d458936372380c870f0722ef1`](https://git.savannah.gnu.org/cgit/bash.git/commit/?id=1e9f5e10b2a6f81d458936372380c870f0722ef1),
committed by Chet Ramey on 2026-06-02. It is not present in Bash 5.3.9 or the
published `readline83-001` through `readline83-003` patches. A local
Bash/Readline build containing the upstream fix was subsequently confirmed to
recover correctly in xterm+.

This note remains intentionally broader than an xterm+ bug report. Similar
Readline-versus-terminal reflow artifacts have been discussed for years, but
the exact invisible-run cursor displacement documented here is a Readline 8.3
regression introduced by its 2025 redisplay rewrite.

## Symptom

Start an interactive Bash at a long prompt, shrink the terminal until the
prompt wraps, and widen it again. With nonprinting sequences in `PS1`, the
cursor can finish inside the visible prompt instead of after it. Repeated
resizes can leave duplicated prompt fragments.

The visible prompt used in the deterministic xterm+ fixture is 45 columns:

```text
toppk@foundation:~/workspace/xterm-plus (0)$
```

It starts with the cursor at column 45. One observed 80-to-38-to-80 cycle
leaves the cursor at column 37, over the `u` in `plus`.

That eight-column error is not incidental: the final nonprinting run in the
prompt is `ESC ] 133 ; B BEL`, exactly eight bytes. In the SGR control below,
the final nonprinting run is `ESC [ 0 m`, exactly four bytes, and the cursor
finishes at column 41 over the `0` in `(0)`. Both results match the upstream
Readline defect exactly.

This is separate from xterm+'s fixed frame-cache resize bug. That bug allowed
an old-grid pixel snapshot to be painted using new-grid geometry. Here the
terminal state itself contains the cursor and prompt produced after Readline's
`SIGWINCH` redisplay.

## What is supposed to happen

The terminal emulator and the interactive application maintain different
state:

1. The window manager changes the terminal's pixel size.
2. The emulator selects a new row and column count and resizes its screen. A
   reflowing emulator maps existing cells and its cursor into the new grid.
3. The emulator publishes the new PTY size with `TIOCSWINSZ`. The kernel sends
   `SIGWINCH` to the foreground process group.
4. Bash/Readline reads the new dimensions. Readline knows the prompt, editable
   input, logical insertion point, and which prompt bytes were declared
   nonprinting with Bash's `\[` and `\]` escapes.
5. Readline emits carriage returns, cursor movements, erases, and replacement
   text to redisplay its editable line. The emulator executes those bytes in
   order.

The emulator never needs Bash to report the terminal cursor. It owns the
cursor and maps it deterministically during resize. Conversely, Bash does not
normally query the physical cursor with `CSI 6 n`; it predicts the cursor from
what it previously printed and the old and new screen widths.

For example, a 45-cell prompt ending at row 0, column 45 should initially map
to row 1, column 6 at a width of 39. Readline must then account for both
physical prompt rows when it erases or redraws them. If it returns only to
column zero of the second row and prints the prompt again, the cursor finishes
one row too low even though the emulator performed every requested operation
correctly.

There is no standardized resize transaction in which a shell tells the
emulator where its prompt begins or supplies an authoritative post-resize
cursor coordinate. OSC 133 can annotate prompt, input, and output regions, but
it does not make subsequent incorrect cursor movements safe for an emulator to
ignore.

## Background: nonprinting prompt bytes

Bash prompt escapes `\[` and `\]` tell Readline that the enclosed bytes do not
consume terminal columns. They are required around styling and other control
sequences in `PS1`, for example:

```sh
PS1='\[\e[1m\]\u@\h:\w ($?)\$ \[\e[0m\]'
```

Ghostty's Bash integration adds the same kind of nonprinting region for
semantic prompt marks:

```sh
PS1='\[\e]133;P;k=i\a\]'$PS1'\[\e]133;B\a\]'
```

Its other hooks emit `OSC 133;A` before a prompt, `OSC 133;C` before command
output, `OSC 133;D` with command status, and OSC 7 for the working directory.
Ghostty's `--shell-integration=none` disables automatic loading of those
hooks; it does not disable the terminal's OSC 133 parser.

The present failure is not specific to OSC 133. An ordinary, correctly
bracketed SGR sequence also triggers a wrong final cursor. Readline loses the
byte count of the final invisible run after that run moves off the last prompt
screen line during widening.

## Controlled xterm+ cases

Build xterm+ and start each case at 80 columns. Shrink it well below the
visible prompt width, then widen it past that width. Do not submit a command
between the two resizes.

### Plain long prompt: observed to recover correctly

```sh
PS1='\u@\h:\w ($?)\$ ' \
  PROMPT_COMMAND= \
  ./build/xterm+ -debug -e bash --noprofile --norc
```

The prompt is 45 visible cells in the recorded working directory. It wraps at
narrow widths and returns with the cursor at column 45 once the grid is wider
than the prompt.

### OSC 133 in `PS1`: observed failure

```sh
PS1='\[\e]133;P;k=i\a\]\u@\h:\w ($?)\$ \[\e]133;B\a\]' \
  PROMPT_COMMAND= \
  ./build/xterm+ -debug -e bash --noprofile --norc
```

After the shrink-and-grow cycle, the cursor was observed over the `u` in the
prompt rather than after the final space.

### Ordinary SGR in `PS1`: observed failure

```sh
PS1='\[\e[1m\]\u@\h:\w ($?)\$ \[\e[0m\]' \
  PROMPT_COMMAND= \
  ./build/xterm+ -debug -e bash --noprofile --norc
```

After the same cycle, the cursor was observed over the `0` in `(0)`. Because
SGR has no semantic-prompt meaning, this result rules out OSC 133 parsing as a
necessary trigger. The two different wrong positions also make the invisible
bytes and Readline's display accounting important evidence.

### Ghostty-style prompt redraw: also observed to fail

The repository fixture adds Ghostty's `redraw=last` declaration as well as
the `P` and `B` marks:

```sh
PS1='\[\e]133;P;k=i\a\]toppk@foundation:~/workspace/xterm-plus (0)$ \[\e]133;B\a\]'
PROMPT_COMMAND='printf "\e]133;A;redraw=last;cl=line\a"'
```

Run it with:

```sh
just reflow-prompt
just reflow-resize WINDOW_ID
```

The `WINDOW_ID` is printed by the first command's `shell: realized window=`
debug line. Prompt annotations plus an explicit promise that the shell redraws
the prompt therefore do not repair the xterm+ result.

## Why `--noprofile --norc` mattered

An early Ghostty reproducer used:

```sh
PS1="someitnh rellayl logn so you can see" \
  GDK_BACKEND=x11 ghostty --shell-integration=none \
  -e bash --noprofile
```

That disabled Ghostty's automatic injection, but it did not isolate Bash from
the user's configuration. For an interactive non-login Bash, `--noprofile`
does not suppress `~/.bashrc`; `--norc` does.

Environment snapshots from the two Bash invocations exposed the important
difference:

```diff
-PS1=\[\e]133;P;k=i\a\]\u@\h:\w ($?)\$ \[\e]133;B\a\]
+PS1=\u@\h:\w ($?)\$
```

Other differences were a PTY-specific `GPG_TTY`, repeated `PATH`/`INFOPATH`
components, and no known display-relevant value. An environment comparison
cannot show unexported shell functions, traps, `PROMPT_COMMAND`, or `PS0`, so
it is not a complete startup-state diff. It does prove that the supposedly
plain `--noprofile` case still acquired OSC 133 prompt wrappers, while the
`--norc` case retained the same visible prompt without them.

## Cross-implementation observations

The following results have been reported during this investigation:

| Application and shell | Result | Qualification |
| --- | --- | --- |
| xterm+ with plain long Bash `PS1`, `--noprofile --norc` | Recovers correctly | Controlled case above |
| xterm+ with OSC-marked Bash `PS1`, `--noprofile --norc` | Wrong final cursor | Controlled case above |
| xterm+ with SGR-styled Bash `PS1`, `--noprofile --norc` | Wrong final cursor | Controlled case above |
| xterm+ with OSC marks and `redraw=last` | Wrong final cursor | Repository fixture |
| xterm with the affected Bash setup | Same class of failure | Reported manually; repeat the exact controlled matrix before relying on offsets |
| Ghostty 1.3.1, X11, Bash | Same class of failure | Original command loaded `.bashrc`; repeat with `--norc` for a clean comparison |
| Ghostty 1.3.1, X11, `zsh -f` | Recovers correctly | No Bash/Readline involved |
| Bash 5.2 / Readline 8.2 and earlier | Recovers correctly | Confirmed by the upstream version sweep |
| Bash 5.3 / Readline 8.3 without the development fix | Wrong final cursor | Regression introduced by the Readline 8.3 redisplay rewrite |
| Readline 8.3 with the upstream fix | Recovers correctly | Confirmed by the upstream standalone `examples/rl` matrix and a local Bash/xterm+ test |
| Ghostty 1.3.1, native Wayland | Did not reproduce in one manual run | In tension with the deterministic PTY reproducer; recheck that the prompt actually wrapped before widening |

The Ghostty observation is tracked in
[discussion #14026](https://github.com/ghostty-org/ghostty/discussions/14026).
That discussion led to the useful Bash-versus-zsh comparison, but its original
claim that the reproducer excluded all shell integration must be read with the
`--noprofile`/`--norc` correction above.

The terminal frontend is not part of the minimum upstream reproducer. The
[May bug-bash report](https://lists.gnu.org/archive/html/bug-bash/2026-05/msg00065.html)
drives one `TIOCSWINSZ` from 80 to 200 columns on a PTY and reproduces
reliably. The August report also reproduces with Readline's standalone
`examples/rl`, separating the defect from Bash-specific signal hooks.

## What the xterm+ logs show

At startup, a plain 45-cell prompt produces:

```text
frame ... graphemes=45 cursor=visible@45,0
```

During a narrow resize, each `pty: resize` is followed by PTY output from
Bash/Readline and then a full render. This is expected: publishing a new PTY
size causes `SIGWINCH`, and Readline redisplays.

In the plain `--norc` run, intermediate narrow frames contain additional
redisplay cells, but widening beyond the prompt restores exactly 45 graphemes
and cursor column 45. In the marked fixture, the narrow frame contains the
reflowed cells plus another prompt-width redraw, and widening leaves the
cursor at the wrong visible character.

These logs record the state after both terminal resize and newly received
Readline output. The upstream PTY reproducer and source fix now make a separate
trace between libghostty reflow and Readline output unnecessary for ownership,
though it remains useful renderer regression coverage.

## Confirmed root cause

Readline 8.3's redisplay rewrite introduced per-screen-line prompt metadata:

- `local_prompt_newlines[]` records prompt line boundaries.
- `local_prompt_invis_chars[]` records invisible prompt bytes on each screen
  line.
- `WRAP_OFFSET(line)` consumes that invisible-byte count while computing the
  physical cursor.

`expand_prompt()` builds those arrays for the current screen width. Before the
fix, `_rl_redisplay_after_sigwinch()` refreshed them only while the new screen
was narrower than the visible prompt:

```c
if (_rl_screenwidth < prompt_visible_length)
  _rl_reset_prompt ();
```

That condition handles shrinking but not the failing transition: the prompt
wrapped at the old narrow width and fits on one line after widening. The
arrays therefore still described the narrow layout. An invisible run formerly
assigned to the prompt's second screen line disappeared from the new cursor
calculation, moving the physical cursor left by that run's byte length.

The [August bug-readline report](https://lists.gnu.org/archive/html/bug-readline/2026-08/msg00008.html)
refined the condition further: a prompt with no invisible runs or only one run
does not fail in its matrix; a prompt with two or more runs loses the final
run after widening. Changing the last SGR sequence from four to seven bytes
changes the cursor displacement from four to seven columns.

This predicts the xterm+ evidence exactly:

| Case | Final invisible run | Expected cursor | Observed cursor |
| --- | --- | --- | --- |
| OSC 133 | `ESC ] 133 ; B BEL` (8 bytes) | 45 − 8 = 37 | 37, over `u` in `plus` |
| SGR reset | `ESC [ 0 m` (4 bytes) | 45 − 4 = 41 | 41, over `0` in `(0)` |
| Plain prompt | None | 45 | 45 |

The original [May bug-bash report](https://lists.gnu.org/archive/html/bug-bash/2026-05/msg00065.html)
identified Readline 8.3 as the first affected release and traced the regression
to the redisplay changes shipped in Readline commit `447b829`. Its version
sweep found Bash 5.0 through 5.2 unaffected and Bash 5.3 affected; zsh 5.9 was
also unaffected.

## Upstream fix and release status

Chet Ramey's fix removes the width condition and always resets the expanded
prompt metadata after `SIGWINCH`:

```c
/* Let expand_prompt() update local_prompt_newlines and
   local_prompt_invis_chars arrays. */
_rl_reset_prompt ();
```

The Bash development changelog credits the report and fix to Félix Bouynot.
Chet later confirmed on
[bug-readline](https://lists.gnu.org/archive/html/bug-readline/2026-08/msg00009.html)
that the August report was the same issue and had been fixed in May using the
same approach.

As of 2026-08-26, GNU publishes only
[`readline83-001` through `readline83-003`](https://ftp.gnu.org/gnu/readline/readline-8.3-patches/),
and none contains this change. Distributions using a released Bash 5.3 or
Readline 8.3 therefore remain affected unless they backport the Bash
development commit.

## Local fixed-build validation

A local Bash/Readline build containing commit `1e9f5e10b2` was reported on
2026-08-26 to resize cleanly in xterm+: the misplaced cursor no longer
appeared. For a reproducible record of that result, repeat the three controlled
xterm+ cases and capture the build identity:

1. Record `bash --version` and how the fixed Readline was linked.
2. Run the plain, OSC 133, and SGR prompts at 80 columns.
3. Resize each once below the prompt width and back above it.
4. Verify all three finish at cursor column 45 and typed text begins after the
   final prompt space.
5. Run `just reflow-prompt` and `just reflow-resize WINDOW_ID` as the fixed
   fixture check.

The OSC and SGR cases are the decisive controls. The fixed build changes
columns 37 and 41 to 45 without requiring a visible-prompt or xterm+ change.

## Related but distinct resize history

The exact defect above is new to Readline 8.3, but the boundary between
terminal reflow and line-editor redisplay is older:

- In a [2014 Readline discussion](https://lists.gnu.org/archive/html/bug-readline/2014-05/msg00005.html),
  an emulator author reported that Readline 6.3's deferred `SIGWINCH` handling
  complicated prompt reflow. The deferral avoided unsafe work in a signal
  handler.
- In a [2019 psql discussion](https://lists.gnu.org/archive/html/bug-readline/2019-12/msg00005.html),
  Chet explained duplicated lines after maximize/restore: the terminal had
  already reflowed cells, while Readline independently performed a full
  redisplay without knowing what the terminal had done.

Those reports explain why similar visual artifacts may have been noticed for
years. They are background, not evidence that this Readline 8.3 regression was
present in earlier releases.

## Terminal-side policy

xterm+ should continue to maintain an exact terminal cursor, resize the PTY
and libghostty state as one ordered event-loop transaction, and execute child
output literally. It should not generally guess that a cursor movement from
an interactive application is erroneous.

xterm+ must not work around this regression. OSC 133 prompt knowledge is
insufficient because the SGR-only case fails too, and an emulator cannot
safely discard arbitrary cursor control emitted after resize. The correct fix
is the upstream Readline prompt-metadata reset.
