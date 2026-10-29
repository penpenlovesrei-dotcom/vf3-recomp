"""A PowerPC disassembler, in the standard library, for reading the game's program.

Why write one when the chain in `tools/fv2/README.md` can load the image into
Ghidra: because that needs an instance of Ghidra open, and reading thirty
instructions at a precise address does not deserve setting up a project. The same
reason as the arcade notes' `desas_sh4.py`, for the other processor.

It is NOT a complete disassembler. It covers what the game's code uses --
loads, stores, comparisons, branches, immediate arithmetic, the extended forms of
group 31 that keep coming back -- and it prints the raw word for everything else,
rather than inventing a name. A `.long` line is a useful admission; a wrong
mnemonic is not.

This port's trap: `--ram-at` keeps the 32-bit words in the HOST's order, so the
byte at address A is at index `A ^ 3`. So this file reads every word back by its
address, never by its raw offset.

Usage:
    py -3 tools/fv2/disasm_ppc.py <ram.bin> <address hex> [count]
"""

import sys

SPR = {1: "xer", 8: "lr", 9: "ctr"}


def word(ram, address):
    """The 32-bit word at that game address, in the PowerPC's order."""
    return int.from_bytes(bytes(ram[(address + k) ^ 3] for k in range(4)), "big")


def sign16(v):
    return v - 0x10000 if v & 0x8000 else v


def conditions(bo, bi):
    """A conditional branch's suffix, when it is of a common form."""
    field, bit = bi >> 2, bi & 3
    name = ["lt", "gt", "eq", "so"][bit]
    opposite = {"lt": "ge", "gt": "le", "eq": "ne", "so": "ns"}[name]
    if bo & 0x14 == 0x14:
        return "", ""
    if bo & 0x10:
        return "dnz" if bo & 2 else "dz", ""
    what = name if bo & 8 else opposite
    return what, ("" if field == 0 else "cr%d," % field)


def one(ram, address):
    w = word(ram, address)
    op = w >> 26
    rd = (w >> 21) & 31
    ra = (w >> 16) & 31
    rb = (w >> 11) & 31
    d = sign16(w & 0xFFFF)
    u = w & 0xFFFF

    if op == 14:
        return "li r%d,%d" % (rd, d) if ra == 0 else "addi r%d,r%d,%d" % (rd, ra, d)
    if op == 15:
        return "lis r%d,0x%04X" % (rd, u) if ra == 0 else "addis r%d,r%d,0x%04X" % (rd, ra, u)
    if op == 7:
        return "mulli r%d,r%d,%d" % (rd, ra, d)
    if op == 11:
        return "cmpwi %sr%d,%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, d)
    if op == 10:
        return "cmplwi %sr%d,%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, u)
    if op == 24:
        return "nop" if w == 0x60000000 else "ori r%d,r%d,0x%04X" % (ra, rd, u)
    if op == 28:
        return "andi. r%d,r%d,0x%04X" % (ra, rd, u)
    if op == 21:
        sh, mb, me = (w >> 11) & 31, (w >> 6) & 31, (w >> 1) & 31
        return "rlwinm%s r%d,r%d,%d,%d,%d" % ("." if w & 1 else "", ra, rd, sh, mb, me)
    if op in (32, 34, 36, 38, 40, 44, 33, 35, 37, 39):
        name = {32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu", 36: "stw", 37: "stwu",
               38: "stb", 39: "stbu", 40: "lhz", 44: "sth"}[op]
        return "%s r%d,%d(r%d)" % (name, rd, d, ra)
    if op == 18:
        target = (w & 0x03FFFFFC)
        if target & 0x02000000:
            target -= 0x04000000
        if not (w & 2):
            target += address
        return "%s 0x%08X" % ("bl" if w & 1 else "b", target & 0xFFFFFFFF)
    if op == 16:
        bo, bi = rd, ra
        target = sign16(w & 0xFFFC)
        if not (w & 2):
            target += address
        what, cr = conditions(bo, bi)
        if what:
            return "b%s%s %s0x%08X" % (what, "l" if w & 1 else "", cr, target & 0xFFFFFFFF)
        return "bc %d,%d,0x%08X" % (bo, bi, target & 0xFFFFFFFF)
    if op == 19:
        sub = (w >> 1) & 0x3FF
        if sub == 16:
            what, cr = conditions(rd, ra)
            return ("blr" if not what else "b%slr %s" % (what, cr)) + ("l" if w & 1 else "")
        if sub == 528:
            return "bctr" + ("l" if w & 1 else "")
        return ".long 0x%08X" % w
    if op == 31:
        sub = (w >> 1) & 0x3FF
        if sub == 339:
            s = (ra | (rb << 5))
            return "mf%s r%d" % (SPR.get(s, "spr%d" % s), rd)
        if sub == 467:
            s = (ra | (rb << 5))
            return "mt%s r%d" % (SPR.get(s, "spr%d" % s), rd)
        if sub == 444:
            return "mr r%d,r%d" % (ra, rd) if rd == rb else "or r%d,r%d,r%d" % (ra, rd, rb)
        if sub == 0:
            return "cmpw %sr%d,r%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, rb)
        if sub == 32:
            return "cmplw %sr%d,r%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, rb)
        names = {266: "add", 40: "subf", 235: "mullw", 23: "lwzx", 87: "lbzx", 215: "stbx",
                151: "stwx", 28: "and", 316: "xor", 124: "nor", 60: "andc", 491: "divw"}
        if sub in names:
            if sub in (23, 87, 215, 151):
                return "%s r%d,r%d,r%d" % (names[sub], rd, ra, rb)
            if sub in (28, 316, 124, 60):
                return "%s r%d,r%d,r%d" % (names[sub], ra, rd, rb)
            return "%s r%d,r%d,r%d" % (names[sub], rd, ra, rb)
    return ".long 0x%08X" % w


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    ram = open(argv[1], "rb").read()
    address = int(argv[2], 16)
    combien = int(argv[3]) if len(argv) > 3 else 40
    for i in range(combien):
        a = address + i * 4
        print("%08X  %08X  %s" % (a, word(ram, a), one(ram, a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
