#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "fonttools==4.59.1",
#   "wcwidth==0.2.13",
# ]
# ///
"""
t0-facename-oracle.py - characterize stock xterm's faceName /
faceNameDoublesize list grammar. THE ORACLE IS XTERM, NOT REVENANT:
this harness never runs revenant; a separate conformance test compares
revenant against the reviewed fixture this tool records.

Canonical oracle: xterm patch 411. (The original T0 plan named 410;
the project moved to 411 on its upstream release, before any fixture
was blessed, so 411 is the first and only canonical record. Non-411
records are self-test artifacts, marked canonical=false, and --check
refuses them.)

Modes:
  --record   run all cases against the oracle xterm, write a normalized
             candidate under /tmp (never overwrites a checked-in file)
  --check    run all cases and compare against a reviewed fixture
             (e.g. compat/xterm-411-face-name.json)

Requirements (failures, never skips):
  - uv, which creates the pinned PEP 723 environment above
  - an explicit --xterm executable reporting the expected patch level
    (default 411; --expect-patch for harness self-testing only)
  - Xft support and working -report-fonts (preflighted)
  - an isolated fontconfig universe via --fontconfig (FONTCONFIG_FILE)
  - a live X DISPLAY
  - Xft DPI is pinned to 100 through the command-line resource database

Discipline:
  - all resources go on the command line via -xrm; RESOURCE_MANAGER is
    never read or mutated
  - the child publishes $WINDOWID to a file; no window manager needed
  - per-case artifact dirs are retained on failure/mismatch (and with
    --keep), removed on success
  - normalized records replace the universe font dir with $FONTS/ and
    the run dir with $T0/; raw logs are always kept in the case dir
"""

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

from fontTools.ttLib import TTFont
from wcwidth import wcwidth

HARNESS_VERSION = 4
SCHEMA = 1
PROBE_CONTRACT_VERSION = 1

# The canonical oracle is xterm patch 411 (project decision, 2026-08:
# 411 superseded 410 upstream during T0 development; fixtures recorded
# against any other patch level are marked canonical=false and must not
# be blessed into compat/). Override --expect-patch ONLY for harness
# self-testing.
CANONICAL_PATCH = 411
CANONICAL_XFT_DPI = 100

# ---------------------------------------------------------------------------
# cases: durable IDs. argv entries may reference {DV} and {CJK} which are
# replaced by the family names probed from the fontconfig universe.
# "wide": True appends a wide-char probe so lazy doublesize loading fires.
# ---------------------------------------------------------------------------

