#!/usr/bin/env python3
"""Disassembles the sound board's 68000 program, and searches inside it.

  tools/sound/disasm_sound.py <image.bin> --functions
  tools/sound/disasm_sound.py <image.bin> --at 600120 [lines]
  tools/sound/disasm_sound.py <image.bin> --search 1007c0 [1007ff]
  tools/sound/disasm_sound.py <image.bin> --map

The image is the one tools/sound/extract-sound.py writes: the sound ROM put back into the 68000's
order, based at 0x600000 as the board maps it (src/sound/soundboard.h).

Why this tool beside Ghidra: Ghidra is for READING a function, this one for SEARCHING the whole
program -- which instructions touch a range of SCSP registers, who writes into the voice records in
the sound RAM -- and for doing it repeatably from a command line, with no project and no interface.
The two answers this port needed are searches, not readings: see docs/fv2-sound-program.md.

The walk is recursive from the entry points the 68000 carries itself (its vector table), not
linear: a linear pass disassembles the data tables as code and drowns the result. The targets of
bsr/jsr give the functions.
"""
import sys

try:
    from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000
except ImportError:
    print("capstone is missing: py -3 -m pip install capstone", file=sys.stderr)
    sys.exit(2)

BASE = 0x600000
SIZE = 0x80000

# The board's regions, to name what an address touches (soundboard.h).
REGIONS = (
    (0x000000, 0x100000, "SCSP 0's RAM"),
    (0x100000, 0x101000, "SCSP 0's registers"),
    (0x200000, 0x300000, "SCSP 1's RAM"),
    (0x300000, 0x301000, "SCSP 1's registers"),
    (0x400000, 0x400002, "the bank register"),
    (0x600000, 0x680000, "the program ROM"),
    (0x800000, 0x1000000, "the samples (per bank)"),
)


