#!/usr/bin/env python3
"""Live Mouse/Tcap Ops and startup-resource regression through X11 and a PTY."""
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import termios
import time
import tty


def wait(predicate, label):
    deadline = time.monotonic() + 8
    while not predicate():
        if time.monotonic() >= deadline:
            raise RuntimeError(label)
        time.sleep(.01)


def driver(directory, initial, exception):
    saved = termios.tcgetattr(0)
    tty.setraw(0)
    def handshake(name):
        (directory/(name+'.ready')).touch()
        wait(lambda: (directory/(name+'.done')).exists(), name)
    def read(timeout=.2):
        result = b''
        end = time.monotonic()+timeout
        while time.monotonic() < end:
            if select.select([0], [], [], max(0, end-time.monotonic()))[0]:
                result += os.read(0, 4096)
        return result
    try:
        os.write(1, b'\x1b[?1000h\x1b[?1006h')
        for phase, enabled in enumerate([initial, not initial, initial]):
            handshake(f'{phase}-setup')
            termios.tcflush(0, termios.TCIFLUSH)
            os.write(1, b'\x1b[6n\x1bP+q436f\x1b\\\x1b[6n')
            data = read()
            assert data.count(b'\x1b[1;1R') == 2, data
            assert (b'\x1bP1+r' in data) == (enabled or exception), data
            termios.tcflush(0, termios.TCIFLUSH)
            handshake(f'{phase}-click')
            data = read()
            assert (b'\x1b[<' in data) == enabled, (phase, enabled, data)
        (directory/'passed').touch()
    except BaseException as error:
        (directory/'error').write_text(repr(error))
        raise
    finally:
        termios.tcsetattr(0, termios.TCSANOW, saved)


def harness(root, terminal, toggle, selection):
    for name, initial, exception in [('enabled', True, False), ('disabled', False, False), ('exception', False, True)]:
        directory = root/name
        directory.mkdir()
        (directory/'home').mkdir()
        log = directory/'log'
        flag = str(initial).lower()
        args = [terminal, '-debug', '-fn', 'fixed', '-geometry', '40x8',
                '-xrm', f'XTerm*allowMouseOps: {flag}', '-xrm', f'XTerm*allowTcapOps: {flag}',
                '-xrm', 'XTerm*fontMenu*font: fixed', '-xrm', 'XTerm*fontMenu*vertSpace: 0']
        if exception:
            args += ['-xrm', 'XTerm*disallowedTcapOps: *,~GetTcap']
        args += ['-e', sys.executable, __file__, '--driver', str(directory), flag, str(exception).lower()]
        env = dict(os.environ, HOME=str(directory/'home'), XENVIRONMENT='/dev/null', XFILESEARCHPATH='/dev/null')
        with log.open('w') as stream:
            child = subprocess.Popen(args, env=env, stdout=stream, stderr=stream)
        def checkpoint(marker):
            wait(lambda: (directory/(marker+'.ready')).exists() or (directory/'error').exists() or child.poll() is not None, marker)
            if (directory/'error').exists():
                raise AssertionError((directory/'error').read_text())
            assert (directory/(marker+'.ready')).exists(), log.read_text()
        def flip(family, enabled):
            start = len(log.read_text())
            subprocess.run([toggle, window, family.lower()], check=True, stdout=subprocess.DEVNULL)
            wait(lambda: f'allow{family}Ops={str(enabled).lower()}' in log.read_text()[start:], 'menu '+family)
        try:
            checkpoint('0-setup')
            window = re.search(r'shell: realized window=(0x[0-9a-f]+)', log.read_text()).group(1)
            for phase, enabled in enumerate([initial, not initial, initial]):
                checkpoint(f'{phase}-setup')
                if phase:
                    flip('Mouse', enabled)
                    flip('Tcap', enabled)
                (directory/f'{phase}-setup.done').touch()
                checkpoint(f'{phase}-click')
                subprocess.run([selection, window, '20', '20', '50', '20'], check=True)
                (directory/f'{phase}-click.done').touch()
            child.wait(timeout=8)
            assert child.returncode == 0, log.read_text()
            assert (directory/'passed').exists(), (directory/'error').read_text() if (directory/'error').exists() else log.read_text()
        except BaseException:
            print(log.read_text()[-10000:], file=sys.stderr)
            raise
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait()
    print('Live Mouse/Tcap Ops, startup denial, and Tcap exceptions verified')


if sys.argv[1] == '--driver':
    driver(Path(sys.argv[2]), sys.argv[3] == 'true', sys.argv[4] == 'true')
else:
    harness(Path(sys.argv[1]), *sys.argv[2:])