CASES = [
    dict(id="FN-01-unprefixed-single",
         xrm={"faceName": "{DV}:size=11"}),
    dict(id="FN-02-xft-then-x",
         xrm={"faceName": "xft:{DV}:size=11,x:fixed"}),
    dict(id="FN-03-x-then-xft",
         xrm={"faceName": "x:fixed,xft:{DV}:size=11"}),
    dict(id="FN-04-x11-prefix",
         xrm={"faceName": "x11:fixed,xft:{DV}:size=11"}),
    dict(id="FN-05-multi-unprefixed",
         xrm={"faceName": "{DV},{CJK}:size=11"}),
    dict(id="FN-06-bad-first-then-valid",
         xrm={"faceName": "NoSuchFontZZZQQ:size=11,{DV}:size=11"}),
    dict(id="FN-07-empty-entries-and-edge-commas",
         xrm={"faceName": ",{DV}:size=11,"}),
    dict(id="FN-08-whitespace",
         xrm={"faceName": " {DV}:size=11 , {CJK} "}),
    dict(id="FN-09-size-in-first-entry",
         xrm={"faceName": "{DV}:size=14,{CJK}"}),
    dict(id="FN-10-size-in-later-entry",
         xrm={"faceName": "{DV},{CJK}:size=14"}),
    dict(id="PREC-01-fa-before-xrm",
         fa="{DV}:size=11",
         xrm={"faceName": "{CJK}:size=11"},
         order=["fa", "xrm"]),
    dict(id="PREC-02-xrm-before-fa",
         fa="{DV}:size=11",
         xrm={"faceName": "{CJK}:size=11"},
         order=["xrm", "fa"]),
    dict(id="FD-01-doublesize-single",
         xrm={"faceName": "{DV}:size=11", "faceNameDoublesize": "{CJK}"},
         wide=True),
    dict(id="FD-02-doublesize-list-prefixed",
         xrm={"faceName": "{DV}:size=11",
              "faceNameDoublesize": "xft:{CJK},x:fixed"},
         wide=True),
    dict(id="FD-03-doublesize-bad-first",
         xrm={"faceName": "{DV}:size=11",
              "faceNameDoublesize": "NoSuchFontZZZQQ,{CJK}"},
         wide=True),
    # --- harness v3 supplement: glyph-time fallback characterization ---
    dict(id="FB-01-second-entry-glyph-fallback",
         xrm={"faceName": "{DV}:size=11,{CJK}"},
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="FB-02-bad-primary-valid-secondary-glyph-probe",
         xrm={"faceName": "NoSuchFontZZZQQ:size=11,{CJK}"},
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="FB-03-third-entry-two-entry-limit",
         xrm={"faceName": "{DV}:size=11,{CJK},{THIRD}"},
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="FB-04-fa-supplied-list",
         fa="{DV}:size=11,{CJK}",
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="FD-04-doublesize-x-first-xft-second",
         xrm={"faceName": "{DV}:size=11",
              "faceNameDoublesize": "x:fixed,xft:{CJK}"},
         wide=True),
    dict(id="FD-05-doublesize-x11-prefix",
         xrm={"faceName": "{DV}:size=11",
              "faceNameDoublesize": "x11:fixed,xft:{CJK}"},
         wide=True),
    # --- harness v4 supplement: style-specific fallback + governors ---
    dict(id="ST-01-bold-glyph-fallback",
         xrm={"faceName": "{DV}:size=11,{CJK}"},
         probe="\\033[1m\\u65e5\\u672c\\u8a9e\\033[0m"),
    dict(id="ST-02-italic-glyph-fallback",
         xrm={"faceName": "{DV}:size=11,{CJK}"},
         probe="\\033[3m\\u65e5\\u672c\\u8a9e\\033[0m"),
    dict(id="ST-03-bolditalic-glyph-fallback",
         xrm={"faceName": "{DV}:size=11,{CJK}"},
         probe="\\033[1;3m\\u65e5\\u672c\\u8a9e\\033[0m"),
    dict(id="ST-04-boldFont-xft-entry",
         xrm={"faceName": "{DV}:size=11",
              "boldFont": "xft:{DV},xft:{CJK}"},
         probe="\\033[1m\\u65e5\\u672c\\u8a9e\\033[0m"),
    dict(id="ST-05-wideBoldFont-xft-entry",
         xrm={"faceName": "{DV}:size=11",
              "faceNameDoublesize": "{DV}",
              "wideBoldFont": "xft:{DV},xft:{CJK}"},
         probe="\\033[1m\\u65e5\\u672c\\u8a9e\\033[0m"),
    dict(id="WD-01-wide-miss-vs-normal-chain",
         xrm={"faceName": "{CJK}:size=11",
              "faceNameDoublesize": "{DV}"},
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="LM-01-limitFontsets-zero",
         xrm={"faceName": "{DV}:size=11,{CJK}",
              "limitFontsets": "0"},
         probe="\\u65e5\\u672c\\u8a9e"),
    dict(id="LM-02-limitFontsets-one",
         xrm={"faceName": "{DV}:size=11,{CJK}",
              "limitFontsets": "1"},
         probe="\\u65e5\\U0001f600"),
    dict(id="LM-03-limitFontsets-two",
         xrm={"faceName": "{DV}:size=11,{CJK}",
              "limitFontsets": "2"},
         probe="\\u65e5\\U0001f600"),
    dict(id="LM-04-limitFontHeight-default-decdhl",
         xrm={"faceName": "{DV}:size=11"},
         probe="\\033[2J\\033[H\\033[?25l\\033#3DEC\\015\\012\\033#4DEC"),
    dict(id="LM-05-limitFontHeight-cap-decdhl",
         xrm={"faceName": "{DV}:size=11",
              "limitFontHeight": "51"},
         probe="\\033[2J\\033[H\\033[?25l\\033#3DEC\\015\\012\\033#4DEC"),
]

