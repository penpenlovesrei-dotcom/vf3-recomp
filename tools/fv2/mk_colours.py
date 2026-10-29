#!/usr/bin/env python3
"""Combien de colours un costume a-t-il, et comment on les atteint.

    mk_colours.py <curseur> [<curseur> ...]

Ecrit dans tools/scripts/colours/ une course par nombre de pressions de HAUT
sur la case, de 0 a 3, et une par pression de BAS, de 1 a 3. Chacune s'arrete
juste apres l'image 3300, ou le lanceur lit *1P COLOR (0x007005B9).

Pourquoi BAS aussi : rien ne dit que les colours se prennent dans un seul
sens. Si la rangee de colours boucle, HAUT x3 et BAS x1 donnent la meme, et
c'est la mesure qui le dira, pas le raisonnement.

Aucune marque de rendu : sans `shot` le port ne dessine rien, et on ne veut
ici que l'octet.
"""

import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
SORTIE = os.path.normpath(os.path.join(ICI, "..", "scripts", "colours"))


def course(curseur, sens, combien):
    L = ["# Case %d du curseur, %d pression(s) de %s avant le poing."
         % (curseur, combien, sens.upper()),
         "# Genere par tools/fv2/mk_colours.py -- ne pas editer a la main.",
         "1900 press p1.coin",
         "1930 press p1.coin",
         "1960 press p1.start"]
    t = 2040
    for _ in range(curseur):
        L.append("%d press p1.right" % t)
        t += 16
    for _ in range(combien):
        t += 24
        L.append("%d press p1.%s" % (t, sens))
    t += 30
    L.append("%d press p1.punch" % t)
    assert t < 2400, (curseur, sens, combien, t)
    L.append("3310 quit")
    return "\r\n".join(L) + "\r\n"


os.makedirs(SORTIE, exist_ok=True)
for f in os.listdir(SORTIE):
    if f.endswith(".txt"):
        os.remove(os.path.join(SORTIE, f))

n = 0
for curseur in (int(a) for a in sys.argv[1:]):
    for combien in range(4):
        nom = "c%02d-up%d.txt" % (curseur, combien)
        open(os.path.join(SORTIE, nom), "wb").write(course(curseur, "up", combien).encode("ascii"))
        n += 1
    for combien in range(1, 4):
        nom = "c%02d-down%d.txt" % (curseur, combien)
        open(os.path.join(SORTIE, nom), "wb").write(course(curseur, "down", combien).encode("ascii"))
        n += 1
print("%d scripts dans %s" % (n, SORTIE))
