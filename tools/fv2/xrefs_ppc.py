"""Who touches this address? The references to an address in the game's program.

Why this tool exists, and why it was missing. `disasm_ppc.py` can read thirty
instructions AT A GIVEN ADDRESS; it cannot answer "who writes this byte". And that is
the first question asked every time one starts from a piece of data: the byte moves
when the cursor moves, and then what? With no answer, one falls back on pixels -- which
cost three attempts on training mode's CPU label, on 2026-10-28.

The game has no symbol for what it draws. What is named is named because Sega named it
itself (the handler name table, at 0x000244F4). For everything else the road is: the
data, then who writes it, then the thirty instructions around that. This one takes the
middle step.

HOW. The PowerPC has no absolute addressing: an address is made in two steps,

    lis   r3,0x0012          r3 = 0x00120000
    addi  r3,r3,1312         r3 = 0x00120520
    lbz   r4,56(r3)          reads 0x00120558

so the VALUE of the registers has to be followed to know what an instruction touches.
That is what this file does: a small model of the registers, cleared at every branch,
and a report at every load or store whose effective address falls inside the target. It
models only what the game's code uses to make an address -- lis, addis, li, addi, ori --
and sets a register to unknown as soon as it is written by anything else. An unknown
register produces no line: the tool keeps quiet rather than guessing, just as the
disassembler prints `.long` rather than inventing a mnemonic.

WHAT IT DOES NOT FIND, and this is worth knowing before concluding that an address is
touched by nobody: a computed address (an indexed table, `lbzx r3,r4,r5`), an address
passed as an argument, an address read from a pointer in memory. So this tool's silence
is not a proof; a line it prints is one.

Usage:
    py -3 tools/fv2/xrefs_ppc.py <ram.bin> <address hex> [bytes] [start hex] [end hex]

`bytes` widens the target to a range (1 by default: the address alone). `start` and
`end` bound the area swept; by default 0x00001000 to 0x00140000, which is where
Fighting Vipers 2's program sits once unpacked.

The dump is made with VF3_RAM_DUMP=bytes@frame (main.cpp), which writes ram.bin into the
data folder, in the HOST's word order -- the byte at address A is at index A^3.
disasm_ppc.py is imported to read the words back and print them, so the two tools read
the same file the same way.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disasm_ppc as d


# The D-form loads and stores: opcode -> (name, does it write?)
FORM_D = {
    32: ("lwz", False), 33: ("lwzu", False), 34: ("lbz", False), 35: ("lbzu", False),
    40: ("lhz", False), 42: ("lha", False),
    36: ("stw", True), 37: ("stwu", True), 38: ("stb", True), 39: ("stbu", True),
    44: ("sth", True), 45: ("sthu", True),
}


def registers_after(w, regs):
    """The register model after this instruction. Returns False if it is a branch."""
    op = w >> 26
    rd = (w >> 21) & 31
    ra = (w >> 16) & 31
    u = w & 0xFFFF
    s = d.sign16(u)

    if op in (16, 18):                       # a branch: we do not follow, we forget everything
        return False
    if op == 19:                             # blr, bctr and company: the same
        return False
    if op == 15:                             # lis / addis
        if ra == 0:
            regs[rd] = (u << 16) & 0xFFFFFFFF
        elif ra in regs:
            regs[rd] = (regs[ra] + (u << 16)) & 0xFFFFFFFF
        else:
            regs.pop(rd, None)
        return True
    if op == 14:                             # li / addi
        if ra == 0:
            regs[rd] = s & 0xFFFFFFFF
        elif ra in regs:
            regs[rd] = (regs[ra] + s) & 0xFFFFFFFF
        else:
            regs.pop(rd, None)
        return True
    if op == 24:                             # ori rA,rS,u -- an address's second half
        if rd in regs:
            regs[ra] = regs[rd] | u
        else:
            regs.pop(ra, None)
        return True
    if op in FORM_D:
        # the register loaded becomes unknown; a `u` form also updates the base
        if not FORM_D[op][1]:
            regs.pop(rd, None)
        if op in (33, 35, 37, 39, 45) and ra in regs:
            regs[ra] = (regs[ra] + s) & 0xFFFFFFFF
        return True
    # everything else: what is written becomes unknown, and we do not try to find out what
    if op == 31:
        sub = (w >> 1) & 0x3FF
        if sub == 444:                       # or / mr: the only form that carries a value on
            rb = (w >> 11) & 31
            if rd == rb and rd in regs:
                regs[ra] = regs[rd]
            else:
                regs.pop(ra, None)
            return True
        # the forms that write rA (logical) and those that write rD (arithmetic)
        if sub in (28, 316, 124, 60, 444, 467, 824, 536, 792):
            regs.pop(ra, None)
        else:
            regs.pop(rd, None)
        return True
    if op in (7, 10, 11, 21, 28):            # mulli, cmp*, rlwinm, andi.
        if op in (7,):
            regs.pop(rd, None)
        elif op in (21, 28):
            regs.pop(ra, None)
        return True
    regs.pop(rd, None)
    return True


def search(ram, target, size, start, end):
    """The instructions whose effective address falls inside [target, target+size)."""
    found = []
    regs = {}
    a = start
    end = min(end, (len(ram) & ~3))
    while a < end:
        w = d.word(ram, a)
        op = w >> 26
        if op in FORM_D:
            ra = (w >> 16) & 31
            if ra in regs:
                eff = (regs[ra] + d.sign16(w & 0xFFFF)) & 0xFFFFFFFF
                if target <= eff < target + size:
                    name, writes = FORM_D[op]
                    found.append((a, eff, writes, d.one(ram, a)))
        elif op == 14 and ((w >> 16) & 31) != 0:
            # `addi rD,rA,low` that finishes making the address without reading it: the real
            # access is a few instructions further on, with a displacement. It is reported,
            # because that is the shape a settings block takes (0x00120520 + 56).
            ra = (w >> 16) & 31
            rd = (w >> 21) & 31
            if ra in regs:
                val = (regs[ra] + d.sign16(w & 0xFFFF)) & 0xFFFFFFFF
                if target <= val < target + size:
                    found.append((a, val, None, d.one(ram, a)))
                del rd
        if not registers_after(w, regs):
            regs = {}
        a += 4
    return found


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    ram = open(argv[1], "rb").read()
    target = int(argv[2], 16)
    size = int(argv[3]) if len(argv) > 3 else 1
    start = int(argv[4], 16) if len(argv) > 4 else 0x00008000
    end = int(argv[5], 16) if len(argv) > 5 else 0x00140000
    found = search(ram, target, size, start, end)
    print("%08X..%08X : %d reference%s in %08X..%08X"
          % (target, target + size - 1, len(found), "s" if len(found) > 1 else "", start, end))
    for a, eff, writes, text in found:
        what = "  " if writes is None else ("WRITES" if writes else "reads ")
        print("  %08X  %-6s %08X  %s" % (a, what if writes is not None else "form", eff, text))
    if not found:
        print("  (nothing: see WHAT IT DOES NOT FIND, at the head of this file)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
