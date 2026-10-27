"""Lit le journal d'une partie et dit si la couche console est passee par les etats attendus.

Pourquoi un verificateur et pas une trace a lire : une trace, on la lit le jour ou on l'ecrit. Le
lendemain on la survole, et une ligne qui manque au milieu ne saute pas aux yeux. Ici la suite attendue
est ecrite une fois, dans l'appel, et la machine compare.

Il lit les lignes que `console_ui::set` ecrit, et elles seules :

    the console: PLAY -> MENU (Start on the title screen)

Usage :
    py -3 tools/fv2/verifier_console.py <log.txt> PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU

Sortie : une ligne par etat attendu, et le code de sortie 0 si la suite est exactement celle-la.
"""

import re
import sys

LIGNE = re.compile(r"the console: (\w+) -> (\w+) \(")


def suite(journal):
    """La suite des etats traverses, le premier compris."""
    vus = []
    for ligne in journal.splitlines():
        m = LIGNE.search(ligne)
        if not m:
            continue
        depuis, vers = m.group(1), m.group(2)
        if not vus:
            vus.append(depuis)
        elif vus[-1] != depuis:
            # un changement d'etat qui ne part pas d'ou le precedent est arrive : la trace est trouee
            vus.append("?" + depuis)
        vus.append(vers)
    return vus


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    attendu = argv[2:]
    with open(argv[1], "r", encoding="utf-8", errors="replace") as f:
        obtenu = suite(f.read())
    largeur = max(len(e) for e in attendu + obtenu) if obtenu else 10
    bon = True
    for i in range(max(len(attendu), len(obtenu))):
        a = attendu[i] if i < len(attendu) else "-"
        o = obtenu[i] if i < len(obtenu) else "-"
        marque = "  " if a == o else "<-- ici"
        bon = bon and a == o
        print(f"  {a:<{largeur}}  {o:<{largeur}}  {marque}")
    print()
    print("la suite des etats est celle attendue" if bon else "LA SUITE DES ETATS N'EST PAS CELLE ATTENDUE")
    return 0 if bon else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
