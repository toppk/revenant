#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["PySide6-Essentials>=6.8,<7"]
# ///
"""Preview and set Xresources themes with capability-checked GUI reload targets.

Run with uv run tools/theme-setter-utility.py (GUI), or use --help for CLI.
"""

import argparse
import base64
import ctypes as C
import ctypes.util
from datetime import datetime
import html
import json
import logging
import os
import re
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import uuid

from theme_resources import (
    DEFAULT_THEMES,
    XTERM_COLORS,
    RESOURCE_NAMES,
    fetch,
    load_theme,
    render,
)

LOG = logging.getLogger("theme-setter")

PROPERTIES = ("RESOURCE_MANAGER", "SCREEN_RESOURCES")
SESSION_PROPERTY = "_XTP_THEME_SETTER_ORIGINAL_ID"
STATE_DIR = (
    Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state"))
    / "theme-setter"
)


class XResources:
    """Read/restore raw server properties, preserving absent and per-screen data."""

    def __init__(self, display=None):
        self.lib = C.CDLL(ctypes.util.find_library("X11") or "libX11.so.6")
        signatures = {
            "XOpenDisplay": (C.c_void_p, [C.c_char_p]),
            "XCloseDisplay": (C.c_int, [C.c_void_p]),
            "XScreenCount": (C.c_int, [C.c_void_p]),
            "XRootWindow": (C.c_ulong, [C.c_void_p, C.c_int]),
            "XInternAtom": (C.c_ulong, [C.c_void_p, C.c_char_p, C.c_int]),
            "XGetWindowProperty": (
                C.c_int,
                [
                    C.c_void_p,
                    C.c_ulong,
                    C.c_ulong,
                    C.c_long,
                    C.c_long,
                    C.c_int,
                    C.c_ulong,
                    C.POINTER(C.c_ulong),
                    C.POINTER(C.c_int),
                    C.POINTER(C.c_ulong),
                    C.POINTER(C.c_ulong),
                    C.POINTER(C.c_void_p),
                ],
            ),
            "XChangeProperty": (
                C.c_int,
                [
                    C.c_void_p,
                    C.c_ulong,
                    C.c_ulong,
                    C.c_ulong,
                    C.c_int,
                    C.c_int,
                    C.c_void_p,
                    C.c_int,
                ],
            ),
            "XDeleteProperty": (C.c_int, [C.c_void_p, C.c_ulong, C.c_ulong]),
            "XSync": (C.c_int, [C.c_void_p, C.c_int]),
            "XFree": (C.c_int, [C.c_void_p]),
        }
        for name, (result, args) in signatures.items():
            function = getattr(self.lib, name)
            function.restype, function.argtypes = result, args
        self.display = display or os.environ.get("DISPLAY", "")
        if not self.display:
            raise ValueError(
                "DISPLAY is required for the X11/Xwayland resource database"
            )
        self.connection = self.lib.XOpenDisplay(self.display.encode())
        if not self.connection:
            raise ValueError(f"cannot connect to DISPLAY={self.display}")
        self.roots = [
            self.lib.XRootWindow(self.connection, i)
            for i in range(self.lib.XScreenCount(self.connection))
        ]
        self.string = self.atom("STRING")

    def close(self):
        if self.connection:
            self.lib.XCloseDisplay(self.connection)
            self.connection = None

    def atom(self, name):
        return self.lib.XInternAtom(self.connection, name.encode(), 0)

    def read(self, root, name):
        actual, form, count, remaining, data = (
            C.c_ulong(),
            C.c_int(),
            C.c_ulong(),
            C.c_ulong(),
            C.c_void_p(),
        )
        result = self.lib.XGetWindowProperty(
            self.connection,
            root,
            self.atom(name),
            0,
            1024 * 1024,
            0,
            0,
            C.byref(actual),
            C.byref(form),
            C.byref(count),
            C.byref(remaining),
            C.byref(data),
        )
        try:
            if result != 0 or remaining.value:
                raise ValueError(f"cannot read complete {name} (limit 4 MiB)")
            if actual.value == 0:
                return None
            if actual.value != self.string or form.value != 8:
                raise ValueError(f"{name} is not a STRING/8 property")
            return C.string_at(data, count.value)
        finally:
            if data.value:
                self.lib.XFree(data)

    def write(self, root, name, value):
        if value is None:
            self.lib.XDeleteProperty(self.connection, root, self.atom(name))
        else:
            self.lib.XChangeProperty(
                self.connection,
                root,
                self.atom(name),
                self.string,
                8,
                0,
                C.c_char_p(value),
                len(value),
            )

    def snapshot(self):
        return {
            "schema": 1,
            "screens": [
                {
                    name: None
                    if (value := self.read(root, name)) is None
                    else base64.b64encode(value).decode()
                    for name in PROPERTIES
                }
                for root in self.roots
            ],
        }

    def restore(self, snapshot):
        screens = snapshot.get("screens") if isinstance(snapshot, dict) else None
        if (
            not isinstance(snapshot, dict)
            or snapshot.get("schema") != 1
            or not isinstance(screens, list)
            or len(screens) != len(self.roots)
            or any(
                not isinstance(screen, dict)
                or set(screen) != set(PROPERTIES)
                or any(
                    value is not None and not isinstance(value, str)
                    for value in screen.values()
                )
                for screen in screens
            )
        ):
            raise ValueError("saved database does not match this X server's screens")
        values = [
            [
                None
                if screen[name] is None
                else base64.b64decode(screen[name], validate=True)
                for name in PROPERTIES
            ]
            for screen in screens
        ]
        if any(
            value is not None and len(value) > 4 * 1024 * 1024
            for screen in values
            for value in screen
        ):
            raise ValueError("saved resource property exceeds 4 MiB")
        for root, screen in zip(self.roots, values):
            for name, value in zip(PROPERTIES, screen):
                self.write(root, name, value)
        self.lib.XSync(self.connection, 0)

    def current_colors(self, application_class="XTerm"):
        """Resolve live server colors for xterm.vt100, with explicit xterm fallbacks."""

        class Value(C.Structure):
            _fields_ = [("size", C.c_uint), ("addr", C.c_void_p)]

        class Color(C.Structure):
            _fields_ = [
                ("pixel", C.c_ulong),
                ("red", C.c_ushort),
                ("green", C.c_ushort),
                ("blue", C.c_ushort),
                ("flags", C.c_char),
                ("pad", C.c_char),
            ]

        self.lib.XrmInitialize()
        self.lib.XrmGetStringDatabase.argtypes = [C.c_char_p]
        self.lib.XrmGetStringDatabase.restype = C.c_void_p
        self.lib.XrmGetResource.argtypes = [
            C.c_void_p,
            C.c_char_p,
            C.c_char_p,
            C.POINTER(C.c_char_p),
            C.POINTER(Value),
        ]
        self.lib.XrmDestroyDatabase.argtypes = [C.c_void_p]
        self.lib.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
        self.lib.XDefaultColormap.restype = C.c_ulong
        self.lib.XParseColor.argtypes = [
            C.c_void_p,
            C.c_ulong,
            C.c_char_p,
            C.POINTER(Color),
        ]
        self.lib.XDefaultScreen.argtypes = [C.c_void_p]
        self.lib.XDefaultScreen.restype = C.c_int
        screen = self.lib.XDefaultScreen(self.connection)
        raw = b"\n".join(
            (
                self.read(self.roots[0], "RESOURCE_MANAGER") or b"",
                self.read(self.roots[screen], "SCREEN_RESOURCES") or b"",
            )
        )
        database = self.lib.XrmGetStringDatabase(raw)
        colors = dict(XTERM_COLORS)
        try:
            if database:
                for name in RESOURCE_NAMES:
                    value, kind = Value(), C.c_char_p()
                    resource_class = name[0].upper() + name[1:]
                    if self.lib.XrmGetResource(
                        database,
                        f"xterm.vt100.{name}".encode(),
                        f"{application_class}.VT100.{resource_class}".encode(),
                        C.byref(kind),
                        C.byref(value),
                    ):
                        color = Color()
                        text = C.string_at(value.addr, value.size).rstrip(b"\0")
                        if not self.lib.XParseColor(
                            self.connection,
                            self.lib.XDefaultColormap(self.connection, screen),
                            text,
                            C.byref(color),
                        ):
                            raise ValueError(f"invalid live resource {name}: {text!r}")
                        colors[name] = (
                            f"#{color.red >> 8:02x}{color.green >> 8:02x}{color.blue >> 8:02x}"
                        )
            return colors
        finally:
            if database:
                self.lib.XrmDestroyDatabase(database)

    def color_diagnostics(self, application_class="XTerm"):
        """Resolve winning resource keys with Xrm, rather than guessing specificity."""
        # current_colors initializes the Xrm ctypes signatures.
        self.current_colors(application_class)
        screen = self.lib.XDefaultScreen(self.connection)
        entries = []
        names = {name.casefold() for name in RESOURCE_NAMES}
        for root, prop in (
            (self.roots[0], "RESOURCE_MANAGER"),
            (self.roots[screen], "SCREEN_RESOURCES"),
        ):
            for line in (
                (self.read(root, prop) or b"").decode(errors="replace").splitlines()
            ):
                match = re.match(r"^([^!#\s:]+)\s*:\s*(.*)$", line)
                if match and re.split(r"[.*]", match[1])[-1].casefold() in names:
                    entries.append({"key": match[1], "value": match[2], "source": prop})
        database = self.lib.XrmGetStringDatabase(
            "\n".join(
                f"{entry['key']}: marker{index}" for index, entry in enumerate(entries)
            ).encode()
        )
        winners = {}
        # Use the same ABI structure as XrmGetResource's configured pointer argument.
        Value = self.lib.XrmGetResource.argtypes[-1]._type_
        try:
            if database:
                for name in RESOURCE_NAMES:
                    kind, value = C.c_char_p(), Value()
                    if self.lib.XrmGetResource(
                        database,
                        f"xterm.vt100.{name}".encode(),
                        f"{application_class}.VT100.{name[0].upper() + name[1:]}".encode(),
                        C.byref(kind),
                        C.byref(value),
                    ):
                        marker = (
                            C.string_at(value.addr, value.size).rstrip(b"\0").decode()
                        )
                        winners[name] = entries[int(marker.removeprefix("marker"))]
        finally:
            if database:
                self.lib.XrmDestroyDatabase(database)
        alternatives = {
            name: entry
            for name, entry in winners.items()
            if entry["key"] != f"{application_class}*{name}"
        }
        variants = sorted(
            {
                entry["key"]
                for entry in entries
                if re.split(r"[.*]", entry["key"])[0].casefold()
                == application_class.casefold()
                and re.split(r"[.*]", entry["key"])[0]
                not in (application_class, "xterm")
            }
        )
        return {
            "winners": winners,
            "alternatives": alternatives,
            "case_variants": variants,
        }

    def blank(self):
        self.restore(
            {"schema": 1, "screens": [dict.fromkeys(PROPERTIES) for root in self.roots]}
        )


