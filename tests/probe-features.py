#!/usr/bin/env python3
"""Probe transport/cleanup tests using a PTY, without emitting real terminal controls."""
import importlib.util
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import termios
import time
import unittest

PROBE = Path(__file__).resolve().parents[1]/'tools/probe-features.py'
spec = importlib.util.spec_from_file_location('feature_probe', PROBE)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def run_probe(args, interrupt_at=None):
    master, slave = pty.openpty()
    before = termios.tcgetattr(slave)
    child = subprocess.Popen([sys.executable, str(PROBE), *args], stdin=slave, stdout=slave, stderr=slave)
    output = b''
    interrupted = False
    deadline = time.monotonic()+10
    try:
        while child.poll() is None:
            if time.monotonic() > deadline:
                raise AssertionError(('probe timed out', args, output))
            if select.select([master], [], [], .02)[0]:
                output += os.read(master, 65536)
            if interrupt_at and interrupt_at in output and not interrupted:
                os.write(master, b'\x03')
                interrupted = True
        while select.select([master], [], [], .01)[0]:
            output += os.read(master, 65536)
        after = termios.tcgetattr(slave)
        return child.returncode, output, before == after
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        os.close(master)
        os.close(slave)


class ProbeTests(unittest.TestCase):
    def test_tcap_decoder(self):
        replies = b'\x1bP1+r544e=787465726d\x1b\\\x1bP0+r626164\x1b\\'
        self.assertEqual(probe.decode_tcap(replies), [(True, b'TN', b'xterm'), (False, b'bad', None)])
        self.assertEqual(probe.decode_tcap(b'\x1bP1+rzz=00\x1b\\')[0][1], b'malformed hex')

    def test_fixtures_run_and_restore_tty(self):
        for name in probe.PROBES:
            with self.subTest(probe=name):
                args = [name, '--timeout', '.02', '--delay', '.01']
                if name == 'mouse':
                    args += ['--seconds', '.05']
                code, output, restored = run_probe(args)
                self.assertEqual(code, 0, output)
                self.assertTrue(restored, name)
                if name == 'graphics':
                    self.assertIn(b'\x1b_Ga=d,d=I,i=', output)

    def test_ctrl_c_cleans_up(self):
        for name, marker, reset in [('progress', b'Requested progress state', b'\x1b]9;4;0\x1b\\'),
                                    ('mouse', b'Move, click', b'\x1b[?1000l'),
                                    ('pointer', b'Requested pointer', b'\x1b]22;default\x1b\\')]:
            with self.subTest(probe=name):
                code, output, restored = run_probe([name, '--timeout', '.02'], marker)
                self.assertEqual(code, 130, output)
                self.assertIn(reset, output[output.index(marker):])
                self.assertTrue(restored)


if __name__ == '__main__':
    unittest.main()
