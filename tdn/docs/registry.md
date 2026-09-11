# Maintaining the feature database

Each terminal owns one file: `tdn/terminals/<terminal-id>.yaml`. The build
discovers these files automatically; there is no central terminal list to edit.
The file's `features` mapping is keyed by stable feature ID:

```yaml
schema_version: 1
terminal: example-terminal
name: Example Terminal
features:
  osc-8-hyperlinks:
    status: supported
    as-of: "1.2.0"
    notes: "HTTP and HTTPS targets; requires modifier-click."
    configuration: "Default profile"
    review: reviewed
    checked: "2026-09-10"
    evidence:
      - kind: observation
        url: https://example.com/tests/osc-8/1.2.0
```

This is an example shape, not a support claim about a real terminal.
An optional `page: terminals/example-terminal.md` attaches records to an
existing profile. Without `page`, MkDocs generates the profile. An empty
`features: {}` registers a terminal without making any support claims.

## Record meanings

| Field | Meaning |
| --- | --- |
| `status` | `supported`, `partial`, `unsupported`, or `unknown` |
| `as-of` | Quoted assessed version or `git:<revision>`; `null` for unversioned imports |
| `notes` | Scope, exceptions, and limitations, as plain text |
| `configuration` | Optional build options, resources, permissions, or host environment |
| `review` | `reviewed`, `imported`, or `conflicting` |
| `checked` | Assessment date, required for reviewed records |
| `evidence` | One or more HTTP(S) source links, each with a `kind` |
| `history` | Optional list of earlier records with the same fields, excluding nested history |

Evidence kinds are `specification`, `source`, `test`, `observation`, and
`import`. A reviewed record requires a version, date, and evidence beyond
an import. A test's mere existence is not proof of support: inspect its
assertions and scope, and record the tested revision and relevant settings.

`as-of` is **not** “supported since.” Do not infer a first-supported version
from the oldest version tested. When updating a record, copy the previous
assessment into `history` if it describes a different version or configuration.
The current record is displayed in comparisons; history remains available on
the feature page and in JSON. A development commit is not a released version.

Missing features mean unknown. A default-denied permission is separate from
implementation support; describe it in `configuration` and `notes`. An
unanswered query alone does not prove a feature is unsupported.

## Shared definitions

`tdn/data/features.yaml` defines the features, independently of terminal
claims. Each entry has a human title, kind (`feature` or `group`), category,
related specification pages, specification IDs, and policy labels:

```yaml
schema_version: 1
features:
  osc-8-hyperlinks:
    title: OSC 8 hyperlinks
    kind: feature
    category: osc
    pages:
      - osc/hyperlinks.md
    specifications:
      - osc-8-hyperlinks-spec
    labels: []
    sequence: "OSC 8 ; params ; URI ST"
```

Every referenced Markdown page contains `<!-- tdn:compatibility -->` where
the shared table should appear, normally at the bottom. It is replaced at
build time; generated HTML is never edited by hand. Each feature also gets
its own permanent URL, such as `features/osc-8-hyperlinks/`.

Specification IDs resolve through `tdn/data/specifications.yaml` to a named
source document and URL. They are independent of feature IDs: one document
can describe many features. Prefer section anchors where available; use the
feature's sequence and linked TDN explanation to locate its exact behavior.
References include vendor documentation for behaviors without a formal standard.

Use lowercase ASCII words and digits separated by hyphens. Include the
namespace and a meaningful name: `osc-8-hyperlinks`,
`dec-mode-2027-grapheme-clusters`, `sgr-58-underline-color`, or
`text-color-emoji`. Do not assign a fictitious protocol number to a rendering
behavior. Disambiguate collisions with vendor or behavior names, as with
`osc-6-apple-document` and `osc-6-xterm-special-color`.

Once published, keep IDs stable even when titles or page locations change.
Do not repurpose an ID for a different behavior. Register independently
testable subsets separately. Legacy aggregate assessments are explicitly
`group` records; their support is never inherited by individual operations.

Policy labels are `window-ops`, `title-ops`, `color-ops`, `font-ops`,
`tcap-ops`, and `mouse-ops`. Apply them to the affected feature, not every
feature on its page. `policy_note` explains mixed groups, such as title
push/pop where Title Ops only governs applying saved labels on pop.

## Pending implementation work

Register features before dispatching their implementation. Revenant's tracked
dispatch guide maps local work IDs (such as S2) to these stable feature IDs;
its handoff maps the remaining compatibility gaps and future designs. One
chunk can implement several features, and a feature can need several chunks.
Keep that work mapping separate from each terminal's evidence-based support
records. Pending, optional and blocked work does not imply unsupported, and
creating a record does not imply implementation.

Use [user-action design scopes](practices/user-actions.md) for behavior with
no wire protocol, and [Ops policy scopes](policies/ops.md) for permission
behavior. Internal cleanup and release checks do not need feature IDs.

## Migration and evidence

The initial import preserves the old TDN compatibility cells, notes, and
links to their exact source revision. Those records are **imported and
unverified**, not current measurements. Release strings in the old notes
remain notes; they are not silently promoted to assessed versions. Where
overlapping tables disagreed, the status is unknown and both claims remain
visible. Resolve these by reviewing evidence, not by choosing the more
optimistic cell.

Revenant has its own records and version evidence. It does not inherit
support from Ghostty or upstream xterm. The initial records cover a small
set of reviewed changes; other implementation assessments remain follow-up
work. Adding an identifier is not an assertion of support.

## Validate and preview

From the repository root, after installing `tdn/requirements.txt`:

```sh
python3 tdn/hooks/registry.py
python3 -m unittest discover -s tdn/tests
mkdocs build --strict -f tdn/mkdocs.yml
mkdocs serve -f tdn/mkdocs.yml
```

The build checks duplicate YAML keys, IDs, feature and specification references,
page paths, record fields, version types, evidence URLs, and reviewed-record
metadata. YAML files are watched during `serve`; changes update every view.
Restart `serve` after changing hook Python code, as required by
[MkDocs hooks](https://www.mkdocs.org/user-guide/configuration/#hooks).

The same build produces the comparison page, feature pages, profile tables,
and `assets/compatibility.json`. For scripts without MkDocs:

```sh
python3 tdn/hooks/registry.py --json > compatibility.json
```

All compatibility data and documentation are CC BY-SA 4.0. Build hooks and
tests, like the probe tools, are MIT licensed.
