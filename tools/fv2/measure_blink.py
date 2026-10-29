# Measures the chosen line's blink, on captures of the window.
#
# The chosen line's alpha falls to 0.5 and comes back up (src/host/fv2_mode_list.h). A still picture cannot
# show it: several captures taken at different moments of the cycle are needed, and the chosen line's ink
# has to be compared with that of a line which does not move.
#
#   py -3 -I tools/fv2/measure_blink.py build/blink-*.png
#
# The bands are the measured ones (docs/fv2-states.md), brought down to the capture's pixels.
import sys
from PIL import Image

IMG_W, IMG_H = 709, 897
BANDS = [0.0, 124.5, 254.0, 381.5, 511.5, 640.5, 769.0, 897.0]
NAMES = ["ARCADE", "V.S.", "RANDOM", "SURVIVAL", "NETWORK", "TRAINING", "OPTIONS"]
INK_MID = [0.4006, 0.1728, 0.4379, 0.4732, 0.5035, 0.4746, 0.4281]
TOP, HEIGHT, CENTRE = 0.26157, 0.35557, 0.5
GROW, LIFT = 7.0 / 6.0, 50.0 / 1080.0
DRAWN_HEIGHT = HEIGHT * GROW
DRAWN_TOP = TOP + (HEIGHT - DRAWN_HEIGHT) * 0.5 - LIFT


def ink(px, w, h, line):
    """The mean lightness of this line's ink pixels (the lightest ones in its band)."""
    block_h = DRAWN_HEIGHT * h
    block_w = block_h * IMG_W / IMG_H
    block_x = w * CENTRE - block_w * 0.5
    block_y = DRAWN_TOP * h
    y0 = int(block_y + BANDS[line] / IMG_H * block_h)
    y1 = int(block_y + BANDS[line + 1] / IMG_H * block_h)
    x0 = int(block_x + (0.5 - INK_MID[line]) * block_w)
    x1 = int(x0 + block_w)
    vals = []
    for y in range(max(0, y0), min(h, y1)):
        for x in range(max(0, x0), min(w, x1)):
            r, g, b = px[x, y]
            v = (r + g + b) / 3
            if v > 90:          # the ink, not the darkened background
                vals.append(v)
    return (sum(vals) / len(vals), len(vals)) if vals else (0.0, 0)


def main(paths):
    print("%-22s %10s %10s %8s" % ("capture", NAMES[0], NAMES[3], "ratio"))
    for p in paths:
        im = Image.open(p).convert("RGB")
        px, (w, h) = im.load(), im.size
        a, na = ink(px, w, h, 0)       # ARCADE, the line chosen when it opens
        s, ns = ink(px, w, h, 3)       # SURVIVAL, which does not blink
        print("%-22s %10.1f %10.1f %8.3f" % (p.split("/")[-1].split("\\")[-1], a, s, a / s if s else 0))


if __name__ == "__main__":
    main(sys.argv[1:])
