"""TDN data validation. No MkDocs dependency; usable by editors and CI."""

from __future__ import annotations

import argparse
import datetime
import json
import re
from pathlib import Path
from urllib.parse import urlsplit

import yaml

SLUG = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
STATUSES = {"supported", "partial", "unsupported", "unknown"}
REVIEWS = {"imported", "reviewed", "conflicting"}
LABELS = {"window-ops", "title-ops", "color-ops", "font-ops", "tcap-ops", "mouse-ops"}
MARKER = "<!-- tdn:compatibility -->"


class RegistryError(ValueError):
    pass


class UniqueLoader(yaml.SafeLoader):
    """YAML duplicate keys must fail instead of silently replacing a claim."""


def unique_mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if not isinstance(key, str):
            raise RegistryError(
                f"line {key_node.start_mark.line + 1}: mapping keys must be strings"
            )
        if key in result:
            raise RegistryError(
                f"line {key_node.start_mark.line + 1}: duplicate key {key!r}"
            )
        result[key] = loader.construct_object(value_node, deep=deep)
    return result


UniqueLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping
)


def require(condition, message):
    if not condition:
        raise RegistryError(message)


def fields(value, required, optional, where):
    require(isinstance(value, dict), f"{where}: expected a mapping")
    require(
        not (required - value.keys()),
        f"{where}: missing {sorted(required - value.keys())}",
    )
    require(
        not (value.keys() - required - optional),
        f"{where}: unknown fields {sorted(value.keys() - required - optional)}",
    )


def read_yaml(path):
    try:
        data = yaml.load(path.read_text(encoding="utf-8"), Loader=UniqueLoader)
    except (yaml.YAMLError, RegistryError) as error:
        raise RegistryError(f"{path}: {error}") from error
    require(
        isinstance(data, dict)
        and type(data.get("schema_version")) is int
        and data["schema_version"] == 1,
        f"{path}: schema_version must be 1",
    )
    return data


def text(value, where):
    require(
        isinstance(value, str) and bool(value.strip()),
        f"{where}: expected nonempty text",
    )


def url(value, where):
    text(value, where)
    parsed = urlsplit(value)
    require(
        parsed.scheme in {"https", "http"} and bool(parsed.netloc),
        f"{where}: expected an HTTP(S) URL",
    )


def string_list(value, where):
    require(
        isinstance(value, list) and all(isinstance(x, str) for x in value),
        f"{where}: expected a list of strings",
    )
    require(len(set(value)) == len(value), f"{where}: duplicate entries")