class ThemeSetter:
    def __init__(self, resources, state_dir=STATE_DIR):
        self.x = resources
        self.state_dir = Path(state_dir)

    def original_path(self):
        token = self.x.read(self.x.roots[0], SESSION_PROPERTY)
        if token is None:
            return None
        token = token.decode("ascii")
        if len(token) != 32 or any(c not in "0123456789abcdef" for c in token):
            raise ValueError("invalid theme-setter session identifier")
        return self.state_dir / (token + ".json")

    def capture(self, replace=False):
        path = self.original_path()
        if path and path.exists() and not replace:
            return path
        snapshot = self.x.snapshot()
        self.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        token = uuid.uuid4().hex
        path = self.state_dir / (token + ".json")
        with tempfile.NamedTemporaryFile(
            mode="w", dir=self.state_dir, delete=False
        ) as output:
            json.dump(snapshot, output)
            temporary = Path(output.name)
        temporary.replace(path)
        self.x.write(self.x.roots[0], SESSION_PROPERTY, token.encode())
        self.x.lib.XSync(self.x.connection, 0)
        return path

    def apply(self, colors=None, application_class="XTerm", replace=False):
        # Validate before capturing or changing the database.
        text = render(colors, application_class) if colors is not None else None
        LOG.debug(
            "Updating XRDB display=%s class=%s replace=%s colors=%s",
            self.x.display,
            application_class,
            replace,
            colors is not None,
        )
        before = self.x.snapshot()
        self.capture()
        try:
            if replace or colors is None:
                self.x.blank()
            if text is not None:
                subprocess.run(
                    ["xrdb", "-display", self.x.display, "-nocpp", "-merge", "-all"],
                    input=text,
                    text=True,
                    check=True,
                    capture_output=True,
                    timeout=10,
                )
        except Exception:
            LOG.exception("XRDB update failed; restoring previous resources")
            self.x.restore(before)
            raise
        LOG.debug("XRDB update completed")
        if colors is None:
            return {
                "overridden": {},
                "diagnostics": self.x.color_diagnostics(application_class),
            }
        effective = self.x.current_colors(application_class)
        overridden = {
            name: {"requested": colors[name], "effective": effective[name]}
            for name in RESOURCE_NAMES
            if colors[name].lower() != effective[name].lower()
        }
        diagnostics = self.x.color_diagnostics(application_class)
        if overridden:
            LOG.debug(
                "XRDB was written, but %d colors are overridden for xterm.vt100",
                len(overridden),
            )
            for name, values in overridden.items():
                winning = diagnostics["winners"].get(name, {})
                LOG.debug(
                    "%s: requested=%s effective=%s winning resource=%s: %s (%s)",
                    name,
                    values["requested"],
                    values["effective"],
                    winning.get("key", "<default>"),
                    winning.get("value", ""),
                    winning.get("source", ""),
                )
        return {"overridden": overridden, "diagnostics": diagnostics}

    def restore_original(self):
        path = self.original_path()
        if not path or not path.exists():
            raise ValueError("no saved original database for this X server session")
        self.x.restore(json.loads(path.read_text()))


class ReloadTargets:
    """Discover capable local windows and signal only their verified process."""

    def __init__(self, display):
        self.display = display

    def xprop(self, *args):
        return subprocess.run(
            ["xprop", "-display", self.display, *args],
            text=True,
            capture_output=True,
            timeout=3,
            check=True,
        ).stdout

    def inspect(self, window):
        if not isinstance(window, int) or not 0 < window <= 0xFFFFFFFF:
            return None
        try:
            output = self.xprop(
                "-id",
                hex(window),
                "_REVENANT_CAPABILITIES",
                "_REVENANT_VERSION",
                "_NET_WM_PID",
                "WM_CLIENT_MACHINE",
                "WM_CLASS",
                "_NET_WM_NAME",
                "WM_NAME",
            )
        except subprocess.CalledProcessError:
            return None  # A window can close while enumerating.
        capabilities = re.search(
            r"^_REVENANT_CAPABILITIES\(ATOM\) = (.*)$", output, re.M
        )
        if not capabilities or "_REVENANT_RELOAD_SIGHUP_V1" not in capabilities[
            1
        ].split(", "):
            return None
        pid = re.search(r"^_NET_WM_PID\(CARDINAL\) = ([0-9]+)$", output, re.M)
        host = re.search(r'^WM_CLIENT_MACHINE\(STRING\) = "([^"\n]+)"$', output, re.M)
        version = re.search(
            r'^_REVENANT_VERSION\(UTF8_STRING\) = "([^"\n]+)"$', output, re.M
        )
        if not pid or not host or not version:
            return None
        local = {
            name.casefold().rstrip(".")
            for name in (socket.gethostname(), os.uname().nodename)
        }
        if host[1].casefold().rstrip(".") not in local:
            return None
        pid = int(pid[1])
        try:
            if pid <= 1 or Path(f"/proc/{pid}").stat().st_uid != os.getuid():
                return None
        except OSError:
            return None
        title = re.search(
            r'^(?:_NET_WM_NAME\(UTF8_STRING\)|WM_NAME\(STRING\)) = "(.*)"$',
            output,
            re.M,
        )
        klass = re.search(
            r'^WM_CLASS\(STRING\) = "([^"\n]+)", "([^"\n]+)"$', output, re.M
        )
        return {
            "window": window,
            "pid": pid,
            "version": version[1],
            "host": host[1],
            "title": title[1] if title else "Terminal",
            "class": klass[2] if klass else "XTerm",
        }

    def discover(self):
        candidates = set()
        launching = os.environ.get("WINDOWID", "")
        if re.fullmatch(r"(?:0x[0-9a-fA-F]+|[0-9]+)", launching):
            candidates.add(int(launching, 16 if launching.startswith("0x") else 10))
        output = self.xprop("-root", "_NET_CLIENT_LIST")
        candidates.update(
            int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", output)
        )
        return [
            target
            for window in sorted(candidates)
            if (target := self.inspect(window)) is not None
        ]

    def hup(self, selected):
        if not hasattr(os, "pidfd_open") or not hasattr(signal, "pidfd_send_signal"):
            raise ValueError("targeted reload requires Linux pidfd support")
        # Pin the chosen process first; revalidate the window/capability before signaling.
        fd = os.pidfd_open(selected["pid"])
        try:
            current = self.inspect(selected["window"])
            if current is None or any(
                current[key] != selected[key]
                for key in ("pid", "host", "version", "class")
            ):
                raise ValueError(
                    "reload target changed or no longer advertises safe HUP; refresh targets"
                )
            signal.pidfd_send_signal(fd, signal.SIGHUP)
            LOG.debug(
                "Sent SIGHUP to verified terminal pid=%s window=%s version=%s",
                current["pid"],
                hex(current["window"]),
                current["version"],
            )
        finally:
            os.close(fd)


def theme_path(name, directory):
    path = Path(name).expanduser()
    if not path.is_file():
        path = directory / name
    if not path.is_file():
        raise ValueError(f"theme not found: {name}")
    return path


def describe_snapshot(snapshot):
    lines = []
    for index, screen in enumerate(snapshot["screens"]):
        for name, value in screen.items():
            lines.append(
                f"! Screen {index}: {name}" + (" (absent)" if value is None else "")
            )
            if value is not None:
                text = base64.b64decode(value).decode(errors="replace")
                lines.append(
                    "".join(
                        c
                        if c in "\n\t" or (ord(c) >= 32 and ord(c) != 127)
                        else f"\\x{ord(c):02x}"
                        for c in text
                    )
                )
    return "\n".join(lines)


