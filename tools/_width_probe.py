"""Shared CPR and mode-2027 machinery for interactive width probes."""

import argparse
from dataclasses import dataclass
import os
import re
import select
import sys
import termios
import tty

BOLD, DIM, RESET = "\033[1m", "\033[2m", "\033[0m"
CYAN, YELLOW, GREEN, RED, MAG = (
    "\033[36m",
    "\033[33m",
    "\033[32m",
    "\033[31m",
    "\033[35m",
)


@dataclass(frozen=True)
class RightMarginSample:
    """A width-two sample drawn from the terminal's final column."""

    label: str
    text: str
    note: str = ""


def raw_reply(query, terminator, timeout=0.4):
    """Send a terminal query and read through its terminating character."""
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd, termios.TCSANOW)
        while select.select([fd], [], [], 0)[0]:
            os.read(fd, 1024)
        sys.stdout.write(query)
        sys.stdout.flush()
        buf = b""
        while select.select([fd], [], [], timeout)[0]:
            buf += os.read(fd, 64)
            if terminator.encode() in buf:
                break
        return buf.decode("ascii", "replace")
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)


def cursor_position():
    """Return the current one-based (row, column) from CPR, or None."""
    reply = raw_reply("\033[6n", "R")
    match = re.search(r"\x1b\[\??(\d+);(\d+)R", reply)
    return (int(match.group(1)), int(match.group(2))) if match else None


def decrqm_2027():
    """Return the reported DEC mode 2027 state."""
    reply = raw_reply("\033[?2027$p", "y")
    match = re.search(r"\x1b\[\?2027;(\d+)\$y", reply)
    if not match:
        return "unknown"
    return {
        0: "unknown",
        1: "set",
        2: "reset",
        3: "permanent-set",
        4: "permanent-reset",
    }.get(int(match.group(1)), "unknown")


def set_2027(on):
    sys.stdout.write("\033[?2027h" if on else "\033[?2027l")
    sys.stdout.flush()


def header(title):
    print(f"\n{BOLD}{CYAN}== {title} =={RESET}\n")


def codepoints(text):
    return " ".join(f"U+{ord(char):04X}" for char in text)


def wait(pause, is_tty):
    if pause and is_tty:
        try:
            input(f"{DIM}  [Enter]{RESET}")
        except EOFError:
            pass


def measure(sample):
    """Print a sample and return (consumed cells, measurement problem)."""
    sys.stdout.write("    ")
    sys.stdout.flush()
    start = cursor_position()
    sys.stdout.write(sample)
    sys.stdout.flush()
    end = cursor_position()
    if start is None or end is None:
        return None, "no CPR"
    if end[0] != start[0] or end[1] < start[1]:
        return None, f"wrapped from {start[0]};{start[1]} to {end[0]};{end[1]}"
    return end[1] - start[1], None


def run_right_margin_sample(sample, contract, is_tty):
    """Verify that a wide glyph at the final column wraps as one unit."""
    print(f"  {BOLD}{sample.label}{RESET}  {DIM}{codepoints(sample.text)}{RESET}")
    if not is_tty:
        print(f"    {sample.text}")
        verdict, detail, color = "visual only", "no CPR", DIM
    else:
        # Probe above the reporting row so a bottom-margin scroll cannot hide
        # the row transition. DECSC/DECRC restore the reporting position.
        sys.stdout.write("\0337\033[1A\033[999G")
        sys.stdout.flush()
        start = cursor_position()
        sys.stdout.write(sample.text)
        sys.stdout.flush()
        end = cursor_position()
        sys.stdout.write("\0338")
        sys.stdout.flush()
        if start is None or end is None:
            verdict, detail, color = "visual only", "no CPR", DIM
        elif end[0] == start[0] + 1 and end[1] in (1, 3):
            verdict, color = "wrap-faithful", GREEN
            detail = (
                f"wide cluster stayed atomic at right margin; CPR {start[0]};{start[1]} "
                f"-> {end[0]};{end[1]} under {contract} contract"
            )
        else:
            verdict, color = "wrong", RED
            detail = (
                f"right-margin CPR {start} -> {end}; expected next row column 1 or 3"
            )
    print(f"    {color}{verdict}: {detail}{RESET}")
    if sample.note:
        print(f"    {YELLOW}{sample.note}{RESET}")
    print()
    return {"verdict": verdict}


