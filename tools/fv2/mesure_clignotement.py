# Mesure le clignotement de la ligne choisie, sur des captures de la fenetre.
#
# La ligne choisie voit son alpha descendre a 0,5 et remonter (src/host/fv2_mode_list.h). Une image fixe ne
# peut pas le montrer : il faut plusieurs captures prises a des instants differents du cycle, et comparer
# l'encre de la ligne choisie a celle d'une ligne qui, elle, ne bouge pas.
#
#   py -3 -I tools/fv2/mesure_clignotement.py build/blink-*.png
#
# Les bandes sont celles du relevé (docs/fv2-etats.md), ramenées aux pixels de la capture.
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
    """La clarte moyenne des pixels d'encre de cette ligne (les plus clairs de sa bande)."""
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
            if v > 90:          # l'encre, pas le fond assombri
                vals.append(v)
    return (sum(vals) / len(vals), len(vals)) if vals else (0.0, 0)


def main(paths):
    print("%-22s %10s %10s %8s" % ("capture", NAMES[0], NAMES[3], "rapport"))
    for p in paths:
        im = Image.open(p).convert("RGB")
        px, (w, h) = im.load(), im.size
        a, na = ink(px, w, h, 0)       # ARCADE, la ligne choisie a l'ouverture
        s, ns = ink(px, w, h, 3)       # SURVIVAL, qui ne clignote pas
        print("%-22s %10.1f %10.1f %8.3f" % (p.split("/")[-1].split("\\")[-1], a, s, a / s if s else 0))


if __name__ == "__main__":
    main(sys.argv[1:])
