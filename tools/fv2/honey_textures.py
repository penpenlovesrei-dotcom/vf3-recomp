#!/usr/bin/env python3
"""Trie trois captures de textures et laisse sur le bureau le costume de Honey.

    honey_textures.py <dossier des captures> <dossier de sortie>

Le dossier des captures porte trois sous-dossiers ecrits par `vf3run --hd-dump` :

    couleur1/   HONEY confirmee au poing
    couleur2/   HONEY, un HAUT sur sa case avant le poing
    temoin/     EMI, la meme course en tout le reste

Une texture est nommee par le hachage de son contenu : le nom de fichier EST
l'identite, deux fichiers de meme nom dans deux captures sont la meme image au
pixel pres. Le tri est donc une affaire d'ensembles, pas de comparaison d'images.

    couleur-1 = couleur1 \\ temoin
    couleur-2 = couleur2 \\ temoin

Ce que la course de Honey a et que le temoin n'a pas ne peut etre ni le decor,
ni le ciel, ni l'adversaire, ni les effets, ni l'interface : les trois courses
les traversent a la meme image. Reste le personnage joue.

Rien n'est efface : les deux dossiers de sortie sont vides a la creation et le
script refuse d'ecrire par-dessus un fichier deja la sans le dire.
"""

import csv
import os
import shutil
import sys


def lire(dossier):
    """Les PNG d'une capture, et ce que son textures.csv dit de chacun."""
    if not os.path.isdir(dossier):
        return None, {}
    noms = {f for f in os.listdir(dossier) if f.lower().endswith(".png")}
    ou = {}
    csvp = os.path.join(dossier, "textures.csv")
    if os.path.isfile(csvp):
        with open(csvp, newline="", encoding="utf-8", errors="replace") as fh:
            for ligne in csv.DictReader(fh):
                # Le dump ecrit ses en-tetes en anglais (name,hash,width,...) ;
                # les autres noms sont acceptes au cas ou ils changeraient.
                f = (ligne.get("name") or ligne.get("fichier")
                     or ligne.get("file") or "").strip()
                if f:
                    ou[f] = ligne
    return noms, ou


def taille_et_format(nom):
    """`<hash>_<w>x<h>_f<format>.png` -> (w*h, w, h, format), pour trier."""
    corps = nom[:-4]
    morceaux = corps.split("_")
    try:
        w, h = (int(v) for v in morceaux[1].split("x"))
        fmt = int(morceaux[2].lstrip("f"))
    except (IndexError, ValueError):
        return (0, 0, 0, 0)
    return (w * h, w, h, fmt)


