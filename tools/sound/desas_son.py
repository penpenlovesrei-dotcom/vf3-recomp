#!/usr/bin/env python3
"""Desassemble le programme 68000 de la carte son, et cherche dedans.

  tools/sound/desas_son.py <image.bin> --fonctions
  tools/sound/desas_son.py <image.bin> --at 600120 [lignes]
  tools/sound/desas_son.py <image.bin> --cherche 1007c0 [1007ff]
  tools/sound/desas_son.py <image.bin> --carte

L'image est celle que tools/sound/extraire-son.py ecrit : la ROM son remise dans l'ordre du 68000,
basee a 0x600000 comme la carte la mappe (src/sound/soundboard.h).

Pourquoi cet outil a cote de Ghidra : Ghidra sert a LIRE une fonction, celui-ci a CHERCHER dans
tout le programme -- quelles instructions touchent une plage de registres du SCSP, qui ecrit dans
les fiches de voix de la RAM son -- et a le faire de facon refaisable depuis une ligne de commande,
sans projet ni interface. Les deux reponses dont ce portage a besoin sont des recherches, pas des
lectures : voir docs/fv2-son-programme.md.

Le parcours est recursif depuis les points d'entree que le 68000 porte lui-meme (sa table de
vecteurs), pas lineaire : le lineaire desassemble les tables de donnees comme du code et noie le
resultat. Les cibles de bsr/jsr donnent les fonctions.
"""
import sys

try:
    from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000
except ImportError:
    print("capstone manque : py -3 -m pip install capstone", file=sys.stderr)
    sys.exit(2)

BASE = 0x600000
SIZE = 0x80000

# Les regions de la carte, pour nommer ce qu'une adresse touche (soundboard.h).
REGIONS = (
    (0x000000, 0x100000, "RAM du SCSP 0"),
    (0x100000, 0x101000, "registres du SCSP 0"),
    (0x200000, 0x300000, "RAM du SCSP 1"),
    (0x300000, 0x301000, "registres du SCSP 1"),
    (0x400000, 0x400002, "registre de banque"),
    (0x600000, 0x680000, "ROM programme"),
    (0x800000, 0x1000000, "echantillons (par banque)"),
)

