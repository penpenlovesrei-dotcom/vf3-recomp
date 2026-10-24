#!/usr/bin/env python3
"""Desassemble le programme 68000 de la carte son, et cherche dedans.

  tools/son/desas_son.py <image.bin> --fonctions
  tools/son/desas_son.py <image.bin> --at 600120 [lignes]
  tools/son/desas_son.py <image.bin> --cherche 1007c0 [1007ff]
  tools/son/desas_son.py <image.bin> --carte

L'image est celle que tools/son/extraire-son.py ecrit : la ROM son remise dans l'ordre du 68000,
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

    vues, appels = parcourir(d, list(vec))
    fonctions = sorted(set(vec) | appels)

    if mode == "--fonctions":
        print("%d instructions atteintes, %d fonctions (%d depuis les vecteurs, %d appelees)"
              % (len(vues), len(fonctions), len(vec), len(appels)))
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
