# -*- coding: utf-8 -*-
# exporter-scsp.py : sort le SCSP du projet en depot autonome.
#
#     python tools/son/exporter-scsp.py [dossier]     (defaut : C:\\Temp\\scsp-seul)
#
# CE QUE CA ETABLIT, et ce n'est pas rien : docs/son.md §7 disait « pas encore un depot a part », et nommait
# le moment -- « quand le coeur autonome existera et passera le harnais differentiel ». C'est fait, et deux
# mesures du 2026-10-28 l'etablissent au-dela de l'intention :
#
#   1. `make test-sans-mame` construit les sept tests de bloc avec scsp_select.o, scsp_maison.o et
#      dsp_maison.o SEULS. Le coeur du projet n'a plus aucun lien de construction avec celui de MAME.
#   2. Le sous-systeme sonore de HEAD s'est pose tel quel sur fv2-stable-1, une base vieille de dix jours :
#      dix-huit fichiers copies, quatre lignes ajoutees au Makefile, et le jeu tourne. C'est la portabilite
#      reelle et non supposee -- et elle a servi a autre chose au passage, en montrant que le correctif du
#      miroir MADRS tient en une ligne de l'hote.
#
# CE QUI PART ET CE QUI RESTE. Partent le coeur, son contrat, le selecteur, le DSP, la trace, le banc, les
# sept tests de bloc et les documents. Restent scsp.{h,cpp}, scspdsp et scsp_mame : c'est MAME, BSD-3, et la
# frontiere de licence passe la. Ne peuvent pas partir non plus test_scsp_core et test_scspdsp -- leur sujet
# EST le coeur de MAME -- ni la seconde moitie de test_scsp_contract, qui lit la ROM du pilote.
#
# LES CHEMINS SONT CEUX DE CE DEPOT, src/sound et tools/son : pas une ligne des sources ne change entre les
# deux, et un `git subtree` ou un `git diff` entre eux reste lisible.
#
# POURQUOI UN EXPORT ET PAS UN SUBTREE, pour l'instant : `git subtree` veut un REPERTOIRE unique, et le SCSP
# est reparti sur trois. Un vrai subtree demande donc de restructurer ce depot, ce qui ne se fait pas a la
# legere ni pendant que d'autres y travaillent. Cet export est a sens unique, mais il est refaisable et il
# se VERIFIE : le depot sorti se construit et passe ses sept tests, tout seul.
import io, os, shutil, sys

SP = os.path.dirname(os.path.abspath(__file__))
DEPOT = os.path.abspath(os.path.join(SP, '..', '..'))
CIBLE = sys.argv[1] if len(sys.argv) > 1 else r"C:\Temp\scsp-seul"

AUTONOMES = [
    'src/sound/scsp_core.h', 'src/sound/scsp_contract.h',
    'src/sound/scsp_select.h', 'src/sound/scsp_select.cpp',
    'src/sound/scsp_maison.h', 'src/sound/scsp_maison.cpp',
    'src/sound/dsp_maison.h', 'src/sound/dsp_maison.cpp',
    'src/sound/scsp_trace.h', 'src/sound/scsp_trace.cpp',
    'src/sound/sound_prof.h',
    'tools/son/banc_scsp.h',
    'tools/son/test_scsp_regs.cpp', 'tools/son/test_scsp_fentes.cpp', 'tools/son/test_scsp_adsr.cpp',
    'tools/son/test_scsp_lfo.cpp', 'tools/son/test_scsp_irq.cpp', 'tools/son/test_scsp_mix.cpp',
    'tools/son/test_scsp_dsp.cpp',
    'tools/son/mesurer-coeur.cpp', 'tools/son/rejouer.cpp',
    'tools/son/variantes-dsp.py', 'tools/son/batir-variantes.py',
    'docs/son.md', 'docs/scsp_architecture.md', 'docs/scsp_audit.md', 'docs/scsp_validation.md',
    'docs/scsp_references.md', 'docs/scsp_migration_plan.md', 'docs/scsp_benchmark_baseline.md',
]

