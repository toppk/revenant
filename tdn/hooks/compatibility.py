"""Build all compatibility views from independent terminal YAML files."""

from __future__ import annotations

from html import escape
import json
from pathlib import Path

from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File
from mkdocs.utils import get_relative_url
from registry import Registry, RegistryError, MARKER
from probe_catalog import catalog

_registry = None
_probe_features = {}
LABEL = {
    "supported": "Supported",
    "partial": "Partial",
    "unsupported": "Unsupported",
    "unknown": "Unknown",
}
DEFAULT_TERMINALS = {"xterm", "revenant", "ghostty", "kitty", "vte"}


def on_files(files, config):
    global _registry, _probe_features
    try:
        _registry = Registry(Path(config.config_file_path).parent)
        _probe_features = {
            f["id"]: f for f in catalog(Path(config.config_file_path).parent.parent)
        }
    except RegistryError as error:
        raise PluginError(f"TDN registry: {error}") from error
    for fid, feature in _registry.features.items():
        path = f"features/{fid}.md"
        if files.get_file_from_path(path):
            raise PluginError(f"TDN feature URL collides with source page: {path}")
        files.append(
            File.generated(
                config,
                path,
                content=f"# {escape(feature['title'])}\n\n<!-- tdn:feature {fid} -->\n",
            )
        )
    for tid, terminal in _registry.terminals.items():
        if tid in _registry.generated_terminals:
            terminal["page"] = f"terminals/{tid}.md"
            if files.get_file_from_path(terminal["page"]):
                raise PluginError(f"TDN terminal URL collision: {tid}")
            files.append(
                File.generated(
                    config,
                    terminal["page"],
                    content=f"# {escape(terminal['name'])}\n\nCompatibility records from `{tid}.yaml`.\n",
                )
            )
    files.append(
        File.generated(
            config,
            "assets/compatibility.json",
            content=json.dumps(_registry.export(), ensure_ascii=False, indent=2) + "\n",
        )
    )
    return files


def href(path, page, files):
    file = files.get_file_from_path(path)
    if file is None:
        raise PluginError(f"TDN link points to missing page {path}")
    return escape(get_relative_url(file.url, page.url), quote=True)


def feature_link(fid, page, files):
    feature = _registry.features[fid]
    return f'<a href="{href(f"features/{fid}.md", page, files)}">{escape(feature["title"])}</a><br><code>{fid}</code>'


def terminal_link(tid, page, files):
    terminal = _registry.terminals[tid]
    return f'<a href="{href(terminal["page"], page, files)}">{escape(terminal["name"])}</a>'


def badge(record):
    status = record.get("status", "unknown")
    review = record.get("review")
    qualifier = {
        "imported": "Imported · unverified",
        "conflicting": "Conflicting sources",
    }.get(review, "")
    return f'<span class="tdn-status tdn-{status}">{LABEL[status]}</span>' + (
        f"<small>{qualifier}</small>" if qualifier else ""
    )


def evidence_html(record):
    return " ".join(
        f'<a href="{escape(e["url"], quote=True)}">{escape(e["kind"])}</a>'
        for e in record.get("evidence", [])
    )


def record_details(record):
    if not record:
        return "No assessment recorded."
    # Old Markdown is shown as text: no executable HTML or page-relative links leak into other pages.
    notes = escape(record["notes"])
    configuration = escape(record.get("configuration", ""))
    checked = (
        f" Checked {escape(str(record['checked']))}." if record.get("checked") else ""
    )
    return f"{notes}<p>{configuration}{checked}</p><p>{evidence_html(record)}</p>"


def matrix(ids, page, files):
    if not ids:
        return ""
    terminals = list(_registry.terminals)
    controls = '<div class="tdn-controls" hidden><label>Find a feature <input type="search" data-tdn-search placeholder="Name, ID, sequence, or label"></label>'
    controls += '<label>Policy label <select data-tdn-label><option value="">All labels</option>'
    for label in sorted(
        {label for fid in ids for label in _registry.features[fid]["labels"]}
    ):
        controls += f'<option value="{label}">{escape(label)}</option>'
    controls += '</select></label><label><input type="checkbox" data-tdn-differences> Only differences between selected terminals</label>'
    controls += "<details><summary>Choose terminals</summary><fieldset><legend>Terminal columns</legend>"
    for tid in terminals:
        checked = " checked" if tid in DEFAULT_TERMINALS else ""
        controls += f'<label><input type="checkbox" data-tdn-terminal="{tid}"{checked}> {escape(_registry.terminals[tid]["name"])}</label>'
    controls += '</fieldset><button type="button" data-tdn-all>Show all terminals</button></details><p role="status" aria-live="polite" data-tdn-count></p></div>'
    out = [
        '<div class="tdn-comparison">',
        controls,
        '<div class="tdn-scroll" tabindex="0" role="region" aria-label="Terminal compatibility"><table><caption>Feature support by terminal. Imported claims have not been reverified; unknown is not unsupported.</caption><thead><tr><th scope="col">Feature / stable ID</th>',
    ]
    for tid in terminals:
        out.append(
            f'<th scope="col" data-terminal="{tid}">{terminal_link(tid, page, files)}</th>'
        )
    out.append("</tr></thead><tbody>")
    for fid in ids:
        feature = _registry.features[fid]
        search = " ".join(
            [fid, feature["title"], feature.get("sequence", ""), *feature["labels"]]
        ).lower()
        out.append(
            f'<tr data-feature="{fid}" data-search="{escape(search, quote=True)}" data-labels="{" ".join(feature["labels"])}"><th scope="row">{feature_link(fid, page, files)}</th>'
        )
        for tid in terminals:
            record = _registry.terminals[tid]["features"].get(fid, {})
            title = escape(record.get("notes", "No assessment recorded."), quote=True)
            status = record.get("status", "unknown")
            review = record.get("review", "unknown")
            out.append(
                f'<td data-terminal="{tid}" data-support="{status}:{review}" title="{title}">{badge(record)}</td>'
            )
        out.append("</tr>")
    out.append("</tbody></table></div></div>")
    return "\n".join(out)