class ThemeCatalog:
    """Personal palettes and favorites, independent of X server snapshots."""

    def __init__(self, state_dir, themes_dir):
        self.path = Path(state_dir) / "catalog.json"
        self.themes_dir = Path(themes_dir)
        self.data = {
            "themes": {},
            "favorites": [],
            "default": None,
            "stock_captured": False,
            "appearance": "Dark",
            "font_size": 11,
        }
        if self.path.exists():
            self.data = json.loads(self.path.read_text())
            if (
                not isinstance(self.data, dict)
                or not {"themes", "favorites"} <= self.data.keys()
            ):
                raise ValueError("invalid saved theme catalog")
            for key, value in {
                "default": None,
                "stock_captured": False,
                "appearance": "Dark",
                "font_size": 11,
            }.items():
                self.data.setdefault(key, value)
            if self.data["default"] is not None and not isinstance(
                self.data["default"], str
            ):
                raise ValueError("invalid default theme")
            if not isinstance(self.data["themes"], dict) or not isinstance(
                self.data["favorites"], list
            ):
                raise ValueError("invalid saved theme catalog")
            for key, colors in self.data["themes"].items():
                if not isinstance(key, str) or not isinstance(colors, dict):
                    raise ValueError("invalid saved theme")
                render(colors, "XTerm")
            if not all(isinstance(key, str) for key in self.data["favorites"]):
                raise ValueError("invalid saved favorites")

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        with tempfile.NamedTemporaryFile(
            mode="w", dir=self.path.parent, delete=False
        ) as stream:
            temporary = Path(stream.name)
            try:
                json.dump(self.data, stream, indent=2)
                stream.flush()
                temporary.replace(self.path)
            finally:
                temporary.unlink(missing_ok=True)

    def entries(self, catalog):
        if catalog in ("Ghostty", "iTerm2"):
            return (
                [("xterm/default", "xterm default colors")]
                + [
                    ("ghostty/" + path.name, path.name)
                    for path in sorted(self.themes_dir.iterdir())
                    if path.is_file()
                ]
                if self.themes_dir.exists()
                else [("xterm/default", "xterm default colors")]
            )
        keys = (
            self.data["favorites"]
            if catalog == "Favorites"
            else [key for key in self.data["themes"] if key.startswith(catalog + "/")]
        )
        entries = [(key, key.split("/", 1)[-1]) for key in sorted(keys)]
        return ([("meta/current", "Current")] if catalog == "Mine" else []) + entries

    def colors(self, key):
        if key == "xterm/default":
            return dict(XTERM_COLORS)
        if key.startswith("ghostty/"):
            name = key.split("/", 1)[1]
            if Path(name).name != name:
                raise ValueError("invalid cached theme name")
            return load_theme(self.themes_dir / name)
        return dict(self.data["themes"][key])

    def store(self, catalog, name, colors):
        if (
            catalog not in ("Mine", "terminal.love")
            or not name.strip()
            or name.strip().casefold() == "current"
        ):
            raise ValueError("choose a name and personal catalog")
        render(colors, "XTerm")
        key = catalog + "/" + name.strip()
        self.data["themes"][key] = dict(colors)
        self.save()
        return key

    def favorite(self, key, enabled):
        self.colors(key)
        keys = set(self.data["favorites"])
        if enabled:
            keys.add(key)
        else:
            keys.discard(key)
        self.data["favorites"] = sorted(keys)
        self.save()

    def initialize_stock(self, colors):
        if not self.data["stock_captured"]:
            render(colors, "XTerm")
            self.data["themes"].setdefault("Mine/Stock", dict(colors))
            self.data["stock_captured"] = True
            self.save()

    def delete(self, key):
        if not key.startswith("Mine/") or key not in self.data["themes"]:
            raise ValueError("only saved Mine themes can be deleted")
        del self.data["themes"][key]
        self.data["favorites"] = [
            item for item in self.data["favorites"] if item != key
        ]
        if self.data["default"] == key:
            self.data["default"] = None
        self.save()

    def set_default(self, key):
        self.colors(key)
        self.data["default"] = key
        self.save()

    def default_colors(self):
        key = self.data["default"]
        if not key:
            raise ValueError(
                "no default theme selected; choose Set as default in the GUI"
            )
        return self.colors(key)


def is_dark(colors):
    rgb = [int(colors["background"][i : i + 2], 16) / 255 for i in (1, 3, 5)]
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in rgb]
    return (
        sum(v * weight for v, weight in zip(linear, (0.2126, 0.7152, 0.0722))) < 0.179
    )


def preview_html(colors, label):
    """A complete indexed palette plus everyday terminal output."""

    def ink(index, text):
        return (
            f'<span style="color:{colors[f"color{index}"]}">{html.escape(text)}</span>'
        )

    indexed = [colors[f"color{i}"] for i in range(16)]
    levels = (0, 95, 135, 175, 215, 255)
    indexed += [
        f"#{r:02x}{g:02x}{b:02x}" for r in levels for g in levels for b in levels
    ]
    indexed += [f"#{v:02x}{v:02x}{v:02x}" for v in range(8, 239, 10)]
    rows = [
        f"<h2>{html.escape(label)}</h2><p>TERMINAL PREVIEW · normal output, ANSI colors and indexed palette</p>"
    ]
    lines = [
        ink(2, "dev@workstation")
        + ":"
        + ink(4, "~/workspace/project")
        + " $ git status --short",
        ink(2, " M tools/theme-setter-utility.py"),
        ink(1, "?? themes/my-evening-theme"),
        "",
        "$ ls -lh",
        "drwxr-xr-x  dev  staff  4.0K  Oct 13  " + ink(12, "src/"),
        "-rwxr-xr-x  dev  staff   28K  Oct 13  " + ink(10, "theme-setter-utility.py"),
        "lrwxrwxrwx  dev  staff    7B  Oct 13  " + ink(14, "current → themes/"),
        "",
        "$ python -m pytest",
        ink(
            6, "========================= test session starts ========================="
        ),
        "tests/test_theme.py "
        + ink(2, "........")
        + "                            [100%]",
        ink(10, "8 passed in 0.42s"),
        ink(3, "warning: using explicit xterm defaults for unspecified colors"),
        ink(9, "error: example diagnostic — can you read this comfortably?"),
        "",
        "$ git diff",
        ink(6, "@@ -1,2 +1,2 @@"),
        ink(1, "-background = #ffffff"),
        ink(2, "+background = #161b22"),
        "",
        ink(13, "def") + " " + ink(12, "apply_theme") + "(palette):",
        "    " + ink(8, "# Preserve the running shell"),
        "    " + ink(13, "return") + " palette.get(" + ink(2, '"background"') + ")",
        "",
        "$ "
        + f'<span style="background:{colors["cursorColor"]};color:{colors["background"]}"> </span>',
    ]
    rows.append("<pre>" + "\n".join(lines) + "</pre>")
    rows.append(
        '<h3>Theme colors · ANSI 0–15</h3><table cellspacing="4" cellpadding="9">'
    )
    for offset in (0, 8):
        rows.append("<tr>")
        for i in range(offset, offset + 8):
            value = indexed[i]
            fg = "#111111" if not is_dark({"background": value}) else "#ffffff"
            rows.append(
                f'<td bgcolor="{value}" style="color:{fg}">{i:02d}<br>{value}</td>'
            )
        rows.append("</tr>")
    rows.append(
        '</table><h3>Full indexed palette · 0–255</h3><p>0–15 use your theme; 16–255 are the standard color cube and grayscale.</p><table cellspacing="1" cellpadding="3">'
    )
    for offset in range(0, 256, 16):
        rows.append("<tr>")
        for i in range(offset, offset + 16):
            fg = "#111111" if not is_dark({"background": indexed[i]}) else "#ffffff"
            rows.append(f'<td bgcolor="{indexed[i]}" style="color:{fg}">{i:03d}</td>')
        rows.append("</tr>")
    rows.append("</table>")
    return "".join(rows)


