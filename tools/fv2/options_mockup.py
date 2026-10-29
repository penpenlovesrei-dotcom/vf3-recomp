"""A mock-up of the OPTIONS page: built on the measurements of the Dreamcast capture.

Every number here comes from docs/fv2-dreamcast-menus.md section 3bis, except two,
which are stated where they are used. The picture is exactly the capture's size
(1878 x 1405), so that the fractions carry over without conversion.

    py -3 options_mockup.py <chosen line> <output> [title 0|1] [slant 0|1]
"""
import math
import os
import sys
from PIL import Image, ImageChops

A = "assets/menu-console/"
CELL, COLS = 64, 16
TRACK, SPACE = 5.0 / CELL, 16.0 / CELL
CAP = 31.0 / CELL            # the capital height within the cell: y 16 to 46, measured
SLANT = math.radians(7.5)    # the slant of the disc's setting lines, measured
# The disc's title is a NARROW bold, which font.png does not carry. Four numbers
# measured on the capture: its width is 4.143 capitals and its stems 0.254 of the
# capital, where font.png is 6.000 and 0.194.
#
# It is approached by condensing by a factor c and then THICKENING HORIZONTALLY by s
# pixels -- not with a square dilation, which was tried first: that rounds the corners
# and closes the counters, where the disc has them sharp and open. A narrow bold carries
# its weight on the vertical stems in any case. Horizontal thickening does not touch the
# height, so the wanted capital is kept:
#     6.000*cap*c + s = 4.143*cap        (the width)
#     0.194*cap*c + s = 0.254*cap        (the weight)
# -> c = 0.6698 and s = 0.1241*cap.
WIDTH_DC, WIDTH_FONT = 4.143, 6.000
BOLD_DC, BOLD_FONT = 0.254, 0.194
COND = (WIDTH_DC - BOLD_DC) / (WIDTH_FONT - BOLD_FONT)
THICKEN = BOLD_DC - BOLD_FONT * COND        # in fractions of the capital


def sheet_path():
    """font.png is the repository's name; police.png, the name the sheet arrived
    under, is still accepted."""
    for p in (A + "font.png", A + "police.png"):
        if os.path.isfile(p):
            return p
    return A + "font.png"


FONT = Image.open(sheet_path()).convert("RGBA")
INK = []


def measure():
    px = FONT.load()
    for r in range(6):
        for c in range(COLS):
            x0, y0 = c * CELL, r * CELL
            xs = [x for x in range(CELL) for y in range(CELL) if px[x0 + x, y0 + y][3]]
            INK.append((min(xs), max(xs) - min(xs) + 1) if xs else (0, 0))


def width(s, h):
    x, first = 0.0, True
    for ch in s:
        i = ord(ch) - 0x20
        i = i if 0 <= i < 96 else 0
        if not first:
            x += TRACK * h
        first = False
        x += (INK[i][1] / CELL if INK[i][1] else SPACE) * h
    return x


