"""Generate the standalone probe's embedded TDN metadata (development only)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from registry import read_yaml, require

ROOT = Path(__file__).resolve().parents[2]


def catalog(root=ROOT):
    features = read_yaml(root / "tdn/data/features.yaml")["features"]
    nav = json.loads((root / "tdn/data/probe-navigation.json").read_text())
    require(nav.get("schema_version") == 1, "probe navigation: invalid schema")
    entries, seen, paths = [], set(), []
    for group in nav["groups"]:
        path = group["breadcrumb"]
        require(
            isinstance(path, list)
            and path
            and all(
                isinstance(p, str)
                and p.strip()
                and "/" not in p
                and all(ord(c) >= 32 for c in p)
                for p in path
            ),
            "invalid probe breadcrumb",
        )
        require(path not in paths, "duplicate probe breadcrumb")
        paths.append(path)
        for fid in group["features"]:
            require(fid in features, f"unknown probe feature: {fid}")
            require(fid not in seen, f"duplicate probe feature: {fid}")
            seen.add(fid)
            feature = features[fid]
            entries.append(
                dict(
                    id=fid,
                    title=feature["title"],
                    breadcrumb=path,
                    specifications=feature["specifications"],
                    sequence=feature.get("sequence", ""),
                )
            )
    for a in paths:
        for b in paths:
            require(
                a == b or a != b[: len(a)],
                "breadcrumb cannot be both a group and a feature list",
            )
    return entries


def rendered():
    return json.dumps(catalog(), ensure_ascii=False, indent=2) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    target = ROOT / "tools/probe/data/features.json"
    expected = rendered()
    if args.check:
        require(
            target.read_text() == expected,
            "probe feature metadata is stale; run just update-probe-registry",
        )
    else:
        target.write_text(expected)


if __name__ == "__main__":
    main()
