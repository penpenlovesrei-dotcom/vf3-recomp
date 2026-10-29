"""Mesure la feuille de glyphes du menu console et imprime la table pour src/host/fv2_text.h.

assets/menu-console/police.png est un atlas ASCII : 1024 x 384, 16 colonnes sur 6
rangees, des cases de 64 x 64, de l'espace (0x20) au DEL (0x7F). Les glyphes sont
blancs sur transparent et CENTRES dans leur case, pas cales a gauche -- donc un
rendu proportionnel ne peut pas se servir de la case, il lui faut la boite d'encre
de chacun. C'est ce que ce script releve.

Il n'invente rien : il lit le canal alpha et donne, pour chaque case, les colonnes
extremes ou il y a de l'encre. Les deux seuls nombres qui ne sortent pas de l'image
sont dans fv2_text.h et disent pourquoi (l'approche entre deux glyphes, et la
chasse de l'espace, qui n'a pas d'encre a mesurer).

Usage :
    py -3 tools/fv2/measure_font.py [chemin de police.png]
"""

import sys

COLONNES, RANGEES = 16, 6
PREMIER = 0x20


def boites(chemin):
    from PIL import Image
    im = Image.open(chemin).convert("RGBA")
    w, h = im.size
    cw, ch = w // COLONNES, h // RANGEES
    px = im.load()
    out = []
    for r in range(RANGEES):
        for c in range(COLONNES):
            x0, y0 = c * cw, r * ch
            gauche, droite, haut, bas = cw, -1, ch, -1
            for y in range(ch):
                for x in range(cw):
                    if px[x0 + x, y0 + y][3]:
                        gauche = min(gauche, x); droite = max(droite, x)
                        haut = min(haut, y); bas = max(bas, y)
            out.append((PREMIER + r * COLONNES + c, gauche, droite, haut, bas))
    return (w, h, cw, ch), out


def main(argv):
    chemin = argv[1] if len(argv) > 1 else "assets/menu-console/police.png"
    (w, h, cw, ch), tout = boites(chemin)
    encres = [b for b in tout if b[2] >= 0]
    print("// %s : %dx%d, cases %dx%d, %d cases, %d avec de l'encre"
          % (chemin, w, h, cw, ch, len(tout), len(encres)))
    print("// vides : %s" % ", ".join("0x%02X" % b[0] for b in tout if b[2] < 0))
    # La ligne de base est celle du gros des capitales, pas leur bas extreme : la queue du Q
    # descend dessous et fausserait un simple maximum. On prend donc la valeur la plus
    # frequente, qui est celle que partagent les 25 autres.
    bas = [b[4] for b in tout if 0x41 <= b[0] <= 0x5A]
    base = max(set(bas), key=bas.count)
    print("// ligne de base : y = %d (le Q descend a %d) ; jambages jusqu'a %d ; haut de capitale : y = %d"
          % (base, max(bas), max(b[4] for b in encres),
             min(b[3] for b in tout if 0x41 <= b[0] <= 0x5A)))
    print("\t// x du bord gauche de l'encre dans la case, puis sa largeur ; 0,0 = pas d'encre")
    print("\tstatic constexpr uint8_t INK[96][2] = {")
    for i in range(0, 96, 8):
        bout = []
        for code, g, d, _, _ in tout[i:i + 8]:
            bout.append("{%2d,%2d}" % ((g, d - g + 1) if d >= 0 else (0, 0)))
        car = "".join(chr(c) if 0x21 <= c <= 0x7E else " " for c, _, _, _, _ in tout[i:i + 8])
        print("\t\t%s,   // 0x%02X  %s" % (", ".join(bout), tout[i][0], car))
    print("\t};")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
