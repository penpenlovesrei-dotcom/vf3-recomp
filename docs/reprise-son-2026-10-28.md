# Reprise — le son, état à la fin du 2026-10-28

**Lis `docs/son.md` d'abord** : il dit où est quoi. Ce fichier-ci ne dit que ce qui a bougé ce jour-là, ce
qui est vérifié, et ce qui reste — avec, à chaque fois, l'instrument qui l'a établi.

Huit commits, de `7876901` à `609b2ac`, poussés sur `mine/fv2`.

---

## 1. `VF3_SND_PROF` ne sortait rien — quatre raisons, toutes muettes

Chacune rendait la mesure **vide ou fausse sans le dire**. Toutes fermées, et les trois premières **parlent**
maintenant. Détail en `docs/scsp_audit.md` §4 sexies.

1. l'appel à `sndprof::report` vivait dans le bloc `if (prof)` de `tools/vf3run.cpp` : `VF3_SND_PROF=1` sans
   `VF3_PROF=1` n'imprimait rien. Il a son propre test, et écrit sur **stdout** comme la ligne `prof`.
2. **`vf3run` ne fait tourner la carte son qu'avec `--wav`.** Sans lui la sonde comptait zéro et l'affichait
   comme une mesure. L'outil le dit au lancement, et le rapport dit « rien mesuré ».
3. le rapport n'arrivait qu'aux multiples de 500 images : un rapport final couvre le reste.
4. les portées `SCSP` et `DSP` ne vivaient que dans le code de MAME ; sur `VF3_SCSP_COEUR=maison` le rapport
   donnait **97 % au 68000, zéro aux voix, zéro au DSP** — un chiffre faux, pas un trou.

## 2. Ce que le cœur du projet coûte, dans le jeu

Trois passages par cœur, alternés, fenêtres 1500-4200, 18 mesures par cœur, médiane. Les six WAV identiques
à l'octet.

| | `mame` | `maison` | écart |
|---|---|---|---|
| 68000 (témoin : même code) | 1 015 kc | 947 kc | **−68 kc** — le jeu de la mesure |
| voix | 443 | 484 | +41 kc, **sous le jeu : ne se lit pas** |
| **DSP** | **1 737** | **2 039** | **+302 kc (+17,4 %)** |
| toute la carte son | 3 284 | 3 550 | **+266 kc (+8,1 %)** |

Sans dénominateur : **9,60 → 11,50 cycles par pas de DSP, +19,8 %**. Et dans l'image sous `--rec` :
**+0,09 ms, soit 0,5 % du budget de 17,4 ms** ; la part du son passe de 30,7 % à 31,6 %.

## 3. La trace du rejoueur n'était complète que sur 300 images — refermé

Le contrôle « la trace est-elle complète ? » échouait sur la scène entière (échantillons 772 067 et
1 149 006). **La cause : le miroir MADRS**, le seul des quatre extras hors de `scsp::core` qui change le son,
posé par `dynamic_cast` dans `SoundBoard::reset` **après** la création de l'enregistreur — donc invisible à
la trace, qu'un rejeu laissait à son défaut `true` alors que Fighting Vipers 2 le veut faux.

Une seule porte maintenant (`scsp::regle_miroir_madrs` / `miroir_madrs` dans `src/sound/scsp_select.h`), le
miroir posé **avant** que l'enregistreur existe et **relu de la puce** par lui, un en-tête de trace
**version 2** qui le porte, et une trace v1 **refusée** au lieu d'être devinée. Toute la mesure, les pistes
écartées et la leçon sont en `docs/scsp_validation.md` §3 c.

## 4. Sept tests ne traînent plus le cœur de MAME

`tools/son/banc_scsp.h` incluait `scsp_mame.h` **et** `scsp_maison.h` pour deux aiguillages de trois lignes,
si bien que sept tests de bloc liaient les 2 110 lignes du cœur de MAME sans aucune raison de fond. Le même
`dynamic_cast` existait en **trois** copies : la carte, le banc, `test_scsp_regs`.

