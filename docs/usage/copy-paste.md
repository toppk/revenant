# Copy and paste on X11

Copy and paste in xterm comes from X11, not from the terminal protocol. X11
has several named selections, and they do not behave like the single clipboard
most people know from other desktops.

## Default behavior at a glance

xterm+ follows xterm's traditional default: `selectToClipboard` is `false`,
so xterm's symbolic `SELECT` source means `PRIMARY`.

| Operation | What xterm uses by default |
| --- | --- |
| Select with Button 1 | `PRIMARY`, with `CUT_BUFFER0` as a legacy copy |
| Extend with Button 3 | The same selection |
| Paste with Button 2 | `PRIMARY`, falling back to `CUT_BUFFER0` |
| Paste with Shift+Insert | `PRIMARY`, falling back to `CUT_BUFFER0` |

The paste actions are not permanently bound to `PRIMARY`. Both Button 2 and
Shift+Insert request `SELECT`, so changing `selectToClipboard` changes the
copy and paste sides together:

| `selectToClipboard` | Button-1 selection publishes to | Button 2 and Shift+Insert paste from |
| --- | --- | --- |
| `false` (default) | `PRIMARY`, plus `CUT_BUFFER0` | `PRIMARY`, then `CUT_BUFFER0` |
| `true` | `CLIPBOARD`, plus `CUT_BUFFER0` | `CLIPBOARD`, then `CUT_BUFFER0` |

There is no default binding that explicitly pastes `CLIPBOARD` while leaving
ordinary selection on `PRIMARY`. xterm also does not bind Ctrl+Shift+C or
Ctrl+Shift+V by default. A user who wants both channels available at once must
add an explicit `insert-selection(CLIPBOARD)` translation; an explicit atom
name bypasses `selectToClipboard`.

Shift+Insert is paste, not copy, in the default bindings.

This page describes upstream xterm patch 410, which is xterm+'s compatibility
baseline, and then identifies the parts xterm+ implements today.

## The X11 mental model

An X selection is usually not a bucket of bytes stored by the X server. It is
a name, called an *atom*, with a current owner:

1. An application claims ownership of a selection.
2. Another application asks the X server for that selection in a particular
   data format.
3. The server forwards the request to the owner.
4. The owner converts and returns the data.

Consequently, selecting text and highlighting text are related but different.
A terminal may retain the copied text after its highlight is erased, or show a
highlight only while it still owns the corresponding selection. Another
application can take ownership at any time.

A clipboard manager can request and retain `CLIPBOARD` data, which is why it
may survive after the source application exits. That persistence is a service
provided by the manager; it is not inherent in the selection atom.

### PRIMARY

`PRIMARY` is X11's direct-selection channel. Selecting text normally claims
it; Button 2 normally pastes it. There is no separate copy command in the
usual workflow because completing the selection is the copy operation.

Making a new selection in another application replaces the previous
`PRIMARY` owner. This is why selecting text while preparing to paste can
replace the text you intended to paste.

### CLIPBOARD

`CLIPBOARD` is intended for an explicit copy-then-paste workflow. Applications
commonly connect it to Copy and Paste menu items or Ctrl+C/Ctrl+V-style
bindings. xterm supports it, but does not use it in its default translations.

xterm's `selectToClipboard` resource changes what its private `SELECT` token
means. It does not create a second copy:

```xrdb
XTerm*selectToClipboard: true
```

With the default value, `false`, `SELECT` resolves to `PRIMARY`. With `true`,
it resolves to `CLIPBOARD`. The **Select to Clipboard** VT menu item and the
`set-select(on|off|toggle)` action can change the same policy at runtime.
Because the default copy and paste actions both name `SELECT`, this toggle
changes both directions. It is a mode switch, not an additional CLIPBOARD
shortcut.

### SECONDARY

`SECONDARY` is a standard but rarely used selection. The ICCCM describes it
for operations that need a second argument or need temporary data without
disturbing `PRIMARY`. xterm can name it in custom translations, but none of
the normal VT bindings use it.

### CUT_BUFFER0 through CUT_BUFFER7

Cut buffers predate the selection-owner protocol. They are properties on the
X root window, so the bytes are actually retained by the X server. Their
official interchange encoding is ISO-8859-1, they are size-limited in
practice, and Unicode text may be lost or substituted.

xterm's default actions name both `SELECT` and `CUT_BUFFER0`. The selection is
the preferred, encoding-aware path; the cut buffer is a persistent legacy
fallback. Cut buffers should not be mistaken for eight modern clipboards.

### SELECT is an xterm name

`SELECT` is not a fourth X11 selection. It is an xterm translation token that
resolves to `PRIMARY` or `CLIPBOARD` according to `selectToClipboard`.

That indirection lets the same default bindings serve either policy:

```text
select-end(SELECT, CUT_BUFFER0)
insert-selection(SELECT, CUT_BUFFER0)
```

The arguments are tried or populated in order. Explicit names such as
`PRIMARY`, `CLIPBOARD`, `SECONDARY`, and `CUT_BUFFER1` bypass `SELECT` and can
be used in custom translations.

## xterm's default gestures

