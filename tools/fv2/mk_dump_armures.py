#!/usr/bin/env python3
"""Les scripts du dump « armures explosees » : le second visage du jeu.

    mk_dump_armures.py [<nombre de couleurs>]

Une course par case du curseur et par couleur, dans
tools/scripts/dump-armures/.

Pourquoi un second dump. Un combattant de Fighting Vipers 2 porte une armure,
et sous l'armure il y a un autre modele, avec ses propres textures. Le premier
dump tenait la garde tout le round pour que l'armure ne tombe pas : il n'a donc
jamais vu ce qui est dessous. Le chrome de la case 14 en est la preuve par
l'absurde -- zero texture propre sur 240 rencontres, parce que tout son corps
est l'armure.

Comment on fait tomber l'armure. `flag0` du joueur 1 est a `0x00701444`
(docs/fv2-honey.md) ; ses bits : 0x01 et 0x02 « la piece a atteint sa limite »,
0x04 et 0x08 « la piece est TOMBEE ». C'est 0x04 et 0x08 que le modele lit, pas
0x01 et 0x02. Le lanceur ecrit donc 15 (0x0F) dans l'octet de poids faible, ce
qui est « les deux pieces a leur limite et les deux tombees ».

Et le moment compte plus que la valeur : **le modele est choisi quand un round
se charge**, jamais pendant. Une armure arrachee en plein round ne change rien
a l'image avant la fin de ce round. D'ou la forme de ces courses :

    3150   lecture aleatoire, pour que le round se joue
    3200   le poke commence et ne s'arrete plus
    ~4900  le round 1 se termine (le chrono y suffit, mesure)
    5150   les marques de rendu commencent -- c'est le round 2 qu'on capture
    7400   fin

Les marques avant 5150 seraient du gaspillage : elles ne montreraient que le
modele intact, que le premier dump a deja.
"""

import os
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
SORTIE = os.path.normpath(os.path.join(ICI, "..", "scripts", "dump-armures"))

CURSEUR_MAX = 14
DEBUT, FIN, PAS = 5150, 7400, 12
GRAINE = 20261021


def course(curseur, couleur):
    L = ["# Case %d du curseur, couleur %d, armure explosee." % (curseur, couleur),
         "# Le lanceur tient VF3_POKE=701444=15 a partir de l'image 3200 ; les",
         "# marques de rendu ne commencent qu'au round 2, ou le modele est relu.",
         "# Genere par tools/fv2/mk_dump_armures.py -- ne pas editer a la main.",
         "1900 press p1.coin",
         "1930 press p1.coin",
         "1960 press p1.start"]
    t = 2040
    for _ in range(curseur):
        L.append("%d press p1.right" % t)
        t += 16
    for _ in range(couleur - 1):
        t += 24
        L.append("%d press p1.up" % t)
    t += 30
    L.append("%d press p1.punch" % t)
    assert t < 2400, (curseur, couleur, t)
    # La lecture aleatoire fait vivre le round ; le chrono le termine de toute
    # facon, ce qui rend la course independante du combattant.
    L.append("3150 random p1 %d" % GRAINE)
    L += ["%d shot t%03d" % (f, i)
          for i, f in enumerate(range(DEBUT, FIN + 1, PAS))]
    L.append("%d quit" % (FIN + 100))
    return "\r\n".join(L) + "\r\n"


couleurs = int(sys.argv[1]) if len(sys.argv) > 1 else 2
os.makedirs(SORTIE, exist_ok=True)
for f in os.listdir(SORTIE):
    if f.endswith(".txt"):
        os.remove(os.path.join(SORTIE, f))

n = 0
for c in range(CURSEUR_MAX + 1):
    for k in range(1, couleurs + 1):
        nom = "a%02d-couleur%d.txt" % (c, k)
        open(os.path.join(SORTIE, nom), "wb").write(course(c, k).encode("ascii"))
        n += 1
print("%d scripts dans %s  (%d couleurs)" % (n, SORTIE, couleurs))
