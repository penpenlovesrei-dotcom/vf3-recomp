#!/usr/bin/env python3
"""Sorts three texture captures and leaves Honey's costume on the desktop.

    honey_textures.py <captures folder> <output folder>

The captures folder holds three sub-folders written by `vf3run --hd-dump`:

    colour1/   HONEY confirmed with punch
    colour2/   HONEY, one UP on her cell before the punch
    control/   EMI, the same run in everything else

A texture is named by the hash of its content: the file's name IS its identity,
and two files of the same name in two captures are the same image to the pixel.
So the sorting is a matter of sets, not of comparing images.

    colour-1 = colour1 \\ control
    colour-2 = colour2 \\ control

What Honey's run has and the control does not can be neither the scenery, nor
the sky, nor the opponent, nor the effects, nor the interface: all three runs
cross them at the same frame. What is left is the character being played.

Nothing is deleted: the two output folders are empty when created and the script
refuses to write over a file already there without saying so.
"""

import csv
import os
import shutil
import sys


def read(folder):
    """A capture's PNGs, and what its textures.csv says about each of them."""
    if not os.path.isdir(folder):
        return None, {}
    names = {f for f in os.listdir(folder) if f.lower().endswith(".png")}
    where = {}
    csvp = os.path.join(folder, "textures.csv")
    if os.path.isfile(csvp):
        with open(csvp, newline="", encoding="utf-8", errors="replace") as fh:
            for line in csv.DictReader(fh):
                # The dump writes its headers in English (name,hash,width,...).
                # `fichier` is the header this script itself used to write, and
                # `file` is the one it writes now: both are still read, so a
                # folder sorted before today still has an inventory.
                f = (line.get("name") or line.get("fichier")
                     or line.get("file") or "").strip()
                if f:
                    where[f] = line
    return names, where


def size_and_format(name):
    """`<hash>_<w>x<h>_f<format>.png` -> (w*h, w, h, format), for sorting."""
    body = name[:-4]
    parts = body.split("_")
    try:
        w, h = (int(v) for v in parts[1].split("x"))
        fmt = int(parts[2].lstrip("f"))
    except (IndexError, ValueError):
        return (0, 0, 0, 0)
    return (w * h, w, h, fmt)


