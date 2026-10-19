#!/usr/bin/env python3
# Writes a recompiler address map from coverage runs of this port's own interpreter (vf3run --coverage),
# for a game whose program is not in the ROM as it stands and so cannot be read the way mkmap.py reads
# Virtua Fighter 3's (Fighting Vipers 2: the program is a gzip in the CROM, which the game's boot stub
# unpacks into RAM at 0).
#
# A coverage run lists the words the program was entered at other than by falling through from the word
# before: call targets, the targets of indirect jumps and returns, interrupt entries. Everything else that
# runs follows one of those in a straight line, and a straight line is exactly what the recompiler's
# analyse() walks, so these addresses alone are the map. Several runs are merged, so each new run can only
# add to it.
#
# Addresses only; nothing read from a ROM goes into the output.
#
#   tools/recomp/mkmap_run.py <name> <out.cpp> <coverage.txt> [more coverage.txt ...]
#     name: the symbol prefix, e.g. fv2  ->  fv2_map[] and fv2_map_count
import sys, os

if len(sys.argv) < 4:
    sys.exit(__doc__ or 'usage: mkmap_run.py <name> <out.cpp> <coverage.txt> ...')

name, out_path, runs = sys.argv[1], sys.argv[2], sys.argv[3:]

seeds = set()
per_run = []
for path in runs:
    here = set()
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            here.add(int(line, 16))
    per_run.append((os.path.basename(path), len(here), len(here - seeds)))
    seeds |= here

seeds = sorted(seeds)
if not seeds:
    sys.exit('no addresses in those coverage runs')

title = {'fv2': 'Fighting Vipers 2'}.get(name, name)
with open(out_path, 'w') as out:
    out.write("// The recompiler's address map for %s: the words its program is entered at other than by\n" % title)
    out.write('// falling through from the word before -- call targets, the targets of indirect jumps and returns,\n')
    out.write('// interrupt entries. Everything else it runs follows one of these in a straight line, which is what\n')
    out.write('// analyse() walks.\n//\n')
    out.write('// Made by tools/recomp/mkmap_run.py from coverage runs of this port\'s own interpreter\n')
    out.write('// (vf3run --coverage), merged:\n')
    for base, n, added in per_run:
        out.write('//   %-28s %6d entry points, %6d of them new\n' % (base, n, added))
    out.write('// Addresses only; no ROM data is here.\n')
    out.write('#include "ppc_rec.h"\n\nnamespace ppcrec {\n\nconst uint32_t %s_map[] = {\n' % name)
    for k in range(0, len(seeds), 10):
        out.write('\t' + ', '.join('0x%06X' % a for a in seeds[k:k + 10]) + ',\n')
    out.write('};\nconst size_t %s_map_count = sizeof(%s_map) / sizeof(%s_map[0]);\n\n} // namespace ppcrec\n'
              % (name, name, name))

print('%s: %d addresses from %d run(s), %#x to %#x' % (out_path, len(seeds), len(runs), seeds[0], seeds[-1]))