**Chaque cœur s'inscrit maintenant dans son propre `.cpp`** (`scsp::inscrit`), avec sa fabrique et trois
fonctions qui rendent faux quand le cœur passé n'est pas le leur. Le sélecteur ne nomme plus aucune classe
concrète, et `noms_connus()` rend ce qui est **lié**. La preuve est une cible : **`make test-sans-mame`**
construit les sept tests avec `scsp_select.o`, `scsp_maison.o`, `dsp_maison.o` et rien d'autre, et
`make test` la refait à chaque fois. **C'était le dernier préalable technique d'un dépôt séparé**
(`son.md` §7).

## 5. Le DSP par caractéristique : ce n'est aucune instruction, c'est le socle

`mesurer-coeur --dsp` descend d'un cran sous le poste « DSP ». Des programmes de 112 pas où tous les pas ne
font qu'une chose, les deux cœurs côte à côte, et la colonne qui compte est le coût **propre** — socle
déduit (`scsp_audit.md` §4 septies).

- **le socle** — 112 pas qui ne font rien — coûte **+0,520 ns par pas (+19 %)** au cœur du projet ;
- **le témoin**, la forme du jeu, a un coût propre **quasi nul** entre les deux cœurs. Une fois le socle
  retiré, les deux DSP exécutent le mélange du jeu au même prix ;
- le cœur du projet est **meilleur** sur la moitié de la table : COEF, IWT, le produit, `SHIFT 1`, et les
  quatre champs rares de −0,33 à −0,65 ;
- il n'est vraiment moins bon que sur l'**accès mémoire** : `MRD` +0,72, `MWT` +0,73, `PACK` +0,89,
  `SHIFT 3` +0,53. Dans le jeu, 33 pas sur 112, soit ≈ +0,21 ns par pas.

Un second banc sépare le par-pas du par-échantillon (trois longueurs de programme) : **l'écart est par pas**.
L'ordonnée à l'origine n'est **pas résolue**, et le contrôle de linéarité posé dans l'outil le dit.

## 6. L'assembleur, lu après que le banc ait resserré la question

`SCSPDSP::Run<0>` : 108 instructions, **16 branchements**. `dsp_maison::boucle<false>` : 145 et **31**. Pour
le même algorithme. **Cinq hypothèses mortes à l'inspection**, aucune par raisonnement : les deux corps
existent chez `maison` ; son pas décodé est **plus petit** (16 octets contre 28) ; pas de table de saut, pas
de division ; le produit 64 bits n'est pas une divergence du portage — MAME écrit la même ligne ; et les
deux compilent `std::clamp` en `cmov`.

## 7. Deux lots pour enlever ces branchements — et les deux perdent

Défaits, `src/` revenu à l'identique et vérifié (`scsp_audit.md` §4 octies).

| | rapport au socle de `mame` |
|---|---|
| **l'original** | **1,22** |
| lot A — cinq octets de plus dans `Pas` | 1,30 |
| lot B — les mêmes décisions dans les bits libres du mot | 1,42 |

**Le nombre de branchements n'était pas le coût.** Le lot A dit pourquoi : cinq octets font passer `Pas` de
16 à 24 octets, donc le programme de 2 à 3 Ko — **+7 % pour un kilo-octet**, et c'est l'exacte réciproque de
ce que §4 septies avait écarté. Le lot B dit que le remède est pire : garder les 16 octets en déballant trois
champs par pas coûte **+16 %**.

D'où **`mesurer-coeur --socle`**, écrit pour ces essais : sept passages, assez rapide pour entrelacer une
référence et une variante du même fichier. Il fallait — le socle de `mame` a dérivé de 2,70 à 3,21 pendant
les essais, et sans lui on aurait pris cette dérive pour un résultat.

## 8. Les quatre champs du DSP : cherchés partout, puis éprouvés

**Aucun jeu servi ne les pose.** `dspprog` lu sur tous les sets que `romset.cpp` connaît, en attraction et à
travers un combat : **152 écritures de programme, cinq formes distinctes, trois jeux** — `fvipers2` (4
formes, dont une de 127 pas), `vf3` (les mêmes que `fvipers2`), `srally2` (un **autre** programme, le témoin
qui compte). `vf3tb` et `fvipers2p` sont des sets incomplets. Donc **rien à aller chercher** : le rejoueur ne
pouvait pas en être juge (`scsp_audit.md` §5).

**`tools/son/test_scsp_dsp.cpp` cas 9 à 13** les mesurent, chacun par le seul chemin de lecture qui existe :

