#!/usr/bin/env python3
"""Fait des planches contact des deux lots de textures de Honey.

    planche_honey.py <dossier des captures> <dossier des planches>

Ecrit trois images :

    planche-couleur-1.png   tout le lot, chaque texture a sa taille d'origine
    planche-couleur-2.png   idem
    planche-paires.png      les deux cote a cote, par emplacement en memoire
                            de textures : couleur 1 en haut, couleur 2 en bas

La troisieme est la seule qui prouve quelque chose. Deux textures au meme
(page, x, y) sont la meme piece du modele ; les voir l'une au-dessus de l'autre
montre ce que la couleur change, et ce qu'elle ne change pas.

Le damier derriere chaque vignette est la transparence : ces textures ont un
canal alpha, et une bonne part de Honey est decoupee dedans.
"""

import csv
import os
import sys

from PIL import Image

MARGE, PAS = 8, 4
FOND = (26, 26, 28)
TRAIT = (90, 90, 96)


def lot(dossier):
    noms = {f for f in os.listdir(dossier) if f.endswith(".png")}
    ou = {}
    with open(os.path.join(dossier, "textures.csv"), newline="") as fh:
        for r in csv.DictReader(fh):
            ou[r["name"]] = r
    return noms, ou


def damier(taille):
    """Le fond qui rend l'alpha lisible, 8 pixels de cote."""
    w, h = taille
    im = Image.new("RGB", taille, (62, 62, 66))
    clair = Image.new("RGB", (8, 8), (82, 82, 88))
    for y in range(0, h, 8):
        for x in range(0, w, 8):
            if (x // 8 + y // 8) % 2:
                im.paste(clair, (x, y))
    return im


def pose(toile, chemin, x, y):
    v = Image.open(chemin).convert("RGBA")
    f = damier(v.size)
    f.paste(v, (0, 0), v)
    toile.paste(f, (x, y))
    return v.size


def planche(noms, source, ou, sortie):
    """Une rangee par bande, les textures rangees de la plus grande a la plus
    petite ; chacune garde sa taille d'origine, une planche a l'echelle 1 etant
    le seul moyen de juger ce qu'il y a dedans."""
    ordre = sorted(noms, key=lambda n: (-int(ou[n]["width"]) * int(ou[n]["height"]),
                                        -int(ou[n]["width"]), n))
    largeur = 1100
    x, y, haut_rangee, places = MARGE, MARGE, 0, []
    for n in ordre:
        w, h = int(ou[n]["width"]), int(ou[n]["height"])
        if x + w + MARGE > largeur and x > MARGE:
            x, y = MARGE, y + haut_rangee + PAS
            haut_rangee = 0
        places.append((n, x, y))
        x += w + PAS
        haut_rangee = max(haut_rangee, h)
    hauteur = y + haut_rangee + MARGE
    toile = Image.new("RGB", (largeur, hauteur), FOND)
    for n, px, py in places:
        pose(toile, os.path.join(source, n), px, py)
    toile.save(sortie)
    return len(ordre), toile.size


def paires(s1, s2, d1, d2, o1, sortie):
    """Les emplacements communs, couleur 1 au-dessus de couleur 2."""
    communs = sorted(set(s1) & set(s2),
                     key=lambda k: (-int(o1[s1[k]]["width"]) * int(o1[s1[k]]["height"]), k))
    largeur = 1100
    x, y, haut_rangee, places = MARGE, MARGE, 0, []
    for k in communs:
        w, h = int(o1[s1[k]]["width"]), int(o1[s1[k]]["height"])
        bloc = h * 2 + PAS
        if x + w + MARGE > largeur and x > MARGE:
            x, y = MARGE, y + haut_rangee + PAS * 3
            haut_rangee = 0
        places.append((k, x, y, w, h))
        x += w + PAS
        haut_rangee = max(haut_rangee, bloc)
    hauteur = y + haut_rangee + MARGE
    toile = Image.new("RGB", (largeur, hauteur), FOND)
    for k, px, py, w, h in places:
        pose(toile, os.path.join(d1, s1[k]), px, py)
        pose(toile, os.path.join(d2, s2[k]), px, py + h + PAS)
    toile.save(sortie)
    return len(communs), toile.size


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    captures, sortie = sys.argv[1], sys.argv[2]
    os.makedirs(sortie, exist_ok=True)
    d1 = os.path.join(captures, "couleur1")
    d2 = os.path.join(captures, "couleur2")
    c1, o1 = lot(d1)
    c2, o2 = lot(d2)
    t, _ = lot(os.path.join(captures, "temoin"))
    h1, h2 = c1 - t, c2 - t

    n, taille = planche(h1, d1, o1, os.path.join(sortie, "planche-couleur-1.png"))
    print("planche-couleur-1  %3d textures  %dx%d" % (n, taille[0], taille[1]))
    n, taille = planche(h2, d2, o2, os.path.join(sortie, "planche-couleur-2.png"))
    print("planche-couleur-2  %3d textures  %dx%d" % (n, taille[0], taille[1]))

    s1 = {(o1[f]["page"], o1[f]["x"], o1[f]["y"]): f for f in h1}
    s2 = {(o2[f]["page"], o2[f]["x"], o2[f]["y"]): f for f in h2}
    n, taille = paires(s1, s2, d1, d2, o1, os.path.join(sortie, "planche-paires.png"))
    print("planche-paires     %3d paires    %dx%d" % (n, taille[0], taille[1]))

    memes = sum(1 for k in set(s1) & set(s2) if s1[k] == s2[k])
    autres = len(set(s1) & set(s2)) - memes
    print()
    print("au meme emplacement, meme image    %3d" % memes)
    print("au meme emplacement, autre image   %3d" % autres)
    print("vues dans une seule course         c1 %d, c2 %d"
          % (len(set(s1) - set(s2)), len(set(s2) - set(s1))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
