"""Qui touche cette adresse ? Les references a une adresse dans le programme du jeu.

Pourquoi cet outil existe, et pourquoi il manquait. `desas_ppc.py` sait lire trente
instructions A UNE ADRESSE DONNEE ; il ne sait pas repondre a « qui ecrit cet octet ».
Or c'est la question qu'on se pose en premier chaque fois qu'on part d'une donnee :
l'octet bouge quand le curseur bouge, et apres ? Sans reponse, on se rabat sur les
pixels -- ce qui a coute trois tentatives au label CPU du mode training, le 2026-10-28.

Le jeu n'a aucun symbole pour ce qu'il dessine. Ce qui est nomme l'est parce que Sega
l'a nomme lui-meme (la table des noms des gestionnaires, en 0x000244F4). Pour tout le
reste la route est : la donnee, puis qui l'ecrit, puis les trente instructions autour.
Celui-ci fait le pas du milieu.

COMMENT. Le PowerPC n'a pas d'adressage absolu : une adresse se fabrique en deux temps,

    lis   r3,0x0012          r3 = 0x00120000
    addi  r3,r3,1312         r3 = 0x00120520
    lbz   r4,56(r3)          lit 0x00120558

donc il faut suivre la VALEUR des registres pour savoir ce qu'une instruction touche.
C'est ce que fait ce fichier : un petit modele de registres, remis a zero a chaque
branchement, et un rapport a chaque chargement ou rangement dont l'adresse effective
tombe dans la cible. Il ne modelise que ce que le code de jeu emploie pour fabriquer
une adresse -- lis, addis, li, addi, ori -- et met un registre a l'inconnu des qu'il
est ecrit par autre chose. Un registre inconnu ne produit aucune ligne : l'outil se
tait plutot que de deviner, comme le desassembleur imprime `.long` plutot qu'inventer
une mnemonique.

CE QU'IL NE TROUVE PAS, et c'est a savoir avant de conclure qu'une adresse n'est
touchee par personne : une adresse calculee (une table indexee, `lbzx r3,r4,r5`), une
adresse passee en argument, une adresse lue depuis un pointeur en memoire. Le silence
de cet outil n'est donc pas une preuve ; une ligne qu'il imprime en est une.

Usage :
    py -3 tools/fv2/xrefs_ppc.py <ram.bin> <adresse hex> [octets] [debut hex] [fin hex]

`octets` elargit la cible a une plage (1 par defaut : l'adresse seule). `debut` et
`fin` bornent la zone balayee ; par defaut 0x00001000 a 0x00140000, qui est ou se
trouve le programme de Fighting Vipers 2 une fois depaquete.

Le vidage se fait avec VF3_RAM_DUMP=octets@image (main.cpp), qui ecrit ram.bin dans le
dossier de donnees, dans l'ordre des mots de l'HOTE -- l'octet d'adresse A est a
l'indice A^3. desas_ppc.py est importe pour relire les mots et les imprimer, donc les
deux outils lisent le meme fichier de la meme facon.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import desas_ppc as d


# Les chargements et rangements de forme D : opcode -> (nom, ecrit-il ?)
FORME_D = {
    32: ("lwz", False), 33: ("lwzu", False), 34: ("lbz", False), 35: ("lbzu", False),
    40: ("lhz", False), 42: ("lha", False),
    36: ("stw", True), 37: ("stwu", True), 38: ("stb", True), 39: ("stbu", True),
    44: ("sth", True), 45: ("sthu", True),
}


def registres_apres(w, regs):
    """Le modele de registres apres cette instruction. Renvoie False si c'est un branchement."""
    op = w >> 26
    rd = (w >> 21) & 31
    ra = (w >> 16) & 31
    u = w & 0xFFFF
    s = d.signe16(u)

    if op in (16, 18):                       # un branchement : on ne suit pas, on oublie tout
        return False
    if op == 19:                             # blr, bctr et compagnie : pareil
        return False
    if op == 15:                             # lis / addis
        if ra == 0:
            regs[rd] = (u << 16) & 0xFFFFFFFF
        elif ra in regs:
            regs[rd] = (regs[ra] + (u << 16)) & 0xFFFFFFFF
        else:
            regs.pop(rd, None)
        return True
    if op == 14:                             # li / addi
        if ra == 0:
            regs[rd] = s & 0xFFFFFFFF
        elif ra in regs:
            regs[rd] = (regs[ra] + s) & 0xFFFFFFFF
        else:
            regs.pop(rd, None)
        return True
    if op == 24:                             # ori rA,rS,u -- la deuxieme moitie d'une adresse
        if rd in regs:
            regs[ra] = regs[rd] | u
        else:
            regs.pop(ra, None)
        return True
    if op in FORME_D:
        # le registre charge devient inconnu ; une forme `u` met aussi a jour la base
        if not FORME_D[op][1]:
            regs.pop(rd, None)
        if op in (33, 35, 37, 39, 45) and ra in regs:
            regs[ra] = (regs[ra] + s) & 0xFFFFFFFF
        return True
    # tout le reste : ce qui est ecrit devient inconnu, et on ne cherche pas a savoir quoi
    if op == 31:
        sous = (w >> 1) & 0x3FF
        if sous == 444:                      # or / mr : la seule forme qui propage une valeur
            rb = (w >> 11) & 31
            if rd == rb and rd in regs:
                regs[ra] = regs[rd]
            else:
                regs.pop(ra, None)
            return True
        # les formes qui ecrivent rA (logiques) et celles qui ecrivent rD (arithmetiques)
        if sous in (28, 316, 124, 60, 444, 467, 824, 536, 792):
            regs.pop(ra, None)
        else:
            regs.pop(rd, None)
        return True
    if op in (7, 10, 11, 21, 28):            # mulli, cmp*, rlwinm, andi.
        if op in (7,):
            regs.pop(rd, None)
        elif op in (21, 28):
            regs.pop(ra, None)
        return True
    regs.pop(rd, None)
    return True