def create_window(args):
    if args.display:
        os.environ["DISPLAY"] = args.display
    if os.environ.get("DISPLAY"):
        os.environ["QT_QPA_PLATFORM"] = "xcb"
    from PySide6.QtCore import QEvent, QObject, Qt, QTimer, QUrl, Signal
    from PySide6.QtGui import (
        QColor,
        QDesktopServices,
        QFontDatabase,
        QIcon,
        QPainter,
        QPen,
        QPixmap,
    )
    from PySide6.QtWidgets import (
        QApplication,
        QColorDialog,
        QGridLayout,
        QScrollArea,
        QCheckBox,
        QComboBox,
        QInputDialog,
        QFileDialog,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QListWidget,
        QListWidgetItem,
        QMainWindow,
        QMenu,
        QMessageBox,
        QPlainTextEdit,
        QPushButton,
        QSplitter,
        QTabWidget,
        QTextBrowser,
        QVBoxLayout,
        QWidget,
    )

    class FetchWorker(QObject):
        done = Signal(str)

        def run(self):
            try:
                fetch(args.themes_dir)
                self.done.emit("")
            except Exception as error:
                self.done.emit(str(error))

    def control_icon(kind, color):
        pixmap = QPixmap(20, 20)
        pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setPen(QPen(QColor(color), 1.5))
        if kind == "minimize":
            painter.drawLine(4, 13, 16, 13)
        elif kind == "maximize":
            painter.drawRect(4, 4, 12, 12)
        elif kind == "restore":
            painter.drawRect(7, 3, 10, 10)
            painter.fillRect(3, 7, 10, 10, QColor(color).darker(500))
            painter.drawRect(3, 7, 10, 10)
        else:
            painter.drawLine(5, 5, 15, 15)
            painter.drawLine(15, 5, 5, 15)
        painter.end()
        return QIcon(pixmap)

    class TitleBar(QWidget):
        def __init__(self, window):
            super().__init__(window)
            self.owner = window
            self.anchor = None
            self.setFixedHeight(44)

        def mousePressEvent(self, event):
            if event.button() == Qt.MouseButton.LeftButton:
                LOG.debug("Title bar drag")
                handle = self.owner.windowHandle()
                if handle is None or not handle.startSystemMove():
                    self.anchor = event.globalPosition().toPoint() - self.owner.pos()
                    self.grabMouse()
                event.accept()

        def mouseMoveEvent(self, event):
            if self.anchor is not None and event.buttons() & Qt.MouseButton.LeftButton:
                self.owner.move(event.globalPosition().toPoint() - self.anchor)

        def mouseReleaseEvent(self, event):
            if self.anchor is not None:
                self.anchor = None
                self.releaseMouse()

        def mouseDoubleClickEvent(self, event):
            if event.button() == Qt.MouseButton.LeftButton:
                self.owner.toggle_maximize()

    class ResizeHandle(QWidget):
        def __init__(self, window, edges):
            super().__init__(window)
            self.owner, self.edges = window, edges
            self.anchor = None
            self.setObjectName("resizeHandle")
            horizontal = bool(edges & (Qt.Edge.LeftEdge | Qt.Edge.RightEdge))
            vertical = bool(edges & (Qt.Edge.TopEdge | Qt.Edge.BottomEdge))
            corner = horizontal and vertical
            diagonal = bool(edges & Qt.Edge.LeftEdge) == bool(edges & Qt.Edge.TopEdge)
            self.setCursor(
                (
                    Qt.CursorShape.SizeFDiagCursor
                    if diagonal
                    else Qt.CursorShape.SizeBDiagCursor
                )
                if corner
                else (
                    Qt.CursorShape.SizeHorCursor
                    if horizontal
                    else Qt.CursorShape.SizeVerCursor
                )
            )
            self.setToolTip("Drag to resize")

        def paintEvent(self, event):
            painter = QPainter(self)
            painter.setPen(QPen(QColor("#769fff"), 1))
            if self.width() > 8 and self.height() > 8:
                left = bool(self.edges & Qt.Edge.LeftEdge)
                top = bool(self.edges & Qt.Edge.TopEdge)
                x, y = (
                    (3 if left else self.width() - 4),
                    (3 if top else self.height() - 4),
                )
                painter.drawLine(x, y, x + (7 if left else -7), y)
                painter.drawLine(x, y, x, y + (7 if top else -7))

        def mousePressEvent(self, event):
            if event.button() == Qt.MouseButton.LeftButton:
                LOG.debug("Resize started: %s", self.edges)
                handle = self.owner.windowHandle()
                if handle is None or not handle.startSystemResize(self.edges):
                    self.anchor = event.globalPosition().toPoint()
                    self.initial = self.owner.geometry()
                    self.grabMouse()
                event.accept()

        def mouseMoveEvent(self, event):
            if self.anchor is None or not event.buttons() & Qt.MouseButton.LeftButton:
                return
            delta = event.globalPosition().toPoint() - self.anchor
            rect = self.initial.adjusted(0, 0, 0, 0)
            if self.edges & Qt.Edge.LeftEdge:
                rect.setLeft(
                    min(
                        rect.right() - self.owner.minimumWidth() + 1,
                        rect.left() + delta.x(),
                    )
                )
            if self.edges & Qt.Edge.RightEdge:
                rect.setRight(
                    max(
                        rect.left() + self.owner.minimumWidth() - 1,
                        rect.right() + delta.x(),
                    )
                )
            if self.edges & Qt.Edge.TopEdge:
                rect.setTop(
                    min(
                        rect.bottom() - self.owner.minimumHeight() + 1,
                        rect.top() + delta.y(),
                    )
                )
            if self.edges & Qt.Edge.BottomEdge:
                rect.setBottom(
                    max(
                        rect.top() + self.owner.minimumHeight() - 1,
                        rect.bottom() + delta.y(),
                    )
                )
            self.owner.setGeometry(rect)

        def mouseReleaseEvent(self, event):
            if self.anchor is not None:
                self.anchor = None
                self.releaseMouse()

    class Window(QMainWindow):
        def __init__(self):
            super().__init__()
            LOG.info(
                "Creating theme window for DISPLAY=%s",
                args.display or os.environ.get("DISPLAY"),
            )
            self.setWindowFlags(
                Qt.WindowType.Window | Qt.WindowType.FramelessWindowHint
            )
            self.resize_handles = [
                ResizeHandle(self, edge)
                for edge in (
                    Qt.Edge.LeftEdge,
                    Qt.Edge.RightEdge,
                    Qt.Edge.TopEdge,
                    Qt.Edge.BottomEdge,
                    Qt.Edge.LeftEdge | Qt.Edge.TopEdge,
                    Qt.Edge.RightEdge | Qt.Edge.TopEdge,
                    Qt.Edge.LeftEdge | Qt.Edge.BottomEdge,
                    Qt.Edge.RightEdge | Qt.Edge.BottomEdge,
                )
            ]
            self.setMinimumSize(900, 600)
            self.setWindowTitle("themesetter")
            self.resize(1280, 900)
            self.resources = XResources(args.display)
            self.setter = ThemeSetter(self.resources, args.state_dir)
            self.catalog_store = ThemeCatalog(args.state_dir, args.themes_dir)
            self.catalog_store.initialize_stock(
                self.resources.current_colors(args.application_class)
            )
            self.dirty = False
            self.font_size = max(8, min(24, int(self.catalog_store.data["font_size"])))
            self.theme_key = None
            self.colors = dict(XTERM_COLORS)
            self.label = "xterm default colors"
            self.worker = FetchWorker()
            self.worker.done.connect(self.fetched)
            body = QWidget()
            self.setCentralWidget(body)
            layout = QVBoxLayout(body)
            # Only the resize border surrounds the header; content padding belongs
            # below it, so there is no dead band above or beside the draggable bar.
            layout.setContentsMargins(6, 6, 6, 6)
            layout.setSpacing(0)
            self.title_bar = TitleBar(self)
            heading = QHBoxLayout(self.title_bar)
            heading.setContentsMargins(12, 0, 4, 0)
            heading.setSpacing(6)
            self.title_label = QLabel("themesetter — reading current palette…")
            self.title_label.setObjectName("caption")
            self.title_label.setAttribute(
                Qt.WidgetAttribute.WA_TransparentForMouseEvents
            )
            heading.addWidget(self.title_label, 1)
            # Keep the appearance preference model; the title bar exposes it as one icon.
            self.appearance = QComboBox(self)
            self.appearance.hide()
            self.appearance.addItems(["Dark", "Light"])
            self.appearance.setCurrentText(self.catalog_store.data["appearance"])
            self.appearance.currentTextChanged.connect(self.style_interface)
            self.appearance_button = QPushButton()
            self.appearance_button.clicked.connect(
                lambda: self.appearance.setCurrentText(
                    "Light" if self.appearance.currentText() == "Dark" else "Dark"
                )
            )
            self.menu_button = QPushButton("☰")
            self.menu_button.setToolTip("Menu")
            self.menu_button.setAccessibleName("Menu")
            self.window_menu = QMenu(self)
            self.menu_button.setMenu(self.window_menu)
            self.minimize_button = QPushButton()
            self.minimize_button.setToolTip("Minimize")
            self.minimize_button.setAccessibleName("Minimize")
            self.minimize_button.clicked.connect(self.showMinimized)
            self.maximize_button = QPushButton()
            self.maximize_button.setToolTip("Maximize")
            self.maximize_button.clicked.connect(self.toggle_maximize)
            self.close_button = QPushButton()
            self.close_button.setObjectName("closeButton")
            self.close_button.setToolTip("Close")
            self.close_button.setAccessibleName("Close")
            self.close_button.clicked.connect(self.close)
            for button in (
                self.appearance_button,
                self.menu_button,
                self.minimize_button,
                self.maximize_button,
                self.close_button,
            ):
                if button is not self.close_button:
                    button.setObjectName("titleButton")
                button.setFixedSize(36, 34)
                heading.addWidget(button)
            layout.addWidget(self.title_bar)
            content = QWidget()
            layout.addWidget(content, 1)
            layout = QVBoxLayout(content)
            layout.setContentsMargins(18, 12, 18, 10)
            layout.setSpacing(14)
            controls = QHBoxLayout()
            self.catalog = QComboBox()
            self.catalog.addItems(
                ["Mine", "Ghostty", "iTerm2", "terminal.love", "Favorites"]
            )
            self.catalog.setCurrentText("Ghostty")
            self.catalog.currentTextChanged.connect(self.populate)
            controls.addWidget(QLabel("Catalog:"))
            controls.addWidget(self.catalog)
            self.search = QLineEdit()
            self.search.setPlaceholderText("Find a theme…")
            self.search.textChanged.connect(self.filter)
            controls.addWidget(self.search, 1)
            self.tone = QComboBox()
            self.tone.addItems(["All palettes", "Dark palettes", "Light palettes"])
            self.tone.currentTextChanged.connect(
                lambda: self.filter(self.search.text())
            )
            controls.addWidget(self.tone)
            self.fetch_button = QPushButton("Fetch Ghostty collection")
            self.fetch_button.clicked.connect(self.fetch_themes)
            controls.addWidget(self.fetch_button)
            open_button = QPushButton("Open theme file…")
            open_button.clicked.connect(self.open_file)
            controls.addWidget(open_button)
            layout.addLayout(controls)
            links = QHBoxLayout()
            self.catalog_note = QLabel()
            self.catalog_note.setWordWrap(True)
            links.addWidget(self.catalog_note, 1)
            self.browse_button = QPushButton("Browse catalog online")
            self.browse_button.clicked.connect(self.browse_catalog)
            links.addWidget(self.browse_button)
            self.favorite_button = QPushButton("☆ Favorite")
            self.favorite_button.clicked.connect(lambda: self.act(self.toggle_favorite))
            links.addWidget(self.favorite_button)
            save_button = QPushButton("Save selection to Mine…")
            save_button.clicked.connect(lambda: self.act(self.save_mine))
            links.addWidget(save_button)
            layout.addLayout(links)
            personal = QHBoxLayout()
            self.save_button = QPushButton("Save edits")
            self.save_button.clicked.connect(lambda: self.act(self.save_edits))
            self.delete_button = QPushButton("Delete from Mine")
            self.delete_button.clicked.connect(self.confirm_delete)
            self.default_button = QPushButton("Set as default")
            self.default_button.clicked.connect(lambda: self.act(self.set_default))
            personal.addWidget(self.save_button)
            personal.addWidget(self.delete_button)
            personal.addWidget(self.default_button)
            self.default_label = QLabel()
            self.default_label.setObjectName("muted")
            personal.addWidget(self.default_label, 1)
            layout.addLayout(personal)
            split = QSplitter(Qt.Orientation.Horizontal)
            self.themes = QListWidget()
            self.themes.setMinimumWidth(230)
            self.themes.currentItemChanged.connect(self.selected)
            split.addWidget(self.themes)
            tabs = QTabWidget()
            self.preview = QTextBrowser()
            self.preview.setOpenLinks(False)
            self.preview.setFont(
                QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
            )
            tabs.addTab(self.preview, "Terminal preview")
            editor_scroll = QScrollArea()
            editor_scroll.setWidgetResizable(True)
            editor = QWidget()
            grid = QGridLayout(editor)
            self.color_fields = {}
            self.color_buttons = {}
            grid.addWidget(
                QLabel(
                    "Edit a Mine palette, then Save edits. Copy other palettes to Mine first."
                ),
                0,
                0,
                1,
                6,
            )
            for i, name in enumerate(RESOURCE_NAMES):
                row, column = i // 3 + 1, (i % 3) * 2
                button = QPushButton(name)
                button.clicked.connect(
                    lambda checked=False, name=name: self.choose_color(name)
                )
                field = QLineEdit()
                field.setMaxLength(7)
                field.textEdited.connect(
                    lambda text, name=name: self.edit_color(name, text)
                )
                grid.addWidget(button, row, column)
                grid.addWidget(field, row, column + 1)
                self.color_fields[name] = field
                self.color_buttons[name] = button
            grid.setRowStretch(9, 1)
            editor_scroll.setWidget(editor)
            tabs.addTab(editor_scroll, "Edit palette")
            self.output = QPlainTextEdit()
            self.output.setReadOnly(True)
            tabs.addTab(self.output, "Theme Xresources")
            self.current = QPlainTextEdit()
            self.current.setReadOnly(True)
            tabs.addTab(self.current, "Current database")
            self.conflicts = QPlainTextEdit()
            self.conflicts.setReadOnly(True)
            tabs.addTab(self.conflicts, "Resource conflicts")
            split.addWidget(tabs)
            split.setSizes([290, 750])
            layout.addWidget(split, 1)
            scope = QHBoxLayout()
            scope.addWidget(QLabel("Resource class:"))
            self.resource_class = QLineEdit(args.application_class)
            self.resource_class.textChanged.connect(self.class_changed)
            scope.addWidget(self.resource_class)
            target_row = QHBoxLayout()
            target_row.addWidget(QLabel("Reload target:"))
            self.reload_targets = ReloadTargets(self.resources.display)
            self.reload_target = QComboBox()
            self.reload_target.currentIndexChanged.connect(self.target_changed)
            target_row.addWidget(self.reload_target, 1)
            refresh_targets = QPushButton("Refresh targets")
            refresh_targets.clicked.connect(lambda: self.act(self.refresh_targets))
            target_row.addWidget(refresh_targets)
            layout.addLayout(target_row)
            self.auto_apply = QCheckBox("Live preview: apply selected palette")
            self.auto_apply.setToolTip(
                "Up/down applies each palette to XRDB and HUPs the chosen verified reload target, if any."
            )
            self.auto_apply.toggled.connect(self.toggle_auto_apply)
            layout.addWidget(self.auto_apply)
            self.preview_timer = QTimer(self)
            self.preview_timer.setSingleShot(True)
            self.preview_timer.setInterval(180)
            self.preview_timer.timeout.connect(self.apply_preview)
            self.replace = QCheckBox("Replace entire database when applying")
            scope.addWidget(self.replace)
            layout.addLayout(scope)
            actions = QHBoxLayout()
            for text, callback in (
                ("Apply selected theme", self.apply_theme),
                ("Restore original", self.restore_original),
                ("Blank database", self.blank),
                ("Reset to xterm default colors", self.defaults),
                ("Refresh database view", self.refresh),
            ):
                button = QPushButton(text)
                if text == "Apply selected theme":
                    self.apply_button = button
                    button.setObjectName("primary")
                elif text == "Restore original":
                    self.original_button = button
                button.clicked.connect(
                    lambda checked=False, callback=callback: self.act(callback)
                )
                actions.addWidget(button)
            layout.addLayout(actions)
            self.active_label = QLabel(self)
            self.active_label.hide()
            self.resource_warning = QLabel()
            self.resource_warning.setObjectName("resourceWarning")
            self.resource_warning.setWordWrap(True)
            layout.addWidget(self.resource_warning)
            self.action_status = QLabel(
                "Select a palette. Choose a verified reload target to update one running terminal."
            )
            self.action_status.setWordWrap(True)
            layout.addWidget(self.action_status)
            note = QLabel(
                "Applies foreground, background, cursorColor and color0–color15. HUP is sent only to the selected capability-checked terminal window."
            )
            note.setWordWrap(True)
            layout.addWidget(note)
            for title, callback in (
                ("Import theme…", self.open_file),
                ("Fetch Ghostty collection", self.fetch_themes),
                ("Browse catalog online", self.browse_catalog),
                ("Load saved default", self.load_default),
                ("Refresh active palette", self.refresh),
            ):
                action = self.window_menu.addAction(title)
                action.triggered.connect(
                    lambda checked=False, callback=callback: self.act(callback)
                )
            self.window_menu.addSeparator()
            self.window_menu.addAction(
                "Open log file",
                lambda: QDesktopServices.openUrl(
                    QUrl.fromLocalFile(str(args.state_dir / "theme-setter.log"))
                ),
            )
            self.window_menu.addAction("Quit", self.close)
            self.style_interface()
            self.update_window_controls()
            QApplication.instance().installEventFilter(self)
            self.populate()
            self.refresh_targets(initial=True)
            self.refresh()

        def style_interface(self):
            dark = self.appearance.currentText() == "Dark"
            bg, card, text, muted, border, hover = (
                ("#10151f", "#1a2230", "#e7edf7", "#a4b2c6", "#303d50", "#28364b")
                if dark
                else ("#f0f3f9", "#ffffff", "#202b3c", "#5a6b83", "#cdd6e3", "#e3eaf5")
            )
            self.setStyleSheet(f"""
                QMainWindow, QWidget {{ background:{bg}; color:{text}; font-size:{self.font_size}pt; }}
                QLabel#caption {{ font-size:{self.font_size + 1}pt; font-weight:600; }}
                QPushButton#titleButton, QPushButton#closeButton {{ border:0; padding:0; background:transparent; border-radius:6px; }}
                QPushButton#titleButton:hover {{ background:{hover}; }}
                QPushButton#titleButton {{ font-size:18px; }}
                QPushButton#titleButton::menu-indicator {{ image:none; width:0; }}
                QPushButton#closeButton:hover {{ background:#d43c56; color:white; }}
                QMenu {{ background:{card}; border:1px solid {border}; padding:6px; }}
                QMenu::item {{ padding:8px 24px 8px 12px; border-radius:4px; }}
                QMenu::item:selected {{ background:{hover}; }}
                QWidget#resizeHandle {{ background:transparent; }}
                QLabel#muted {{ color:{muted}; }}
                QLabel#resourceWarning {{ color:{"#ffd080" if dark else "#8a4900"}; font-weight:600; }}
                QPushButton, QComboBox, QLineEdit {{ background:{card}; border:1px solid {border}; border-radius:7px; padding:8px 11px; }}
                QPushButton:hover, QComboBox:hover {{ background:{hover}; border-color:#769fff; }}
                QPushButton:disabled {{ color:{muted}; border-color:{border}; }}
                QPushButton#primary {{ background:#496ee8; color:white; border:0; font-weight:700; }}
                QLineEdit:focus {{ border-color:#769fff; }}
                QListWidget, QPlainTextEdit, QTextBrowser, QScrollArea {{ background:{card}; border:1px solid {border}; border-radius:10px; padding:12px; }}
                QListWidget::item {{ padding:10px; border-radius:6px; }}
                QListWidget::item:selected {{ background:#3855a3; color:white; }}
                QListWidget::item:hover {{ background:{hover}; }}
                QTabWidget::pane {{ border:0; }}
                QTabBar::tab {{ background:{bg}; padding:10px 16px; color:{muted}; border-bottom:2px solid transparent; }}
                QTabBar::tab:selected {{ color:{text}; border-bottom:2px solid #769fff; }}
                QSplitter::handle {{ background:{bg}; width:12px; }}
                QStatusBar {{ color:{muted}; }}
                QComboBox::drop-down {{ border:0; width:24px; }}
                QScrollBar:vertical {{ background:{bg}; width:10px; margin:3px; }}
                QScrollBar::handle:vertical {{ background:{border}; min-height:28px; border-radius:4px; }}
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{ height:0; }}
                QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{ background:transparent; }}
            """)
            self.update_window_controls()
            self.appearance_button.setText("☀" if dark else "☾")
            self.appearance_button.setToolTip(
                "Switch to light interface" if dark else "Switch to dark interface"
            )
            self.appearance_button.setAccessibleName(self.appearance_button.toolTip())
            mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
            mono.setPointSize(self.font_size)
            for widget in (self.preview, self.output, self.current, self.conflicts):
                widget.setFont(mono)
            self.catalog_store.data.update(
                appearance=self.appearance.currentText(), font_size=self.font_size
            )
            self.catalog_store.save()
            if self.theme_key:
                self.draw()

        def update_window_controls(self):
            maximized = self.isMaximized()
            color = "#e7edf7" if self.appearance.currentText() == "Dark" else "#202b3c"
            self.minimize_button.setIcon(control_icon("minimize", color))
            self.maximize_button.setIcon(
                control_icon("restore" if maximized else "maximize", color)
            )
            self.close_button.setIcon(control_icon("close", color))
            self.maximize_button.setToolTip("Restore" if maximized else "Maximize")
            self.maximize_button.setAccessibleName(self.maximize_button.toolTip())
            self.layout_handles()

        def toggle_maximize(self):
            self.showNormal() if self.isMaximized() else self.showMaximized()
            self.update_window_controls()

        def layout_handles(self):
            width, height = self.width(), self.height()
            regions = [
                (0, 14, 6, height - 28),
                (width - 6, 14, 6, height - 28),
                (14, 0, width - 28, 6),
                (14, height - 6, width - 28, 6),
                (0, 0, 14, 14),
                (width - 14, 0, 14, 14),
                (0, height - 14, 14, 14),
                (width - 14, height - 14, 14, 14),
            ]
            for handle, region in zip(self.resize_handles, regions):
                handle.setGeometry(*region)
                handle.setVisible(not self.isMaximized())
                handle.raise_()

        def resizeEvent(self, event):
            super().resizeEvent(event)
            if hasattr(self, "resize_handles"):
                self.layout_handles()

        def changeEvent(self, event):
            super().changeEvent(event)
            if event.type() == QEvent.Type.WindowStateChange and hasattr(
                self, "maximize_button"
            ):
                self.update_window_controls()

        def load_default(self):
            result = self.setter.apply(
                self.catalog_store.default_colors(), self.resource_class.text()
            )
            if result["overridden"]:
                self.action_status.setText(
                    f"Warning: default written to XRDB, but {len(result['overridden'])} colors remain overridden. See Resource conflicts."
                )
                return
            self.catalog_store.data["last_applied"] = self.catalog_store.data["default"]
            self.catalog_store.save()
            self.action_status.setText(
                "Saved default applied to XRDB. " + self.reload_selected()
            )

        def eventFilter(self, watched, event):
            if event.type() == QEvent.Type.MouseButtonPress:
                LOG.debug(
                    "Mouse press: widget=%s name=%s button=%s",
                    type(watched).__name__,
                    watched.objectName(),
                    event.button(),
                )
            if (
                event.type() == QEvent.Type.KeyPress
                and event.modifiers() & Qt.KeyboardModifier.ControlModifier
            ):
                widget = QApplication.focusWidget()
                if widget is not None and (widget is self or self.isAncestorOf(widget)):
                    key = event.key()
                    if key in (Qt.Key.Key_Plus, Qt.Key.Key_Equal, Qt.Key.Key_Minus):
                        self.zoom(1 if key != Qt.Key.Key_Minus else -1)
                        return True
            return super().eventFilter(watched, event)

        def zoom(self, delta):
            self.font_size = max(8, min(24, self.font_size + delta))
            self.style_interface()

        def class_changed(self):
            if self.theme_key == "meta/current":
                self.selected(self.themes.currentItem())
            else:
                self.draw()

        def palette(self, key):
            return (
                self.resources.current_colors(self.resource_class.text())
                if key == "meta/current"
                else self.catalog_store.colors(key)
            )

        def edit_color(self, name, text):
            if not self.theme_key or not self.theme_key.startswith("Mine/"):
                return
            candidate = dict(self.colors, **{name: text})
            try:
                render(candidate, self.resource_class.text())
            except ValueError:
                self.color_fields[name].setStyleSheet("border:1px solid #f26680;")
                self.save_button.setEnabled(False)
                self.apply_button.setEnabled(False)
                return
            self.color_fields[name].setStyleSheet("")
            self.colors = candidate
            self.dirty = True
            self.draw()

        def choose_color(self, name):
            color = QColorDialog.getColor(QColor(self.colors[name]), self, name)
            if color.isValid():
                self.edit_color(name, color.name())

        def save_edits(self):
            if self.theme_key and self.theme_key.startswith("Mine/"):
                self.catalog_store.store(
                    "Mine", self.theme_key.split("/", 1)[1], self.colors
                )
                self.dirty = False
                self.draw()

        def delete_mine(self):
            self.catalog_store.delete(self.theme_key)
            self.populate()

        def confirm_delete(self):
            if (
                QMessageBox.question(
                    self, "Delete palette", f"Delete {self.label} from Mine?"
                )
                == QMessageBox.StandardButton.Yes
            ):
                self.act(self.delete_mine)

        def set_default(self):
            self.save_edits()
            key = self.theme_key
            if key == "meta/current":
                key = self.catalog_store.store("Mine", "Current default", self.colors)
            self.catalog_store.set_default(key)

        def act(self, callback):
            applying = callback.__name__ == "apply_theme"
            if not applying:
                LOG.debug("Action started: %s", callback.__name__)
            try:
                callback()
                self.active_fingerprint = None
                self.refresh()
                if applying:
                    target = self.reload_target.currentData()
                    target_label = (
                        f"pid={target['pid']} window={hex(target['window'])}"
                        if target
                        else "XRDB only"
                    )
                    LOG.log(
                        logging.WARNING
                        if self.action_status.text().startswith("Warning:")
                        else logging.INFO,
                        "Apply theme=%r active=%r target=%s status=%s",
                        self.label,
                        self.title_label.text().removeprefix("themesetter — "),
                        target_label,
                        "overridden; no HUP"
                        if self.action_status.text().startswith("Warning:")
                        else ("applied; HUP sent" if target else "applied; XRDB only"),
                    )
                else:
                    LOG.info("Action completed: %s", callback.__name__)
            except Exception as error:
                LOG.exception("Action failed: %s", callback.__name__)
                self.action_status.setText(
                    f"Failed: {error}. See stdout or theme-setter.log."
                )

        def populate(self):
            previous = self.theme_key
            self.themes.clear()
            catalog = self.catalog.currentText()
            LOG.debug("Catalog: %s", catalog)
            for key, name in self.catalog_store.entries(catalog):
                item = QListWidgetItem(name)
                item.setData(Qt.ItemDataRole.UserRole, key)
                item.setToolTip(key)
                self.themes.addItem(item)
                if key == previous:
                    self.themes.setCurrentItem(item)
            if self.themes.currentItem() is None:
                self.themes.setCurrentRow(0)
            self.filter(self.search.text())
            self.catalog_note.setText(
                {
                    "Mine": "Your saved and imported palettes; stored locally in catalog.json.",
                    "Ghostty": "Cached collection pinned by Ghostty, sourced from iTerm2 Color Schemes.",
                    "iTerm2": "The same iTerm2 Color Schemes collection distributed by Ghostty, in Ghostty format.",
                    "terminal.love": "Browse online, then open an exported Ghostty or Xresources theme to keep it here.",
                    "Favorites": "Your saved favorites across catalogs.",
                }[catalog]
            )
            self.browse_button.setEnabled(
                catalog in ("Ghostty", "iTerm2", "terminal.love")
            )

        def browse_catalog(self):
            url = (
                "https://terminal.love/"
                if self.catalog.currentText() == "terminal.love"
                else "https://iterm2colorschemes.com/"
            )
            QDesktopServices.openUrl(QUrl(url))

        def toggle_favorite(self):
            if self.theme_key:
                enabled = self.theme_key not in self.catalog_store.data["favorites"]
                self.catalog_store.favorite(self.theme_key, enabled)
                self.populate()

        def save_mine(self):
            name, accepted = QInputDialog.getText(
                self, "Save palette to Mine", "Theme name:", text=self.label
            )
            if accepted and name.strip():
                self.theme_key = self.catalog_store.store("Mine", name, self.colors)
                self.search.clear()
                self.catalog.setCurrentText("Mine")
                self.populate()

        def filter(self, text):
            visible = []
            for i in range(self.themes.count()):
                item = self.themes.item(i)
                matches = text.casefold() in item.text().casefold()
                if matches and self.tone.currentIndex():
                    try:
                        dark = is_dark(
                            self.palette(item.data(Qt.ItemDataRole.UserRole))
                        )
                        matches = dark == (self.tone.currentIndex() == 1)
                    except (ValueError, OSError, KeyError):
                        matches = False
                item.setHidden(not matches)
                if not item.isHidden():
                    visible.append(item)
            current = self.themes.currentItem()
            if not visible:
                self.themes.setCurrentRow(-1)
            elif current is None or current.isHidden():
                self.themes.setCurrentItem(visible[0])

        def selected(self, item, previous=None):
            self.preview_timer.stop()
            self.theme_key = None
            self.dirty = False
            self.favorite_button.setEnabled(False)
            self.save_button.setEnabled(False)
            self.delete_button.setEnabled(False)
            self.default_button.setEnabled(False)
            if item is None:
                self.apply_button.setEnabled(False)
                return
            try:
                key = item.data(Qt.ItemDataRole.UserRole)
                self.colors = self.palette(key)
                self.theme_key = key
                self.label = item.text()
                self.favorite_button.setEnabled(key != "meta/current")
                self.default_button.setEnabled(True)
                self.delete_button.setEnabled(key.startswith("Mine/"))
                self.favorite_button.setText(
                    "★ Unfavorite"
                    if key in self.catalog_store.data["favorites"]
                    else "☆ Favorite"
                )
                self.draw()
                if (
                    self.auto_apply.isChecked()
                    and self.apply_button.isEnabled()
                    and key != "meta/current"
                ):
                    self.preview_timer.start()
            except Exception as error:
                LOG.exception("Cannot load selected palette")
                self.apply_button.setEnabled(False)
                self.action_status.setText(f"Cannot load palette: {error}")
                self.statusBar().showMessage(str(error))

        def draw(self):
            colors = self.colors
            self.preview.setStyleSheet(
                f"background: {colors['background']}; color: {colors['foreground']};"
            )
            self.preview.setHtml(preview_html(colors, self.label))
            editable = bool(self.theme_key and self.theme_key.startswith("Mine/"))
            self.save_button.setEnabled(editable and self.dirty)
            for name, field in self.color_fields.items():
                field.setText(colors[name])
                field.setEnabled(editable)
                self.color_buttons[name].setEnabled(editable)
                value = colors[name]
                contrast = "#ffffff" if is_dark({"background": value}) else "#111111"
                self.color_buttons[name].setStyleSheet(
                    f"background:{value};color:{contrast};"
                )
            try:
                self.output.setPlainText(
                    render(colors, self.resource_class.text(), self.label)
                )
                self.apply_button.setEnabled(self.theme_key is not None)
            except ValueError as error:
                self.apply_button.setEnabled(False)
                self.output.setPlainText(str(error))

        def open_file(self):
            path, _ = QFileDialog.getOpenFileName(
                self, "Open Ghostty or Xresources theme"
            )
            if path:

                def load():
                    colors = load_theme(Path(path))
                    catalog = (
                        "terminal.love"
                        if self.catalog.currentText() == "terminal.love"
                        else "Mine"
                    )
                    self.theme_key = self.catalog_store.store(
                        catalog, Path(path).name, colors
                    )
                    self.search.clear()
                    self.catalog.setCurrentText(catalog)
                    self.populate()

                self.act(load)

        def fetch_themes(self):
            self.fetch_button.setEnabled(False)
            self.statusBar().showMessage(
                "Fetching the collection pinned by the local Ghostty checkout…"
            )
            threading.Thread(target=self.worker.run, daemon=True).start()

        def fetched(self, error):
            self.fetch_button.setEnabled(True)
            if error:
                QMessageBox.critical(self, "Fetch themes", error)
            else:
                self.populate()
                self.refresh()

        def refresh_targets(self, initial=False):
            selected = self.reload_target.currentData()
            preferred = selected["window"] if selected else None
            if initial:
                launching = os.environ.get("WINDOWID", "")
                if re.fullmatch(r"(?:0x[0-9a-fA-F]+|[0-9]+)", launching):
                    preferred = int(launching, 16 if launching.startswith("0x") else 10)
            self.reload_target.blockSignals(True)
            self.reload_target.clear()
            self.reload_target.addItem("XRDB only — no terminal signal", None)
            try:
                for target in self.reload_targets.discover():
                    self.reload_target.addItem(
                        f"{target['title'][:70]} · {target['version']} · PID {target['pid']} · {hex(target['window'])}",
                        target,
                    )
                    if target["window"] == preferred:
                        self.reload_target.setCurrentIndex(
                            self.reload_target.count() - 1
                        )
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                LOG.warning("Cannot discover reload targets: %s", error)
                self.action_status.setText(
                    f"XRDB only: reload discovery unavailable: {error}"
                )
            self.reload_target.blockSignals(False)
            self.target_changed()

        def target_changed(self):
            target = self.reload_target.currentData()
            if target:
                LOG.info(
                    "Reload target: pid=%s window=%s version=%s",
                    target["pid"],
                    hex(target["window"]),
                    target["version"],
                )
                self.resource_class.setText(target["class"])
            else:
                LOG.info("Reload target: XRDB only; no terminal signal")

        def reload_selected(self):
            target = self.reload_target.currentData()
            if not target:
                return "No terminal reloaded (XRDB only)."
            self.reload_targets.hup(target)
            return f"HUP sent to PID {target['pid']} / {hex(target['window'])}."

        def toggle_auto_apply(self, enabled):
            LOG.info(
                "Live preview %s",
                "enabled" if enabled else "disabled",
            )
            if enabled:
                self.replace.setChecked(False)
                if (
                    self.theme_key
                    and self.theme_key != "meta/current"
                    and self.apply_button.isEnabled()
                ):
                    self.preview_timer.start()
            else:
                self.preview_timer.stop()
            # Live browsing must preserve unrelated settings in the resource database.
            self.replace.setEnabled(not enabled)

        def apply_preview(self):
            if (
                self.auto_apply.isChecked()
                and self.theme_key
                and self.theme_key != "meta/current"
                and self.apply_button.isEnabled()
            ):
                self.act(self.apply_theme)

        def apply_theme(self):
            self.preview_timer.stop()
            result = self.setter.apply(
                self.colors, self.resource_class.text(), self.replace.isChecked()
            )
            if result["overridden"]:
                keys = sorted(
                    {
                        entry["key"]
                        for name, entry in result["diagnostics"]["winners"].items()
                        if name in result["overridden"]
                    }
                )
                self.action_status.setText(
                    f"Warning: wrote {self.label} to XRDB, but {len(result['overridden'])} colors are overridden by "
                    + ", ".join(keys[:4])
                    + ". See Resource conflicts. Running terminals were not reloaded."
                )
                return
            if self.theme_key != "meta/current":
                self.catalog_store.data["last_applied"] = self.theme_key
                self.catalog_store.save()
            reload_message = self.reload_selected()
            self.action_status.setText(
                f"Applied {self.label} to XRDB at {datetime.now():%H:%M:%S}. {reload_message}"
            )

        def restore_original(self):
            self.setter.restore_original()
            self.action_status.setText(
                "Original XRDB restored. " + self.reload_selected()
            )

        def blank(self):
            self.setter.apply()
            self.action_status.setText(
                "XRDB cleared. No HUP sent: reload retains omitted terminal settings."
            )

        def defaults(self):
            self.setter.apply(XTERM_COLORS, self.resource_class.text(), replace=True)
            self.action_status.setText(
                "xterm defaults applied. " + self.reload_selected()
            )

        def refresh(self):
            self.default_label.setText(
                "Default: "
                + (self.catalog_store.data["default"] or "not selected")
                + "  ·  Zoom: Ctrl + / −"
            )
            live = self.resources.current_colors(self.resource_class.text())
            diagnostics = self.resources.color_diagnostics(self.resource_class.text())
            alternatives = diagnostics["alternatives"]
            keys = sorted({entry["key"] for entry in alternatives.values()})
            variants = diagnostics["case_variants"]
            messages = []
            if keys:
                messages.append(
                    "Other color keys are active: "
                    + ", ".join(keys[:4])
                    + ("…" if len(keys) > 4 else "")
                    + ". They may override theme writes; see Resource conflicts."
                )
            if variants:
                messages.append(
                    "Case-sensitive resource prefixes found: "
                    + ", ".join(variants[:4])
                    + ". Xterm, xterm and XTerm are different."
                )
            self.resource_warning.setText("\n".join(messages))
            self.resource_warning.setVisible(bool(messages))
            details = [
                "Theme writes use " + self.resource_class.text() + "*RESOURCE.",
                "Xrm is case-sensitive: xterm is the application name; XTerm is the default class.",
                "Name bindings (xterm) can beat class bindings (XTerm); tight paths and screen resources can also win.",
                "Winning keys for xterm.vt100:",
            ]
            for name in RESOURCE_NAMES:
                entry = diagnostics["winners"].get(name)
                details.append(
                    f"{name}: {entry['key']}: {entry['value']} [{entry['source']}]"
                    if entry
                    else f"{name}: explicit xterm fallback"
                )
            if variants:
                details += [
                    "",
                    "Case variants present (not equivalent to XTerm):",
                ] + variants
            details += [
                "",
                "Merge preserves existing keys. Resolve the listed overrides in XRDB or your startup resources.",
                "Replace entire database clears ALL XRDB settings, including fonts and bindings; use only if you intend that.",
                "The utility does not edit .Xdefaults/.Xresources or silently delete conflicting keys.",
            ]
            self.conflicts.setPlainText("\n".join(details))
            fingerprint = tuple(live[name] for name in RESOURCE_NAMES)
            if getattr(self, "active_fingerprint", None) != fingerprint:
                self.active_fingerprint = fingerprint
                label = "Custom / unmatched colors"
                preferred = self.catalog_store.data.get("last_applied")
                candidates = (
                    ([preferred] if preferred else [])
                    + list(self.catalog_store.data["themes"])
                    + [key for key, name in self.catalog_store.entries("Ghostty")]
                )
                for key in candidates:
                    try:
                        if self.catalog_store.colors(key) == live:
                            label = key
                            break
                    except (OSError, ValueError, KeyError):
                        continue
                self.active_label.setText(
                    f"Active XRDB palette: {label}  ·  background {live['background']} / foreground {live['foreground']}"
                )
                self.title_label.setText("themesetter — " + label.split("/", 1)[-1])
                self.title_label.setToolTip(self.active_label.text())
                self.setWindowTitle(self.title_label.text())
                LOG.debug("%s", self.active_label.text())
            if self.theme_key == "meta/current":
                self.colors = live
                self.draw()
            self.current.setPlainText(describe_snapshot(self.resources.snapshot()))
            original = self.setter.original_path()
            self.original_button.setEnabled(original is not None and original.exists())
            self.statusBar().showMessage(
                f"DISPLAY={self.resources.display} · {self.themes.count()} catalog entries · Original: {original or 'not captured yet'}"
            )

        def closeEvent(self, event):
            self.preview_timer.stop()
            QApplication.instance().removeEventFilter(self)
            self.resources.close()
            event.accept()

    return Window()