def write(img, s, x, y, h, col, slant=False):
    """The whole cell is drawn: the sheet centres its glyphs, so capitals and digits
    -- the only ones used here -- keep their alignment with no computation."""
    for ch in s:
        i = ord(ch) - 0x20
        i = i if 0 <= i < 96 else 0
        if not INK[i][1]:
            x += SPACE * h + TRACK * h
            continue
        sx, sw = INK[i]
        g = FONT.crop(((i % COLS) * CELL + sx, (i // COLS) * CELL,
                       (i % COLS) * CELL + sx + sw, (i // COLS) * CELL + CELL))
        w = sw / CELL * h
        g = g.resize((max(1, round(w)), max(1, round(h))), Image.LANCZOS)
        if slant:
            d = math.tan(SLANT) * g.height
            g = g.transform((g.width + int(d) + 2, g.height), Image.AFFINE,
                            (1, math.tan(SLANT), -d, 0, 1, 0), Image.BICUBIC)
        tint = Image.new("RGBA", g.size, col + (255,))
        tint.putalpha(g.split()[3])
        img.alpha_composite(tint, (round(x), round(y)))
        x += w + TRACK * h
    return x


LINES = [
    ("MATCH COUNT 1P", "2"), ("MATCH COUNT VS", "3"),
    ("ROUND TIME 1P", "30"), ("ROUND TIME VS", "45"),
    ("DIFFICULTY", "NORMAL"),
    ("ENERGY MAX 1P", "210"), ("ENERGY MAX VS", "210"),
    ("ADVERTISE SOUND", "ON"), ("SURVIVAL MODE", "OFF"),
    ("SUPER K.O.", "ON"), ("VS FINISH", "OFF"), ("BIKE LOGO", "ON"),
    ("COUNTRY", "JAPAN"),
]

SEL = int(sys.argv[1]) if len(sys.argv) > 1 else 5
OUT = sys.argv[2] if len(sys.argv) > 2 else "options_mockup.png"
TITLE = int(sys.argv[3]) if len(sys.argv) > 3 else 1
SLANTED = int(sys.argv[4]) if len(sys.argv) > 4 else 1

measure()

W, H = 1878, 1405
img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
# the disc's background is option_bg.png AS IT IS: measured, it is not darkened
img.alpha_composite(Image.open(A + "option_bg.png").convert("RGBA")
                    .crop((447, 0, 3183, 2048)).resize((W, H), Image.LANCZOS))

YELLOW = (252, 252, 10)     # the chosen line's ink, measured
WHITE = (247, 246, 246)     # the others', measured
MUTED = (150, 150, 150)     # for COUNTRY, read-only: not the disc's, chosen here

LABEL_X = 0.3136 * W        # the labels, flush left
VALUE_MID = 0.665 * W       # the values, centred
RULE_L, RULE_R = 0.2913 * W, 0.7295 * W

h = (0.0292 / CAP) * H      # the cell height that gives the disc's capital
TOP = 0.185 * H             # the first line (the disc puts it at 0.2128, but it has only seven lines)
# The disc holds SEVEN lines at a pitch of 0.0584; we need THIRTEEN. So the pitch is
# tightened for the last one to stop before INITIALIZE -- the only number in this
# layout that does not come from the capture.
PITCH = (0.800 * H - TOP) / (len(LINES) - 1)


def title(img, s, cap, cy, col, grow=True):
    """The disc's narrow bold, approached on font.png (see the head of this file)."""
    th = round(THICKEN * cap) if grow else 0
    c = COND if grow else WIDTH_DC / WIDTH_FONT
    h = cap / CAP                            # the cell that gives this capital
    layer = Image.new("RGBA", (int(width(s, h)) + 8, int(h) + 8), (0, 0, 0, 0))
    write(layer, s, 4, 4, h, (255, 255, 255))
    layer = layer.resize((max(1, int(layer.width * c)), layer.height), Image.LANCZOS)
    a = layer.split()[3]
    if th:
        wide = Image.new("L", (a.width + th, a.height), 0)
        for dx in range(th + 1):
            wide.paste(ImageChops.lighter(wide.crop((dx, 0, dx + a.width, a.height)), a), (dx, 0))
        a = wide
    solid = Image.new("RGBA", a.size, col + (255,))
    solid.putalpha(a)
    # cy is the top of the INK, which begins at 16/64 of the cell
    img.alpha_composite(solid, (int(W * 0.5 - a.width / 2), int(cy - 4 - h * (16.0 / CELL))))


if TITLE:
    title(img, "OPTIONS", 0.0442 * H, 0.0996 * H, WHITE, TITLE == 1)

cursor = Image.open(A + "option_cursor.png").convert("RGBA")

for i, (name, val) in enumerate(LINES):
    y = TOP + i * PITCH - h * (16.0 / CELL)   # the cell top, the ink beginning at 16/64
    chosen = i == SEL
    if chosen:
        cw = int(RULE_R - RULE_L)
        img.alpha_composite(cursor.resize((cw, max(1, round(h * 0.40))), Image.LANCZOS),
                            (int(RULE_L), int(y + h * (46.0 / CELL) + h * 0.04)))
    col = YELLOW if chosen else (MUTED if name == "COUNTRY" else WHITE)
    write(img, name, LABEL_X, y, h, col, SLANTED)
    write(img, val, VALUE_MID - width(val, h) / 2, y, h, col, SLANTED)

ini = Image.open(A + "option_initialize.png").convert("RGBA")
ih = int(0.0242 * H * 1.45)
iw = int(ini.width * ih / ini.height)
img.alpha_composite(ini.resize((iw, ih), Image.LANCZOS), (int(W * 0.5 - iw / 2), int(0.848 * H)))

hp = h * 0.62
for line, yy in (("USE THE DIRECTION BUTTON TO SELECT.", 0.902),
                 ("A : DECIDE        B : BACK", 0.940)):
    write(img, line, W * 0.5 - width(line, hp) / 2, yy * H, hp, WHITE)

img.convert("RGB").save(OUT)
print("written", OUT)
