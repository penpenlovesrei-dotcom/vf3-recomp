# Turn vf3run's --ram-at dump into a big-endian guest image Ghidra can read.
# The dump keeps 32-bit words in the HOST's order, so guest byte A sits at A^3.
import sys, pathlib
src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
d = bytearray(src.read_bytes())
for i in range(0, len(d), 4):
    d[i:i+4] = d[i:i+4][::-1]
PROG = 0x29381E
dst.write_bytes(bytes(d[:PROG]))
# the anchor the brief names
a = 0x0005F3BC
print("program bytes:", PROG)
print("at %08X: %s" % (a, " ".join("%02X" % b for b in d[a:a+4])))
print("expected    : 2C 80 00 C0")