| Gesture | xterm action and behavior |
| --- | --- |
| Button 1 press and drag | Starts and extends a character selection. Repeated clicks select a word, then a line. |
| Button 1 release | Runs `select-end(SELECT, CUT_BUFFER0)`, claiming `SELECT` and copying a legacy fallback. |
| Button 3 | Extends the existing selection. Its unit follows how that selection began: character, word, or line. |
| Button 2 release | Runs `insert-selection(SELECT, CUT_BUFFER0)`. |
| Shift+Insert | Runs `insert-selection(SELECT, CUT_BUFFER0)`. |
| Meta+Button 1 | Starts rectangular selection. |
| Shift+Select key | Starts and ends a keyboard-driven selection. |

When a terminal application has enabled mouse reporting, xterm normally
sends mouse events to the application. Shift overrides that reporting so the
user can still select and paste locally. Ctrl+buttons 1, 2, and 3 remain the
xterm popup-menu gestures.

`insert-selection` can accept several sources and tries them in order until
one succeeds. `select-end` finishes a selection and publishes it to each
named target. `copy-selection` republishes an existing highlighted selection
without changing its endpoints.

Ctrl+Shift+C and Ctrl+Shift+V appear in the xterm manual as an example of
*custom* Xt translations, not as defaults. That example also rebinds
Shift+Insert from paste to copy, so it should not be copied blindly by someone
who expects the traditional Shift+Insert behavior.

## Text formats and Unicode

The source owner and receiving application negotiate a *selection target*.
This target describes the returned representation; it is distinct from the
selection atom such as `PRIMARY`.

xterm in wide-character mode requests these text targets in order:

1. `UTF8_STRING`
2. `TEXT`
3. `COMPOUND_TEXT`
4. `STRING`

`TEXT` and `COMPOUND_TEXT` are included when `i18nSelections` is true, which
is the default. `STRING` means ISO-8859-1. The `utf8SelectTypes` and
`eightBitSelectTypes` resources can change the request order.

xterm copies characters from its terminal cells, not the original byte stream
that produced those cells. Base and combining characters stored in a cell are
therefore selection data; control sequences and the original input encoding
are not.

## Selection and paste policy in xterm

Several xterm resources affect what is copied or pasted:

| Resource | xterm default | Effect |
| --- | --- | --- |
| `selectToClipboard` | `false` | Maps `SELECT` to `PRIMARY`; `true` maps it to `CLIPBOARD`. |
| `keepSelection` | `true` | Retains owned selection data after terminal output invalidates the highlight. It cannot stop another client taking ownership. |
| `keepClipboard` | `false` | Reuses xterm's saved CLIPBOARD data in a specialized ownership case; this is not ordinary clipboard persistence. |
| `cutNewline` | `true` | Includes the newline in a triple-click line selection. |
| `trimSelection` | `false` | Removes trailing spaces from selected rows when enabled. |
| `i18nSelections` | `true` | Enables `TEXT` and `COMPOUND_TEXT` negotiation. |
| `allowPasteControls` | `false` | Suppresses most non-formatting control characters unless explicitly allowed. |
| `disallowedPasteControls` | `BS,DEL,ENQ,EOT,ESC,NUL,STTY` | Selects controls that are replaced when paste controls are not allowed. |

An application inside the terminal can enable bracketed paste mode. In that
mode, the terminal wraps pasted text in begin/end markers so the application
can distinguish a paste from typing. This is independent of whether the X11
source was `PRIMARY` or `CLIPBOARD`.

## What xterm+ supports today

xterm+ deliberately exposes the traditional workflow first:

| Capability | Status |
| --- | --- |
| Button-1 character, word, line, rectangular, and deep-scrollback selection | Supported |
| Button-3 extension using the original selection unit | Supported |
| Publish selected text to `PRIMARY`, `CLIPBOARD`, `SECONDARY`, or another named selection | Supported |
| Resolve `SELECT` through `selectToClipboard` (resource, menu, or `set-select` action) | Supported |
| Button-2 and Shift+Insert ordered paste from named action arguments | Supported |
| Request `UTF8_STRING`, then `STRING` | Supported |
| Offer `TARGETS`, `TIMESTAMP`, `UTF8_STRING`, `TEXT`, and `STRING` to requesters | Supported |
| Read `CUT_BUFFER0` through `CUT_BUFFER7` as ordered fallbacks | Supported |
| Send paste through libghostty's control filtering and bracketed-paste encoder | Supported |
| Publish new selections to `CUT_BUFFER0` through `CUT_BUFFER7` | Supported |
| Honor arbitrary selection names in Xt action parameters | Supported |
| `TEXT` and `COMPOUND_TEXT` requests | Not yet |
| `copy-selection` and keyboard-driven Shift+Select | Not yet |
| Remaining xterm selection and paste-policy resources | Not yet |

The default remains the traditional `PRIMARY`-first workflow because
`selectToClipboard` defaults to false. Set the resource to true, toggle the
**Select to Clipboard** VT-menu item, or invoke `set-select(on)` to make the
same default translations use `CLIPBOARD` instead. Explicit action arguments
bypass that policy, and cut buffers are converted between UTF-8 terminal text
and their ISO-8859-1 wire representation, substituting `?` for characters the
legacy encoding cannot represent.

For example, this launch selects and pastes through `CLIPBOARD` for that
xterm+ process:

```sh
xterm+ -xrm 'XTerm*selectToClipboard:true'
```

Returning the VT-menu item to off, invoking `set-select(off)`, or using the
default resource value restores the normal `PRIMARY` behavior.

The remaining work is tracked under selection, copy, and paste in the project
roadmap. The compatibility goal is to implement xterm's named-source action
model rather than add a separate set of terminal-specific clipboard rules.
