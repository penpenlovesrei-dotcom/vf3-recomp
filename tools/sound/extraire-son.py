#!/usr/bin/env python3
# Extrait le programme 68000 de la carte son d'un zip de ROM et l'ecrit dans l'ordre du 68000.
#
#   tools/sound/extraire-son.py <jeu.zip> <sortie.bin>
#
# L'ordre des octets est la seule chose delicate, et elle a ete TRANCHEE PAR LA MESURE, pas
# supposee. romset.cpp pose la region SOUND avec stride 1, chunk 1, swap false : les octets du
# fichier y entrent tels quels. Puis SoundBoard::Bus::word lit `v[a] | v[a+1] << 8`, soit l'octet
# BAS en premier -- alors que le 68000 est big-endian. Donc le fichier est echange par rapport a
# ce que le 68000 voit, et il faut le remettre d'aplomb pour un desassembleur.
#
# La preuve est dans les vecteurs de reset, qui sont les huit premiers mots :
#
#   echange (octet bas en premier)  SSP 00002000  PC 00600120   <- sain : pile en haut des 8 Ko,
#                                                                  code dans la ROM programme
#   big-endian direct               SSP 00000020  PC 60002001   <- adresse impaire, hors carte
#
# Et avec l'echange, les vecteurs 2 et 3 (bus error, address error) tombent a 006005AC et
# 006005B0, dans la ROM eux aussi. Quatre vecteurs sains contre un faux : l'ordre est tranche.
import sys
import zipfile

# Le nom de la ROM son dans chaque set, comme src/machine/romset.cpp les nomme.
SOUND_ROMS = (
    "epr-20600a.21",   # Fighting Vipers 2
    "epr-20636.21",
    "epr-19231.21",    # Virtua Fighter 3
)


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2].strip("# "), file=sys.stderr)
        return 2
    src, dst = argv[1], argv[2]
    with zipfile.ZipFile(src) as z:
        names = {n.lower(): n for n in z.namelist()}
        found = next((names[r] for r in SOUND_ROMS if r in names), None)
        if found is None:
            print("aucune ROM son connue dans %s ; il y a : %s"
                  % (src, ", ".join(sorted(names.values())[:8])), file=sys.stderr)
            return 1
        data = bytearray(z.read(found))

    if len(data) % 2:
        print("%s fait %d octets, un nombre impair : ce n'est pas une ROM 16 bits"
              % (found, len(data)), file=sys.stderr)
        return 1

    for i in range(0, len(data), 2):
        data[i], data[i + 1] = data[i + 1], data[i]

    # Le controle qui dit que l'echange etait le bon sens : apres lui, le PC de reset doit tomber
    # dans la ROM programme, a une adresse paire. Si ce n'est pas le cas, on s'arrete au lieu
    # d'ecrire une image fausse qu'on desassemblerait ensuite pendant une heure.
    ssp = int.from_bytes(data[0:4], "big")
    pc = int.from_bytes(data[4:8], "big")
    if not (0x600000 <= pc < 0x680000) or pc % 2:
        print("apres l'echange, le PC de reset vaut %08X : hors de la ROM programme ou impair. "
              "L'ordre des octets de cette ROM n'est pas celui attendu ; rien n'a ete ecrit."
              % pc, file=sys.stderr)
        return 1

    with open(dst, "wb") as f:
        f.write(bytes(data))
    print("%s : %d octets, SSP %08X, PC de reset %08X -> %s" % (found, len(data), ssp, pc, dst))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
