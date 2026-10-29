#!/usr/bin/env python3
"""Ce que le dump complet a reellement couvert.

    dump_coverage.py <Textures3D-dump>

Lit les vidages de RAM que chaque course a laisses et dit, pour chacune, quel
combattant a ete joue et dans quelle couleur -- parce qu'une case du curseur
n'est pas une promesse : la case 10 tire au sort, et au-dela de 10 la rangee ne
va quelque part que si --full-roster a bien ete pris.

Adresses, mesurees (docs/fv2-honey.md) :
    0x007005B8  le combattant du joueur 1, par son identifiant de combat
    0x007005B9  *1P COLOR, 0 ou 1
L'octet d'adresse A est a l'indice A^3 du vidage : board.ram garde les mots de
32 bits dans l'ordre de l'hote.
"""

import collections
import csv
import os
import sys

NOMS = {0: "GRACE", 1: "BAHN", 2: "RAXEL", 3: "TOKIO", 4: "SANMAN", 5: "JANE",
        6: "HONEY", 7: "PICKY", 8: "MAHLER", 9: "BM", 10: "CHARLIE", 11: "TECHNO (Emi)"}

PERSO, COULEUR = 0x007005B8, 0x007005B9


def octet(d, a):
    i = a ^ 3
    return d[i] if i < len(d) else None


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    base = sys.argv[1]
    rams = os.path.join(base, "ram")
    lignes, vus = [], collections.Counter()
    for nom in sorted(os.listdir(rams)) if os.path.isdir(rams) else []:
        if not nom.endswith(".ram"):
            continue
        d = open(os.path.join(rams, nom), "rb").read()
        p, c = octet(d, PERSO), octet(d, COULEUR)
        lignes.append((nom[:-4], p, c))
        if p is not None:
            vus[(p, c)] += 1

    print("  course              combattant            couleur")
    for course, p, c in lignes:
        qui = "%2d %s" % (p, NOMS.get(p, "?")) if p is not None else " - "
        print("  %-18s  %-20s  %s" % (course, qui, c if c is not None else "-"))

    distincts = {p for p, _ in vus}
    print()
    print("  combattants distincts vus : %d" % len(distincts))
    manquants = [NOMS[i] for i in sorted(NOMS) if i not in distincts]
    if manquants:
        print("  jamais joues              : " + ", ".join(manquants))
    paires = len([1 for k in vus if k[1] == 1])
    print("  courses en couleur 1 / 2  : %d / %d"
          % (len([1 for k in vus if k[1] == 0]), paires))

    tex = os.path.join(base, "fvipers2", "textures.csv")
    if os.path.isfile(tex):
        # Le csv s'ajoute d'une course a l'autre, et chaque course y remet une
        # ligne pour une texture qu'elle revoit -- le PNG, lui, n'est ecrit
        # qu'une fois. Compter les lignes compterait les rencontres, pas les
        # textures : on replie par nom de fichier.
        formats, alphas, nb_lignes = {}, {}, 0
        desaccord = collections.Counter()
        with open(tex, newline="") as fh:
            for r in csv.DictReader(fh):
                nb_lignes += 1
                n, a = r["name"], r.get("alpha", "?")
                formats[n] = r["format"]
                if n in alphas and alphas[n] != a:
                    desaccord[(alphas[n], a)] += 1
                alphas.setdefault(n, a)
        pf = collections.Counter(formats.values())
        pa = collections.Counter(alphas.values())
        png = len([f for f in os.listdir(os.path.join(base, "fvipers2"))
                   if f.endswith(".png")])
        print()
        print("  textures distinctes       : %d  (%d PNG sur le disque, %d lignes de csv)"
              % (len(formats), png, nb_lignes))
        print("  par format                : "
              + ", ".join("f%s %d" % (k, v) for k, v in sorted(pf.items(), key=lambda e: -e[1])))
        print("  par alpha                 : "
              + ", ".join("%s %d" % (k, v) for k, v in pa.most_common()))
        if desaccord:
            print("  dessinees avec deux alphas selon la course :")
            for (a, b), v in desaccord.most_common():
                print("      %-8s puis %-8s  %d fois" % (a, b, v))

    with open(os.path.join(base, "couverture.txt"), "w", encoding="utf-8") as fh:
        fh.write("Dump complet des textures 3D de Fighting Vipers 2\n")
        fh.write("=================================================\n\n")
        for course, p, c in lignes:
            fh.write("%-18s  %-20s  couleur %s\n"
                     % (course, "%2d %s" % (p, NOMS.get(p, "?")) if p is not None else "-", c))
        fh.write("\ncombattants distincts : %d\n" % len(distincts))
        if manquants:
            fh.write("jamais joues : " + ", ".join(manquants) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
