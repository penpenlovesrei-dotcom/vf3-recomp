"""Turn the debug-table scan into an address,name list Ghidra can apply."""
import json, pathlib, re, collections
j = json.loads(pathlib.Path("dbg.json").read_text())
by = collections.defaultdict(list)
for e in j["entries"]:
    if e["addr"] is None or not (0x1000 <= e["addr"] < 0x800000):
        continue
    by[e["addr"]].append(e)
def clean(s):
    return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_") or "x"
out = {}
for a, es in by.items():
    # several pages may name one address; take the shortest label, and keep the page
    # that gave it so the name says where Sega wrote it
    e = es[0]          # the first page that names it, in the program's own order
    out[a] = "%s__%s" % (clean(e["page"]), clean(e["label"]))
lines = ["# address,name -- Sega's own debug-menu names, read out of the program image",
         "# %d addresses, from %d named entries on %d pages" % (len(out), len(j["entries"]), len(j["pages"]))]
for a in sorted(out):
    lines.append("%08X,%s" % (a, out[a]))
pathlib.Path("symbols_dbg.csv").write_text("\n".join(lines) + "\n")
print("%d addresses -> symbols_dbg.csv" % len(out))