# The SCSP's registers, by offset from its base (scsp.cpp: w16/r16).
def register_name(off):
    if off < 0x400:
        return "slot %d, word %d" % (off // 0x20, (off % 0x20) // 2)
    if off < 0x460:
        return "common +%03X" % off
    if 0x600 <= off < 0x700:
        return "MIXS/EXTS +%03X" % off
    if 0x700 <= off < 0x780:
        return "COEF[%d]" % ((off - 0x700) // 2)
    if 0x780 <= off < 0x7c0:
        return "MADRS[%d]" % ((off - 0x780) // 2)
    if 0x7c0 <= off < 0x800:
        return "0x7C0-0x7FF (THE RANGE IN QUESTION, MADRS+%d)" % ((off - 0x7c0) // 2)
    if 0x800 <= off < 0xc00:
        return "MPRO step %d word %d" % ((off - 0x800) // 8, ((off - 0x800) % 8) // 2)
    if 0xc00 <= off < 0xe00:
        return "TEMP +%03X" % off
    if 0xe00 <= off < 0xec0:
        return "MEMS +%03X" % off
    if 0xec0 <= off < 0xee0:
        return "EFREG[%d]" % ((off - 0xec0) // 2)
    return "+%03X" % off


def region(a):
    for lo, hi, name in REGIONS:
        if lo <= a < hi:
            if name.endswith("registers"):
                return "%s: %s" % (name, register_name(a - lo))
            return name
    return None


STOPS = ("rts", "rte", "rtr")
JUMPS = ("jmp", "bra", "bra.b", "bra.w", "bra.s")
CALLS = ("jsr", "bsr", "bsr.b", "bsr.w", "bsr.s")


def read(path):
    d = open(path, "rb").read()
    if len(d) != SIZE:
        print("careful: %d bytes, expected %d" % (len(d), SIZE), file=sys.stderr)
    return d


def vectors(d):
    """The exception table's targets that fall inside the program ROM."""
    out = {}
    names = {1: "reset", 2: "bus_error", 3: "address_error", 4: "illegal", 9: "trace", 24: "spurious"}
    for i in range(1, 256):
        if 4 * i + 4 > len(d):
            break
        v = int.from_bytes(d[4 * i:4 * i + 4], "big")
        if not (BASE <= v < BASE + SIZE) or v % 2:
            continue
        n = names.get(i) or ("irq%d" % (i - 24) if 25 <= i <= 31 else "vec%d" % i)
        out.setdefault(v, []).append(n)
    return out


def tables(d):
    """The entry points the program's computed jumps hide from the recursive walk.

    WHY. The walk follows bsr/jsr/bra and the branches, but it stops at a `jmp (An)`: nothing in
    the instruction says where it goes. Eleven computed jumps exist in this program, and everything
    reachable only through them was invisible -- 95 functions found against the 168 Ghidra holds. A
    search run over that half RETURNS A COUNT WITHOUT SAYING SO: on 2026-10-25, `--search ee9`
    returned 4 uses while missing at least three, because the control-change handlers are only
    reachable through the table at 0x60141C.

    All eleven are measured, and each has its check in docs/fv2-sound-program.md:

      - four tables of WORDS, each entry being an offset relative to the table's base. Their
        length is written nowhere: it comes from the rule "the table ends where its lowest target
        begins", checked first on the one at 0x602730 whose 16 entries had been counted by hand,
        then cross-checked by the size of the hole Ghidra leaves on each.
      - four tables of branch SLOTS, whose count comes from the index's mask, which is written in
        the code (& 0x0E, & 0x1C, (&7)<<2, and eight for the one at 0x60088E). Two of them have
        only seven `bra`: the eighth case is a handler placed inline.
      - four unrolled blocks of 128 instructions (Duff's devices): two copy, two clear. Their entry
        is seeded.
    """
    out = []
    # base, number of entries -- words, each one an offset from the base
    for base, n in ((0x60141C, 81), (0x602730, 16), (0x602EE0, 26)):
        for k in range(n):
            o = base - BASE + 2 * k
            out.append(base + int.from_bytes(d[o:o + 2], "big"))
    # base, number of slots, step -- the slots themselves are the destinations
    for base, n, step in ((0x60088E, 8, 4), (0x60030A, 8, 2), (0x603496, 8, 4), (0x60376C, 8, 4)):
        out.extend(base + step * k for k in range(n))
    # the unrolled blocks' entries
    out.extend((0x601F3C, 0x6023E2, 0x602044, 0x6024EA))
    return [a for a in out if BASE <= a < BASE + SIZE and not a % 2]


def walk(d, start):
    """A recursive descent. Returns (instructions by address, call targets)."""
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    seen = {}
    calls = set()
    stack = list(start)
    while stack:
        a = stack.pop()
        while True:
            if a in seen or not (BASE <= a < BASE + SIZE) or a % 2:
                break
            off = a - BASE
            try:
                ins = next(md.disasm(d[off:off + 16], a, count=1))
            except StopIteration:
                break
            seen[a] = ins
            m = ins.mnemonic
            op = ins.op_str
            target = None
            if op.startswith("$") or op.startswith("0x"):
                t = op.split(",")[0].strip().lstrip("$")
                try:
                    target = int(t, 16)
                except ValueError:
                    target = None
            if m in CALLS or m.startswith("bsr") or m == "jsr":
                if target is not None and BASE <= target < BASE + SIZE:
                    calls.add(target)
                    stack.append(target)
                a += ins.size
                continue
            if m in STOPS or m.startswith("rts") or m.startswith("rte"):
                break
            if m in JUMPS or m.startswith("bra") or m == "jmp":
                if target is not None:
                    stack.append(target)
                break
            if m.startswith("b") or m.startswith("db"):   # a conditional branch
                if target is not None:
                    stack.append(target)
                a += ins.size
                continue
            a += ins.size
    return seen, calls


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    d = read(argv[1])
    mode = argv[2]
    vec = vectors(d)
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)

    if mode == "--map":
        print("the vector table: %d distinct targets inside the program ROM" % len(vec))
        for a in sorted(vec):
            print("  %08X  %s" % (a, ", ".join(vec[a])))
        return 0

    tab = tables(d)
    seen, calls = walk(d, list(vec) + tab)
    functions = sorted(set(vec) | calls | set(tab))

    if mode == "--functions":
        print("%d instructions reached, %d functions (%d from the vectors, %d called,"
              " %d seeded by the jump tables)"
              % (len(seen), len(functions), len(vec), len(calls), len(set(tab))))
        print("coverage: %.1f %% of the ROM's bytes" % (100.0 * sum(i.size for i in seen.values()) / SIZE))
        for a in functions:
            n = ", ".join(vec[a]) if a in vec else ""
            print("  %08X %s" % (a, n))
        return 0

    if mode == "--at":
        a = int(argv[3], 16)
        n = int(argv[4]) if len(argv) > 4 else 60
        off = a - BASE
        for ins in md.disasm(d[off:off + n * 10], a, count=n):
            r = operand_region(ins)
            print("  %08X  %-10s %-34s %s" % (ins.address, ins.mnemonic, ins.op_str, r or ""))
        return 0

    if mode == "--search":
        lo = int(argv[3], 16)
        hi = int(argv[4], 16) if len(argv) > 4 else lo
        # The scope is printed BEFORE the results, because a count returned by an incomplete walk
        # looks like a complete count. This one includes the eleven computed jumps.
        print("scope: %d instructions reached from %d entry points (%d vectors,"
              " %d seeded by the jump tables)"
              % (len(seen), len(set(list(vec) + tab)), len(vec), len(set(tab))))
        print("instructions reached whose operand names %06X..%06X:" % (lo, hi))
        n = 0
        for a in sorted(seen):
            ins = seen[a]
            for v in values(ins):
                if lo <= v <= hi:
                    print("  %08X  %-10s %-34s -> %06X  %s"
                          % (a, ins.mnemonic, ins.op_str, v, region(v) or ""))
                    n += 1
                    break
        print("%d found" % n)
        return 0

    print(__doc__)
    return 2


def values(ins, addresses_only=False):
    """The operand's hexadecimal constants, as integers.

    addresses_only: drops the immediates (`#$12`), which are not addresses. Without that sorting,
    `andi.b #$12,d7` gets annotated "SCSP 0's RAM" and the annotation is worth nothing any more.
    """
    out = []
    s = ins.op_str
    i = 0
    while i < len(s):
        if s[i] == "#":
            # an immediate: the constant that follows is skipped
            j = i + 1
            if j < len(s) and s[j] == "$":
                j += 1
            while j < len(s) and s[j] in "0123456789abcdefABCDEF":
                j += 1
            if not addresses_only:
                t = s[i + 1:j].lstrip("$")
                if t:
                    try:
                        out.append(int(t, 16))
                    except ValueError:
                        pass
            i = j
            continue
        if s[i] == "$" or (s[i] == "0" and i + 1 < len(s) and s[i + 1] == "x"):
            j = i + (2 if s[i] == "0" else 1)
            k = j
            while k < len(s) and s[k] in "0123456789abcdefABCDEF":
                k += 1
            if k > j:
                try:
                    out.append(int(s[j:k], 16))
                except ValueError:
                    pass
            i = k
        else:
            i += 1
    return out


def operand_region(ins):
    for v in values(ins, addresses_only=True):
        r = region(v)
        if r:
            return "[%06X: %s]" % (v, r)
    return None


if __name__ == "__main__":
    sys.exit(main(sys.argv))
