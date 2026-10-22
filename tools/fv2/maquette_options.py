"""Maquette de la page OPTIONS : batie sur le releve de la capture Dreamcast.

Tout nombre ici vient de docs/fv2-dreamcast-menus.md section 3bis, sauf deux, qui
sont dits a leur place. L'image fait exactement la taille de la capture (1878 x
1405), pour que les fractions se transportent sans conversion.

    py -3 maquette2.py <ligne choisie> <sortie> [titre 0|1] [oblique 0|1]
"""
import math
import sys
from PIL import Image

A = "assets/menu-console/"
CELL, COLS = 64, 16
TRACK, SPACE = 5.0 / CELL, 16.0 / CELL
CAP = 31.0 / CELL           # la hauteur de capitale dans la case : y 16 a 46, mesure
OBLIQUE = math.radians(7.5)  # la pente des lignes de reglages du disque, mesuree

POLICE = Image.open(A + "police.png").convert("RGBA")
INK = []


def mesurer():
    px = POLICE.load()
    for r in range(6):
        for c in range(COLS):
            x0, y0 = c * CELL, r * CELL
            xs = [x for x in range(CELL) for y in range(CELL) if px[x0 + x, y0 + y][3]]
            INK.append((min(xs), max(xs) - min(xs) + 1) if xs else (0, 0))


def largeur(s, h):
    x, premier = 0.0, True
    for ch in s:
        i = ord(ch) - 0x20
        i = i if 0 <= i < 96 else 0
        if not premier:
            x += TRACK * h
        premier = False
        x += (INK[i][1] / CELL if INK[i][1] else SPACE) * h
    return x


def ecrire(img, s, x, y, h, col, pencher=False):
    """La case entiere est dessinee : la feuille centre ses glyphes, donc capitales et
    chiffres -- les seuls employes ici -- gardent leur alignement sans calcul."""
    for ch in s:
        i = ord(ch) - 0x20
        i = i if 0 <= i < 96 else 0
        if not INK[i][1]:
            x += SPACE * h + TRACK * h
            continue
        sx, sw = INK[i]
        g = POLICE.crop(((i % COLS) * CELL + sx, (i // COLS) * CELL,
                         (i % COLS) * CELL + sx + sw, (i // COLS) * CELL + CELL))
        w = sw / CELL * h
        g = g.resize((max(1, round(w)), max(1, round(h))), Image.LANCZOS)
        if pencher:
            d = math.tan(OBLIQUE) * g.height
            g = g.transform((g.width + int(d) + 2, g.height), Image.AFFINE,
                            (1, math.tan(OBLIQUE), -d, 0, 1, 0), Image.BICUBIC)
        teinte = Image.new("RGBA", g.size, col + (255,))
        teinte.putalpha(g.split()[3])
        img.alpha_composite(teinte, (round(x), round(y)))
        x += w + TRACK * h
    return x


LIGNES = [
    ("MATCH COUNT 1P", "2"), ("MATCH COUNT VS", "3"),
    ("ROUND TIME 1P", "30"), ("ROUND TIME VS", "45"),
    ("DIFFICULTY", "NORMAL"),
    ("ENERGY MAX 1P", "210"), ("ENERGY MAX VS", "210"),
    ("ADVERTISE SOUND", "ON"), ("SURVIVAL MODE", "OFF"),
    ("SUPER K.O.", "ON"), ("VS FINISH", "OFF"), ("BIKE LOGO", "ON"),
    ("COUNTRY", "JAPAN"),
]

SEL = int(sys.argv[1]) if len(sys.argv) > 1 else 5
SORTIE = sys.argv[2] if len(sys.argv) > 2 else "maquette2.png"
TITRE = int(sys.argv[3]) if len(sys.argv) > 3 else 1
PENCHE = int(sys.argv[4]) if len(sys.argv) > 4 else 1

mesurer()

W, H = 1878, 1405
img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
# le fond du disque est option_bg.png TEL QUEL : mesure, il n'est pas assombri
img.alpha_composite(Image.open(A + "option_bg.png").convert("RGBA")
                    .crop((447, 0, 3183, 2048)).resize((W, H), Image.LANCZOS))

JAUNE = (252, 252, 10)      # l'encre de la ligne choisie, relevee
BLANC = (247, 246, 246)     # celle des autres, relevee
SOURD = (150, 150, 150)     # pour COUNTRY, en lecture seule : pas du disque, choisi ici

LIB_X = 0.3136 * W          # les libelles, cales a gauche
VAL_MID = 0.665 * W         # les valeurs, centrees
FILET_G, FILET_D = 0.2913 * W, 0.7295 * W

h = (0.0292 / CAP) * H      # la hauteur de case qui donne la capitale du disque
HAUT = 0.185 * H            # la premiere ligne (le disque la met a 0.2128, mais il n a que sept lignes)
# Le disque tient SEPT lignes a un pas de 0.0584 ; il nous en faut TREIZE. Le pas est
# donc resserre pour que la derniere s'arrete avant INITIALIZE -- le seul nombre de
# cette disposition qui ne vienne pas de la capture.
PAS = (0.800 * H - HAUT) / (len(LIGNES) - 1)

if TITRE:
    ht = (0.0442 / CAP) * H
    ecrire(img, "OPTIONS", W * 0.5 - largeur("OPTIONS", ht) / 2, 0.0996 * H, ht, BLANC)

curseur = Image.open(A + "option_cursor.png").convert("RGBA")

for i, (nom, val) in enumerate(LIGNES):
    y = HAUT + i * PAS - h * (16.0 / CELL)   # le haut de case, l'encre commencant a 16/64
    choisi = i == SEL
    if choisi:
        cw = int(FILET_D - FILET_G)
        img.alpha_composite(curseur.resize((cw, max(1, round(h * 0.40))), Image.LANCZOS),
                            (int(FILET_G), int(y + h * (46.0 / CELL) + h * 0.04)))
    col = JAUNE if choisi else (SOURD if nom == "COUNTRY" else BLANC)
    ecrire(img, nom, LIB_X, y, h, col, PENCHE)
    ecrire(img, val, VAL_MID - largeur(val, h) / 2, y, h, col, PENCHE)

ini = Image.open(A + "option_initialize.png").convert("RGBA")
ih = int(0.0242 * H * 1.45)
iw = int(ini.width * ih / ini.height)
img.alpha_composite(ini.resize((iw, ih), Image.LANCZOS), (int(W * 0.5 - iw / 2), int(0.848 * H)))

hp = h * 0.62
for ligne, yy in (("USE THE DIRECTION BUTTON TO SELECT.", 0.902),
                  ("A : DECIDE        B : BACK", 0.940)):
    ecrire(img, ligne, W * 0.5 - largeur(ligne, hp) / 2, yy * H, hp, BLANC)

img.convert("RGB").save(SORTIE)
print("ecrit", SORTIE)