# ---------------------------------------------------------------------------

def die(msg, code=2):
    print(f"t0: FAIL: {msg}", file=sys.stderr)
    sys.exit(code)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def oracle_version(xterm):
    p = run([xterm, "-version"])
    m = re.search(r"XTerm\((\d+)\)", p.stdout + p.stderr)
    if not m:
        die(f"{xterm} did not report an XTerm(NNN) version")
    return int(m.group(1)), (p.stdout + p.stderr).strip()


def universe_families(fc_file):
    """Probe the isolated universe for one mono text family and one CJK
    (or second) family; the harness fails if the universe is empty."""
    env = dict(os.environ, FONTCONFIG_FILE=fc_file)
    p = run(["fc-list", "--format", "%{family[0]}\\n"], env=env)
    fams = [f for f in dict.fromkeys(p.stdout.splitlines()) if f.strip()]
    if not fams:
        die(f"fontconfig universe {fc_file} exposes no fonts")
    dv = next((f for f in fams if "Mono" in f and "CJK" not in f), fams[0])
    cjk = next((f for f in fams if "CJK" in f),
               fams[1] if len(fams) > 1 else fams[0])
    return dv, cjk


def family_cmap(fc_file, family):
    """Resolve one family inside the isolated universe and return its cmap.

    The oracle still records xterm's behavior.  This helper validates only
    that the controlled universe makes each question discriminating.
    """
    env = dict(os.environ, FONTCONFIG_FILE=fc_file)
    p = run(["fc-match", "--format", "%{file}\\t%{index}\\n", family],
            env=env)
    if p.returncode != 0 or not p.stdout.strip():
        die(f"probe contract: fontconfig cannot resolve family {family!r}")
    fields = p.stdout.splitlines()[0].split("\t")
    path = fields[0]
    index = int(fields[1] or 0) if len(fields) > 1 else 0
    try:
        font = TTFont(path, fontNumber=index, lazy=True)
        cmap = set((font.getBestCmap() or {}).keys())
        font.close()
    except Exception as exc:
        die(f"probe contract: cannot inspect {family!r} ({path}#{index}): "
            f"{exc}")
    return path, index, cmap


def validate_probe_contract(fc_file, dv, cjk, third, cases):
    """Fail before recording if fixture fonts cannot answer v4 questions."""
    ids = {case["id"] for case in cases}
    if not any(i.startswith(("ST-", "WD-", "LM-")) for i in ids):
        return {"validated": False, "reason": "no-v4-probe-cases-selected"}

    japanese = (0x65E5, 0x672C, 0x8A9E)
    emoji = 0x1F600
    dv_id = family_cmap(fc_file, dv)
    cjk_id = family_cmap(fc_file, cjk)
    dv_cmap, cjk_cmap = dv_id[2], cjk_id[2]
    if dv_id[:2] == cjk_id[:2]:
        die("probe contract: text and wide families resolve to one face")
    if any(cp in dv_cmap for cp in japanese):
        die(f"probe contract: text family {dv!r} covers the Japanese probe")
    if any(cp not in cjk_cmap for cp in japanese):
        die(f"probe contract: wide family {cjk!r} lacks the Japanese probe")
    if any(wcwidth(chr(cp)) != 2 for cp in japanese):
        die("probe contract: wcwidth does not classify the Japanese probe as 2")

    needs_emoji = bool(ids & {"LM-02-limitFontsets-one",
                              "LM-03-limitFontsets-two"})
    if needs_emoji:
        if not third:
            die("probe contract: LM-02/03 require a third family")
        third_id = family_cmap(fc_file, third)
        if third_id[:2] in (dv_id[:2], cjk_id[:2]):
            die("probe contract: third family resolves to a text/wide face")
        if emoji in dv_cmap or emoji in cjk_cmap:
            die("probe contract: an earlier family covers the emoji probe")
        if emoji not in third_id[2]:
            die(f"probe contract: third family {third!r} lacks U+1F600")
        if wcwidth(chr(emoji)) != 2:
            die("probe contract: wcwidth does not classify U+1F600 as 2")

    return {
        "validated": True,
        "japanese": [f"U+{cp:04X}" for cp in japanese],
        "japanese_width": 2,
        "emoji": "U+1F600" if needs_emoji else None,
        "emoji_width": 2 if needs_emoji else None,
    }


