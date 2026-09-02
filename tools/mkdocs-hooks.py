"""mkdocs hooks for the xman-style theme: whatis table and cross-reference links."""

import datetime
import re
import subprocess
import sys
from pathlib import Path

from mkdocs.utils import get_relative_url
from mkdocs.utils.meta import get_data

XREF = re.compile(r"\b([a-z][a-z0-9-]*)\((1|[5-8]|tdn)\)")
PROTECTED = re.compile(r"(<pre\b.*?</pre>|<a\b.*?</a>|<code\b.*?</code>)", re.S)


def on_config(config):
    root = Path(config.config_file_path).resolve().parent
    subprocess.run([sys.executable, str(root / "tools/generate-doc-fragments"), str(root / ".cache/fragments")], check=True)
    if "tdn_url" not in config.extra:
        raise ValueError("mkdocs.yml: extra.tdn_url is required")
    config.extra["build_date"] = datetime.date.today().strftime("%B %d, %Y")
    return config


def on_files(files, config):
    whatis = []
    for file in files.documentation_pages():
        with open(file.abs_src_path, encoding="utf-8") as source:
            _, meta = get_data(source.read())
        if "man" not in meta:
            raise ValueError(f"{file.src_path}: front matter needs man, section, manual, description")
        whatis.append(
            {
                "man": meta["man"],
                "section": str(meta.get("section", 7)),
                "manual": meta["manual"],
                "description": meta.get("description", ""),
                "url": file.url,
            }
        )
    whatis.sort(key=lambda entry: (entry["manual"], entry["man"]))
    config.extra["whatis"] = whatis
    config.extra["whatis_by_name"] = {entry["man"]: entry for entry in whatis}
    return files


def on_page_content(html, page, config, files):
    """Turn name(section) in prose into links, the way mandoc -T html does."""
    table = config.extra["whatis_by_name"]
    tdn_url = config.extra["tdn_url"]

    def link(match):
        name, section = match.group(1), match.group(2)
        if section == "tdn":
            return f'<a class="xr ext" href="{tdn_url}" target="_blank" rel="noopener">{name}(tdn)</a>'
        entry = table.get(name)
        if entry is None or entry["section"] != section or entry["url"] == page.url:
            return match.group(0)
        href = get_relative_url(entry["url"], page.url)
        return f'<a class="xr" href="{href}">{name}({section})</a>'

    parts = PROTECTED.split(html)
    return "".join(part if index % 2 else XREF.sub(link, part) for index, part in enumerate(parts))
