"""Work out where each state handler ends.

A handler is a switch case, so it is not bounded by a return: it ends with an
unconditional branch, either to the shared epilogue at 0x0000AC64 or back to the
dispatcher's head at 0x0000ABF0, or with a plain `blr`. Ghidra, left to itself,
follows that branch and decompiles the whole chain of cases as one function --
4455 lines for a 160-byte body. Bounding the body keeps each one its own size.

The end is taken as the FIRST of: an unconditional `b` to the epilogue or the
dispatcher head, a `blr`, or the start of the next handler.
"""
import struct, pathlib, re

d = pathlib.Path("fv2_prog.bin").read_bytes()
u32 = lambda a: struct.unpack_from(">I", d, a)[0]
EPILOGUE, HEAD = 0x0000AC64, 0x0000ABF0

starts = []
for line in pathlib.Path("fnames.csv").read_text().split("\n"):
    if line.startswith("#") or not line.strip():
        continue
    p = line.split(",", 2)
    starts.append((int(p[0], 16), p[1]))
starts.sort()
addrs = [a for a, _ in starts]
# A called function can sit between two handlers (0x0000D02C does, between
# JUDGE_DSP and SKO_REPLAY_WAIT), so a handler's extent is bounded by those too.
calls = sorted(int(l, 16) for l in pathlib.Path("calls.txt").read_text().split())

def se(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v

rows = []
for i, (a, nm) in enumerate(starts):
    nxt = addrs[i + 1] if i + 1 < len(addrs) else a + 0x1000
    for c in calls:
        if a < c < nxt:
            nxt = c
            break
    if nxt - a > 0x1000:
        nxt = a + 0x1000
    end = nxt
    p = a
    while p < nxt - 3:
        op = u32(p)
        if op == 0x4E800020:                                  # blr
            end = p + 4
            break
        if (op & 0xFC000003) == 0x48000000:                   # b (no link, relative)
            t = (p + se((op >> 2) & 0xFFFFFF, 24) * 4) & 0xFFFFFFFF
            if t in (EPILOGUE, HEAD):
                end = p + 4
                break
        p += 4
    rows.append((a, end, nm))

out = ["# address,end -- each state handler's own extent, nothing of the next one"]
for a, end, nm in rows:
    out.append("%08X,%08X" % (a, end))
pathlib.Path("bodies.csv").write_text("\n".join(out) + "\n")
print("%d bodies" % len(rows))
for a, end, nm in rows[:12]:
    print("  %-30s %08X..%08X  %4d bytes" % (nm, a, end, end - a))
print("  ...")
print("  total code bounded: %d bytes" % sum(e - a for a, e, _ in rows))