def cherche(ram, cible, taille, debut, fin):
    """Les instructions dont l'adresse effective tombe dans [cible, cible+taille)."""
    trouvees = []
    regs = {}
    a = debut
    fin = min(fin, (len(ram) & ~3))
    while a < fin:
        w = d.mot(ram, a)
        op = w >> 26
        if op in FORME_D:
            ra = (w >> 16) & 31
            if ra in regs:
                eff = (regs[ra] + d.signe16(w & 0xFFFF)) & 0xFFFFFFFF
                if cible <= eff < cible + taille:
                    nom, ecrit = FORME_D[op]
                    trouvees.append((a, eff, ecrit, d.une(ram, a)))
        elif op == 14 and ((w >> 16) & 31) != 0:
            # `addi rD,rA,bas` qui finit de fabriquer l'adresse sans la lire : le vrai acces
            # est quelques instructions plus loin, avec un deplacement. On le signale, parce
            # que c'est la forme que prend un bloc de reglages (0x00120520 + 56).
            ra = (w >> 16) & 31
            rd = (w >> 21) & 31
            if ra in regs:
                val = (regs[ra] + d.signe16(w & 0xFFFF)) & 0xFFFFFFFF
                if cible <= val < cible + taille:
                    trouvees.append((a, val, None, d.une(ram, a)))
                del rd
        if not registres_apres(w, regs):
            regs = {}
        a += 4
    return trouvees


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    ram = open(argv[1], "rb").read()
    cible = int(argv[2], 16)
    taille = int(argv[3]) if len(argv) > 3 else 1
    debut = int(argv[4], 16) if len(argv) > 4 else 0x00008000
    fin = int(argv[5], 16) if len(argv) > 5 else 0x00140000
    trouvees = cherche(ram, cible, taille, debut, fin)
    print("%08X..%08X : %d reference%s dans %08X..%08X"
          % (cible, cible + taille - 1, len(trouvees), "s" if len(trouvees) > 1 else "", debut, fin))
    for a, eff, ecrit, texte in trouvees:
        quoi = "  " if ecrit is None else ("ECRIT" if ecrit else "lit  ")
        print("  %08X  %-5s %08X  %s" % (a, quoi if ecrit is not None else "forme", eff, texte))
    if not trouvees:
        print("  (rien : voir CE QU'IL NE TROUVE PAS, en tete de ce fichier)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
