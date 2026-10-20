#!/usr/bin/env python3
"""Writes the test portraits this port's own screenshots were taken with.

Nothing here comes from the game or from anyone's artwork: sixteen 64x64 PNGs,
each a flat colour with its slot number drawn on it and a border, so that a
screenshot of the select screen says at a glance which case went where and
which of the two sheets the screen is drawing. The `dark` set is the same
pictures at 40 % brightness, which is roughly what Sega's own second copy is --
so a screenshot still tells the selected case from the others.

    py -3 tools/fv2/portraits_test.py assets/fv2-portraits-test

Then:  build\\bin\\vf3.exe fvipers2.zip --portraits assets\\fv2-portraits-test
See docs/fv2-portraits.md.

No third-party module: PNG is written straight, with zlib from the standard
library.
"""

import os
import struct
import sys
import zlib

SIDE = 64

# One colour a slot, in the sheet's own order (00 Bahn ... 10 Tokio, 11-15 free)
COLOURS = [
    (220, 40, 40), (240, 130, 30), (240, 215, 40), (150, 215, 50),
    (50, 200, 90), (40, 205, 190), (50, 150, 240), (70, 80, 230),
    (150, 60, 225), (225, 60, 190), (245, 120, 150), (170, 170, 170),
    (120, 120, 120), (90, 110, 130), (70, 70, 70), (30, 30, 30),
]

# A 5x7 bitmap for the ten digits: all this needs to do is be readable at 64x64
DIGITS = {
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11111", "00010", "00100", "00010", "00001", "10001", "01110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
}


def write_png(path, pixels):
    """pixels: SIDE rows of SIDE (r, g, b) triples."""
    raw = b"".join(b"\0" + b"".join(bytes(p) for p in row) for row in pixels)
    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", SIDE, SIDE, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def case(slot, dim):
    r, g, b = COLOURS[slot]
    if dim:
        r, g, b = int(r * 0.4), int(g * 0.4), int(b * 0.4)
    ink = (0, 0, 0) if (r + g + b) > 330 else (255, 255, 255)
    px = [[(r, g, b) for _ in range(SIDE)] for _ in range(SIDE)]
    # a border, so the case's edges are visible in a screenshot
    for i in range(SIDE):
        for k in (0, 1, SIDE - 2, SIDE - 1):
            px[k][i] = ink
            px[i][k] = ink
    # the slot number, two digits, 5x7 each blown up by 4 (20x28), side by side
    text = "%02d" % slot
    scale = 4
    w = (5 * len(text) + (len(text) - 1)) * scale
    x0, y0 = (SIDE - w) // 2, (SIDE - 7 * scale) // 2
    for d, ch in enumerate(text):
        for ry, row in enumerate(DIGITS[ch]):
            for rx, bit in enumerate(row):
                if bit != "1":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        x = x0 + (d * 6 + rx) * scale + sx
                        y = y0 + ry * scale + sy
                        px[y][x] = ink
    # a corner mark that differs between the two sheets: one square for light, two for dark
    for n in range(2 if dim else 1):
        for y in range(4, 10):
            for x in range(4 + n * 8, 10 + n * 8):
                px[y][x] = ink
    return px


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "assets/fv2-portraits-test"
    for name, dim in (("light", False), ("dark", True)):
        folder = os.path.join(out, name)
        os.makedirs(folder, exist_ok=True)
        for slot in range(16):
            write_png(os.path.join(folder, "%02d.png" % slot), case(slot, dim))
    print("wrote 32 test portraits under", out)


if __name__ == "__main__":
    main()
