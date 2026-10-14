#!/usr/bin/env python3
"""Measure real window cursors across XRDB, command-line and environment modes."""

import ctypes as C
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time


class Attributes(C.Structure):
    _fields_ = [
        (name, kind)
        for name, kind in [
            ("x", C.c_int),
            ("y", C.c_int),
            ("width", C.c_int),
            ("height", C.c_int),
            ("border_width", C.c_int),
            ("depth", C.c_int),
            ("visual", C.c_void_p),
            ("root", C.c_ulong),
            ("window_class", C.c_int),
            ("bit_gravity", C.c_int),
            ("win_gravity", C.c_int),
            ("backing_store", C.c_int),
            ("backing_planes", C.c_ulong),
            ("backing_pixel", C.c_ulong),
            ("save_under", C.c_int),
            ("colormap", C.c_ulong),
            ("map_installed", C.c_int),
            ("map_state", C.c_int),
            ("all_event_masks", C.c_long),
            ("your_event_mask", C.c_long),
            ("do_not_propagate_mask", C.c_long),
            ("override_redirect", C.c_int),
            ("screen", C.c_void_p),
        ]
    ]


class CursorImage(C.Structure):
    _fields_ = [
        ("x", C.c_short),
        ("y", C.c_short),
        ("width", C.c_ushort),
        ("height", C.c_ushort),
        ("xhot", C.c_ushort),
        ("yhot", C.c_ushort),
        ("serial", C.c_ulong),
        ("pixels", C.POINTER(C.c_ulong)),
        ("atom", C.c_ulong),
        ("name", C.c_char_p),
    ]


def bind(library, name, result, arguments):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = arguments
    return function


def wait_for(predicate, process=None):
    deadline = time.monotonic() + 5
    while not predicate():
        assert process is None or process.poll() is None, (
            "terminal exited before cursor capture"
        )
        assert time.monotonic() < deadline, "timed out waiting for a viewable terminal"
        time.sleep(0.01)


def fixture(directory, monochrome=False):
    """Xcursor file with 24/44-pixel multicolor or monochrome image chunks."""
    chunks = []
    entries = []
    position = 16 + 2 * 12
    for size in (24, 44):
        chunk = struct.pack("<9I", 36, 0xFFFD0002, size, 1, size, size, 0, 0, 0)
        chunk += b"".join(
            struct.pack(
                "<I",
                (
                    0xFFFF00FF
                    if monochrome
                    else (0xFFFF00FF, 0xFF00FFFF, 0xFF00FF00, 0x80800080)[i % 4]
                ),
            )
            for i in range(size * size)
        )
        entries.append(struct.pack("<3I", 0xFFFD0002, size, position))
        chunks.append(chunk)
        position += len(chunk)
    contents = struct.pack("<4I", 0x72756358, 16, 0x10000, 2)
    contents += b"".join(entries + chunks)
    cursors = (
        directory / ("test-theme-mono" if monochrome else "test-theme") / "cursors"
    )
    cursors.mkdir(parents=True)
    for name in ("text", "xterm", "crosshair", "left_ptr", "arrow", "hand2"):
        (cursors / name).write_bytes(contents)


def child(directory):
    (directory / "environment").write_text(
        json.dumps(
            {
                name: os.environ.get(name)
                for name in ("XCURSOR_PATH", "XCURSOR_THEME", "XCURSOR_SIZE")
            }
        )
    )
    (directory / "ready").write_text(os.environ["WINDOWID"])
    wait_for(lambda: (directory / "stop").exists())