def lay_down(names, source, target, where):
    os.makedirs(target, exist_ok=True)
    lines, written = [], 0
    for name in sorted(names, key=lambda n: (-size_and_format(n)[0], n)):
        dst = os.path.join(target, name)
        if os.path.exists(dst):
            print("   already there, left as it is: " + name)
        else:
            shutil.copy2(os.path.join(source, name), dst)
            written += 1
        # The inventory describes the whole set, whether a file has just been
        # copied or was already there: it says what the folder CONTAINS.
        _, w, h, fmt = size_and_format(name)
        d = where.get(name, {})
        lines.append((name, w, h, fmt, d.get("page", ""), d.get("x", ""), d.get("y", ""),
                      d.get("alpha", "")))
    with open(os.path.join(target, "textures.csv"), "w", newline="", encoding="utf-8") as fh:
        e = csv.writer(fh)
        # alpha: what the polygon does with the texel's alpha -- opaque, contour
        # (cut at one bit) or graded. It is the alpha the image carries.
        e.writerow(["file", "width", "height", "format", "page", "x", "y", "alpha"])
        e.writerows(lines)
    print("   %d files copied, %d already there" % (written, len(lines) - written))
    return lines


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    captures, out = sys.argv[1], sys.argv[2]

    c1, where1 = read(os.path.join(captures, "colour1"))
    c2, where2 = read(os.path.join(captures, "colour2"))
    ctl, _ = read(os.path.join(captures, "control"))

    missing = [n for n, s in (("colour1", c1), ("colour2", c2), ("control", ctl)) if s is None]
    if missing:
        print("capture absent: " + ", ".join(missing))
        return 1

    h1, h2 = c1 - ctl, c2 - ctl

    print("The three captures, as they come out of the game")
    print("   colour1 (Honey, punch)      %4d textures" % len(c1))
    print("   colour2 (Honey, UP+punch)   %4d textures" % len(c2))
    print("   control (Emi, punch)        %4d textures" % len(ctl))
    print()
    print("What each run shares with the control -- scenery, sky, opponent,")
    print("effects, interface: what is not the character being played")
    print("   colour1 ^ control           %4d" % len(c1 & ctl))
    print("   colour2 ^ control           %4d" % len(c2 & ctl))
    print()
    print("The costume, once the control is taken away")
    print("   colour-1                    %4d textures" % len(h1))
    print("   colour-2                    %4d textures" % len(h2))
    print("   common to both              %4d  (identical to the pixel in both colours)" % len(h1 & h2))
    print("   colour-1's own              %4d" % len(h1 - h2))
    print("   colour-2's own              %4d" % len(h2 - h1))
    print()

    l1 = lay_down(h1, os.path.join(captures, "colour1"), os.path.join(out, "colour-1"), where1)
    l2 = lay_down(h2, os.path.join(captures, "colour2"), os.path.join(out, "colour-2"), where2)
    print("   colour-1: %d textures in the folder" % len(l1))
    print("   colour-2: %d textures in the folder" % len(l2))

    survey = os.path.join(out, "SURVEY.txt")
    with open(survey, "w", encoding="utf-8") as fh:
        fh.write("Honey, the basic costume, in both of its colours\n")
        fh.write("===============================================\n\n")
        fh.write("Textures taken out of Fighting Vipers 2 by the Desktop\\fv2-recomp port,\n")
        fh.write("a passive capture of the 3D engine (vf3run --hd-dump, docs\\fv2-hd-3d.md).\n")
        fh.write("A file's name is the hash of its content, then its original size and its\n")
        fh.write("Real3D format. That name is the key a replacement pack uses: enlarge the\n")
        fh.write("image, redraw it, put it back under the same name.\n\n")
        fh.write("How the two sets were separated\n")
        fh.write("-------------------------------\n")
        fh.write("Three identical runs, without a window, the same opening and the same frames:\n")
        fh.write("  colour1  HONEY confirmed with PUNCH\n")
        fh.write("  colour2  HONEY, one UP on her cell before the punch (same costume, other colour)\n")
        fh.write("  control  EMI   confirmed with PUNCH\n")
        fh.write("No frame is rendered before the confirmation, so that the models passed over\n")
        fh.write("on the select screen enter no capture; guard is held all round long so that\n")
        fh.write("Honey's armour does not fall.\n")
        fh.write("One set = its run minus the control.\n\n")
        fh.write("The numbers\n")
        fh.write("-----------\n")
        fh.write("  colour1                 %4d textures\n" % len(c1))
        fh.write("  colour2                 %4d textures\n" % len(c2))
        fh.write("  control                 %4d textures\n" % len(ctl))
        fh.write("  colour1 ^ control       %4d\n" % len(c1 & ctl))
        fh.write("  colour2 ^ control       %4d\n" % len(c2 & ctl))
        fh.write("  colour-1 kept           %4d\n" % len(h1))
        fh.write("  colour-2 kept           %4d\n" % len(h2))
        fh.write("  common to both          %4d\n" % len(h1 & h2))
        fh.write("  colour-1's own          %4d\n" % len(h1 - h2))
        fh.write("  colour-2's own          %4d\n" % len(h2 - h1))
        fh.write("\nThe textures common to both colours are in BOTH folders: they are the parts\n")
        fh.write("of the model that the change of colour does not touch.\n")
        fh.write("\nEach folder's textures.csv gives, for every image, its size, its format, and\n")
        fh.write("the page and position where the game had loaded it.\n")
    print("   survey: " + survey)
    return 0


if __name__ == "__main__":
    sys.exit(main())