def poser(noms, source, cible, ou):
    os.makedirs(cible, exist_ok=True)
    lignes, ecrits = [], 0
    for nom in sorted(noms, key=lambda n: (-taille_et_format(n)[0], n)):
        dst = os.path.join(cible, nom)
        if os.path.exists(dst):
            print("   deja la, laisse tel quel : " + nom)
        else:
            shutil.copy2(os.path.join(source, nom), dst)
            ecrits += 1
        # L'inventaire decrit le lot entier, qu'un fichier vienne d'etre copie
        # ou qu'il ait deja ete la : il dit ce que le dossier CONTIENT.
        _, w, h, fmt = taille_et_format(nom)
        d = ou.get(nom, {})
        lignes.append((nom, w, h, fmt, d.get("page", ""), d.get("x", ""), d.get("y", ""),
                       d.get("alpha", "")))
    with open(os.path.join(cible, "textures.csv"), "w", newline="", encoding="utf-8") as fh:
        e = csv.writer(fh)
        # alpha : ce que le polygone fait de l'alpha du texel -- opaque, contour
        # (decoupe a un bit) ou graded (degrade). C'est l'alpha que porte l'image.
        e.writerow(["fichier", "largeur", "hauteur", "format", "page", "x", "y", "alpha"])
        e.writerows(lignes)
    print("   %d fichiers copies, %d deja la" % (ecrits, len(lignes) - ecrits))
    return lignes


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    captures, sortie = sys.argv[1], sys.argv[2]

    c1, ou1 = lire(os.path.join(captures, "couleur1"))
    c2, ou2 = lire(os.path.join(captures, "couleur2"))
    tem, _ = lire(os.path.join(captures, "temoin"))

    manque = [n for n, s in (("couleur1", c1), ("couleur2", c2), ("temoin", tem)) if s is None]
    if manque:
        print("capture absente : " + ", ".join(manque))
        return 1

    h1, h2 = c1 - tem, c2 - tem

    print("Les trois captures, telles qu'elles sortent du jeu")
    print("   couleur1 (Honey, poing) %4d textures" % len(c1))
    print("   couleur2 (Honey, HAUT+poing) %4d textures" % len(c2))
    print("   temoin   (Emi, poing)   %4d textures" % len(tem))
    print()
    print("Ce que chaque course partage avec le temoin -- decor, ciel, adversaire,")
    print("effets, interface : ce qui n'est pas le personnage joue")
    print("   couleur1 ^ temoin       %4d" % len(c1 & tem))
    print("   couleur2 ^ temoin       %4d" % len(c2 & tem))
    print()
    print("Le costume, une fois le temoin retire")
    print("   couleur-1               %4d textures" % len(h1))
    print("   couleur-2               %4d textures" % len(h2))
    print("   communes aux deux       %4d  (identiques au pixel dans les deux couleurs)" % len(h1 & h2))
    print("   propres a couleur-1     %4d" % len(h1 - h2))
    print("   propres a couleur-2     %4d" % len(h2 - h1))
    print()

    l1 = poser(h1, os.path.join(captures, "couleur1"), os.path.join(sortie, "couleur-1"), ou1)
    l2 = poser(h2, os.path.join(captures, "couleur2"), os.path.join(sortie, "couleur-2"), ou2)
    print("   couleur-1 : %d textures dans le dossier" % len(l1))
    print("   couleur-2 : %d textures dans le dossier" % len(l2))

    releve = os.path.join(sortie, "RELEVE.txt")
    with open(releve, "w", encoding="utf-8") as fh:
        fh.write("Honey, costume de base, ses deux couleurs\n")
        fh.write("=========================================\n\n")
        fh.write("Textures tirees de Fighting Vipers 2 par le portage de Desktop\\fv2-recomp,\n")
        fh.write("capture passive du moteur 3D (vf3run --hd-dump, docs\\fv2-hd-3d.md).\n")
        fh.write("Le nom d'un fichier est le hachage de son contenu, puis sa taille d'origine\n")
        fh.write("et son format Real3D. C'est ce nom qui sert de cle a un pack de remplacement :\n")
        fh.write("agrandir l'image, la redessiner, la remettre sous le meme nom.\n\n")
        fh.write("Comment les deux lots ont ete separes\n")
        fh.write("-------------------------------------\n")
        fh.write("Trois courses identiques, sans fenetre, meme ouverture et memes images :\n")
        fh.write("  couleur1  HONEY confirmee au POING\n")
        fh.write("  couleur2  HONEY, un HAUT sur sa case avant le poing (meme costume, autre couleur)\n")
        fh.write("  temoin    EMI  confirmee au POING\n")
        fh.write("Aucune image n'est rendue avant la confirmation, pour que les modeles\n")
        fh.write("survoles sur l'ecran de selection n'entrent dans aucune capture ; la garde\n")
        fh.write("est tenue tout le round pour que l'armure de Honey ne tombe pas.\n")
        fh.write("Un lot = sa course moins le temoin.\n\n")
        fh.write("Les nombres\n")
        fh.write("-----------\n")
        fh.write("  couleur1                %4d textures\n" % len(c1))
        fh.write("  couleur2                %4d textures\n" % len(c2))
        fh.write("  temoin                  %4d textures\n" % len(tem))
        fh.write("  couleur1 ^ temoin       %4d\n" % len(c1 & tem))
        fh.write("  couleur2 ^ temoin       %4d\n" % len(c2 & tem))
        fh.write("  couleur-1 retenues      %4d\n" % len(h1))
        fh.write("  couleur-2 retenues      %4d\n" % len(h2))
        fh.write("  communes aux deux       %4d\n" % len(h1 & h2))
        fh.write("  propres a couleur-1     %4d\n" % len(h1 - h2))
        fh.write("  propres a couleur-2     %4d\n" % len(h2 - h1))
        fh.write("\nLes textures communes aux deux couleurs sont dans les DEUX dossiers : ce\n")
        fh.write("sont les parties du modele que le changement de couleur ne touche pas.\n")
        fh.write("\nLe textures.csv de chaque dossier donne, pour chaque image, sa taille, son\n")
        fh.write("format, et la page et la position ou le jeu l'avait chargee.\n")
    print("   releve : " + releve)
    return 0


if __name__ == "__main__":
    sys.exit(main())