def main(terminal, xrdb, directory, report):
    directory.mkdir()
    fixture(directory / "themes")
    fixture(directory / "themes", monochrome=True)
    fixture(directory / "default-search")
    shutil.copytree(
        directory / "default-search" / "test-theme",
        directory / "default-search" / "default",
    )
    home = directory / "home"
    home.mkdir()
    x11 = C.CDLL(ctypes.util.find_library("X11"))
    fixes = C.CDLL(ctypes.util.find_library("Xfixes"))
    open_display = bind(x11, "XOpenDisplay", C.c_void_p, [C.c_char_p])
    close_display = bind(x11, "XCloseDisplay", C.c_int, [C.c_void_p])
    attributes = bind(
        x11,
        "XGetWindowAttributes",
        C.c_int,
        [C.c_void_p, C.c_ulong, C.POINTER(Attributes)],
    )
    warp = bind(
        x11,
        "XWarpPointer",
        C.c_int,
        [
            C.c_void_p,
            C.c_ulong,
            C.c_ulong,
            C.c_int,
            C.c_int,
            C.c_uint,
            C.c_uint,
            C.c_int,
            C.c_int,
        ],
    )
    sync = bind(x11, "XSync", C.c_int, [C.c_void_p, C.c_int])
    free = bind(x11, "XFree", C.c_int, [C.c_void_p])
    get_cursor = bind(
        fixes, "XFixesGetCursorImage", C.POINTER(CursorImage), [C.c_void_p]
    )
    display = open_display(None)
    assert display, "cannot open isolated test display"
    root_window = bind(x11, "XDefaultRootWindow", C.c_ulong, [C.c_void_p])(display)
    create_window = bind(
        x11,
        "XCreateSimpleWindow",
        C.c_ulong,
        [
            C.c_void_p,
            C.c_ulong,
            C.c_int,
            C.c_int,
            C.c_uint,
            C.c_uint,
            C.c_uint,
            C.c_ulong,
            C.c_ulong,
        ],
    )
    compositor_window = create_window(display, root_window, 0, 0, 1, 1, 0, 0, 0)
    compositor_atom = bind(
        x11, "XInternAtom", C.c_ulong, [C.c_void_p, C.c_char_p, C.c_int]
    )(display, b"_NET_WM_CM_S0", False)
    set_owner = bind(
        x11,
        "XSetSelectionOwner",
        C.c_int,
        [C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong],
    )
    base_env = dict(
        os.environ,
        HOME=str(home),
        XENVIRONMENT="/dev/null",
        XFILESEARCHPATH="/dev/null",
        XUSERFILESEARCHPATH="/dev/null",
        XCURSOR_PATH=str(directory / "themes"),
    )
    for name in ("XCURSOR_THEME", "XCURSOR_SIZE", "XCURSOR_THEME_CORE"):
        base_env.pop(name, None)
    cases = [
        ("absent theme", "core", [], {}, []),
        ("empty theme", "core", [], {}, ["Xcursor.theme:"]),
        ("size alone", "core", ["Xcursor.size: 44"], {}, []),
        (
            "XRDB theme/24",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
            {},
            [],
        ),
        (
            "XRDB theme/44",
            "44",
            ["Xcursor.theme: test-theme", "Xcursor.size: 44"],
            {},
            [],
        ),
        (
            "xrm disables XRDB theme",
            "core",
            ["Xcursor.theme: test-theme"],
            {},
            ["Xcursor.theme:"],
        ),
        (
            "xrm selects theme/44",
            "44",
            [],
            {},
            ["Xcursor.theme: test-theme", "Xcursor.size: 44"],
        ),
        (
            "xrm size overrides XRDB",
            "44",
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
            {},
            ["Xcursor.size: 44"],
        ),
        (
            "environment theme beats empty xrm",
            "24",
            [],
            {"XCURSOR_THEME": "test-theme"},
            ["Xcursor.theme:", "Xcursor.size: 24"],
        ),
        (
            "environment size beats xrm",
            "44",
            [],
            {"XCURSOR_SIZE": "44"},
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
        ),
        (
            "empty environment with no default artwork",
            "core",
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
            {"XCURSOR_THEME": ""},
            [],
        ),
        (
            "missing environment theme",
            "core",
            ["Xcursor.theme: test-theme"],
            {"XCURSOR_THEME": "missing"},
            [],
        ),
        ("missing xrm theme", "core", [], {}, ["Xcursor.theme: missing"]),
        ("core cursor font", "core", [], {}, ["XTerm*pointerFont: cursor"]),
        (
            "missing cursor font fallback",
            "core",
            [],
            {},
            ["XTerm*pointerFont: no-such-cursor-font"],
        ),
        ("core crosshair", "core", [], {}, ["XTerm*pointerShape: crosshair"]),
        ("core arrow", "core", [], {}, ["XTerm*pointerShape: arrow"]),
        ("core left_ptr", "core", [], {}, ["XTerm*pointerShape: left_ptr"]),
        (
            "themed crosshair",
            "24",
            [],
            {},
            [
                "Xcursor.theme: test-theme",
                "Xcursor.size: 24",
                "XTerm*pointerShape: crosshair",
            ],
        ),
        (
            "environment core theming false",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
            {"XCURSOR_THEME_CORE": "false"},
            [],
        ),
        (
            "environment core theming true",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
            {"XCURSOR_THEME_CORE": "true"},
            [],
        ),
        (
            "resource core theming false",
            "24",
            [
                "Xcursor.theme: test-theme",
                "Xcursor.size: 24",
                "Xcursor.theme_core: false",
            ],
            {},
            [],
        ),
        (
            "resource core theming true",
            "24",
            [
                "Xcursor.theme: test-theme",
                "Xcursor.size: 24",
                "Xcursor.theme_core: true",
            ],
            {},
            [],
        ),
        (
            "DPI-derived themed size",
            "44",
            ["Xcursor.theme: test-theme", "Xft.dpi: 192"],
            {},
            [],
        ),
        (
            "empty environment size",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 44"],
            {"XCURSOR_SIZE": ""},
            [],
        ),
        (
            "invalid environment size",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 44"],
            {"XCURSOR_SIZE": "invalid"},
            [],
        ),
        (
            "zero resource size",
            "24",
            ["Xcursor.theme: test-theme", "Xcursor.size: 0"],
            {},
            [],
        ),
        (
            "negative resource size",
            "core",
            ["Xcursor.theme: test-theme", "Xcursor.size: -1"],
            {},
            [],
        ),
        (
            "monochrome themed artwork",
            "mono24",
            ["Xcursor.theme: test-theme-mono", "Xcursor.size: 24"],
            {},
            [],
        ),
        ("core native boat", "core", [], {}, ["XTerm*pointerShape: boat"]),
        ("core native cross", "core", [], {}, ["XTerm*pointerShape: cross"]),
    ]
    cursor_library = C.CDLL(ctypes.util.find_library("Xcursor"))
    library_path = bind(cursor_library, "XcursorLibraryPath", C.c_char_p, [])().decode()
    for size in (24, 44):
        cases.append(
            (
                f"installed default theme/{size}",
                "reference",
                ["Xcursor.theme: default", f"Xcursor.size: {size}"],
                {"XCURSOR_PATH": library_path},
                [],
            )
        )
    for label, database, environment, overrides in [
        (
            "normal search path: empty theme, arrow",
            [],
            {},
            ["Xcursor.theme:", "XTerm*pointerShape: arrow"],
        ),
        ("normal search path: absent theme", [], {}, []),
        ("normal search path: size alone", ["Xcursor.size: 44"], {}, []),
        (
            "normal search path: empty environment",
            ["Xcursor.theme: default"],
            {"XCURSOR_THEME": ""},
            [],
        ),
    ]:
        cases.append(
            (
                label,
                "reference" if label.endswith("empty environment") else "core",
                database,
                {"XCURSOR_PATH": library_path, **environment},
                overrides,
            )
        )
    # Ensure CI exercises substitution even when no desktop cursor theme is installed.
    for label, overrides in [
        (
            "available default artwork: empty theme, arrow",
            ["Xcursor.theme:", "XTerm*pointerShape: arrow"],
        ),
        ("available default artwork: absent theme", []),
    ]:
        cases.append(
            (
                label,
                "core",
                [],
                {"XCURSOR_PATH": str(directory / "default-search")},
                overrides,
            )
        )
    cases.extend(
        [
            (
                "ARGB: core arrow",
                "core",
                [],
                {},
                ["Xcursor.theme:", "XTerm*pointerShape: arrow"],
            ),
            (
                "ARGB: core color from xrm",
                "core",
                [],
                {},
                [
                    "Xcursor.theme:",
                    "XTerm*pointerShape: arrow",
                    "XTerm*pointerColor: blue",
                ],
            ),
            (
                "ARGB: themed cursor",
                "24",
                ["Xcursor.theme: test-theme", "Xcursor.size: 24"],
                {},
                [],
            ),
        ]
    )
    cases.extend(
        [
            (
                "defaults: blue on black",
                "blue-black",
                ["XTerm*foreground: green", "XTerm*background: black"],
                {},
                [
                    "Xcursor.theme:",
                    "XTerm*pointerShape: arrow",
                    "XTerm*pointerColor: blue",
                ],
            ),
            (
                "defaults: inherit both terminal colors",
                "green-black",
                ["XTerm*foreground: green", "XTerm*background: black"],
                {},
                ["Xcursor.theme:", "XTerm*pointerShape: arrow"],
            ),
            (
                "defaults: explicit default names",
                "green-black",
                ["XTerm*foreground: green", "XTerm*background: black"],
                {},
                [
                    "Xcursor.theme:",
                    "XTerm*pointerShape: arrow",
                    "XTerm*pointerColor: XtDefaultForeground",
                    "XTerm*pointerColorBackground: XtDefaultBackground",
                ],
            ),
        ]
    )
    programs = [("candidate", terminal)]
    reference = shutil.which("xterm")
    if reference:
        programs.append(("xterm", reference))
    results = []
    errors = []
    try:
        for index, (label, expected, database, environment, overrides) in enumerate(
            cases
        ):
            set_owner(
                display,
                compositor_atom,
                compositor_window if label.startswith("ARGB:") else 0,
                0,
            )
            sync(display, False)
            captures = {}
            errors_before = len(errors)
            for program, executable in programs:
                work = directory / f"{index}-{program}"
                work.mkdir()
                resources = (
                    []
                    if label.startswith("defaults:")
                    else [
                        "XTerm*pointerColor: blue",
                        "XTerm*pointerColorBackground: yellow",
                    ]
                ) + database
                subprocess.run(
                    [xrdb, "-nocpp", "-load"],
                    input="\n".join(resources) + "\n",
                    text=True,
                    check=True,
                )
                arguments = [
                    executable,
                    "+sb",
                    "-fn",
                    "fixed",
                    "-geometry",
                    "40x10",
                    "-xrm",
                    "XTerm*pointerMode: 0",
                ]
                for override in overrides:
                    arguments += ["-xrm", override]
                arguments += ["-e", sys.executable, __file__, "--child", str(work)]
                with (work / "log").open("w") as log:
                    process = subprocess.Popen(
                        arguments,
                        env=dict(base_env, **environment),
                        stdout=log,
                        stderr=log,
                    )
                    try:
                        ready = work / "ready"
                        wait_for(
                            lambda: ready.exists() and ready.stat().st_size, process
                        )
                        if program == "candidate":
                            inherited = json.loads((work / "environment").read_text())
                            expected_environment = dict(base_env, **environment)
                            assert inherited == {
                                name: expected_environment.get(name)
                                for name in inherited
                            }, "cursor loading changed the child environment"
                        window = int(ready.read_text(), 0)
                        state = Attributes()
                        wait_for(
                            lambda: (
                                attributes(display, window, C.byref(state))
                                and state.map_state == 2
                            ),
                            process,
                        )
                        if program == "candidate" and label.startswith("ARGB:"):
                            assert state.depth == 32, (
                                "compositor test did not select an ARGB visual"
                            )
                        warp(display, 0, window, 0, 0, 0, 0, 40, 40)
                        sync(display, False)
                        time.sleep(0.02)
                        pointer = get_cursor(display)
                        assert pointer, "XFixes returned no cursor image"
                        image = pointer.contents
                        pixels = [
                            image.pixels[i] & 0xFFFFFFFF
                            for i in range(image.width * image.height)
                        ]
                        colors = sorted(
                            {pixel & 0xFFFFFF for pixel in pixels if pixel >> 24}
                        )
                        raw = struct.pack(f"<{len(pixels)}I", *pixels)
                        capture = dict(
                            width=image.width,
                            height=image.height,
                            hotspot=[image.xhot, image.yhot],
                            colors=colors,
                            hash=hashlib.sha256(raw).hexdigest(),
                        )
                        free(pointer)
                        captures[program] = capture
                        if expected == "reference":
                            passed = bool(colors)
                        elif expected == "core":
                            passed = colors == [0x0000FF, 0xFFFF00]
                        elif expected == "blue-black":
                            passed = colors == [0, 0x0000FF]
                        elif expected == "green-black":
                            passed = colors == [0, 0x00FF00]
                        elif expected.startswith("mono"):
                            passed = colors == [0x0000FF] and capture[
                                "width"
                            ] == capture["height"] == int(expected[4:])
                        else:
                            passed = colors == [
                                0x00FF00,
                                0x00FFFF,
                                0x800080,
                                0xFF00FF,
                            ] and capture["width"] == capture["height"] == int(expected)
                        if not passed:
                            errors.append(
                                f"{label} / {program}: expected {expected}, got {capture}"
                            )
                    finally:
                        (work / "stop").touch()
                        try:
                            process.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            process.terminate()
                            process.wait(timeout=2)
            parity = not reference or captures["candidate"] == captures["xterm"]
            if not parity:
                errors.append(f"{label}: cursor differs from xterm: {captures}")
            results.append(
                dict(case=label, expected=expected, parity=parity, captures=captures)
            )
            print(
                f"{label}: {'PASS' if len(errors) == errors_before else 'FAIL'}",
                flush=True,
            )
    finally:
        close_display(display)
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(
            json.dumps(dict(results=results, errors=errors), indent=2) + "\n"
        )
    assert not errors, "\n".join(errors)
    print(
        f"cursor modes: {len(results)} cases passed; reference={'xterm' if reference else 'unavailable'}"
    )


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(Path(sys.argv[2]))
    else:
        main(sys.argv[1], sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]))
