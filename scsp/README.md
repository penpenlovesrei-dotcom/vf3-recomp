# Le SCSP du projet

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
