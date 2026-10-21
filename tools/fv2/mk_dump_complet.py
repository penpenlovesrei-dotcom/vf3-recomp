#!/usr/bin/env python3
"""Ecrit les scripts du dump complet des textures 3D de Fighting Vipers 2.

    tools/scripts/dump-complet/00-attraction.txt      le boot, les logos, l'ecran
                                                      titre et la demo
    tools/scripts/dump-complet/c<NN>-couleur<N>.txt   un combat par case du
                                                      curseur, dans ses deux
                                                      couleurs

Le port ne rend l'image que sur les marques `shot` d'un script : les marques
serrees, une toutes les douze images, ne sont pas la pour faire des images --
le lanceur ne donne pas de `--shots` -- mais pour forcer le rendu, donc
l'echantillonnage. Ce qui n'est pas dessine n'est pas capture.

Le curseur part d'EMI, case 0 ; une pression de droite par case. Sans le
correctif du roster la rangee boucle a la case 10 ; avec `--full-roster` elle
va jusqu'a 14 (docs/fv2-roster.md), et le lanceur le passe. La case 10 est le
tirage au sort : elle donne un combattant quelconque, ce qui ne nuit pas a une
capture dont le but est de voir passer le plus de choses possible.

Une pression de HAUT sur la case avant de confirmer donne la seconde couleur du
costume (*1P COLOR 0x007005B9 passe de 0 a 1).

La garde est tenue pendant le round : une armure qui tombe fait charger un
autre modele. Ici ce n'est pas un defaut -- ces textures-la nous interessent
aussi -- mais la tenir rend les trente courses comparables entre elles.
"""

import os

ICI = os.path.dirname(os.path.abspath(__file__))
SORTIE = os.path.normpath(os.path.join(ICI, "..", "scripts", "dump-complet"))

CURSEUR_MAX = 14          # avec --full-roster ; 10 sans
DEBUT, FIN, PAS = 2400, 4400, 12
ATTRACT_DEBUT, ATTRACT_FIN, ATTRACT_PAS = 60, 1860, 12


def marques(debut, fin, pas, extra=()):
    """Les marques de rendu, plus ce qu'on veut glisser dedans, dans l'ordre :
    le lecteur de scripts s'arrete sur une image qui recule."""
    suite = [(f, "%d shot t%03d" % (f, i))
             for i, f in enumerate(range(debut, fin + 1, pas))]
    suite += list(extra)
    suite.sort(key=lambda c: c[0])
    return [ligne for _, ligne in suite]


def attraction():
    L = ["# Fighting Vipers 2 : le boot, les logos, l'ecran titre et la demo.",
         "# Aucune entree : on regarde tourner l'attraction et tout ce qui",
         "# s'affiche est capture.",
         "# Genere par tools/fv2/mk_dump_complet.py -- ne pas editer a la main."]
    L += marques(ATTRACT_DEBUT, ATTRACT_FIN, ATTRACT_PAS)
    L.append("%d quit" % (ATTRACT_FIN + 60))
    return "\r\n".join(L) + "\r\n"


def combat(curseur, couleur):
    L = ["# Fighting Vipers 2 : case %d du curseur, couleur %d." % (curseur, couleur),
         "# %d pression(s) de droite depuis EMI%s, puis le poing confirme."
         % (curseur, ", une de HAUT" if couleur == 2 else ""),
         "# Genere par tools/fv2/mk_dump_complet.py -- ne pas editer a la main.",
         "1900 press p1.coin",
         "1930 press p1.coin",
         "1960 press p1.start"]
    # Les instants se calculent : a quinze cases, un espacement fixe ferait
    # tomber la derniere pression de droite apres celle de HAUT, et le lecteur
    # de scripts s'arrete sur une image qui recule.
    t = 2040
    for _ in range(curseur):
        L.append("%d press p1.right" % t)
        t += 16
    if couleur == 2:
        t += 30
        L.append("%d press p1.up" % t)
    t += 30
    L.append("%d press p1.punch" % t)
    assert t < DEBUT, (curseur, couleur, t)
    L += marques(DEBUT, FIN, PAS, extra=[(3150, "3150 hold p1.guard")])
    L.append("%d quit" % (FIN + 100))
    return "\r\n".join(L) + "\r\n"


os.makedirs(SORTIE, exist_ok=True)
for f in os.listdir(SORTIE):
    if f.endswith(".txt"):
        os.remove(os.path.join(SORTIE, f))

fichiers = [("00-attraction.txt", attraction())]
for c in range(CURSEUR_MAX + 1):
    for couleur in (1, 2):
        fichiers.append(("c%02d-couleur%d.txt" % (c, couleur), combat(c, couleur)))

for nom, contenu in fichiers:
    with open(os.path.join(SORTIE, nom), "wb") as fh:
        fh.write(contenu.encode("ascii"))
print("%d scripts dans %s" % (len(fichiers), SORTIE))
