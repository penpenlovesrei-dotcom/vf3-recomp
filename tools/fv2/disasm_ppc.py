"""Desassembleur PowerPC, en bibliotheque standard, pour lire le programme du jeu.

Pourquoi en ecrire un, alors que la chaine `tools/fv2/README.md` sait charger
l'image dans Ghidra : parce qu'il faut une instance de Ghidra ouverte, et que
lire trente instructions a une adresse precise ne merite pas qu'on monte un
projet. Meme raison que le `desas_sh4.py` des notes arcade, pour l'autre
processeur.

Ce n'est PAS un desassembleur complet. Il couvre ce que le code de jeu emploie
-- chargements, rangements, comparaisons, branchements, arithmetique immediate,
les formes etendues du groupe 31 qui reviennent -- et il imprime le mot brut
pour tout le reste, plutot que d'inventer un nom. Une ligne `.long` est un aveu
utile ; une mauvaise mnemonique ne l'est pas.

Le piege de ce portage : `--ram-at` garde les mots de 32 bits dans l'ordre de
l'HOTE, donc l'octet d'adresse A est a l'indice `A ^ 3`. Ce fichier relit donc
chaque mot par son adresse, jamais par son offset brut.

Usage :
    py -3 tools/fv2/disasm_ppc.py <ram.bin> <adresse hex> [nombre]
"""

import sys

SPR = {1: "xer", 8: "lr", 9: "ctr"}


def mot(ram, adresse):
    """Le mot de 32 bits a cette adresse de jeu, dans l'ordre du PowerPC."""
    return int.from_bytes(bytes(ram[(adresse + k) ^ 3] for k in range(4)), "big")


def signe16(v):
    return v - 0x10000 if v & 0x8000 else v


def conditions(bo, bi):
    """Le suffixe d'un branchement conditionnel, quand il est d'une forme courante."""
    champ, bit = bi >> 2, bi & 3
    nom = ["lt", "gt", "eq", "so"][bit]
    inverse = {"lt": "ge", "gt": "le", "eq": "ne", "so": "ns"}[nom]
    if bo & 0x14 == 0x14:
        return "", ""
    if bo & 0x10:
        return "dnz" if bo & 2 else "dz", ""
    quoi = nom if bo & 8 else inverse
    return quoi, ("" if champ == 0 else "cr%d," % champ)


def une(ram, adresse):
    w = mot(ram, adresse)
    op = w >> 26
    rd = (w >> 21) & 31
    ra = (w >> 16) & 31
    rb = (w >> 11) & 31
    d = signe16(w & 0xFFFF)
    u = w & 0xFFFF

    if op == 14:
        return "li r%d,%d" % (rd, d) if ra == 0 else "addi r%d,r%d,%d" % (rd, ra, d)
    if op == 15:
        return "lis r%d,0x%04X" % (rd, u) if ra == 0 else "addis r%d,r%d,0x%04X" % (rd, ra, u)
    if op == 7:
        return "mulli r%d,r%d,%d" % (rd, ra, d)
    if op == 11:
        return "cmpwi %sr%d,%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, d)
    if op == 10:
        return "cmplwi %sr%d,%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, u)
    if op == 24:
        return "nop" if w == 0x60000000 else "ori r%d,r%d,0x%04X" % (ra, rd, u)
    if op == 28:
        return "andi. r%d,r%d,0x%04X" % (ra, rd, u)
    if op == 21:
        sh, mb, me = (w >> 11) & 31, (w >> 6) & 31, (w >> 1) & 31
        return "rlwinm%s r%d,r%d,%d,%d,%d" % ("." if w & 1 else "", ra, rd, sh, mb, me)
    if op in (32, 34, 36, 38, 40, 44, 33, 35, 37, 39):
        nom = {32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu", 36: "stw", 37: "stwu",
               38: "stb", 39: "stbu", 40: "lhz", 44: "sth"}[op]
        return "%s r%d,%d(r%d)" % (nom, rd, d, ra)
    if op == 18:
        cible = (w & 0x03FFFFFC)
        if cible & 0x02000000:
            cible -= 0x04000000
        if not (w & 2):
            cible += adresse
        return "%s 0x%08X" % ("bl" if w & 1 else "b", cible & 0xFFFFFFFF)
    if op == 16:
        bo, bi = rd, ra
        cible = signe16(w & 0xFFFC)
        if not (w & 2):
            cible += adresse
        quoi, cr = conditions(bo, bi)
        if quoi:
            return "b%s%s %s0x%08X" % (quoi, "l" if w & 1 else "", cr, cible & 0xFFFFFFFF)
        return "bc %d,%d,0x%08X" % (bo, bi, cible & 0xFFFFFFFF)
    if op == 19:
        sous = (w >> 1) & 0x3FF
        if sous == 16:
            quoi, cr = conditions(rd, ra)
            return ("blr" if not quoi else "b%slr %s" % (quoi, cr)) + ("l" if w & 1 else "")
        if sous == 528:
            return "bctr" + ("l" if w & 1 else "")
        return ".long 0x%08X" % w
    if op == 31:
        sous = (w >> 1) & 0x3FF
        if sous == 339:
            s = (ra | (rb << 5))
            return "mf%s r%d" % (SPR.get(s, "spr%d" % s), rd)
        if sous == 467:
            s = (ra | (rb << 5))
            return "mt%s r%d" % (SPR.get(s, "spr%d" % s), rd)
        if sous == 444:
            return "mr r%d,r%d" % (ra, rd) if rd == rb else "or r%d,r%d,r%d" % (ra, rd, rb)
        if sous == 0:
            return "cmpw %sr%d,r%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, rb)
        if sous == 32:
            return "cmplw %sr%d,r%d" % ("" if rd >> 2 == 0 else "cr%d," % (rd >> 2), ra, rb)
        noms = {266: "add", 40: "subf", 235: "mullw", 23: "lwzx", 87: "lbzx", 215: "stbx",
                151: "stwx", 28: "and", 316: "xor", 124: "nor", 60: "andc", 491: "divw"}
        if sous in noms:
            if sous in (23, 87, 215, 151):
                return "%s r%d,r%d,r%d" % (noms[sous], rd, ra, rb)
            if sous in (28, 316, 124, 60):
                return "%s r%d,r%d,r%d" % (noms[sous], ra, rd, rb)
            return "%s r%d,r%d,r%d" % (noms[sous], rd, ra, rb)
    return ".long 0x%08X" % w


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    ram = open(argv[1], "rb").read()
    adresse = int(argv[2], 16)
    combien = int(argv[3]) if len(argv) > 3 else 40
    for i in range(combien):
        a = adresse + i * 4
        print("%08X  %08X  %s" % (a, mot(ram, a), une(ram, a)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
