#!/usr/bin/env python3
# Extracts the sound board's 68000 program from a ROM zip and writes it in the 68000's order.
#
#   tools/sound/extract-sound.py <game.zip> <out.bin>
#
# The byte order is the only delicate thing, and it was SETTLED BY MEASUREMENT, not supposed.
# romset.cpp lays the SOUND region down with stride 1, chunk 1, swap false: the file's bytes go in
# as they are. Then SoundBoard::Bus::word reads `v[a] | v[a+1] << 8`, that is the LOW byte first --
# where the 68000 is big-endian. So the file is swapped with respect to what the 68000 sees, and it
# has to be put the right way round for a disassembler.
#
# The proof is in the reset vectors, which are the first eight words:
#
#   swapped (low byte first)     SSP 00002000  PC 00600120   <- sound: the stack at the top of the
#                                                               8 KB, the code in the program ROM
#   straight big-endian          SSP 00000020  PC 60002001   <- an odd address, off the board
#
# And with the swap, vectors 2 and 3 (bus error, address error) fall at 006005AC and 006005B0,
# inside the ROM as well. Four sound vectors against one false: the order is settled.
import sys
import zipfile

# The name of the sound ROM in each set, as src/machine/romset.cpp names them.
SOUND_ROMS = (
    "epr-20600a.21",   # Fighting Vipers 2
    "epr-20636.21",
    "epr-19231.21",    # Virtua Fighter 3
)


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2].strip("# "), file=sys.stderr)
        return 2
    src, dst = argv[1], argv[2]
    with zipfile.ZipFile(src) as z:
        names = {n.lower(): n for n in z.namelist()}
        found = next((names[r] for r in SOUND_ROMS if r in names), None)
        if found is None:
            print("no known sound ROM in %s; what is there: %s"
                  % (src, ", ".join(sorted(names.values())[:8])), file=sys.stderr)
            return 1
        data = bytearray(z.read(found))

    if len(data) % 2:
        print("%s is %d bytes, an odd number: this is not a 16-bit ROM"
              % (found, len(data)), file=sys.stderr)
        return 1

    for i in range(0, len(data), 2):
        data[i], data[i + 1] = data[i + 1], data[i]

    # The check that says the swap was the right way round: after it, the reset PC must fall inside
    # the program ROM, at an even address. If it does not, we stop instead of writing a false image
    # that would then be disassembled for an hour.
    ssp = int.from_bytes(data[0:4], "big")
    pc = int.from_bytes(data[4:8], "big")
    if not (0x600000 <= pc < 0x680000) or pc % 2:
        print("after the swap, the reset PC is %08X: outside the program ROM, or odd. "
              "This ROM's byte order is not the one expected; nothing has been written."
              % pc, file=sys.stderr)
        return 1

    with open(dst, "wb") as f:
        f.write(bytes(data))
    print("%s: %d bytes, SSP %08X, reset PC %08X -> %s" % (found, len(data), ssp, pc, dst))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