class Registry:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.features = self.catalog("features")
        self.specifications = self.catalog("specifications")
        self.terminals = {}
        self.generated_terminals = set()
        for path in sorted((self.root / "terminals").glob("*.yaml")):
            terminal = read_yaml(path)
            fields(
                terminal,
                {"schema_version", "terminal", "name", "features"},
                {"page"},
                str(path),
            )
            tid = terminal["terminal"]
            require(
                isinstance(tid, str) and SLUG.fullmatch(tid) and tid == path.stem,
                f"{path}: terminal must match filename slug",
            )
            require(tid not in self.terminals, f"duplicate terminal {tid}")
            text(terminal["name"], f"{tid}.name")
            if "page" in terminal:
                self.page(terminal["page"])
            else:
                terminal["page"] = f"terminals/{tid}.md"
                self.generated_terminals.add(tid)
            self.terminals[tid] = terminal
        require(bool(self.terminals), "no terminal files found")
        self.validate()

    def catalog(self, name):
        data = read_yaml(self.root / "data" / f"{name}.yaml")
        fields(data, {"schema_version", name}, set(), name)
        require(
            isinstance(data[name], dict) and bool(data[name]),
            f"{name}: expected a nonempty mapping",
        )
        for key in data[name]:
            require(bool(SLUG.fullmatch(key)), f"{name}: invalid ID {key!r}")
        return data[name]

    def page(self, value):
        text(value, "page")
        path = (self.root / "docs" / value).resolve()
        require(
            path.is_relative_to(self.root / "docs")
            and path.suffix == ".md"
            and path.is_file(),
            f"missing/invalid page {value}",
        )

    def validate(self):
        for sid, spec in self.specifications.items():
            fields(spec, {"title", "url"}, set(), sid)
            text(spec["title"], sid)
            url(spec["url"], sid)
        for fid, feature in self.features.items():
            fields(
                feature,
                {"title", "kind", "category", "pages", "specifications", "labels"},
                {"sequence", "policy_note"},
                fid,
            )
            text(feature["title"], fid)
            text(feature["category"], fid)
            require(feature["kind"] in {"feature", "group"}, f"{fid}: invalid kind")
            for key in ("pages", "specifications", "labels"):
                string_list(feature[key], f"{fid}.{key}")
            require(bool(feature["pages"]), f"{fid}: needs a specification page")
            require(
                bool(feature["specifications"]),
                f"{fid}: needs a specification reference",
            )
            for page in feature["pages"]:
                self.page(page)
                require(
                    MARKER in (self.root / "docs" / page).read_text(),
                    f"{fid}: {page} missing compatibility marker",
                )
            for spec in feature["specifications"]:
                require(
                    spec in self.specifications, f"{fid}: unknown specification {spec}"
                )
            require(set(feature["labels"]) <= LABELS, f"{fid}: unknown policy label")
            for key in ("sequence", "policy_note"):
                if key in feature:
                    text(feature[key], f"{fid}.{key}")
        for tid, terminal in self.terminals.items():
            require(
                isinstance(terminal["features"], dict),
                f"{tid}.features: expected a mapping",
            )
            for fid, record in terminal["features"].items():
                require(fid in self.features, f"{tid}: unknown feature {fid}")
                self.record(record, f"{tid}/{fid}")
        # A marker must refer to real features; an accidental page rename fails the build.
        for page in (self.root / "docs").rglob("*.md"):
            if MARKER in page.read_text().splitlines():
                key = page.relative_to(self.root / "docs").as_posix()
                require(
                    bool(self.for_page(key)),
                    f"{key}: compatibility marker has no features",
                )

    def record(self, record, where):
        fields(
            record,
            {"status", "as-of", "notes", "review", "evidence"},
            {"checked", "configuration", "history"},
            where,
        )
        require(record["status"] in STATUSES, f"{where}: invalid status")
        require(record["review"] in REVIEWS, f"{where}: invalid review")
        require(
            record["as-of"] is None or isinstance(record["as-of"], str),
            f"{where}: quote the as-of version",
        )
        require(isinstance(record["notes"], str), f"{where}: notes must be text")
        require(
            isinstance(record["evidence"], list) and bool(record["evidence"]),
            f"{where}: needs evidence",
        )
        for evidence in record["evidence"]:
            fields(evidence, {"kind", "url"}, {"description"}, where)
            require(
                evidence["kind"]
                in {"import", "specification", "source", "test", "observation"},
                f"{where}: invalid evidence kind",
            )
            url(evidence["url"], where)
        if record["review"] == "reviewed":
            text(record["as-of"], f"{where}.as-of")
            require("checked" in record, f"{where}: reviewed records need checked date")
            require(
                any(e["kind"] != "import" for e in record["evidence"]),
                f"{where}: import alone is not reviewed evidence",
            )
        if record["review"] == "conflicting":
            require(
                record["status"] == "unknown",
                f"{where}: conflicting claims must be unknown",
            )
        if "checked" in record:
            try:
                datetime.date.fromisoformat(str(record["checked"]))
            except ValueError as error:
                raise RegistryError(f"{where}: invalid checked date") from error
            record["checked"] = str(record["checked"])
        if "configuration" in record:
            text(record["configuration"], f"{where}.configuration")
        if "history" in record:
            require(
                isinstance(record["history"], list), f"{where}: history must be a list"
            )
            for old in record["history"]:
                require(
                    isinstance(old, dict) and "history" not in old,
                    f"{where}: nested history is not allowed",
                )
                self.record(old, where + "/history")

    def for_page(self, page):
        return [
            fid for fid, feature in self.features.items() if page in feature["pages"]
        ]

    def export(self):
        return {
            "schema_version": 1,
            "features": self.features,
            "specifications": self.specifications,
            "terminals": self.terminals,
        }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="write the combined validated database to stdout",
    )
    args = parser.parse_args()
    try:
        registry = Registry(args.root)
    except RegistryError as error:
        parser.exit(1, f"TDN registry: {error}\n")
    if args.json:
        print(json.dumps(registry.export(), ensure_ascii=False, indent=2))
    else:
        print(
            f"TDN registry: {len(registry.features)} features, {len(registry.terminals)} terminals; valid"
        )