def run_sample(sample, pass_name, contract, is_tty):
    label, text, legacy, cluster, note = sample
    print(f"  {BOLD}{label}{RESET}  {DIM}{codepoints(text)}{RESET}")
    accept = cluster if contract == "cluster" else legacy
    expect = min(accept)
    if is_tty:
        cells, problem = measure(text)
        sys.stdout.write("|")
        sys.stdout.write(f"\033[{5 + expect}G{GREEN}|{RESET}\n")
    else:
        sys.stdout.write(f"    {text}|\033[{5 + expect}G{GREEN}|{RESET}\n")
        cells, problem = None, "no CPR"

    if problem == "no CPR":
        verdict, detail, color = "visual only", problem, DIM
    elif problem is not None:
        verdict, detail, color = "wrapped", problem, YELLOW
    elif cells in accept:
        if len(accept) > 1:
            verdict, color = "underspecified", YELLOW
        elif contract == "cluster":
            verdict, color = "cluster-capable", GREEN
        else:
            verdict, color = "legacy-faithful", GREEN
        detail = f"{cells} cells; {contract} accepts {sorted(accept)}"
    elif pass_name == "legacy" and cells in cluster and cluster != legacy:
        verdict, color = "UNILATERAL", MAG
        detail = f"measured cluster width {cells} while 2027 was off"
    else:
        verdict, color = "wrong", RED
        detail = f"measured {cells}; {contract} expects {sorted(accept)}"

    print(f"    {color}{verdict}: {detail}{RESET}")
    if note:
        print(f"    {YELLOW}{note}{RESET}")
    print()
    return {"verdict": verdict}


def run_pass(sections, regime, pause, is_tty):
    """Run one requested regime and record the contract actually active."""
    if regime == "cluster":
        set_2027(True)
        state = decrqm_2027() if is_tty else "unknown"
        contract = "cluster" if state in ("set", "permanent-set") else "legacy"
        if contract != "cluster":
            print(
                f"{MAG}  mode 2027 did not take (DECRQM: {state}); "
                "grading this pass against LEGACY expectations, since "
                "an unverified terminal must keep the legacy contract."
                f"{RESET}\n"
            )
    else:
        set_2027(False)
        state = decrqm_2027() if is_tty else "unknown"
        contract = "cluster" if state in ("set", "permanent-set") else "legacy"
        if contract == "cluster":
            print(
                f"{MAG}  mode 2027 remained set after DECRST (DECRQM: {state}); "
                "a legacy pass is unavailable, so this pass is graded "
                f"against cluster expectations.{RESET}\n"
            )

    results = []
    for title, samples in sections:
        header(f"{title}  [{regime} pass]")
        for sample in samples:
            if isinstance(sample, RightMarginSample):
                results.append(run_right_margin_sample(sample, contract, is_tty))
            else:
                results.append(run_sample(sample, regime, contract, is_tty))
        wait(pause, is_tty)
    return {
        "requested": regime,
        "state": state,
        "contract": contract,
        "results": results,
    }


def summary(passes):
    header("Summary")
    for pass_result in passes:
        counts = {}
        for result in pass_result["results"]:
            verdict = result["verdict"]
            counts[verdict] = counts.get(verdict, 0) + 1
        rendered = ", ".join(
            f"{counts[name]} {name}"
            for name in (
                "legacy-faithful",
                "cluster-capable",
                "underspecified",
                "wrap-faithful",
                "wrapped",
                "UNILATERAL",
                "wrong",
                "visual only",
            )
            if counts.get(name)
        )
        capability = ""
        if pass_result["requested"] == "cluster":
            supported = pass_result["contract"] == "cluster"
            capability = (
                f", cluster capability={'supported' if supported else 'unsupported'}"
            )
        print(
            f"  {pass_result['requested']:<7} request: DECRQM={pass_result['state']}, "
            f"active contract={pass_result['contract']}{capability}; {rendered}"
        )
    print()
    print(
        f"  {DIM}Ideal terminal: all-faithful on the legacy pass AND "
        f"all-capable on the cluster pass, zero unilateral.{RESET}"
    )
    print(
        f"  {DIM}Width is contractual; rendering inside the width budget "
        f"is free and was not graded.{RESET}\n"
    )


def parse_args(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--no-pause",
        action="store_true",
        help="run all sections without waiting for Enter",
    )
    parser.add_argument(
        "--regime",
        choices=("legacy", "cluster", "both"),
        default="both",
        help="select which width contract to probe",
    )
    return parser.parse_args()


def restore_2027(state):
    if state != "unknown":
        set_2027(state in ("set", "permanent-set"))


def run_probe(title, description, sections):
    args = parse_args(description)
    encoding = sys.stdout.encoding or ""
    if "utf" not in encoding.lower():
        sys.exit("stdout is not UTF-8; set PYTHONIOENCODING=utf-8")

    is_tty = sys.stdin.isatty() and sys.stdout.isatty()
    print(f"{BOLD}{title}{RESET}")
    print(
        f"  TERM={os.environ.get('TERM', '?')}  "
        f"TERM_PROGRAM={os.environ.get('TERM_PROGRAM', '-')}  "
        f"LANG={os.environ.get('LANG', '?')}"
    )
    baseline = decrqm_2027() if is_tty else "unknown"
    if is_tty:
        print(f"  DECRQM ?2027 before any set: {BOLD}{baseline}{RESET}")
    else:
        print(f"  {DIM}not a tty: visual bars only, no measurement{RESET}")
    print()

    passes = []
    try:
        if args.regime in ("legacy", "both"):
            passes.append(run_pass(sections, "legacy", not args.no_pause, is_tty))
        if args.regime in ("cluster", "both"):
            passes.append(run_pass(sections, "cluster", not args.no_pause, is_tty))
        summary(passes)
    finally:
        restore_2027(baseline)
