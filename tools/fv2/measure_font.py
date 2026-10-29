"""Measures the console menu's glyph sheet and prints the table for src/host/fv2_text.h.

assets/menu-console/font.png is an ASCII atlas: 1024 x 384, 16 columns by 6 rows,
cells of 64 x 64, from space (0x20) to DEL (0x7F). The glyphs are white on
transparent and CENTRED in their cell, not flush left -- so proportional rendering
cannot use the cell, it needs each one's ink box. That is what this script
measures.

It invents nothing: it reads the alpha channel and gives, for each cell, the
extreme columns where there is ink. The only two numbers that do not come out of
the picture are in fv2_text.h and say why (the approach between two glyphs, and
the space's width, which has no ink to measure).

Usage:
    py -3 tools/fv2/measure_font.py [path of font.png]

(police.png, the name the sheet arrived under, is accepted too.)
"""

import os
import sys

COLUMNS, ROWS = 16, 6
FIRST = 0x20


def boxes(path):
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    cw, ch = w // COLUMNS, h // ROWS
    px = im.load()
    out = []
    for r in range(ROWS):
        for c in range(COLUMNS):
            x0, y0 = c * cw, r * ch
            left, right, top, bottom = cw, -1, ch, -1
            for y in range(ch):
                for x in range(cw):
                    if px[x0 + x, y0 + y][3]:
                        left = min(left, x); right = max(right, x)
                        top = min(top, y); bottom = max(bottom, y)
            out.append((FIRST + r * COLUMNS + c, left, right, top, bottom))
    return (w, h, cw, ch), out


def default_path():
    for p in ("assets/menu-console/font.png", "assets/menu-console/police.png"):
        if os.path.isfile(p):
            return p
    return "assets/menu-console/font.png"


def main(argv):
    path = argv[1] if len(argv) > 1 else default_path()
    (w, h, cw, ch), all_cells = boxes(path)
    inked = [b for b in all_cells if b[2] >= 0]
    print("// %s: %dx%d, cells %dx%d, %d cells, %d with ink"
          % (path, w, h, cw, ch, len(all_cells), len(inked)))
    print("// empty: %s" % ", ".join("0x%02X" % b[0] for b in all_cells if b[2] < 0))
    # The baseline is the one the bulk of the capitals sit on, not their extreme bottom: the Q's
    # tail goes below it and would skew a plain maximum. So the most frequent value is taken,
    # which is the one the other 25 share.
    bottoms = [b[4] for b in all_cells if 0x41 <= b[0] <= 0x5A]
    base = max(set(bottoms), key=bottoms.count)
    print("// baseline: y = %d (the Q goes down to %d); descenders to %d; capital top: y = %d"
          % (base, max(bottoms), max(b[4] for b in inked),
             min(b[3] for b in all_cells if 0x41 <= b[0] <= 0x5A)))
    print("\t// x of the ink's left edge in the cell, then its width; 0,0 = no ink")
    print("\tstatic constexpr uint8_t INK[96][2] = {")
    for i in range(0, 96, 8):
        piece = []
        for code, g, d, _, _ in all_cells[i:i + 8]:
            piece.append("{%2d,%2d}" % ((g, d - g + 1) if d >= 0 else (0, 0)))
        chars = "".join(chr(c) if 0x21 <= c <= 0x7E else " " for c, _, _, _, _ in all_cells[i:i + 8])
        print("\t\t%s,   // 0x%02X  %s" % (", ".join(piece), all_cells[i][0], chars))
    print("\t};")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