def feature_detail(fid, page, files):
    feature = _registry.features[fid]
    out = [f"Feature ID: `{fid}`. Type: **{feature['kind']}**."]
    if fid in _probe_features:
        trail = " / ".join(_probe_features[fid]["breadcrumb"])
        out.append(
            f"Manual tests: `just probe {fid}` opens this feature's scenarios.\n\nBrowse under **{escape(trail)}**. A scenario may exercise related features; its existence does not establish support."
        )
    if feature["kind"] == "group":
        out.append(
            "This is a legacy aggregate assessment. It does not establish support for each individual operation; assess those feature IDs separately."
        )
    if feature.get("sequence"):
        out.append(f"<p>Sequence: <code>{escape(feature['sequence'])}</code></p>")
    if feature["labels"]:
        out.append(
            "Policy labels: "
            + ", ".join(f"`{label}`" for label in feature["labels"])
            + ". "
            + feature.get("policy_note", "")
        )
    out += ["## Specification and related documentation", "<ul>"]
    for path in feature["pages"]:
        out.append(f'<li><a href="{href(path, page, files)}">{escape(path)}</a></li>')
    for sid in feature["specifications"]:
        spec = _registry.specifications[sid]
        out.append(
            f'<li><a href="{escape(spec["url"], quote=True)}">{escape(spec["title"])}</a> <code>{sid}</code></li>'
        )
    out += [
        "</ul>",
        "## Terminal compatibility",
        "Missing records mean **unknown**. `as-of` identifies the assessed version or revision, not the first release to support a feature.",
        '<div class="tdn-scroll"><table><thead><tr><th scope="col">Terminal</th><th scope="col">Support</th><th scope="col">As of</th><th scope="col">Notes and evidence</th></tr></thead><tbody>',
    ]
    for tid, terminal in _registry.terminals.items():
        record = terminal["features"].get(fid, {})
        version = escape(record.get("as-of") or "Unversioned")
        detail = record_details(record)
        if record.get("history"):
            detail += "<details><summary>Earlier assessments</summary>"
            for old in record["history"]:
                detail += f"<p>{escape(old.get('as-of') or 'Unversioned')}: {badge(old)}</p>{record_details(old)}"
            detail += "</details>"
        out.append(
            f'<tr><th scope="row">{terminal_link(tid, page, files)}</th><td>{badge(record)}</td><td><code>{version}</code></td><td>{detail}</td></tr>'
        )
    out += ["</tbody></table></div>"]
    return "\n\n".join(out)


def terminal_detail(tid, page, files):
    terminal = _registry.terminals[tid]
    out = [
        "## Feature support",
        f"Records are maintained independently in `tdn/terminals/{tid}.yaml`. Unlisted features are unknown. See the [comparison](../comparison.md) for all features.",
        '<div class="tdn-scroll"><table><thead><tr><th scope="col">Feature</th><th scope="col">Support</th><th scope="col">As of</th><th scope="col">Notes and evidence</th></tr></thead><tbody>',
    ]
    for fid, record in terminal["features"].items():
        out.append(
            f'<tr><th scope="row">{feature_link(fid, page, files)}</th><td>{badge(record)}</td><td>{escape(record.get("as-of") or "Unversioned")}</td><td>{record_details(record)}</td></tr>'
        )
    out.append("</tbody></table></div>")
    return "\n\n".join(out)


def on_page_markdown(markdown, page, config, files):
    path = page.file.src_uri
    if path.startswith("features/") and path != "features/index.md":
        fid = Path(path).stem
        return markdown.replace(
            f"<!-- tdn:feature {fid} -->", feature_detail(fid, page, files)
        )
    if "<!-- tdn:comparison -->" in markdown:
        markdown = markdown.replace(
            "<!-- tdn:comparison -->",
            '<div class="tdn-comparison-page">\n'
            + matrix(sorted(_registry.features), page, files)
            + "\n</div>",
        )
    if "<!-- tdn:feature-index -->" in markdown:
        items = ["<ul>"]
        for fid in sorted(_registry.features):
            items.append(f"<li>{feature_link(fid, page, files)}</li>")
        items.append("</ul>")
        markdown = markdown.replace("<!-- tdn:feature-index -->", "\n".join(items))
    if MARKER in markdown.splitlines():
        markdown = markdown.replace(
            MARKER,
            "## Compatibility\n\n" + matrix(_registry.for_page(path), page, files),
        )
    for tid, terminal in _registry.terminals.items():
        if path == terminal["page"]:
            markdown += "\n\n" + terminal_detail(tid, page, files)
    return markdown
