"""Every address this program CALLS, taken from the image itself.

The recompiler's map holds every word the program is entered at, branch targets
inside a function included, so it is the wrong thing to cut functions at.  A call
target is: the target of a `bl` (primary opcode 18, LK=1, AA=0) or of a `bcl`
(opcode 16, LK=1, AA=0), reached from a word the map says is really code.
"""
import struct, pathlib

d = pathlib.Path("fv2_prog.bin").read_bytes()
N = len(d)
code = set(int(l, 16) for l in pathlib.Path("map.txt").read_text().split())
CODE_HI = 0x74B7C          # the reference's own count of the code this game runs

def se(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v

bl, bcl = set(), set()
for a in range(0, min(N, CODE_HI) - 3, 4):
    op = struct.unpack_from(">I", d, a)[0]
    if (op & 0xFC000003) == 0x48000001:            # bl
        t = (a + se((op >> 2) & 0xFFFFFF, 24) * 4) & 0xFFFFFFFF
        if 0 < t < CODE_HI:
            bl.add(t)
    elif (op & 0xFC000003) == 0x40000001:          # bcl
        t = (a + se((op >> 2) & 0x3FFF, 14) * 4) & 0xFFFFFFFF
        if 0 < t < CODE_HI:
            bcl.add(t)
print("bl targets  : %d" % len(bl))
print("bcl targets : %d" % len(bcl))
tg = bl | bcl
print("together    : %d" % len(tg))
print("of them in the recompiler's map: %d" % len(tg & code))
print("in the map but never called     : %d  (branch targets inside functions)"
      % len(code - tg))
pathlib.Path("calls.txt").write_text("\n".join("%06X" % x for x in sorted(tg)) + "\n")
