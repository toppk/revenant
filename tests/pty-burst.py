#!/usr/bin/env python3

import os
import time


def write_all(data: bytes) -> None:
    offset = 0
    while offset < len(data):
        offset += os.write(1, data[offset:])


old_rows = [f"OLD FRAME {row:02d}".ljust(80) for row in range(24)]
write_all("\r\n".join(old_rows).encode())
time.sleep(0.25)

# Model a Rich Live refresh: erase every row first, emit enough styling bytes
# to cross the Linux PTY's 4095-byte read boundary, then write the replacement.
erase = b"\r" + b"\x1b[2K\x1b[1A" * 23 + b"\x1b[2K"
padding = b"\x1b[0m" * 1000
new_rows = [f"NEW FRAME {row:02d}".ljust(80) for row in range(24)]
replacement = "\r\n".join(new_rows).encode()
write_all(erase + padding + replacement)
time.sleep(0.25)