def preflight(xterm, fc_file, dv, rundir):
    """Xft + -report-fonts must work AND resolve the requested family.
    A different family winning means an external resource database is
    influencing the oracle: contaminated run, hard failure."""
    rec = run_case(xterm, fc_file,
                   dict(id="PRE-00", xrm={"faceName": f"{dv}:size=11"}),
                   dv, "", rundir, timeout=8)
    if not rec["xft_fonts"]:
        die("preflight: -report-fonts produced no 'Loaded XftFonts' blocks; "
            "oracle lacks Xft or -report-fonts (raw log in "
            f"{rundir}/PRE-00)")
    got = rec["xft_fonts"][0].get("resolved", "")
    if dv.lower() not in got.lower():
        die(f"preflight: requested '{dv}' but oracle loaded '{got}' - "
            "an external resource database (xrdb/app-defaults) is "
            "overriding the harness. CONTAMINATED. Raw log in "
            f"{rundir}/PRE-00")


# ---------------------------------------------------------------------------

BLOCK_RE = re.compile(r"^Loaded XftFonts\(([^)]*)\)", re.M)


def parse_report_fonts(text):
    """Extract per-slot blocks: label, resolved pattern line, style,
    file, index."""
    out = []
    blocks = BLOCK_RE.split(text)
    # split yields [pre, label1, body1, label2, body2, ...]
    for i in range(1, len(blocks) - 1, 2):
        label, body = blocks[i], blocks[i + 1]
        d = {"slot": label.strip()}
        m = re.search(r"^\t([^\t=\n]+)$", body, re.M)
        if m:
            d["resolved"] = m.group(1).strip()
        for key in ("style", "file", "index", "fullname", "pixelsize"):
            m = re.search(rf"^\t{key}=(.*)$", body, re.M)
            if m:
                d[key] = m.group(1).strip()
        out.append(d)
    return out


def normalize(rec, fontdir, casedir):
    s = json.dumps(rec)
    if fontdir:
        s = s.replace(fontdir.rstrip("/") + "/", "$FONTS/")
    s = s.replace(casedir.rstrip("/") + "/", "$T0/")
    return json.loads(s)


