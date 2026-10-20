"""Read Sega's own debug-menu tables out of the Fighting Vipers 2 program image.

Measured layout, per page, in this order in memory:

    [ entry array, 0x20 bytes an entry ]
    [ terminator, whose FIRST WORD is the base the entries' +0C are offsets from ]
    [ page record, three words: -> page name, -> first entry, -> terminator ]
    [ four more words, not decoded ]

An entry's fields: +00 -> label, +08 -> name array (enum and bit fields), +0C the
variable, +1C the type.  A blank menu line is an entry whose label is the empty
string, whose variable is 0 and whose type is 0x00001000.
"""
import re, struct, sys, pathlib, json

d = pathlib.Path(sys.argv[1]).read_bytes()
N = len(d)
u32 = lambda a: struct.unpack_from(">I", d, a)[0]

def text(a, maxlen=40):
    """The NUL-terminated string at a, "" for an empty one, None if it is not one."""
    if not (0 < a < N - 1):
        return None
    if d[a] == 0:
        return ""
    m = re.match(rb"[ -~]{1,%d}\x00" % maxlen, d[a:a + maxlen + 1])
    return m.group()[:-1].decode() if m else None

pages = []
for a in range(0, N - 12, 4):
    name_p, first_p, term_p = u32(a), u32(a + 4), u32(a + 8)
    if not (0 < first_p < N and first_p < term_p < N):
        continue
    span = term_p - first_p
    if span == 0 or span % 0x20 or span > 0x20 * 256:
        continue
    name = text(name_p, 32)
    if not name or len(name) < 2:
        continue
    n = span // 0x20
    labels = [text(u32(first_p + i * 0x20)) for i in range(n)]
    if any(l is None for l in labels):
        continue
    if not any(labels):                      # a page of nothing but blanks
        continue
    pages.append(dict(rec=a, name=name, first=first_p, term=term_p, n=n,
                      base=u32(term_p), labels=labels))

seen, keep = set(), []
for p in sorted(pages, key=lambda p: (p["first"], p["rec"])):
    if p["first"] in seen:
        continue
    seen.add(p["first"]); keep.append(p)
pages = keep

entries = []
for p in pages:
    for i in range(p["n"]):
        e = p["first"] + i * 0x20
        var, typ = u32(e + 0x0C), u32(e + 0x1C)
        label = p["labels"][i]
        if not label:                        # a blank menu line
            continue
        # Measured, over all 80 pages: a +0C field below 0x10000 is an OFFSET from the
        # page's base, and one at or above it is already an absolute address.  The two
        # ranges do not overlap -- the largest offset in the whole set is 0x6060 and the
        # smallest absolute 0x114F3C.  Four of the brief's own anchors come back out of
        # this rule unchanged: ADV_INFO "SCENE No." = 0x580DFC, ADV_INFO "FRAME" =
        # 0x580DD2, STAGE_TEST "*STAGE" = 0x58FD9F, ROB0_ARMOR "armor_flag" =
        # 0x700500 + 0xF44 = 0x701444.
        # A var of 0 on a page with no base is a menu line that calls a function rather
        # than showing a variable (EVENT_TIME, MAIN_MENU, TEST are all of these).
        addr = None if (var == 0 and not p["base"]) else (
            p["base"] + var if var < 0x10000 else var)
        entries.append(dict(page=p["name"], base=p["base"], entry=e, label=label,
                            field=var, type=typ, addr=addr, names=u32(e + 8)))

print("pages: %d   entries: %d (%d of them named variables)"
      % (len(pages), sum(p["n"] for p in pages), len(entries)))
print("distinct addresses: %d" % len({e["addr"] for e in entries if e["addr"]}))
pathlib.Path(sys.argv[2]).write_text(json.dumps(
    dict(pages=[{k: v for k, v in p.items() if k != "labels"} for p in pages],
         entries=entries), indent=1))
if len(sys.argv) > 3:
    for p in pages:
        print("  %-24s rec %06X  %3d  base %08X" % (p["name"], p["rec"], p["n"], p["base"]))