# Les registres du SCSP, par offset depuis sa base (scsp.cpp: w16/r16).
def nom_registre(off):
    if off < 0x400:
        return "fente %d, mot %d" % (off // 0x20, (off % 0x20) // 2)
    if off < 0x460:
        return "commun +%03X" % off
    if 0x600 <= off < 0x700:
        return "MIXS/EXTS +%03X" % off
    if 0x700 <= off < 0x780:
        return "COEF[%d]" % ((off - 0x700) // 2)
    if 0x780 <= off < 0x7c0:
        return "MADRS[%d]" % ((off - 0x780) // 2)
    if 0x7c0 <= off < 0x800:
        return "0x7C0-0x7FF (LA PLAGE EN QUESTION, MADRS+%d)" % ((off - 0x7c0) // 2)
    if 0x800 <= off < 0xc00:
        return "MPRO pas %d mot %d" % ((off - 0x800) // 8, ((off - 0x800) % 8) // 2)
    if 0xc00 <= off < 0xe00:
        return "TEMP +%03X" % off
    if 0xe00 <= off < 0xec0:
        return "MEMS +%03X" % off
    if 0xec0 <= off < 0xee0:
        return "EFREG[%d]" % ((off - 0xec0) // 2)
    return "+%03X" % off


def region(a):
    for lo, hi, nom in REGIONS:
        if lo <= a < hi:
            if nom.startswith("registres"):
                return "%s : %s" % (nom, nom_registre(a - lo))
            return nom
    return None


ARRETS = ("rts", "rte", "rtr")
SAUTS = ("jmp", "bra", "bra.b", "bra.w", "bra.s")
APPELS = ("jsr", "bsr", "bsr.b", "bsr.w", "bsr.s")


def lire(path):
    d = open(path, "rb").read()
    if len(d) != SIZE:
        print("attention : %d octets, attendu %d" % (len(d), SIZE), file=sys.stderr)
    return d


def vecteurs(d):
    """Les cibles de la table d'exceptions qui tombent dans la ROM programme."""
    out = {}
    noms = {1: "reset", 2: "bus_error", 3: "address_error", 4: "illegal", 9: "trace", 24: "spurious"}
    for i in range(1, 256):
        if 4 * i + 4 > len(d):
            break
        v = int.from_bytes(d[4 * i:4 * i + 4], "big")
        if not (BASE <= v < BASE + SIZE) or v % 2:
            continue
        n = noms.get(i) or ("irq%d" % (i - 24) if 25 <= i <= 31 else "vec%d" % i)
        out.setdefault(v, []).append(n)
    return out


def tables(d):
    """Les points d'entree que les sauts calcules du programme cachent au parcours recursif.

    POURQUOI. Le parcours suit bsr/jsr/bra et les branchements, mais il s'arrete sur un
    `jmp (An)` : rien dans l'instruction ne dit ou elle va. Onze sauts calcules existent dans ce
    programme, et tout ce qu'on n'atteint que par eux etait invisible -- 95 fonctions trouvees contre
    168 que Ghidra tient. Une recherche lancee sur cette moitie-la REND UN COMPTE SANS LE DIRE : le
    2026-10-25, `--cherche ee9` a rendu 4 emplois en en manquant au moins trois, parce que les
    gestionnaires de changement de controle ne s'atteignent que par la table de 0x60141C.

    Les onze sont mesures, et chacun a sa verification dans docs/fv2-son-programme.md :

      - quatre tables de MOTS, chaque entree etant un decalage relatif a la base de la table. Leur
        longueur n'est ecrite nulle part : elle vient de la regle << la table finit ou commence sa
        cible la plus basse >>, verifiee d'abord sur celle de 0x602730 dont les 16 entrees avaient
        ete comptees a la main, puis recoupee par la taille du trou que Ghidra laisse sur chacune.
      - quatre tables de CRENEAUX de branchement, dont le compte vient du masque de l'index, qui est
        ecrit dans le code (& 0x0E, & 0x1C, (&7)<<2, et huit pour celle de 0x60088E). Deux d'entre
        elles n'ont que sept `bra` : le huitieme cas est un gestionnaire pose en ligne.
      - quatre blocs deroules de 128 instructions (des dispositifs de Duff) : deux copient, deux
        effacent. On seme leur entree.
    """
    out = []
    # base, nombre d'entrees -- des mots, chacun un decalage depuis la base
    for base, n in ((0x60141C, 81), (0x602730, 16), (0x602EE0, 26)):
        for k in range(n):
            o = base - BASE + 2 * k
            out.append(base + int.from_bytes(d[o:o + 2], "big"))
    # base, nombre de creneaux, pas -- les creneaux eux-memes sont les destinations
    for base, n, pas in ((0x60088E, 8, 4), (0x60030A, 8, 2), (0x603496, 8, 4), (0x60376C, 8, 4)):
        out.extend(base + pas * k for k in range(n))
    # les entrees de bloc deroule
    out.extend((0x601F3C, 0x6023E2, 0x602044, 0x6024EA))
    return [a for a in out if BASE <= a < BASE + SIZE and not a % 2]


def parcourir(d, depart):
    """Descente recursive. Rend (instructions par adresse, cibles d'appel)."""
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    vues = {}
    appels = set()
    pile = list(depart)
    while pile:
        a = pile.pop()
        while True:
            if a in vues or not (BASE <= a < BASE + SIZE) or a % 2:
                break
            off = a - BASE
            try:
                ins = next(md.disasm(d[off:off + 16], a, count=1))
            except StopIteration:
                break
            vues[a] = ins
            m = ins.mnemonic
            op = ins.op_str
            cible = None
            if op.startswith("$") or op.startswith("0x"):
                t = op.split(",")[0].strip().lstrip("$")
                try:
                    cible = int(t, 16)
                except ValueError:
                    cible = None
            if m in APPELS or m.startswith("bsr") or m == "jsr":
                if cible is not None and BASE <= cible < BASE + SIZE:
                    appels.add(cible)
                    pile.append(cible)
                a += ins.size
                continue
            if m in ARRETS or m.startswith("rts") or m.startswith("rte"):
                break
            if m in SAUTS or m.startswith("bra") or m == "jmp":
                if cible is not None:
                    pile.append(cible)
                break
            if m.startswith("b") or m.startswith("db"):   # branchement conditionnel
                if cible is not None:
                    pile.append(cible)
                a += ins.size
                continue
            a += ins.size
    return vues, appels


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    d = lire(argv[1])
    mode = argv[2]
    vec = vecteurs(d)
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)

    if mode == "--carte":
        print("table des vecteurs : %d cibles distinctes dans la ROM programme" % len(vec))
        for a in sorted(vec):
            print("  %08X  %s" % (a, ", ".join(vec[a])))
        return 0

    tab = tables(d)
    vues, appels = parcourir(d, list(vec) + tab)
    fonctions = sorted(set(vec) | appels | set(tab))

    if mode == "--fonctions":
        print("%d instructions atteintes, %d fonctions (%d depuis les vecteurs, %d appelees,"
              " %d semees par les tables de saut)"
              % (len(vues), len(fonctions), len(vec), len(appels), len(set(tab))))
        print("couverture : %.1f %% des octets de la ROM" % (100.0 * sum(i.size for i in vues.values()) / SIZE))
        for a in fonctions:
            n = ", ".join(vec[a]) if a in vec else ""
            print("  %08X %s" % (a, n))
        return 0

    if mode == "--at":
        a = int(argv[3], 16)
        n = int(argv[4]) if len(argv) > 4 else 60
        off = a - BASE
        for ins in md.disasm(d[off:off + n * 10], a, count=n):
            r = region_de_operande(ins)
            print("  %08X  %-10s %-34s %s" % (ins.address, ins.mnemonic, ins.op_str, r or ""))
        return 0

    if mode == "--cherche":
        lo = int(argv[3], 16)
        hi = int(argv[4], 16) if len(argv) > 4 else lo
        # La portee est imprimee AVANT les resultats, parce qu'un compte rendu par un parcours
        # incomplet ressemble a un compte complet. Celle-ci inclut les onze sauts calcules.
        print("portee : %d instructions atteintes depuis %d points d'entree (%d vecteurs,"
              " %d semes par les tables de saut)"
              % (len(vues), len(set(list(vec) + tab)), len(vec), len(set(tab))))
        print("instructions atteintes dont une operande nomme %06X..%06X :" % (lo, hi))
        n = 0
        for a in sorted(vues):
            ins = vues[a]
            for v in valeurs(ins):
                if lo <= v <= hi:
                    print("  %08X  %-10s %-34s -> %06X  %s"
                          % (a, ins.mnemonic, ins.op_str, v, region(v) or ""))
                    n += 1
                    break
        print("%d trouvees" % n)
        return 0

    print(__doc__)
    return 2


def valeurs(ins, adresses_seulement=False):
    """Les constantes hexadecimales de l'operande, en entier.

    adresses_seulement : laisse tomber les immediats (`#$12`), qui ne sont pas des adresses. Sans
    ce tri, `andi.b #$12,d7` se fait annoter « RAM du SCSP 0 » et l'annotation ne vaut plus rien.
    """
    out = []
    s = ins.op_str
    i = 0
    while i < len(s):
        if s[i] == "#":
            # un immediat : on saute la constante qui suit
            j = i + 1
            if j < len(s) and s[j] == "$":
                j += 1
            while j < len(s) and s[j] in "0123456789abcdefABCDEF":
                j += 1
            if not adresses_seulement:
                t = s[i + 1:j].lstrip("$")
                if t:
                    try:
                        out.append(int(t, 16))
                    except ValueError:
                        pass
            i = j
            continue
        if s[i] == "$" or (s[i] == "0" and i + 1 < len(s) and s[i + 1] == "x"):
            j = i + (2 if s[i] == "0" else 1)
            k = j
            while k < len(s) and s[k] in "0123456789abcdefABCDEF":
                k += 1
            if k > j:
                try:
                    out.append(int(s[j:k], 16))
                except ValueError:
                    pass
            i = k
        else:
            i += 1
    return out


def region_de_operande(ins):
    for v in valeurs(ins, adresses_seulement=True):
        r = region(v)
        if r:
            return "[%06X : %s]" % (v, r)
    return None


if __name__ == "__main__":
    sys.exit(main(sys.argv))
