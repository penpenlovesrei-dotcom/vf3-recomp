#!/usr/bin/env python3
"""Ecrit les trois scripts d'entrees qui servent a isoler les textures de Honey.

Trois courses, la meme en tout sauf une chose chacune :

    fv2-honey-tex-c1.txt   HONEY confirmee au POING   -> premiere couleur
    fv2-honey-tex-c2.txt   HONEY confirmee a START    -> seconde couleur
    fv2-emi-tex.txt        EMI confirmee au POING     -> le temoin

Le temoin est la seule facon d'isoler Honey : la capture ecrit tout ce qu'un
polygone echantillonne, donc le decor, le ciel, l'adversaire et les effets
arrivent avec elle. Ce que la course de Honey a et que le temoin n'a pas, c'est
Honey.

Deux precautions prises dans les scripts, et elles sont le fond du sujet :

  * Aucune image n'est rendue avant la confirmation. Le curseur traverse les
    cases 1 a 8 pour atteindre HONEY, et l'ecran de selection dessine le
    modele de la case survolee : rendre ces images ferait entrer dans la
    capture les textures de sept autres personnages, que le temoin, lui,
    n'aurait pas. La capture ne commence donc qu'une fois le combat charge.

  * `hold p1.guard` pendant le round. L'armure basse de Honey encaisse 300
    points et l'armure haute 350 (docs/fv2-honey.md) ; une fois tombee, le
    jeu charge un autre modele et ses textures entreraient dans la capture.
    Garde tenue, la demande est le costume de base et rien d'autre.

Le port ne rend l'image que sur les marques `shot` d'un script : les marques
serrees ci-dessous ne sont pas la pour faire des images -- le lanceur ne donne
pas de `--shots` -- elles sont la pour forcer le rendu, donc l'echantillonnage.
"""

import os

ICI = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.normpath(os.path.join(ICI, "..", "scripts"))

# Les deux bornes de la capture. 2400 : le combat est demande, l'ecran de
# selection est derriere nous. 4400 : le premier round se termine entre 4900 et
# 5100 avec la lecture aleatoire (docs/fv2-honey.md) ; on s'arrete avant pour ne
# jamais charger le round suivant.
DEBUT, FIN, PAS = 2400, 4400, 12


def course(titre, droites, confirme, haut=False):
    L = ["# " + l for l in titre.splitlines()]
    L += [
        "# Genere par tools/fv2/mk_honey_tex_scripts.py -- ne pas editer a la main.",
        "1900 press p1.coin",
        "1930 press p1.coin",
        "1960 press p1.start",
    ]
    # Le curseur part d'EMI. Une pression de droite par case, comme
    # fv2-honey.txt, aux memes images pour que les trois courses soient
    # comparables.
    for n in range(droites):
        L.append("%d press p1.right" % (2080 + 30 * n))
    if haut:
        # La seconde couleur du costume : une pression de HAUT sur la case,
        # avant de confirmer. Frederic l'a vue marcher ; START, qu'un
        # commentaire de fv2-honey.txt donnait pour le choix de costume, ne
        # confirme rien du tout -- il appelle le second joueur, et le chrono
        # de selection va au bout (mesure du 2026-10-21).
        L.append("2320 press p1.up")
    L.append("2350 press p1." + confirme)
    L.append("# Marques de rendu, une tous les %d images : c'est le rendu qui" % PAS)
    L.append("# echantillonne les textures, pas l'image ecrite. La garde est prise")
    L.append("# dans l'ordre, le lecteur de scripts refusant une image qui recule.")
    # vf3run lit les commandes dans l'ordre et s'arrete sur une image qui
    # recule : la garde doit donc etre posee a sa place dans la suite.
    suite = [(f, "%d shot t%03d" % (f, i))
             for i, f in enumerate(range(DEBUT, FIN + 1, PAS))]
    suite.append((3150, "3150 hold p1.guard"))
    suite.sort(key=lambda c: c[0])
    L += [ligne for _, ligne in suite]
    L.append("%d quit" % (FIN + 100))
    return "\r\n".join(L) + "\r\n"


FICHIERS = {
    "fv2-honey-tex-c1.txt": course(
        "Fighting Vipers 2 : HONEY, premiere couleur, pour la capture des textures.\n"
        "Huit pressions de droite depuis EMI amenent sur HONEY, case 8 ; le POING\n"
        "confirme, et le poing donne le costume dans sa premiere couleur.",
        8, "punch"),
    "fv2-honey-tex-c2.txt": course(
        "Fighting Vipers 2 : HONEY, seconde couleur, pour la capture des textures.\n"
        "Rigoureusement la course c1, a une pression pres : HAUT sur la case de\n"
        "HONEY avant de confirmer au poing, et le costume change de couleur.",
        8, "punch", haut=True),
    "fv2-emi-tex.txt": course(
        "Fighting Vipers 2 : EMI, le temoin de la capture des textures de Honey.\n"
        "Aucune pression de droite -- le curseur arrive sur EMI -- et le POING\n"
        "confirme. Meme ouverture, meme adversaire, meme decor, meme chronologie\n"
        "que les deux courses de Honey : tout ce que celle-ci partage avec elle\n"
        "n'est donc pas Honey.",
        0, "punch"),
}

for nom, contenu in FICHIERS.items():
    chemin = os.path.join(SCRIPTS, nom)
    with open(chemin, "wb") as f:
        f.write(contenu.encode("ascii"))
    print("%-24s %d lignes" % (nom, contenu.count("\r\n")))
