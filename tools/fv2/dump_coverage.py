#!/usr/bin/env python3
"""What the full dump really covered.

    dump_coverage.py <Textures3D-dump>

Reads the RAM dumps each run left behind and says, for each one, which fighter
was played and in which colour -- because a cursor cell is not a promise: cell 10
draws at random, and past 10 the row only goes anywhere if --full-roster was
really taken.

Addresses, measured (docs/fv2-honey.md):
    0x007005B8  player 1's fighter, by its fight identifier
    0x007005B9  *1P COLOR, 0 or 1
The byte at address A is at index A^3 of the dump: board.ram keeps the 32-bit
words in the host's order.
"""

import collections
import csv
import os
import sys

NAMES = {0: "GRACE", 1: "BAHN", 2: "RAXEL", 3: "TOKIO", 4: "SANMAN", 5: "JANE",
         6: "HONEY", 7: "PICKY", 8: "MAHLER", 9: "BM", 10: "CHARLIE", 11: "TECHNO (Emi)"}

FIGHTER, COLOUR = 0x007005B8, 0x007005B9


def byte(d, a):
    i = a ^ 3
    return d[i] if i < len(d) else None


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    base = sys.argv[1]
    rams = os.path.join(base, "ram")
    lines, seen = [], collections.Counter()
    for name in sorted(os.listdir(rams)) if os.path.isdir(rams) else []:
        if not name.endswith(".ram"):
            continue
        d = open(os.path.join(rams, name), "rb").read()
        p, c = byte(d, FIGHTER), byte(d, COLOUR)
        lines.append((name[:-4], p, c))
        if p is not None:
            seen[(p, c)] += 1

    print("  run                 fighter               colour")
    for run, p, c in lines:
        who = "%2d %s" % (p, NAMES.get(p, "?")) if p is not None else " - "
        print("  %-18s  %-20s  %s" % (run, who, c if c is not None else "-"))

    distinct = {p for p, _ in seen}
    print()
    print("  distinct fighters seen    : %d" % len(distinct))
    missing = [NAMES[i] for i in sorted(NAMES) if i not in distinct]
    if missing:
        print("  never played              : " + ", ".join(missing))
    pairs = len([1 for k in seen if k[1] == 1])
    print("  runs in colour 1 / 2      : %d / %d"
          % (len([1 for k in seen if k[1] == 0]), pairs))

    tex = os.path.join(base, "fvipers2", "textures.csv")
    if os.path.isfile(tex):
        # The csv is added to from one run to the next, and each run puts a line
        # back in it for a texture it sees again -- the PNG itself is written
        # only once. Counting the lines would count the encounters, not the
        # textures: so they are folded by file name.
        formats, alphas, n_lines = {}, {}, 0
        disagree = collections.Counter()
        with open(tex, newline="") as fh:
            for r in csv.DictReader(fh):
                n_lines += 1
                n, a = r["name"], r.get("alpha", "?")
                formats[n] = r["format"]
                if n in alphas and alphas[n] != a:
                    disagree[(alphas[n], a)] += 1
                alphas.setdefault(n, a)
        pf = collections.Counter(formats.values())
        pa = collections.Counter(alphas.values())
        png = len([f for f in os.listdir(os.path.join(base, "fvipers2"))
                   if f.endswith(".png")])
        print()
        print("  distinct textures         : %d  (%d PNGs on the disc, %d csv lines)"
              % (len(formats), png, n_lines))
        print("  by format                 : "
              + ", ".join("f%s %d" % (k, v) for k, v in sorted(pf.items(), key=lambda e: -e[1])))
        print("  by alpha                  : "
              + ", ".join("%s %d" % (k, v) for k, v in pa.most_common()))
        if disagree:
            print("  drawn with two alphas depending on the run:")
            for (a, b), v in disagree.most_common():
                print("      %-8s then %-8s  %d times" % (a, b, v))

    with open(os.path.join(base, "coverage.txt"), "w", encoding="utf-8") as fh:
        fh.write("The full dump of Fighting Vipers 2's 3D textures\n")
        fh.write("===============================================\n\n")
        for run, p, c in lines:
            fh.write("%-18s  %-20s  colour %s\n"
                     % (run, "%2d %s" % (p, NAMES.get(p, "?")) if p is not None else "-", c))
        fh.write("\ndistinct fighters: %d\n" % len(distinct))
        if missing:
            fh.write("never played: " + ", ".join(missing) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
