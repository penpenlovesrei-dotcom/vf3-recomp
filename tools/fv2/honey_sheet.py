#!/usr/bin/env python3
"""Makes contact sheets of Honey's two sets of textures.

    honey_sheet.py <captures folder> <sheets folder>

It writes three pictures:

    sheet-colour-1.png   the whole set, each texture at its original size
    sheet-colour-2.png   the same
    sheet-pairs.png      the two side by side, by texture-memory slot:
                         colour 1 above, colour 2 below

The third is the only one that proves anything. Two textures at the same
(page, x, y) are the same piece of the model; seeing them one above the other
shows what the colour changes, and what it does not.

The chequerboard behind each thumbnail is the transparency: these textures have
an alpha channel, and a good part of Honey is cut out in it.
"""

import csv
import os
import sys

from PIL import Image

MARGIN, STEP = 8, 4
BACK = (26, 26, 28)
LINE = (90, 90, 96)


def batch(folder):
    names = {f for f in os.listdir(folder) if f.endswith(".png")}
    where = {}
    with open(os.path.join(folder, "textures.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            where[r["name"]] = r
    return names, where


def chequer(size):
    """The background that makes the alpha legible, 8 pixels a side."""
    w, h = size
    im = Image.new("RGB", size, (62, 62, 66))
    light = Image.new("RGB", (8, 8), (82, 82, 88))
    for y in range(0, h, 8):
        for x in range(0, w, 8):
            if (x // 8 + y // 8) % 2:
                im.paste(light, (x, y))
    return im


def place(canvas, path, x, y):
    v = Image.open(path).convert("RGBA")
    f = chequer(v.size)
    f.paste(v, (0, 0), v)
    canvas.paste(f, (x, y))
    return v.size


def sheet(names, source, where, out):
    """One row per band, the textures laid from the largest to the smallest;
    each keeps its original size, a sheet at scale 1 being the only way to judge
    what is inside it."""
    order = sorted(names, key=lambda n: (-int(where[n]["width"]) * int(where[n]["height"]),
                                         -int(where[n]["width"]), n))
    width = 1100
    x, y, row_height, slots = MARGIN, MARGIN, 0, []
    for n in order:
        w, h = int(where[n]["width"]), int(where[n]["height"])
        if x + w + MARGIN > width and x > MARGIN:
            x, y = MARGIN, y + row_height + STEP
            row_height = 0
        slots.append((n, x, y))
        x += w + STEP
        row_height = max(row_height, h)
    height = y + row_height + MARGIN
    canvas = Image.new("RGB", (width, height), BACK)
    for n, px, py in slots:
        place(canvas, os.path.join(source, n), px, py)
    canvas.save(out)
    return len(order), canvas.size


def pairs(s1, s2, d1, d2, o1, out):
    """The shared slots, colour 1 above colour 2."""
    common = sorted(set(s1) & set(s2),
                    key=lambda k: (-int(o1[s1[k]]["width"]) * int(o1[s1[k]]["height"]), k))
    width = 1100
    x, y, row_height, slots = MARGIN, MARGIN, 0, []
    for k in common:
        w, h = int(o1[s1[k]]["width"]), int(o1[s1[k]]["height"])
        block = h * 2 + STEP
        if x + w + MARGIN > width and x > MARGIN:
            x, y = MARGIN, y + row_height + STEP * 3
            row_height = 0
        slots.append((k, x, y, w, h))
        x += w + STEP
        row_height = max(row_height, block)
    height = y + row_height + MARGIN
    canvas = Image.new("RGB", (width, height), BACK)
    for k, px, py, w, h in slots:
        place(canvas, os.path.join(d1, s1[k]), px, py)
        place(canvas, os.path.join(d2, s2[k]), px, py + h + STEP)
    canvas.save(out)
    return len(common), canvas.size


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    captures, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    d1 = os.path.join(captures, "colour1")
    d2 = os.path.join(captures, "colour2")
    c1, o1 = batch(d1)
    c2, o2 = batch(d2)
    ctl, _ = batch(os.path.join(captures, "control"))
    h1, h2 = c1 - ctl, c2 - ctl

    n, size = sheet(h1, d1, o1, os.path.join(out, "sheet-colour-1.png"))
    print("sheet-colour-1  %3d textures  %dx%d" % (n, size[0], size[1]))
    n, size = sheet(h2, d2, o2, os.path.join(out, "sheet-colour-2.png"))
    print("sheet-colour-2  %3d textures  %dx%d" % (n, size[0], size[1]))

    s1 = {(o1[f]["page"], o1[f]["x"], o1[f]["y"]): f for f in h1}
    s2 = {(o2[f]["page"], o2[f]["x"], o2[f]["y"]): f for f in h2}
    n, size = pairs(s1, s2, d1, d2, o1, os.path.join(out, "sheet-pairs.png"))
    print("sheet-pairs      %3d pairs     %dx%d" % (n, size[0], size[1]))

    same = sum(1 for k in set(s1) & set(s2) if s1[k] == s2[k])
    other = len(set(s1) & set(s2)) - same
    print()
    print("at the same slot, the same image    %3d" % same)
    print("at the same slot, another image     %3d" % other)
    print("seen in one run only                c1 %d, c2 %d"
          % (len(set(s1) - set(s2)), len(set(s2) - set(s1))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