| cas | relevé |
|---|---|
| `NEGB` — le signe de B sur un pas de maintien | `+2330` / `−2330` |
| `FRCL` — `FRC_REG` relu **comme Y** (`YSEL=0`) | `331` à `SHIFT=0`, `2912` à `SHIFT=3` |
| `YRL` — `Y_REG` relu par `YSEL=2` puis `YSEL=3` | `662` / `946` |
| `ADRL` — l'adresse que `ADREB` décale | `MEMS[1] = 0x567800` |
| les deux corps s'accordent | `EFREG[0] = 2330` |

Le cœur du projet les rend tous les cinq **du premier coup** : une confirmation, pas une correction. Et
**les cas savent échouer** — quatre fautes injectées, chacune attrapée par son propre cas.

---

## Ce qui est vérifié

| | |
|---|---|
| la construction | `make test` et les quatre outils, code 0 |
| les tests | **24 fichiers au vert** : les dix sur les deux cœurs, plus les sept **sans une ligne de MAME** |
| le rejoueur, scène entière | `tools/son/rejouer.cmd fvipers2 tools/scripts/fv2-fight.txt 4200 mame maison` → **TOUT CONCORDE**, les quatre contrôles, 3 219 865 échantillons par puce |
| l'empreinte de bout en bout | `vf3run --frames 4200 --rec --wav` avec chaque cœur : **`9A2310F8AF000E09EB18BD114852B003`** |
| la non-régression de VF3 | `verifier-vf3.cmd 3000` → **IDENTIQUE** sur les quatre contrôles, et le pas à pas interpréteur / recompilé concorde sur 2 231 images |

**Les huit critères d'acceptation du plan de migration sont satisfaits** (`scsp_migration_plan.md`), le 2
inclus, qui n'avait pas tourné depuis les lots.

## Ce qui n'est pas vérifié, et il n'en reste qu'un

**Que MAME ait raison sur les quatre champs.** Personne ne les a mesurés sur du matériel, et
`scsp_references.md` refuse le consensus entre émulateurs comme preuve. Les cas 9 à 13 établissent que le
DSP du projet atteint le comportement **visé**, et rien de plus. Les huit premiers cas, eux, sont recoupés
par le jeu à chaque rejeu.

## Ce qui reste

**À toi**

- **La mesure de concurrence sur le portable deux cœurs** : `C:\Users\frede\Desktop\mesure-concurrence`, un
  double-clic sur `mesurer-concurrence.cmd`. Lire son `LISEZ-MOI.txt` avant de commenter les chiffres —
  notamment le piège des cadences avec `VF3_SND_OFF`.

**Bloqué sur un instrument qui manque**

- **Le socle du DSP.** Trois instruments s'accordent sur l'endroit — le socle, par pas — et divergent sur la
  cause. Ce qu'il faudrait : un **compteur matériel** — cycles, instructions retirées, branchements mal
  prédits, défauts de cache — sur deux boucles de même taille. La question est maintenant assez étroite pour
  qu'un seul passage y réponde, ce qui n'était pas vrai le matin.

**Rien à faire**

- **Les gains par groupe** : ils atténuent par fente avant le mélange, donc ils ne montent pas dans la carte
  sans changer l'interface. Inertes tant que les deux volumes valent 1.
- **La boucle de fentes** : +18 % au banc, invisible dans le jeu (+41 kc mesurés contre un jeu de ±68),
  0,2 % du budget d'image.
- **Le dépôt séparé** : le dernier préalable technique est tombé (point 4). Ce qui reste n'est pas technique
  — la preuve vit dans le jeu, et c'est l'argument de `son.md` §7. Trois tests ne partiraient pas :
  `test_scsp_core`, `test_scspdsp` et `test_scsp_contract` ont le cœur de MAME pour sujet.

## Les outils nouveaux ou changés ce jour-là

| | |
|---|---|
| `mesurer-coeur --dsp` | le DSP par caractéristique, les deux cœurs, socle déduit |
| `mesurer-coeur --socle` | le socle seul, sept passages — pour entrelacer une référence et une variante |
| `make test-sans-mame` | les sept tests de bloc liés sans une ligne du cœur de MAME |
| `VF3_SND_PROF=1` | n'a plus besoin de `VF3_PROF`, exige `--wav`, et vaut pour les deux cœurs |
| trace `# scsp-trace 2` | l'en-tête porte `miroir=0|1` ; une trace v1 est refusée |