def run_case(xterm, fc_file, case, dv, cjk, rundir, timeout=10, third=None):
    casedir = os.path.join(rundir, case["id"])
    os.makedirs(casedir, exist_ok=True)
    widfile = os.path.join(casedir, "windowid")

    def subst(s):
        return (s.replace("{DV}", dv).replace("{CJK}", cjk)
                 .replace("{THIRD}", third or dv))

    fa = ["-fa", subst(case["fa"])] if "fa" in case else []
    xrm = []
    for k, v in case.get("xrm", {}).items():
        xrm += ["-xrm", f"T0Oracle*vt100.{k}: {subst(v)}"]
    order = case.get("order", ["fa", "xrm"])
    fontargs = sum(((fa if o == "fa" else xrm) for o in order), [])

    probe_s = case.get("probe", "\\u65e5\\u672c" if case.get("wide") else "")
    probe = f"printf '{probe_s}'; " if probe_s else ""
    child = (f"echo $WINDOWID > {widfile}; {probe}sleep {timeout}")
    argv = ([xterm, "-class", "T0Oracle", "-name", "t0oracle",
             "-report-fonts", "-geometry", "80x24",
             "-xrm", f"Xft.dpi: {CANONICAL_XFT_DPI}"]
            + fontargs + ["-e", "sh", "-c", child])

    env = dict(os.environ, FONTCONFIG_FILE=fc_file,
               XENVIRONMENT="/dev/null")
    with open(os.path.join(casedir, "stdout.raw"), "w") as so, \
         open(os.path.join(casedir, "stderr.raw"), "w") as se:
        proc = subprocess.Popen(argv, stdout=so, stderr=se, env=env)
        wid = None
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if os.path.exists(widfile) and os.path.getsize(widfile):
                wid = open(widfile).read().strip()
                break
            if proc.poll() is not None:
                break
            time.sleep(0.05)
        hints = ""
        if wid:
            time.sleep(0.3)          # let the wide probe load its font
            hints = run(["xprop", "-id", wid, "WM_NORMAL_HINTS"],
                        env=env).stdout
        inc = re.search(r"resize increment:\s+(\d+)\s+by\s+(\d+)", hints)
        base = re.search(r"base size:\s+(\d+)\s+by\s+(\d+)", hints)
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

    raw_out = open(os.path.join(casedir, "stdout.raw")).read()
    raw_err = open(os.path.join(casedir, "stderr.raw")).read()
    rec = {
        "id": case["id"],
        "input": {"argv_font_args": fontargs,
                  "resources": {k: subst(v)
                                for k, v in case.get("xrm", {}).items()}},
        "xft_fonts": parse_report_fonts(raw_out),
        "resize_inc": [int(inc.group(1)), int(inc.group(2))] if inc else None,
        "base_size": [int(base.group(1)), int(base.group(2))] if base else None,
        "window_found": bool(wid),
        "exit_status": proc.returncode,
        "diagnostics": [ln for ln in raw_err.splitlines()
                        if ln.strip()][:20],
    }
    fontdir = ""
    for b in rec["xft_fonts"]:
        if "file" in b:
            fontdir = os.path.dirname(b["file"])
            break
    return normalize(rec, fontdir, casedir)


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--xterm", required=True)
    ap.add_argument("--fontconfig", required=True,
                    help="isolated universe config (FONTCONFIG_FILE)")
    ap.add_argument("--expect-patch", type=int, default=CANONICAL_PATCH,
                    help=f"oracle patch level (default {CANONICAL_PATCH}, "
                         "the canonical oracle; any other value is for "
                         "harness self-testing ONLY and marks the record "
                         "canonical=false)")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--record", action="store_true")
    mode.add_argument("--check", metavar="FIXTURE")
    ap.add_argument("--keep", action="store_true",
                    help="retain per-case artifacts even on success")
    ap.add_argument("--only", metavar="ID", action="append",
                    help="run selected case IDs")
    ap.add_argument("--text-family", help="override probed text family")
    ap.add_argument("--wide-family", help="override probed wide family")
    ap.add_argument("--third-family",
                    help="third family for the two-entry-limit case "
                         "(default: a distinct family probed from the "
                         "universe, else text family)")
    args = ap.parse_args()

    if not os.environ.get("DISPLAY"):
        die("no DISPLAY")
    if not os.path.isfile(args.fontconfig):
        die(f"fontconfig universe not found: {args.fontconfig}")
    xterm = shutil.which(args.xterm) or args.xterm
    if not os.access(xterm, os.X_OK):
        die(f"not executable: {xterm}")

    patch, verline = oracle_version(xterm)
    if patch != args.expect_patch:
        die(f"oracle reports patch {patch}, expected {args.expect_patch} "
            f"({verline})")

    rundir = f"/tmp/t0-facename-{int(time.time())}"
    os.makedirs(rundir)
    dv, cjk = universe_families(args.fontconfig)
    if args.text_family:
        dv = args.text_family
    if args.wide_family:
        cjk = args.wide_family
    third = args.third_family
    if not third:
        env = dict(os.environ, FONTCONFIG_FILE=args.fontconfig)
        p = run(["fc-list", "--format", "%{family[0]}\\n"], env=env)
        fams = [f for f in dict.fromkeys(p.stdout.splitlines()) if f.strip()]
        third = next((f for f in fams if f not in (dv, cjk)), None)
    print(f"t0: oracle {verline}; Xft.dpi={CANONICAL_XFT_DPI}; "
          f"universe families: text='{dv}' "
          f"wide='{cjk}' third='{third}'; artifacts: {rundir}")
    preflight(xterm, args.fontconfig, dv, rundir)

    cases = [c for c in CASES if not args.only or c["id"] in args.only]
    probe_contract = validate_probe_contract(args.fontconfig, dv, cjk,
                                             third, cases)
    results = {}
    for c in cases:
        rec = run_case(xterm, args.fontconfig, c, dv, cjk, rundir,
                       third=third)
        results[c["id"]] = rec
        n = len(rec["xft_fonts"])
        res = rec["xft_fonts"][0].get("resolved", "?") if n else "NONE"
        print(f"t0: {c['id']}: {n} Xft block(s); primary -> {res}")

    doc = {"schema": SCHEMA, "harness_version": HARNESS_VERSION,
           "oracle": {"version_line": verline, "patch": patch,
                      "canonical_patch": CANONICAL_PATCH,
                      "canonical": patch == CANONICAL_PATCH},
           "environment": {"xft_dpi": CANONICAL_XFT_DPI,
                           "probe_contract_version": PROBE_CONTRACT_VERSION,
                           "probe_contract": probe_contract},
           "cases": results}

    if args.record:
        out = os.path.join(rundir, "candidate.json")
        with open(out, "w") as f:
            json.dump(doc, f, indent=2, sort_keys=True)
        print(f"t0: candidate written: {out}")
        print("t0: review it, then copy into the repo as "
              f"compat/xterm-{CANONICAL_PATCH}-face-name.json"
              + ("" if doc["oracle"]["canonical"] else
                 "  [WARNING: canonical=false - self-test record, "
                 "do NOT bless]"))
        return 0

    fixture = json.load(open(args.check))
    fails = []
    if fixture.get("schema") != SCHEMA:
        fails.append(f"fixture schema {fixture.get('schema')} != {SCHEMA}")
    if fixture.get("harness_version") != HARNESS_VERSION:
        fails.append("fixture harness version "
                     f"{fixture.get('harness_version')} != {HARNESS_VERSION}")
    if fixture.get("environment", {}).get("xft_dpi") != CANONICAL_XFT_DPI:
        fails.append("fixture Xft.dpi "
                     f"{fixture.get('environment', {}).get('xft_dpi')} != "
                     f"{CANONICAL_XFT_DPI}")
    if fixture.get("environment", {}).get("probe_contract_version") != PROBE_CONTRACT_VERSION:
        fails.append("fixture probe contract version "
                     f"{fixture.get('environment', {}).get('probe_contract_version')} != "
                     f"{PROBE_CONTRACT_VERSION}")
    if not fixture.get("oracle", {}).get("canonical", False):
        fails.append("fixture is marked canonical=false (recorded against "
                     f"a non-{CANONICAL_PATCH} oracle); refusing to treat "
                     "it as the blessed record")
    if fixture["oracle"]["patch"] != patch:
        fails.append(f"oracle patch {patch} != fixture "
                     f"{fixture['oracle']['patch']}")
    if not args.only and set(fixture.get("cases", {})) != set(results):
        missing = sorted(set(results) - set(fixture.get("cases", {})))
        extra = sorted(set(fixture.get("cases", {})) - set(results))
        fails.append(f"fixture case set differs: missing={missing} extra={extra}")
    for cid, rec in results.items():
        want = fixture["cases"].get(cid)
        if want is None:
            fails.append(f"{cid}: not in fixture")
        elif want != rec:
            fails.append(f"{cid}: MISMATCH (see {rundir}/{cid}/)")
    if fails:
        for f in fails:
            print(f"t0: {f}", file=sys.stderr)
        print(f"t0: raw artifacts retained: {rundir}", file=sys.stderr)
        return 1
    print(f"t0: all {len(results)} cases match fixture")
    if not args.keep:
        shutil.rmtree(rundir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