MAKEFILE = """# Le SCSP du projet, seul. AUCUN fichier de MAME dans cette chaine.
#
#     make test     les sept tests de bloc, sur le coeur du projet
#     make mesurer  le socle, par poste
#
# Les chemins sont ceux de fv2-recomp -- src/sound, tools/son -- pour que pas une ligne des sources ne
# change entre les deux depots.
CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wno-unused-function
OBJ ?= build/obj
BIN ?= build/bin

COEUR = src/sound/scsp_select.cpp src/sound/scsp_maison.cpp src/sound/dsp_maison.cpp
objs = $(patsubst %.cpp,$(OBJ)/%.o,$(1))

BLOCS = test_scsp_regs test_scsp_fentes test_scsp_adsr test_scsp_lfo test_scsp_irq test_scsp_mix test_scsp_dsp

all: test

$(OBJ)/%.o: %.cpp
\t@mkdir -p $(dir $@)
\t$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN)/%: $(call objs,$(COEUR)) $(OBJ)/tools/son/%.o
\t@mkdir -p $(dir $@)
\t$(CXX) $(CXXFLAGS) $^ -o $@

$(BIN)/mesurer-coeur: $(call objs,$(COEUR)) $(OBJ)/tools/son/mesurer-coeur.o
\t@mkdir -p $(dir $@)
\t$(CXX) $(CXXFLAGS) $^ -o $@

test: $(addprefix $(BIN)/,$(BLOCS))
\t@for t in $(BLOCS); do $(BIN)/$$t maison || exit 1; done

mesurer: $(BIN)/mesurer-coeur
\t$(BIN)/mesurer-coeur --socle

clean:
\trm -rf build

.PHONY: all test mesurer clean
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
"""

LISEZMOI = """# Le SCSP du projet

Une implementation du Yamaha SCSP (YMF292) ecrite pour le portage Model 3 de ce depot, qui ne derive pas de
celle de MAME : elle est ecrite depuis `src/sound/scsp_contract.h` -- le contrat materiel, 48 champs avec
leur provenance -- et depuis les sept tests de bloc, qui sont ici.

    make test      les sept tests de bloc
    make mesurer   ce que le coeur coute, par poste

## Ce qui est ici, et ce qui ne l'est pas

Le coeur (`scsp_maison`), son DSP d'effets (`dsp_maison`), l'interface (`scsp_core.h`), le contrat, le
selecteur, le format de trace, le banc et les sept tests de bloc. Plus les documents, qui sont l'essentiel
du travail : la connaissance pese plus que le code.

**Le coeur de MAME n'est pas ici** -- `scsp.cpp`, `scspdsp.cpp`, `scsp_mame.cpp` restent dans le portage,
sous BSD-3-Clause, et la frontiere de licence passe la. Trois tests restent avec eux, parce que leur sujet
EST ce coeur : `test_scsp_core`, `test_scspdsp`, et la seconde moitie de `test_scsp_contract`, qui lit la
ROM du pilote.

## Ce que ce depot ne peut pas refaire, et il faut le savoir

**La preuve vit dans le jeu.** Les nombres des documents -- l'identite a l'octet sur 3 219 865 echantillons
par puce, la part du son dans une image, les programmes DSP releves sur trois jeux -- sortent tous du
portage pilotant les vraies ROM. Ce depot-ci porte le code et les documents ; il ne peut ni refaire ni
recontroler ces nombres. Pour cela, c'est `fv2-recomp`.

Ce qu'il peut faire seul, et c'est deja beaucoup : se construire sans une ligne de MAME, et passer ses sept
tests de bloc.

## D'ou il vient

Sorti de `fv2-recomp` par `tools/son/exporter-scsp.py`. Les chemins sont les memes des deux cotes, donc un
`diff` entre les deux reste lisible.
"""


def main():
    shutil.rmtree(CIBLE, ignore_errors=True)
    poses, absents = [], []
    for rel in AUTONOMES:
        src = os.path.join(DEPOT, rel.replace('/', os.sep))
        if not os.path.exists(src):
            absents.append(rel)
            continue
        dst = os.path.join(CIBLE, rel.replace('/', os.sep))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        poses.append(rel)
    io.open(os.path.join(CIBLE, 'Makefile'), 'w', encoding='utf-8', newline='\n').write(MAKEFILE)
    io.open(os.path.join(CIBLE, 'README.md'), 'w', encoding='utf-8', newline='\n').write(LISEZMOI)
    print(f"{len(poses)} fichiers dans {CIBLE}, plus le Makefile et le README")
    if absents:
        print("  ABSENTS du depot source :", ', '.join(absents))
    print("  a verifier : make test dans ce dossier doit rendre sept fichiers au vert")


if __name__ == '__main__':
    main()
