"""Name every state handler from the game's own two parallel tables.

Measured: the jump table at 0x00024354 and the name table at 0x00024494 are 80
entries each and run in parallel.  Each dispatcher indexes both at the same base:

    dispatcher        jump table   name table   base
    main   0x0000ABF0 0x00024354   0x00024494    0
    GAME_DSP 0x0000BCB4 0x000243B4 0x000244F4   24
    SEL_DSP  0x0000BAC0 0x00024404 0x00024544   44
    ADV_DSP  0x0000B124 0x0002441C 0x0002455C   50

so handler[i] of the jump table is the state the name table's entry i names.
"""
import struct, pathlib, re

d = pathlib.Path("fv2_prog.bin").read_bytes()
N = len(d)
u32 = lambda a: struct.unpack_from(">I", d, a)[0]

def text(a, m=40):
    if not (0 < a < N - 1):
        return None
    if d[a] == 0:
        return ""
    r = re.match(rb"[ -~]{1,%d}\x00" % m, d[a:a + m + 1])
    return r.group()[:-1].decode() if r else None

JUMP, NAME, COUNT = 0x24354, 0x24494, 80
GROUP = [(0, 24, "mode"), (24, 20, "game"), (44, 6, "sel"),
         (50, 12, "adv"), (62, 7, "ending"), (69, 3, "name"), (72, 8, "sko")]

rows, seen = [], {}
for lo, n, grp in GROUP:
    for k in range(n):
        i = lo + k
        target, label = u32(JUMP + i * 4), text(u32(NAME + i * 4))
        if not label:
            continue
        nm = "fv2_%s_%s" % (grp, re.sub(r"[^A-Za-z0-9]+", "_", label).strip("_"))
        # two states can share a handler (RANKING_INT and RESULT_INT both 0x0000E418)
        if target in seen:
            seen[target] += "__" + label
            continue
        seen[target] = nm
        rows.append((target, nm, "%s state %d of the %s machine, handler from the table at 0x%06X"
                     % (label, k, grp, JUMP + i * 4)))

# the dispatchers and the few routines measurement named
EXTRA = [
    (0x00ABC4, "fv2_mode_tick",
     "The main mode machine, one tick a frame. Called from 0x00039064; measured by "
     "--watch 58FD98, which reports every write to B_MAIN_MODE at PC 0x0000AC10 with LR 0x00039068."),
    (0x00AC64, "fv2_mode_tick_return",
     "The shared epilogue every state handler branches to: restore LR, pop the frame, return."),
    (0x00B13C, "fv2_adv_start_pressed",
     "Leaves the attract for the select screen: sets bit 0x04000000 of ADV_INFO STATUS, "
     "zeroes both fighters' character id (+0xB8) and asks for SEL_INT."),
    (0x00BCCC, "fv2_game_intrude_check",
     "The cut-in (intrude) test: with ROUND_NO not 8 and ADV_INFO STATUS carrying 0x30000000, "
     "asks for INTRUDE_INT."),
]
for a, nm, c in EXTRA:
    rows.append((a, nm, c))

out = ["# address,name,comment -- the state handlers, named from the game's own tables"]
for a, nm, c in sorted(rows):
    out.append("%08X,%s,%s" % (a, nm, c))
pathlib.Path("fnames.csv").write_text("\n".join(out) + "\n")
print("%d handlers named -> fnames.csv" % len(rows))
for a, nm, _ in sorted(rows):
    print("  %08X  %s" % (a, nm))