def gui(args):
    if args.display:
        os.environ["DISPLAY"] = args.display
    if os.environ.get("DISPLAY"):
        os.environ["QT_QPA_PLATFORM"] = "xcb"
    from PySide6.QtCore import QTimer, qInstallMessageHandler
    from PySide6.QtWidgets import QApplication

    def qt_message(kind, context, message):
        LOG.warning("Qt: %s", message)

    def exception_hook(kind, value, traceback):
        LOG.error("Unhandled GUI callback exception", exc_info=(kind, value, traceback))

    qInstallMessageHandler(qt_message)
    sys.excepthook = exception_hook
    app = QApplication(sys.argv[:1])
    window = create_window(args)
    # Qt's C++ event loop otherwise prevents Python from processing Ctrl+C while idle.
    heartbeat = QTimer()
    heartbeat.timeout.connect(lambda: None)
    heartbeat.start(100)
    previous = {}

    def stop(number, frame):
        LOG.info("Received %s; closing", signal.Signals(number).name)
        window.close()
        app.quit()

    for number in (signal.SIGINT, signal.SIGTERM):
        previous[number] = signal.signal(number, stop)
    window.show()
    LOG.info(
        "GUI ready; Ctrl+C exits. HUP is restricted to the selected verified reload target."
    )
    try:
        return app.exec()
    finally:
        heartbeat.stop()
        window.resources.close()
        for number, handler in previous.items():
            signal.signal(number, handler)
        LOG.info("GUI stopped")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--load-default",
        action="store_true",
        help="apply the saved default palette without opening the GUI or sending signals",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="include debug details in stdout and the log file",
    )
    parser.add_argument("--display")
    parser.add_argument("--themes-dir", type=Path, default=DEFAULT_THEMES)
    parser.add_argument("--state-dir", type=Path, default=STATE_DIR)
    parser.add_argument("--class", dest="application_class", default="XTerm")
    commands = parser.add_subparsers(dest="command")
    commands.add_parser("gui")
    commands.add_parser("list")
    commands.add_parser("fetch")
    for name in ("show", "set"):
        command = commands.add_parser(name)
        command.add_argument("theme")
        if name == "set":
            command.add_argument("--replace", action="store_true")
    command = commands.add_parser("reset")
    command.add_argument("target", choices=("original", "blank", "xterm"))
    command = commands.add_parser("capture-original")
    command.add_argument("--replace", action="store_true")
    commands.add_parser("status")
    args = parser.parse_args(argv)
    args.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    handlers = [
        logging.StreamHandler(
            sys.stdout
            if args.command in (None, "gui") and not args.load_default
            else sys.stderr
        )
    ]
    log_path = args.state_dir / "theme-setter.log"
    log_path.touch(mode=0o600, exist_ok=True)
    from logging.handlers import RotatingFileHandler

    handlers.append(RotatingFileHandler(log_path, maxBytes=1024 * 1024, backupCount=2))
    logging.basicConfig(
        level=logging.DEBUG if args.debug else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        handlers=handlers,
        force=True,
    )
    LOG.info(
        "Starting command=%s display=%s log=%s",
        args.command or ("load-default" if args.load_default else "gui"),
        args.display or os.environ.get("DISPLAY"),
        log_path,
    )
    resources = None
    try:
        if args.load_default:
            if args.command is not None:
                raise ValueError("--load-default cannot be combined with a subcommand")
            catalog = ThemeCatalog(args.state_dir, args.themes_dir)
            colors = catalog.default_colors()
            resources = XResources(args.display)
            catalog.initialize_stock(resources.current_colors(args.application_class))
            result = ThemeSetter(resources, args.state_dir).apply(
                colors, args.application_class
            )
            if result["overridden"]:
                print(
                    f"Default resources written; {len(result['overridden'])} colors remain overridden. See warnings. No signals sent."
                )
            else:
                catalog.data["last_applied"] = catalog.data["default"]
                catalog.save()
                print("Default palette applied to Xresources. No signals sent.")
            return 0
        if args.command in (None, "gui"):
            return gui(args)
        if args.command == "list":
            for path in sorted(args.themes_dir.iterdir()):
                if path.is_file():
                    print(path.name)
        elif args.command == "fetch":
            fetch(args.themes_dir)
        elif args.command == "show":
            print(
                render(
                    load_theme(theme_path(args.theme, args.themes_dir)),
                    args.application_class,
                    args.theme,
                ),
                end="",
            )
        else:
            resources = XResources(args.display)
            setter = ThemeSetter(resources, args.state_dir)
            catalog = ThemeCatalog(args.state_dir, args.themes_dir)
            catalog.initialize_stock(resources.current_colors(args.application_class))
            if args.command == "set":
                result = setter.apply(
                    load_theme(theme_path(args.theme, args.themes_dir)),
                    args.application_class,
                    args.replace,
                )
                if result["overridden"]:
                    LOG.warning(
                        "Theme resources written, but %d colors remain overridden; inspect Resource conflicts in the GUI",
                        len(result["overridden"]),
                    )
            elif args.command == "reset":
                if args.target == "original":
                    setter.restore_original()
                elif args.target == "blank":
                    setter.apply()
                else:
                    setter.apply(XTERM_COLORS, args.application_class, replace=True)
            elif args.command == "capture-original":
                print(setter.capture(args.replace))
            else:
                print(
                    f"DISPLAY={resources.display}\nOriginal: {setter.original_path() or 'not captured'}"
                )
                print(describe_snapshot(resources.snapshot()))
            if args.command not in ("status", "capture-original"):
                print(
                    "Xresources updated. No signals sent; existing terminals are unchanged."
                )
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"theme-setter: {error}\n")
    finally:
        if resources:
            resources.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
