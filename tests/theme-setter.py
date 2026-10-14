#!/usr/bin/env python3
"""Theme parsing plus optional isolated-Xvfb database and PySide checks."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from theme_resources import RESOURCE_NAMES, XTERM_COLORS, load_theme, render  # noqa: E402

spec = importlib.util.spec_from_file_location(
    "theme_setter", ROOT / "tools/theme-setter-utility.py"
)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def ghostty_file(path):
    path.write_text(
        "\n".join(
            [
                f"foreground = {XTERM_COLORS['foreground']}",
                f"background = {XTERM_COLORS['background']}",
            ]
            + [f"palette = {i}={XTERM_COLORS[f'color{i}']}" for i in range(16)]
        )
    )
    return path


class Parsing(unittest.TestCase):
    def test_ghostty_and_xresources_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = ghostty_file(Path(directory) / "theme")
            self.assertEqual(load_theme(path), XTERM_COLORS)
            path.write_text(render(XTERM_COLORS, "XTerm") + "XTerm*font: unrelated\n")
            self.assertEqual(load_theme(path), XTERM_COLORS)
            self.assertEqual(len(RESOURCE_NAMES), 19)

    def test_catalog_persistence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            themes = root / "themes"
            themes.mkdir()
            ghostty_file(themes / "Example")
            catalog = tool.ThemeCatalog(root / "state", themes)
            self.assertEqual(catalog.entries("Ghostty"), catalog.entries("iTerm2"))
            key = catalog.store("Mine", "My palette", XTERM_COLORS)
            catalog.favorite(key, True)
            catalog.favorite("ghostty/Example", True)
            imported = catalog.store("terminal.love", "Exported", XTERM_COLORS)
            reopened = tool.ThemeCatalog(root / "state", themes)
            self.assertEqual(reopened.colors(key), XTERM_COLORS)
            self.assertEqual(
                reopened.entries("terminal.love"), [(imported, "Exported")]
            )
            self.assertEqual(len(reopened.entries("Favorites")), 2)
            reopened.favorite(key, False)
            self.assertEqual(
                reopened.entries("Favorites"), [("ghostty/Example", "Example")]
            )
            self.assertEqual(stat.S_IMODE(reopened.path.stat().st_mode), 0o600)

    def test_stock_default_and_delete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            catalog = tool.ThemeCatalog(root, root / "missing")
            catalog.initialize_stock(XTERM_COLORS)
            changed = dict(XTERM_COLORS, background="#141922")
            catalog.initialize_stock(changed)
            self.assertEqual(catalog.colors("Mine/Stock"), XTERM_COLORS)
            key = catalog.store("Mine", "Evening", changed)
            catalog.favorite(key, True)
            catalog.set_default(key)
            self.assertEqual(tool.ThemeCatalog(root, root).default_colors(), changed)
            catalog.delete(key)
            self.assertIsNone(catalog.data["default"])
            self.assertNotIn(key, catalog.data["favorites"])
            with self.assertRaises(ValueError):
                catalog.default_colors()
            self.assertEqual(catalog.entries("Mine")[0], ("meta/current", "Current"))
            self.assertTrue(tool.is_dark(changed))
            self.assertFalse(tool.is_dark(XTERM_COLORS))
            preview = tool.preview_html(changed, "<test>")
            self.assertIn("&lt;test&gt;", preview)
            self.assertIn("255", preview)
            self.assertIn("git status", preview)

    def test_reload_target_validation(self):
        import socket
        import signal

        targets = tool.ReloadTargets(":test")
        pid = os.getpid()
        props = f'_REVENANT_CAPABILITIES(ATOM) = _REVENANT_RELOAD_SIGHUP_V1\n_REVENANT_VERSION(UTF8_STRING) = "0.9.0-dev"\n_NET_WM_PID(CARDINAL) = {pid}\nWM_CLIENT_MACHINE(STRING) = "{socket.gethostname()}"\nWM_CLASS(STRING) = "xterm", "XTerm"\nWM_NAME(STRING) = "Test terminal"\n'
        with patch.object(targets, "xprop", return_value=props):
            selected = targets.inspect(42)
            self.assertEqual(selected["pid"], pid)
        for invalid in (
            props.replace("_REVENANT_RELOAD_SIGHUP_V1", "_REVENANT_RELOAD_SIGHUP_V1" + "0"),
            props.replace(socket.gethostname(), "remote.invalid"),
            props.replace(f"= {pid}", "= 1"),
        ):
            with patch.object(targets, "xprop", return_value=invalid):
                self.assertIsNone(targets.inspect(42))
        with (
            patch.object(os, "pidfd_open", return_value=90),
            patch.object(os, "close"),
            patch.object(signal, "pidfd_send_signal") as send,
        ):
            with patch.object(targets, "inspect", return_value=selected):
                targets.hup(selected)
                send.assert_called_once_with(90, signal.SIGHUP)
            send.reset_mock()
            with patch.object(targets, "inspect", return_value=None):
                with self.assertRaises(ValueError):
                    targets.hup(selected)
                send.assert_not_called()

    def test_invalid_or_incomplete_theme(self):
        with tempfile.TemporaryDirectory() as directory:
            path = ghostty_file(Path(directory) / "theme")
            for text in (
                "background=#123456\n",
                path.read_text().replace("#cd0000", "invalid"),
                "XTerm*background: #123456\n",
            ):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    load_theme(path)

    def test_resource_injection_is_rejected(self):
        for application_class in ("XTerm\nOther", "XTerm*font", "", "#include"):
            with self.assertRaises(ValueError):
                render(XTERM_COLORS, application_class)
        with self.assertRaises(ValueError):
            render(
                dict(XTERM_COLORS, background="#000000\nOther*font: injected"), "XTerm"
            )
        self.assertNotIn("\nOther", render(XTERM_COLORS, "XTerm", "label\nOther"))


def live(work, with_gui=False):
    directory = Path(work)
    themes = directory / "themes"
    themes.mkdir()
    ghostty_file(themes / "Example")
    state = directory / "state"
    x = tool.XResources()
    try:
        assert len(x.roots) >= 2, "test requires two X screens"
        x.write(
            x.roots[0],
            "RESOURCE_MANAGER",
            b"XTerm*font: fixed\nOther*background: #112233\n",
        )
        x.write(x.roots[0], "SCREEN_RESOURCES", b"")
        x.write(x.roots[1], "SCREEN_RESOURCES", b"Other*geometry:\t80x24\n")
        x.lib.XSync(x.connection, 0)
        original = x.snapshot()
        setter = tool.ThemeSetter(x, state)

        def cli(*args, success=True):
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools/theme-setter-utility.py"),
                    "--themes-dir",
                    str(themes),
                    "--state-dir",
                    str(state),
                    *args,
                ],
                text=True,
                capture_output=True,
            )
            assert (result.returncode == 0) == success, result.stderr
            return result

        assert x.current_colors() == XTERM_COLORS
        x.write(
            x.roots[0],
            "RESOURCE_MANAGER",
            b"XTerm*background: navy\nxterm.vt100.color3: #123456\nXTerm*color3: red\n",
        )
        x.lib.XSync(x.connection, 0)
        current = x.current_colors()
        assert current["background"] == "#000080"
        assert current["color3"] == "#123456", "Xrm specificity was lost"
        diagnostics = x.color_diagnostics()
        assert diagnostics["winners"]["color3"]["key"] == "xterm.vt100.color3"
        assert diagnostics["winners"]["background"]["key"] == "XTerm*background"
        x.write(
            x.roots[0],
            "RESOURCE_MANAGER",
            b"xterm*background: black\nxterm*foreground: green\nXterm*background: red\nXTerm*background: white\n",
        )
        x.lib.XSync(x.connection, 0)
        result = setter.apply(XTERM_COLORS)
        assert result["overridden"]["background"]["effective"] == "#000000"
        assert (
            result["diagnostics"]["winners"]["background"]["key"] == "xterm*background"
        )
        assert "Xterm*background" in result["diagnostics"]["case_variants"]
        raw = subprocess.check_output(["xrdb", "-query"], text=True)
        assert "XTerm*background:" in raw and "#ffffff" in raw, (
            "requested class key was not written"
        )
        assert "xterm*background:" in raw, "merge deleted an override"
        x.restore(original)
        # The injected write created the baseline; reset its token for first-run checks below.
        x.write(x.roots[0], tool.SESSION_PROPERTY, None)
        x.lib.XSync(x.connection, 0)
        cli("--load-default", success=False)
        cli("set", "Example")
        catalog = tool.ThemeCatalog(state, themes)
        assert catalog.colors("Mine/Stock") == XTERM_COLORS
        default = dict(XTERM_COLORS, background="#102030")
        key = catalog.store("Mine", "Default test", default)
        catalog.set_default(key)
        cli("--load-default")
        assert x.current_colors()["background"] == "#102030"
        cli("reset", "original")
        cli("set", "Example")
        saved = setter.original_path()
        assert json.loads(saved.read_text()) == original
        assert stat.S_IMODE(saved.stat().st_mode) == 0o600
        query = subprocess.check_output(["xrdb", "-query", "-all"], text=True)
        assert "XTerm*font:" in query and "Other*background:" in query
        assert "XTerm*color15:" in query
        cli("reset", "blank")
        assert all(
            value is None
            for screen in x.snapshot()["screens"]
            for value in screen.values()
        )
        cli("reset", "original")
        assert x.snapshot() == original, (
            "restore changed formatting, absent/empty or screen-specific properties"
        )
        cli("reset", "xterm")
        query = subprocess.check_output(["xrdb", "-query", "-all"], text=True)
        assert "XTerm*font:" not in query and "Other*background:" not in query
        assert "XTerm*background:" in query and "#ffffff" in query
        cli("reset", "original")
        cli("set", "Example", "--replace")
        cli("reset", "original")
        assert x.snapshot() == original
        (themes / "Broken").write_text("invalid theme\n")
        cli("set", "Broken", success=False)
        assert x.snapshot() == original
        with patch.object(tool.subprocess, "run", side_effect=OSError("xrdb failed")):
            try:
                setter.apply(XTERM_COLORS, replace=True)
                raise AssertionError("failed apply was accepted")
            except OSError:
                pass
        assert x.snapshot() == original, "failed apply did not roll back"
        for invalid in (
            {"schema": 1, "screens": []},
            {"schema": 1, "screens": [{name: "!" for name in tool.PROPERTIES}] * 2},
        ):
            try:
                x.restore(invalid)
                raise AssertionError("bad snapshot accepted")
            except ValueError:
                pass
            assert x.snapshot() == original
        x.write(x.roots[0], tool.SESSION_PROPERTY, None)
        x.lib.XSync(x.connection, 0)
        assert setter.original_path() is None
        try:
            setter.restore_original()
            raise AssertionError("stale session original accepted")
        except ValueError:
            pass
        if with_gui:
            from PySide6.QtWidgets import QApplication
            from PySide6.QtCore import Qt

            app = QApplication.instance() or QApplication([])
            args = argparse.Namespace(
                display=os.environ["DISPLAY"],
                state_dir=state,
                themes_dir=themes,
                application_class="XTerm",
            )
            window = tool.create_window(args)
            window.show()
            app.processEvents()
            item = window.themes.findItems("Example", Qt.MatchFlag.MatchExactly)[0]
            window.themes.setCurrentItem(item)
            assert window.apply_button.isEnabled()
            assert "XTerm*background: #ffffff" in window.output.toPlainText()
            assert not window.original_button.isEnabled()
            with unittest.TestCase().assertLogs(tool.LOG, level="INFO") as emitted:
                window.apply_button.click()
            assert len(emitted.records) == 1, emitted.output
            assert "Apply theme='Example'" in emitted.output[0]
            assert window.original_button.isEnabled()
            override = (
                x.read(x.roots[0], "RESOURCE_MANAGER") or b""
            ) + b"xterm*background: #010203\nXterm*foreground: red\n"
            x.write(x.roots[0], "RESOURCE_MANAGER", override)
            x.lib.XSync(x.connection, 0)
            with unittest.TestCase().assertLogs(tool.LOG, level="INFO") as emitted:
                window.apply_button.click()
            assert len(emitted.records) == 1, emitted.output
            assert emitted.records[0].levelno == __import__("logging").WARNING
            assert "Warning: wrote Example" in window.action_status.text()
            assert "xterm*background" in window.resource_warning.text()
            assert "Xterm*foreground" in window.conflicts.toPlainText()
            assert x.current_colors()["background"] == "#010203"
            assert window.title_label.text() != "themesetter — Example"
            x.restore(original)
            # Exercise actual button clicks and event processing across selections.
            alternate = dict(XTERM_COLORS, background="#182332", foreground="#dce6f5")
            window.catalog_store.store("Mine", "Alternate", alternate)
            for attempt in range(6):
                window.catalog.setCurrentText("Mine" if attempt % 2 == 0 else "Ghostty")
                name = "Alternate" if attempt % 2 == 0 else "Example"
                item = window.themes.findItems(name, Qt.MatchFlag.MatchExactly)[0]
                window.themes.setCurrentItem(item)
                app.processEvents()
                assert window.apply_button.isEnabled(), (
                    "Apply disabled after earlier application"
                )
                window.apply_button.click()
                app.processEvents()
                assert f"Applied {name} to XRDB" in window.action_status.text()
                assert x.current_colors() == (
                    alternate if attempt % 2 == 0 else XTERM_COLORS
                ), "repeated GUI application failed"
            window.blank()
            window.original_button.click()
            assert x.snapshot() == original
            window.resource_class.setText("invalid*class")
            assert not window.apply_button.isEnabled()
            window.resource_class.setText("XTerm")
            window.search.setText("Example")
            assert (
                sum(
                    not window.themes.item(i).isHidden()
                    for i in range(window.themes.count())
                )
                == 1
            )
            window.search.setText("no matching theme")
            assert not window.apply_button.isEnabled()
            window.search.setText("Example")
            assert window.apply_button.isEnabled()
            app.processEvents()
            window.grab().save("/tmp/theme-setter-preview.png")
            with patch.object(
                tool,
                "fetch",
                side_effect=lambda target: ghostty_file(target / "Fetched"),
            ):
                window.fetch_themes()
                deadline = time.monotonic() + 5
                while not window.fetch_button.isEnabled():
                    assert time.monotonic() < deadline
                    app.processEvents()
                    time.sleep(0.01)
            window.search.clear()
            window.catalog.setCurrentText("Ghostty")
            # Live preview uses normal list keyboard navigation and debounces writes.
            from PySide6.QtTest import QTest

            preview_colors = dict(
                XTERM_COLORS, background="#182332", foreground="#ccddee"
            )
            (themes / "Fetched").write_text(render(preview_colors, "XTerm"))
            selected = window.themes.findItems("Example", Qt.MatchFlag.MatchExactly)[0]
            window.themes.setCurrentItem(selected)
            window.reload_target.addItem(
                "Isolated mocked target",
                {"pid": 99, "window": 42, "version": "test", "class": "XTerm"},
            )
            window.reload_target.setCurrentIndex(window.reload_target.count() - 1)
            hup = patch.object(window.reload_targets, "hup")
            sent = hup.start()
            window.auto_apply.setChecked(True)
            assert not window.replace.isEnabled()
            QTest.qWait(250)
            assert x.current_colors() == XTERM_COLORS
            window.themes.setFocus()
            QTest.keyClick(window.themes, Qt.Key.Key_Down)
            QTest.qWait(250)
            assert window.label == "Fetched"
            assert x.current_colors() == preview_colors
            QTest.keyClick(window.themes, Qt.Key.Key_Up)
            QTest.qWait(250)
            assert x.current_colors() == XTERM_COLORS
            window.auto_apply.setChecked(False)
            QTest.keyClick(window.themes, Qt.Key.Key_Down)
            QTest.qWait(250)
            assert x.current_colors() == XTERM_COLORS
            assert window.replace.isEnabled()
            assert sent.call_count == 3, sent.call_count
            assert "HUP sent to PID 99" in window.action_status.text()
            hup.stop()
            window.reload_target.setCurrentIndex(0)
            window.catalog.setCurrentText("Ghostty")
            item = window.themes.findItems("Example", Qt.MatchFlag.MatchExactly)[0]
            window.themes.setCurrentItem(item)
            window.favorite_button.click()
            window.catalog.setCurrentText("Favorites")
            assert window.themes.count() == 1
            assert window.theme_key == "ghostty/Example"
            window.favorite_button.click()
            assert window.themes.count() == 0
            assert not window.apply_button.isEnabled()
            window.catalog_store.store("Mine", "Personal", XTERM_COLORS)
            window.catalog.setCurrentText("Mine")
            assert window.themes.findItems("Personal", Qt.MatchFlag.MatchExactly)
            assert window.apply_button.isEnabled()
            personal = window.themes.findItems("Personal", Qt.MatchFlag.MatchExactly)[0]
            window.themes.setCurrentItem(personal)
            window.edit_color("background", "#121926")
            window.edit_color("foreground", "#dce6f5")
            assert window.save_button.isEnabled()
            window.save_button.click()
            assert (
                window.catalog_store.colors("Mine/Personal")["background"] == "#121926"
            )
            window.default_button.click()
            assert window.catalog_store.data["default"] == "Mine/Personal"
            window.tone.setCurrentText("Light palettes")
            assert personal.isHidden()
            window.tone.setCurrentText("Dark palettes")
            assert not personal.isHidden()
            window.tone.setCurrentIndex(0)
            window.appearance.setCurrentText("Light")
            before_size = window.font_size
            from PySide6.QtCore import QEvent
            from PySide6.QtGui import QKeyEvent

            window.search.setFocus()
            app.processEvents()
            for key, modifiers in (
                (Qt.Key.Key_Plus, Qt.KeyboardModifier.ControlModifier),
                (
                    Qt.Key.Key_Plus,
                    Qt.KeyboardModifier.ControlModifier
                    | Qt.KeyboardModifier.KeypadModifier,
                ),
                (Qt.Key.Key_Minus, Qt.KeyboardModifier.ControlModifier),
                (
                    Qt.Key.Key_Minus,
                    Qt.KeyboardModifier.ControlModifier
                    | Qt.KeyboardModifier.KeypadModifier,
                ),
            ):
                app.sendEvent(
                    window.search, QKeyEvent(QEvent.Type.KeyPress, key, modifiers)
                )
            assert window.font_size == before_size
            window.zoom(1)
            assert window.font_size == before_size + 1
            window.appearance.setCurrentText("Dark")
            window.themes.setCurrentItem(personal)
            app.processEvents()
            window.grab().save("/tmp/theme-studio-dark.png")
            window.appearance.setCurrentText("Light")
            app.processEvents()
            window.grab().save("/tmp/theme-studio-light.png")
            window.delete_mine()
            assert "Mine/Personal" not in window.catalog_store.data["themes"]
            assert window.catalog_store.data["default"] is None
            window.catalog_store.store("Mine", "Personal", XTERM_COLORS)
            window.close()
            app.processEvents()
            reopened = tool.create_window(args)
            reopened.catalog.setCurrentText("Mine")
            assert reopened.themes.findItems("Personal", Qt.MatchFlag.MatchExactly)
            # Real mouse events delivered while QApplication.exec is running.
            from PySide6.QtCore import QTimer
            from PySide6.QtTest import QTest

            failures = []
            reopened.show()

            def clicks():
                try:
                    for name in ("Stock", "Personal", "Stock"):
                        row = reopened.themes.findItems(
                            name, Qt.MatchFlag.MatchExactly
                        )[0]
                        QTest.mouseClick(
                            reopened.themes.viewport(),
                            Qt.MouseButton.LeftButton,
                            pos=reopened.themes.visualItemRect(row).center(),
                        )
                        assert reopened.label == name
                        QTest.mouseClick(
                            reopened.apply_button, Qt.MouseButton.LeftButton
                        )
                        assert (
                            f"Applied {name} to XRDB" in reopened.action_status.text()
                        )
                        assert "Active XRDB palette:" in reopened.active_label.text()
                        assert reopened.title_label.text() == f"themesetter — {name}"
                    assert reopened.windowFlags() & Qt.WindowType.FramelessWindowHint
                    initial_mode = reopened.appearance.currentText()
                    QTest.mouseClick(
                        reopened.appearance_button, Qt.MouseButton.LeftButton
                    )
                    assert reopened.appearance.currentText() != initial_mode
                    QTest.mouseClick(
                        reopened.maximize_button, Qt.MouseButton.LeftButton
                    )
                    assert reopened.isMaximized()
                    assert all(not grip.isVisible() for grip in reopened.resize_handles)
                    QTest.mouseClick(
                        reopened.maximize_button, Qt.MouseButton.LeftButton
                    )
                    assert not reopened.isMaximized()
                    assert all(grip.isVisible() for grip in reopened.resize_handles)
                    QTest.mouseClick(
                        reopened.minimize_button, Qt.MouseButton.LeftButton
                    )
                    assert reopened.isMinimized()
                    reopened.showNormal()
                    # Native WM move/resize is unavailable under bare Xvfb: test the fallback.
                    from PySide6.QtCore import QPoint, QPointF
                    from PySide6.QtGui import QMouseEvent

                    handle = reopened.resize_handles[-1]
                    origin = reopened.size()
                    position = handle.rect().center()
                    global_position = handle.mapToGlobal(position)
                    with patch.object(
                        reopened.windowHandle(), "startSystemResize", return_value=False
                    ):
                        QTest.mousePress(
                            handle, Qt.MouseButton.LeftButton, pos=position
                        )
                        app.sendEvent(
                            handle,
                            QMouseEvent(
                                QEvent.Type.MouseMove,
                                QPointF(position + QPoint(30, 20)),
                                QPointF(global_position + QPoint(30, 20)),
                                Qt.MouseButton.NoButton,
                                Qt.MouseButton.LeftButton,
                                Qt.KeyboardModifier.NoModifier,
                            ),
                        )
                        QTest.mouseRelease(handle, Qt.MouseButton.LeftButton)
                    assert reopened.width() >= origin.width() + 30
                    assert reopened.height() >= origin.height() + 20
                    assert reopened.title_bar.mapTo(reopened, QPoint(0, 0)).y() == 6
                    assert reopened.title_bar.height() == 44
                    # Hit-test real screen positions: caption and empty header space
                    # must land on the drag widget, not on an inert label or margin.
                    reopened.move(20, 20)
                    app.processEvents()
                    for point in (QPoint(8, 22), QPoint(120, 22), QPoint(600, 12)):
                        global_point = reopened.title_bar.mapToGlobal(point)
                        actual = app.widgetAt(global_point)
                        assert actual is reopened.title_bar, (point, actual)
                        origin = reopened.pos()
                        with patch.object(
                            reopened.windowHandle(),
                            "startSystemMove",
                            return_value=False,
                        ):
                            QTest.mousePress(
                                actual, Qt.MouseButton.LeftButton, pos=point
                            )
                            app.sendEvent(
                                actual,
                                QMouseEvent(
                                    QEvent.Type.MouseMove,
                                    QPointF(point + QPoint(10, 5)),
                                    QPointF(global_point + QPoint(10, 5)),
                                    Qt.MouseButton.NoButton,
                                    Qt.MouseButton.LeftButton,
                                    Qt.KeyboardModifier.NoModifier,
                                ),
                            )
                            QTest.mouseRelease(
                                actual, Qt.MouseButton.LeftButton, pos=point
                            )
                        assert reopened.pos() == origin + QPoint(10, 5)
                    position = reopened.title_bar.rect().center()
                    global_position = reopened.title_bar.mapToGlobal(position)
                    origin = reopened.pos()
                    with patch.object(
                        reopened.windowHandle(), "startSystemMove", return_value=False
                    ):
                        QTest.mousePress(
                            reopened.title_bar, Qt.MouseButton.LeftButton, pos=position
                        )
                        app.sendEvent(
                            reopened.title_bar,
                            QMouseEvent(
                                QEvent.Type.MouseMove,
                                QPointF(position + QPoint(20, 20)),
                                QPointF(global_position + QPoint(20, 20)),
                                Qt.MouseButton.NoButton,
                                Qt.MouseButton.LeftButton,
                                Qt.KeyboardModifier.NoModifier,
                            ),
                        )
                        QTest.mouseRelease(
                            reopened.title_bar, Qt.MouseButton.LeftButton
                        )
                    assert reopened.pos() == origin + QPoint(20, 20)
                    app.processEvents()
                    reopened.grab().save("/tmp/theme-setter-csd.png")
                except BaseException as error:
                    failures.append(error)
                finally:
                    app.quit()

            QTimer.singleShot(0, clicks)
            app.exec()
            assert not failures, failures
            QTest.mouseClick(reopened.close_button, Qt.MouseButton.LeftButton)
            assert not reopened.isVisible()
            output = directory / "gui-stdout.log"
            with output.open("w") as stream:
                process = subprocess.Popen(
                    [
                        sys.executable,
                        str(ROOT / "tools/theme-setter-utility.py"),
                        "--themes-dir",
                        str(themes),
                        "--state-dir",
                        str(state),
                    ],
                    stdout=stream,
                    stderr=stream,
                )
                try:
                    deadline = time.monotonic() + 10
                    while "GUI ready" not in output.read_text():
                        assert process.poll() is None, output.read_text()
                        assert time.monotonic() < deadline, output.read_text()
                        time.sleep(0.02)
                    process.send_signal(__import__("signal").SIGINT)
                    assert process.wait(timeout=5) == 0, output.read_text()
                    assert "Received SIGINT" in output.read_text()
                    assert "Reload target:" in output.read_text()
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
        print(
            "Theme CLI: merge/replace/defaults/blank/exact original restore, rollback and session scope passed"
        )
        if with_gui:
            print(
                "PySide preview, selection, application, restore, filtering and background fetch passed"
            )
    finally:
        x.close()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--live":
        live(sys.argv[2], "--gui" in sys.argv[3:])
    else:
        unittest.main()
