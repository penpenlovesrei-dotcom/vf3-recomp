# Audit du sous-système sonore — ce qu'il y a, ce qu'il coûte

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [docs/son.md](son.md).

Écrit le 2026-10-24, sur la branche `fv2` au commit `335c050`, machine de Frédéric (Windows 11, g++ 16.1.0
MSYS2, `-O2`). Toutes les mesures de ce document ont été exécutées ; les commandes sont données telles quelles
pour qu'elles se refassent.

**La règle de lecture :** une mesure dit ce qu'elle dit et rien de plus. Les chiffres ci-dessous viennent tous
de `vf3run` (l'outil sans fenêtre, interpréteur PowerPC) sur `fvipers2.zip`, scène `tools/scripts/fv2-fight.txt`
— démarrage, écran titre, sélection, un round. Ils ne disent rien d'une autre machine ni d'une autre scène.


## 1. Les pièces, et qui les fait tourner

| pièce | fichier | ce qu'elle est |
|---|---|---|
| la carte son | `src/sound/soundboard.{h,cpp}` | 68000 à 11,2896 MHz, deux SCSP avec 1 Mo de RAM chacun, le registre de banque, la ROM programme, les échantillons |
| le SCSP | `src/sound/scsp.{h,cpp}` | Yamaha YMF292-F : 32 voix, enveloppes, LFO, pitch, timers, interruptions, DMA, liaison MIDI |
| le DSP d'effets | `src/sound/scspdsp.{h,cpp}` | 128 pas de microcode, mémoire de retard dans la RAM son, 16 sorties EFREG |
| le 68000 | `third_party/mame/m68000/` | le 68000 de MAME (Olivier Galibert), gestionnaires d'instructions générés, précis au cycle de bus |
| le cadre MAME | `src/core/mame_shim.*`, `src/core/shim_include/emu.h` | ce que les cœurs MAME attendent du framework, remplacé par le strict nécessaire |

**Qui appelle qui.** Le jeu (PowerPC) n'envoie que des octets MIDI ; `model3.cpp` les met dans `board.midi`.
L'hôte et `vf3run` les passent à `SoundBoard::midi_in`, puis demandent `SoundBoard::run(1/57.52 s, out)`. Là :

```
run(seconds)
  pour chaque tranche d'au plus un echantillon (256 cycles du 68000) :
      cpu->run(cycles)                          <- le 68000
          Bus::read / Bus::write                    (appel virtuel)
              scsp_device::read / write
                  update() -> sound_stream_update -> DoMasterSamples   <- les echantillons
                                                       UpdateSlot x32
                                                       SCSPDSP::Step   <- 112 pas
      fire_due()                                <- les timers A/B/C, MIDI emission/reception
  scsp[0]->update(); scsp[1]->update()           <- le reste de l'image
  le melange stereo, le limiteur, out.push_back
```

Le point important, et celui qui a rendu la mesure difficile : **les échantillons ne sont pas fabriqués dans une
phase à eux**. Le 68000 les déclenche depuis l'intérieur de ses propres tranches, chaque fois qu'il touche un
registre de chip — c'est le flux de MAME ramené à l'instant présent (`scsp_device::update`). Le reste de
l'image est fabriqué par les deux `update()` qui la referment.

**Où ça tourne.** Dans l'hôte (`src/host/main.cpp`), la carte son est sur **un fil à elle**, une image en
retard : son coût ne s'ajoute donc pas au temps d'une image, il occupe un cœur à côté des fils de rendu. Dans
`vf3run` tout est sur un seul fil, et le coût s'y lit directement. **Aucun gain annoncé ici n'est un gain de
temps d'image** : c'est du temps processeur en moins, ce qui vaut pour la chaleur, la batterie et les cœurs
laissés au rendu, pas pour une image qui arriverait plus tôt.


## 2. Provenance et licences

| chemin | licence | auteurs au titre | état |
|---|---|---|---|
| `src/sound/scsp.{h,cpp}` | BSD-3-Clause | ElSemi, R. Belmont ; merci à kingshriek | MAME `sound/scsp.cpp`, **modifié** (voir plus bas) |
| `src/sound/scspdsp.{h,cpp}` | BSD-3-Clause | ElSemi, R. Belmont | MAME `sound/scspdsp.cpp`, **modifié** (programme décodé une fois, corps spécialisé) |
| `src/sound/soundboard.{h,cpp}` | BSD-3-Clause | R. Belmont, Ville Linde | suit `sega/model3.cpp` (`model3_snd`), le reste est du projet |
| `src/sound/sound_prof.h` | — (projet) | — | neuf, écrit pour cet audit |
| `third_party/mame/m68000/` | BSD-3-Clause | Olivier Galibert | MAME, gestionnaires et table de décodage **inchangés** ; le device réduit à un bus |
| `src/core/mame_shim.*` | — (projet) | — | le remplaçant du framework MAME |

Le dépôt est sous MIT (`LICENSE`), et BSD-3-Clause est compatible avec cette redistribution à condition de
garder l'avis, les conditions et la clause de non-garantie — ce que chaque fichier fait en tête. `third_party/README.md`
tient déjà cet inventaire ; rien n'y manquait.

**Ce que le projet a changé au SCSP de MAME**, et qui doit survivre à toute réécriture (chacun a été mesuré ici,
sur cette borne) :

1. **Le miroir de MADRS à 0x7C0-0x7FF**, par jeu : vrai pour Virtua Fighter 3, **faux** pour Fighting Vipers 2,
   où il fait se replier toutes les lignes de retard du DSP sur une adresse et où le son de l'écran de victoire
   se nourrit lui-même (+24 dB jusqu'au limiteur). MAME a depuis corrigé ça de son côté (mamedev/mame#16527).
   Voir `src/sound/scsp.h` et `docs/fv2-console-son.md`.
2. **La profondeur de modulation FM** comme celle de Supermodel (bit bas de MDL ignoré, étendue 24 et non 26).
3. **Les timers A-C comptés en échantillons**, dus sur un échantillon rond, comme Supermodel.
4. **Les volumes par groupe** (musique / effets) et le niveleur d'effets : ce n'est pas le chip, c'est un
   mélangeur d'hôte, et il est désactivé tant que les deux volumes valent 1.
5. **Le limiteur de crête** de la carte, au-dessus du gain 1.
6. **SDIR** qui met TL à zéro dans les deux mélanges en aval.

Les sondes et les journaux (`VF3_SCSP_WLOG`, `VF3_KEYON_LOG`, `VF3_SCSP_SOLO`, `VF3_DUMP_SOUND_RAM`,
`m_probe_*`) sont du projet aussi, et servent aux enquêtes de fidélité.


## 3. Comment c'est mesuré

### Le temps de la carte son, image par image

Déjà présent : `VF3_PROF=1` fait imprimer à `vf3run`, toutes les 500 images, le temps moyen d'une image passé
dans la carte Model 3, dans la carte son et dans la construction de scène.

```bash
VF3_PROF=1 build/bin/vf3run.exe "<ROMs>/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --wav son.wav --quiet
```

### Où va ce temps, dans la carte son

Neuf : `src/sound/sound_prof.h`, allumé par `VF3_SND_PROF=1`. Des portées imbriquées comptent les cycles TSC du
tour de boucle, du 68000, de la fabrication des échantillons, du DSP et du mélange, et le rapport **retire le
temps imbriqué** pour donner à chacun sa part propre (la formule est dans l'en-tête). Éteint, il ne coûte rien
qu'un test de booléen.

```bash
VF3_PROF=1 VF3_SND_PROF=1 build/bin/vf3run.exe ... --wav son.wav --quiet
```

**`--wav` n'est pas décoratif dans cette commande : sans lui `vf3run` ne fait pas tourner la carte son du
tout**, et la sonde n'a rien à compter. Elle le dit maintenant, au lieu de sortir une ligne de zéros ; les
trois autres manières dont ce branchement se taisait sont en §4 sexies. Les deux portées `SCSP` et `DSP`
vivent des deux côtés depuis le 2026-10-28, donc le rapport vaut pour `VF3_SCSP_COEUR=maison` aussi.

**L'instrumentation a été vérifiée, comme le demande toute mesure qui se respecte** : sur la même scène de 4200
images, sondes éteintes puis allumées, le temps de la carte son est le même à ~0,5 % près (1,60/1,75/1,83/1,83/1,85/1,89/2,03/2,02
contre 1,61/1,77/1,81/1,89/1,85/1,92/1,99/1,98 ms), et le WAV produit est **identique à l'octet**. Le rapport
imprime lui-même ce que ses `rdtsc` coûtent (~58 kcycles par image sur ~4000, soit 1,5 %).

### Le son n'a pas bougé

`tools/son/comparer-son.cmd <jeu> <script> <images> [commit]` fait jouer la même scène au `vf3run` de la branche
et à celui d'un commit de référence (un arbre de travail à part, `C:\Temp\fv2-son-ref`), et compare les deux WAV
**octet par octet**. Pour une optimisation, l'identité exacte est la seule réponse acceptable ; il n'y a pas de
tolérance à élargir. Le script a été exécuté sur les deux jeux ; son verdict est au §7.


## 4. La baseline, et où va le temps

Scène `fv2-fight.txt`, 4200 images, `fvipers2.zip`. Le budget d'une image à 57,52 Hz est de **17,4 ms**.

| images | carte Model 3 | **carte son** | part de l'image |
|---|---|---|---|
| 0-500 | 13,09 ms | 1,60 ms | 9,2 % |
| 500-1000 | 12,89 | 1,75 | 10,1 % |
| 1000-1500 | 13,49 | 1,83 | 10,5 % |
| 1500-2000 | 14,02 | 1,83 | 10,5 % |
| 2000-2500 | 13,45 | 1,85 | 10,6 % |
| 2500-3000 | 14,57 | 1,89 | 10,9 % |
| 3000-3500 | 14,57 | 2,03 | 11,7 % |
| 3500-4000 | 14,67 | 2,02 | 11,6 % |
| **moyenne** | **13,8** | **1,85** | **10,6 %** |

Ce que ça établit : **la carte son n'est pas le goulot** — la carte Model 3 (interpréteur PowerPC et vidéo) coûte
sept fois plus. Et ce que ça n'établit pas : rien sur la version recompilée (`--rec`), où la part du son monte
d'autant que la carte descend, ni sur l'hôte, où le son est sur son propre fil.

Et dedans, part propre de chaque pièce (`VF3_SND_PROF=1`, mêmes 4200 images) :

| images | boucle+timers | 68000 | voix | **DSP** | mélange | pas de DSP / image |
|---|---|---|---|---|---|---|
| 0-500 | 1,4 % | 24,2 % | 5,9 % | **67,9 %** | 0,6 % | 153 416 |
| 500-1000 | 1,3 | 23,5 | 5,7 | **69,0** | 0,5 | 171 395 |
| 1000-1500 | 1,3 | 23,3 | 7,3 | **67,0** | 1,1 | 169 569 |
| 1500-2000 | 1,3 | 23,8 | 9,5 | **64,4** | 1,1 | |
| 2000-2500 | 1,2 | 22,9 | 7,5 | **67,5** | 0,9 | |
| 2500-3000 | 1,2 | 22,9 | 10,1 | **64,6** | 1,1 | |
| 3000-3500 | 1,1 | 22,6 | 13,3 | **62,0** | 1,0 | |
| 3500-4000 | 1,1 | 22,6 | 12,3 | **63,0** | 1,0 | |

**Les deux tiers de la carte son sont le DSP d'effets.** Les voix, que l'on soupçonnerait d'abord, font 6 à
13 % — et leur part monte avec le combat, ce qui est attendu : plus de voix actives. Le 68000 fait un quart
stable. La boucle et le mélange ne valent pas qu'on les regarde.

~170 000 pas de DSP par image : 766 échantillons × 2 chips × **112 pas**. Les deux SCSP font donc tourner un
programme de 112 pas chacun, 44 100 fois par seconde.

## 4 bis. La part du son sous `--rec` : mesurée, et elle triple

La section ci-dessus disait « rien sur la version recompilée (`--rec`), où la part du son monte d'autant que
la carte descend ». **Mesuré le 2026-10-25**, même scène `fv2-fight.txt`, mêmes 4 200 images, `VF3_PROF=1` et
`VF3_SND_PROF=1`, et les fenêtres 1500-4000 des deux côtés pour comparer les mêmes images — `--rec` n'étant
chaud qu'après le second passage du recompilateur, vers l'image 1000.

| | carte Model 3 | carte son | total | **part du son** | du budget de 17,4 ms |
|---|---|---|---|---|---|
| interpréteur | 13,77 ms | 1,46 ms | 15,23 ms | **9,6 %** | 87,5 % |
| **`--rec`** | **3,23 ms** | **1,42 ms** | **4,64 ms** | **30,5 %** | **26,7 %** |

**La carte est 4,3 fois plus rapide ; le son ne bouge pas (×0,97, dans le bruit) ; sa part triple — ×3,18.**

Trois témoins, et c'est ce qui rend ces chiffres utilisables :

1. **Le son ne devait pas bouger, et il ne bouge pas.** Le recompilateur ne touche que le PowerPC ; la carte
   son reste un 68000 interprété. 1,46 contre 1,42 ms : la mesure le confirme au lieu de le supposer.
2. **Le partage interne est identique dans les deux modes** — DSP 52 %, 68000 29 %, voix 16 % — comme il
   doit l'être pour le même code.
3. **Les deux WAV sont identiques octet pour octet** (`md5` `9a2310f8…`, 12 879 504 octets chacun). Donc
   `--rec` ne change pas une image du son, et la comparaison porte bien sur le même travail.

Et le témoin qui vaut pour le travail d'optimisation : le son est à **1,46 ms** là où la baseline de ce
document mesurait **1,85 ms** — soit **−21 %**, reproduit à deux semaines d'écart sur la même scène. Le gain
de −24,3 % annoncé ailleurs tient. De même, la part propre du DSP est passée de 62-68 % à ~52 %, ce qui est
exactement ce qu'on attend d'une optimisation qui a porté sur lui.

**Ce que ça change pour la suite.** Sous `--rec`, le son n'est plus un détail à 10 % : c'est **le deuxième
poste et près d'un tiers du travail d'une image**. Il reste loin d'être un goulot en absolu — 4,64 ms sur un
budget de 17,4 — mais c'est désormais là que se trouve la marge la plus facile, et le DSP en est la moitié.

**Ce que ça n'établit pas** : rien sur une autre scène. Quant à l'hôte, c'était la réserve de cette section
et elle est levée : voir §4 ter juste en dessous.


## 4 ter. Et dans l'hôte, le son ne coûte rien à l'image

Les 30,5 % ci-dessus sont ceux du **lanceur sans interface**, où la carte son tourne **dans** la boucle
d'image. Dans l'hôte elle est sur **un fil à elle, une image en retard** : son coût se sépare en deux, et une
seule moitié est celle de l'image.

| | |
|---|---|
| **le coût CPU** | le travail du fil, qu'il faut bien faire quelque part |
| **le coût d'image** | le temps que le fil du jeu passe **bloqué** à attendre le son de l'image précédente, dans `finish_frame`. Si c'est zéro, le son ne coûte **rien** à la photo, quel que soit le CPU qu'il brûle |

Mesuré le 2026-10-26 avec `VF3_SND_HOST=500`, sans fenêtre (`VF3_HIDDEN=1`), 120 secondes de jeu réel,
7 500 images, fenêtres 2000-7499 une fois le cadencement calé :

| | |
|---|---|
| le fil du son travaille | **1,49 ms par image** (1,35 à 1,57) |
| le jeu l'attend | **0,000 ms**, et le maximum jamais vu est **0,016 ms** |
| la période d'image | **17,38 ms**, soit exactement 1/57,524 |
| donc | **8,5 % d'un cœur**, et **0,00 % de l'image** |

**La période de 17,38 ms est ce qui rend la mesure valable** : l'hôte cadence bien au rythme du Model 3 même
sans fenêtre, et ce n'est pas une supposition — c'est le chiffre que la sonde imprime. Les premières centaines
d'images donnent 16,0 à 19,4 ms, le temps que le cadencement se cale ; c'est pourquoi la moyenne porte sur
2000-7499 et non sur tout.

**La marge est d'un facteur 11,7** : le fil a 17,38 ms pour faire 1,49 ms de travail. C'est pourquoi l'attente
est nulle, et pourquoi elle le reste même dans les scènes chargées — le maximum de 0,016 ms représente
0,09 % d'une image.

### Ce que ça change pour la priorité D

Les deux chiffres disent des choses opposées et les deux sont vrais :

| | part du son |
|---|---|
| `vf3run --rec`, la carte son dans la boucle | **30,5 % de l'image** |
| l'hôte, la carte son sur son fil | **0,00 % de l'image**, 8,5 % d'un cœur |

Donc **optimiser le son davantage n'achète pas d'image** sur une machine qui a des cœurs à revendre : ça
achète du **temps CPU**, c'est-à-dire de la marge thermique, de l'autonomie, et de la place pour une machine
plus petite. Le gain de −24 % obtenu sur le DSP reste un vrai gain ; il ne se voit simplement pas dans la
cadence ici.

**Ce que ça n'établit pas**, et c'était la limite qui comptait : **une seule machine**, celle de Frédéric, avec
des cœurs libres. Sur deux cœurs le fil du son est en concurrence avec celui du jeu et celui des images, et
l'attente pouvait cesser d'être nulle. **Cette mesure a été faite depuis** — §4 quater : l'attente reste
nulle, et le son coûte deux fois plus cher. Rien toujours sur une scène autre que le démarrage et l'attract.


## 4 quater. Sur deux cœurs : deux fois plus cher, et l'attente reste nulle

C'est la mesure que le §4 ter nommait comme la suivante, et elle **dément sa prédiction**. Frédéric l'a lancée
le 2026-10-26 sur son portable : **AMD A9-9425, deux cœurs et deux processeurs logiques** (pas de SMT),
Radeon R5 intégrée, Windows 10 build 19045, 1366 x 768, 4 433 images. Avec fenêtre, donc dans les conditions
réelles du jeu.

État stable, les six fenêtres des images 1500 à 3999 :

| | |
|---|---|
| le fil du son travaille | **5,75 ms par image** (5,04 à 6,47) |
| le jeu l'attend | **0,001 à 0,002 ms**, maximum 0,437 ms |
| la période d'image | **33,5 ms**, soit 29,9 images/s |
| donc | **17,4 % d'un cœur** (12,9 à 20,6), et **0,00 % de l'image** |

**Le son coûte deux fois plus cher qu'il ne coûtait**, 17,4 % d'un cœur contre 8,5 % sur la machine de bureau :
c'est le même travail sur un processeur plus lent, et 52,2 cycles par pas de DSP ici. Avec deux processeurs
logiques, cela fait **8,7 % de la machine**.

**Mais l'attente ne cesse pas d'être nulle.** C'est le résultat, et il était à démontrer : le §4 ter écrivait
que "sur deux cœurs [...] l'attente pourrait cesser d'être nulle". Elle ne cesse pas. Le fil a 33,5 ms pour
faire 5,75 ms de travail, soit une marge de 5,8 ; elle a fondu d'un facteur 11,7 à 5,8, et elle tient.

### Ce qui limite la cadence, et ce n'est pas le son

Les 23 à 31 images/s de cette machine sont fixées par **la photo**, et le journal le dit poste par poste :

| | |
|---|---|
| une image (le CPU du jeu) | 4,8 à 10,3 ms |
| une photo | **18,3 à 32,8 ms**, dont **8,7 à 17,1 ms de téléversements** |
| l'entre-deux du 3D | jusqu'à 11,3 ms |
| le rendu GPU lui-même | 1,5 à 3,3 ms seulement |
| le son, en parallèle | 5,75 ms, attendu 0,001 |

Le GPU ne travaille presque pas ; ce sont les téléversements et la préparation des triangles qui tiennent la
cadence. Le son ne peut pas être la limite à 5,75 ms de travail parallèle pour 0,001 ms d'attente.

### Ce que cette mesure n'établit pas

**La sonde mesure l'attente BLOQUANTE, pas la concurrence.** Sur deux cœurs, le fil du son et le fil
principal se disputent la machine : cache, ordonnanceur, fréquence. Ce coût-là ne se voit pas dans
`attente_ms`, qui ne compte que le temps passé à attendre dans `finish_frame`. Pour l'isoler il faudrait la
même scène **son coupé** et comparer les cadences ; cette mesure n'a pas été faite. On ne peut donc pas dire
"le son est gratuit sur deux cœurs" — seulement que **le jeu ne l'attend pas**.

Deux autres réserves : le démarrage est lent sur cette machine (960 images en 81 s) et la première fenêtre,
à l'image 499, montre une attente maximale de 69,975 ms — c'est la recompilation, pas l'état stable. Et la
file audio ne s'accumule jamais : 0 octet partout, sauf 1 304 une fois.

### Où va le temps du son, sur cette machine

| | part du son |
|---|---|
| DSP | **49,8 %** (47,0 à 54,2) |
| 68000 | 35,1 % |
| voix | 12,9 % |
| boucle et timers | 1,7 % |
| mélange | 0,6 % |

Le DSP reste la moitié, comme sous `--rec` et comme dans l'hôte. Si le son doit un jour coûter moins sur une
machine de cette taille, c'est là et nulle part ailleurs — et la part des voix a doublé (6,4 → 15,0 %) à
mesure que la musique s'est chargée, ce qui en fait le second poste et non le troisième.


## 4 quinquies. Ce que le cœur du projet coûte, et où

Mesuré le 2026-10-27. **Deux instruments, et le second a été écrit parce que le premier ne pouvait pas
répondre à la question** :

| | |
|---|---|
| `tools/son/rejouer` | compare les deux cœurs **à l'octet** sur la même trace. Son temps est dominé par l'analyse de 81 Mo de texte, identique dans les deux et comptée deux fois : il donne un écart juste et une **part fausse** |
| `tools/son/mesurer-coeur` | ne chronomètre que `run()`, et **sépare les postes** : à vide, les fentes seules, le DSP seul, les deux. C'est lui qui dit où aller |

### Par poste, 32 voix et un programme de 112 pas

En pourcentage d'un cœur, 400 000 échantillons par cas :

| poste | `mame` | `maison`, première version | `maison`, après mesure |
|---|---|---|---|
| à vide | 0,15 | 0,21 | 0,13 |
| **les fentes** | **2,10** | **4,44** | **2,48** |
| le DSP | 2,10 | 2,45 | 2,53 |
| les deux | 4,13 | 6,75 | **4,67** (4,55 à 4,74) |

**C'était la boucle de fentes, et non le DSP**, ce que le chiffre global ne pouvait pas dire : le DSP du
projet n'a jamais été qu'à 17 % au-dessus, quand les fentes étaient à **111 %**.

### La cause, et elle est de moi

Les positions de champs venaient de la table du contrat — bonne chose, c'est la source unique — mais par un
**statique de fonction**. En C++, chaque lecture d'un tel statique paie un **contrôle d'initialisation
atomique** : vingt par fente, trente-deux fentes, quarante-quatre mille fois par seconde. Rendre
`scsp_contract::champ_fente` **`constexpr`** résout chaque position à la compilation : la boucle ne fait plus
qu'un chargement, un décalage et un masque, et la source unique est intacte.

Le chemin complet, par le rejoueur (3 219 865 échantillons, deux analyses de trace comprises) :

| étape | |
|---|---|
| recherche par chaîne dans la boucle | 47,80 s |
| positions résolues une fois, par un statique | 22,93 s |
| mot d'indicateurs pour les champs d'un bit du DSP | 22,48 s |
| corps de DSP spécialisé sur ce que le programme emploie | 21,88 s |
| **positions résolues à la compilation** | — et le banc par poste prend le relais |
| | **20,80 s** pour le cœur de MAME |

### Où ça en est, sans arrondir en ma faveur

Le cœur du projet reste **11 à 14 % plus cher** que celui de MAME sur le même travail, soit **+0,5 point d'un
cœur** dans le cas à 32 voix et DSP. Sur les 8,5 % que le son coûte au bureau (§4 ter), le son passerait à
environ 9,5 %. C'est 63 % de mieux que la première version, et ce n'est pas zéro.

Trois choses à en retenir plutôt qu'une :

1. **La justesse n'a jamais été payée par la vitesse.** La première version, 63 % plus lente, était
   **déjà identique à l'octet** ; le rejoueur l'a vérifié après chacune des cinq étapes.
2. **Aucune optimisation n'a sacrifié la source unique.** Le contrat reste le seul endroit où vit la
   position d'un champ ; elle y est lue à la compilation au lieu d'être cherchée un million de fois.
3. **Ce chiffre n'est pas la part du son dans une image.** C'est un banc. La part réelle demandait que le
   cœur soit branché sur la carte, et que `VF3_SND_PROF` la mesure part par part : **c'est fait, §4 sexies**
   — +8,1 % sur toute la carte son, +0,09 ms par image, et le « 11 à 14 % » de ce banc retrouvé dans le jeu.

Et une taille, qui n'est pas une performance mais qui se dit : 1 759 lignes pour le cœur du projet contre
2 540 pour celui de MAME, à comportement identique.


## 4 sexies. La part réelle, avec chaque cœur — et la sonde qui ne sortait rien

§4 quinquies finissait sur une dette : « Ce chiffre n'est pas la part du son dans une image. C'est un banc.
La part réelle demande que le cœur soit branché sur la carte, et `VF3_SND_PROF` la mesurera alors part par
part. » **Mesuré le 2026-10-28**, et la dette est payée — après avoir réparé l'instrument, qui ne sortait rien.

### Trois raisons de ne rien sortir, et chacune se taisait

La sonde était juste ; c'est son branchement qui ne l'était pas. Les trois pièges sont notés parce que
chacun rend une mesure **vide ou fausse sans le dire**, ce qui est pire qu'une erreur bruyante :

| | ce qui se passait | fermé par |
|---|---|---|
| 1 | l'appel à `sndprof::report` vivait **dans le bloc `if (prof)`** : `VF3_SND_PROF=1` sans `VF3_PROF=1` n'imprimait rien du tout | la sonde a son propre test dans `tools/vf3run.cpp`, et elle écrit sur **la même sortie** que la ligne `prof` (stdout) — avant, l'une était sur stdout et l'autre sur stderr, et une redirection gardait l'une en perdant l'autre |
| 2 | **`vf3run` ne fait tourner la carte son qu'avec `--wav`** (tous les sites d'appel sont derrière ce test) : sans lui, le rapport sortait une ligne de **zéros**, qui se lit comme une mesure | `vf3run` le dit au lancement, et le rapport dit « rien mesuré » au lieu d'afficher des zéros |
| 3 | le rapport n'arrivait qu'aux multiples de 500 images : un run plus court n'en voyait **aucun**, et un run de 4 200 perdait sa queue | un rapport final sur les images restantes (`frames 4000-4200` ci-dessous) |

Et un quatrième, qui n'était pas un trou mais **un chiffre faux** : les deux portées `SCSP` et `DSP` ne
vivaient que dans `src/sound/scsp.cpp` et `scspdsp.cpp`, donc dans le cœur de MAME seul. Sur
`VF3_SCSP_COEUR=maison`, le rapport donnait **97 % au 68000, zéro aux voix et zéro au DSP** — non pas des
cases vides, mais une attribution fausse, puisque la part imbriquée que la formule retire du 68000 valait
alors zéro. Les deux portées sont maintenant dans `scsp_maison::run` **aux endroits qui répondent** à
`DoMasterSamples` et à `m_DSP.Step()`. Le témoin que les deux mesures sont comparables : les deux cœurs
comptent désormais **2 886 portées par image, et ~58 kcycles de `rdtsc`** chacun — le surcoût de la sonde
s'annule dans la comparaison au lieu de la biaiser.

### Le protocole, parce qu'un seul passage ne concluait pas

Un passage par cœur donnait entre +3 % et +5 % selon la fenêtre, avec une fenêtre **sous** 1 — c'est-à-dire
rien. Donc : **trois passages par cœur, alternés** (`mame`, `maison`, `mame`, … pour que la dérive thermique
ne porte pas la conclusion), fenêtres **1500-4200** seulement — `--rec` n'est chaud qu'après le second
passage du recompilateur, vers l'image 1000 — soit **18 mesures de 500 images par cœur**, et la médiane.

```bash
VF3_PROF=1 VF3_SND_PROF=1 VF3_SCSP_COEUR=mame|maison build/bin/vf3run.exe "<ROMs>/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --rec --wav son.wav --quiet
```

**Le témoin d'abord : les six WAV sont identiques à l'octet**, md5 `9A2310F8AF000E09EB18BD114852B003` —
l'empreinte d'avant tout ce travail. Les deux cœurs mesurés rendent donc exactement le même son, et la
comparaison porte bien sur le même travail.

### Où va le surcoût : dans le DSP, et nulle part ailleurs de lisible

Coût propre de chaque pièce, en **kcycles par image**, médiane des 18 mesures (l'étendue entre crochets) :

| pièce | `mame` | `maison` | écart |
|---|---|---|---|
| boucle+timers | 46 [44-48] | 45 [43-46] | −2 kc (−3,8 %) |
| 68000 | 1 015 [959-1047] | 947 [896-1045] | **−68 kc (−6,7 %)** |
| voix (la boucle de fentes) | 443 [285-565] | 484 [312-646] | +41 kc (+9,3 %) |
| **le DSP** | **1 737 [1654-1818]** | **2 039 [1937-2172]** | **+302 kc (+17,4 %)** |
| mélange | 45 [34-48] | 46 [34-49] | +1 kc (+2,1 %) |
| **toute la carte son** | **3 284** | **3 550** | **+266 kc (+8,1 %)** |

**Le 68000 est le témoin, et il faut le lire avant le reste** : c'est le *même* code des deux côtés — un
68000 interprété, que le choix du cœur ne touche pas — et il bouge de −68 kc. C'est le **jeu de la mesure**,
et il a une cause nommable : la formule de `sound_prof.h` retire le temps imbriqué
(`68000 = M68K − (SCSP − FLUSH)`), donc une différence de grands nombres. **Aucun écart par pièce plus petit
que 68 kc ne se lit ici.** Le DSP à +302 kc le dépasse d'un facteur quatre ; les voix à +41 kc sont dessous.

Et le DSP **sans aucun dénominateur**, parce que les deux cœurs font tourner le même programme sur les mêmes
images — 180 463 pas par image de part et d'autre :

| | cycles par pas de DSP |
|---|---|
| `mame` | **9,60** [9,1-10,7] |
| `maison` | **11,50** [11,0-12,4] |
| écart | **+19,8 %** |

**+19,8 % contre le +20 % que `tools/son/mesurer-coeur` donne hors du jeu.** Deux instruments qui ne
partagent rien — un banc de 400 000 échantillons sans carte ni 68000 d'un côté, le jeu entier de l'autre —
tombent sur le même nombre. C'est le meilleur contrôle qu'on puisse avoir des deux.

### Trois lectures du même surcoût, et il faut les trois

Le même +266 kc par image, selon ce à quoi on le rapporte :

| rapporté à | `mame` | `maison` | |
|---|---|---|---|
| **la puce seule** (la carte son moins le 68000) | 2 269 kc | 2 535 kc | **+11,7 %** — et c'est exactement le « 11 à 14 % » du banc de §4 quinquies, retrouvé dans le jeu |
| **toute la carte son** (le 68000 compris) | 3 284 kc | 3 550 kc | **+8,1 %** — le 68000 interprété pèse 31 % de la carte et **dilue** le surcoût |
| **l'image sous `--rec`** | 1,46 ms de son sur 4,75 ms | 1,55 ms sur 4,90 ms | **+0,09 ms**, soit **0,5 % du budget de 17,4 ms** ; la part du son passe de **30,7 % à 31,6 %** |

**C'est la troisième qui décide**, parce que c'est elle qui est dans le budget d'une image : brancher la carte
sur le cœur du projet coûte **un demi-point du budget d'image**, dans le cas le plus défavorable qu'on sache
mesurer (`--rec`, où la carte Model 3 est 4,3 fois plus rapide et où la part du son est donc à son maximum).
Sur l'interpréteur, les mêmes 0,09 ms se diluent dans 15 ms et ne se voient pas.

Le bruit de la mesure en millisecondes se lit au même endroit : la carte Model 3, qui ne dépend pas du cœur
sonore, donne 3,29 contre 3,35 ms — **1,8 %**. Les 0,09 ms du son, soit 6 % de 1,46, sont donc au-dessus, mais
pas de beaucoup : c'est pourquoi la conclusion est portée par les kcycles et non par les millisecondes.

La première ligne porte une hypothèse qu'il faut dire : **le 68000 y est tenu constant à 1 015 kc**, parce que
c'est le même code des deux côtés et que son −68 kc mesuré est du jeu, non un gain — tout le +266 kc est donc
mis au compte de la puce. Pris brut, sans cette hypothèse, le même calcul donnerait 2 269 contre 2 603 kc, soit
**+14,7 %** : le haut de la fourchette du banc plutôt que le bas. Les deux lectures restent dans « 11 à 14 % »,
et c'est ce qui compte.

### Ce que cette mesure n'établit pas

1. **Rien sur la boucle de fentes.** Le banc de §4 quinquies la donne à **+18 %** ; ici elle fait +9,3 %,
   **sous le jeu de 68 kc**. +18 % sur 443 kc vaudrait +80 kc : la mesure est donc compatible avec tout
   entre zéro et le chiffre du banc, et elle ne tranche pas. Il faudrait un profilage à l'instruction, que
   `VF3_SND_PROF` ne fait pas — ses portées sont trop grosses pour une boucle de 32 fentes.
2. **Rien sur l'hôte**, où le son est sur son propre fil (§4 ter) : `VF3_SND_HOST` et
   `tools/son/mesurer-hote.cmd` sont les instruments de cette question, et ils n'ont pas été relancés sur le
   cœur du projet.
3. **Rien sur une autre machine.** Un seul processeur de bureau, une seule scène. La mesure sur le portable
   deux cœurs (§4 quater) n'a pas été refaite avec les deux cœurs.
4. **Rien sur les autres arènes.** `fv2-fight.txt` fait tourner **un** programme DSP de 112 pas ; trois
   arènes chargent le leur (§5). Un programme plus long déplacerait la part du DSP, donc le surcoût.


## 4 septies. Le DSP par caractéristique : ce n'est aucune instruction, c'est le socle

§4 sexies laissait le DSP à **+19,8 % par pas** et disait qu'un cran plus fin demandait « un profilage à
l'instruction que ce poste n'a pas ». **Écrit le 2026-10-28** : `tools/son/mesurer-coeur --dsp`, et il n'a
pas fallu de profileur.

### L'instrument

Le poste « DSP » de §4 quinquies est **un seul nombre pour un jeu d'instructions entier**. Donc un cran plus
fin, et de la même façon : des programmes de **112 pas où tous les pas ne font qu'une chose**, chronométrés
sur les deux cœurs. La différence entre le socle et le socle + une caractéristique **est** le coût de cette
caractéristique, par pas. Trois passages, médiane.

Et la colonne qui compte est le **coût propre** — la valeur moins le socle. Le temps brut ne dit rien :
le socle est dans chaque ligne, et il porte déjà un écart.

```bash
build/bin/mesurer-coeur --dsp 600000
```

### Ce que ça donne

600 000 échantillons par cas, 112 pas par programme, ns par pas de DSP :

| caractéristique | `mame` | `maison` | écart | **propre `mame`** | **propre `maison`** | **écart propre** |
|---|---|---|---|---|---|---|
| **socle** (112 pas qui ne font rien) | **2,722** | **3,242** | **+0,520** | — | — | — |
| IRA → MEMS | 2,642 | 3,238 | +0,596 | −0,079 | −0,004 | +0,075 |
| COEF | 3,021 | 3,332 | +0,311 | 0,299 | 0,090 | **−0,209** |
| le produit | 3,021 | 3,453 | +0,432 | 0,300 | 0,211 | −0,088 |
| TWT | 2,636 | 3,318 | +0,682 | −0,086 | 0,076 | +0,162 |
| IWT | 3,196 | 3,522 | +0,326 | 0,474 | 0,280 | **−0,194** |
| **MRD** | 4,141 | 5,384 | +1,242 | 1,420 | 2,142 | **+0,722** |
| MRD + UNPACK | 4,351 | 5,704 | +1,353 | 1,630 | 2,462 | **+0,832** |
| **MWT** | 4,223 | 5,476 | +1,253 | 1,501 | 2,234 | **+0,733** |
| MWT + PACK | 6,387 | 7,796 | +1,409 | 3,665 | 4,554 | **+0,888** |
| ADREB + NXADR | 4,136 | 5,019 | +0,883 | 1,414 | 1,777 | +0,363 |
| EWT | 2,856 | 3,515 | +0,659 | 0,135 | 0,273 | +0,139 |
| SHIFT 1 (écrête) | 3,124 | 3,150 | +0,026 | 0,402 | −0,092 | **−0,494** |
| SHIFT 3 (reboucle) | 2,827 | 3,881 | +1,055 | 0,105 | 0,640 | **+0,534** |
| FRCL | 3,678 | 3,599 | −0,078 | 0,956 | 0,357 | **−0,599** |
| ADRL | 3,687 | 3,734 | +0,046 | 0,966 | 0,492 | −0,474 |
| YRL | 3,608 | 3,597 | −0,011 | 0,886 | 0,355 | −0,532 |
| NEGB | 3,479 | 3,666 | +0,187 | 0,757 | 0,424 | −0,334 |
| les quatre rares ensemble | 3,800 | 3,667 | −0,133 | 1,078 | 0,425 | **−0,653** |
| | | | | | | |
| **la forme du jeu** — le témoin | **4,312** | **4,941** | **+0,629** | 1,590 | 1,699 | **+0,108** |

### Les trois choses que ça établit, et la première renverse ce qu'on croyait

1. **CE N'EST AUCUNE INSTRUCTION. C'EST LE SOCLE.** 112 pas qui ne font rien coûtent **+0,520 ns par pas**
   au cœur du projet, soit **+19 %** — et le témoin, le programme de la forme du jeu, a un **coût propre
   quasi nul entre les deux cœurs** (+0,108 ns, et −0,245 à un second passage : zéro à la précision de ce
   banc). Autrement dit : **une fois le socle retiré, les deux DSP exécutent le mélange d'instructions du
   jeu au même prix.** Tout le +19,8 % mesuré dans le jeu est le chemin inévitable d'un pas — la sélection
   de l'entrée, l'extension de signe, la lecture de TEMP, les deux `switch`, le shifter — et pas une
   instruction en particulier.

2. **LE CŒUR DU PROJET EST MEILLEUR SUR LA MOITIÉ DES CARACTÉRISTIQUES**, et il faut le dire aussi : COEF
   (−0,209), IWT (−0,194), le produit, `SHIFT 1` (−0,494), et **les quatre champs rares** (−0,33 à −0,65).
   Ces derniers s'expliquent : les poser fait basculer les deux cœurs vers leur second corps, et celui de
   `dsp_maison` est moins cher que `Run<USES_ALL>`. **C'est le seul endroit du dépôt où le coût de ces
   quatre-là se lit**, puisque le jeu ne les pose jamais (§5).

3. **LE SEUL POSTE OÙ IL EST VRAIMENT MOINS BON EST L'ACCÈS MÉMOIRE** : `MRD` **+0,72**, `MWT` **+0,73**,
   et l'empaquetage par-dessus (`UNPACK` +0,83, `PACK` +0,89), plus `SHIFT 3` (+0,53). Dans le jeu cela
   pèse 33 pas sur 112 (23 lectures, 10 écritures, §5), soit environ **+0,21 ns par pas** — à peu près
   **40 % de l'écart du socle**, et donc le second endroit où aller.

### Par pas ou par échantillon ? Tranché, et avec son contrôle

Un coût par échantillon — la remise à zéro d'EFREG, le branchement vers l'un des deux corps, ce que `Start`
laisse derrière lui — se divise par le nombre de pas et **se déguise en coût par pas**. Le banc mesure donc
le socle à trois longueurs de programme : la **pente** est le coût d'un pas, l'**ordonnée à l'origine** ce
que l'échantillon coûte avant son premier pas.

| | 16 pas | 48 pas | 112 pas | → ns par pas | → ns par échantillon |
|---|---|---|---|---|---|
| `mame` | 72,6 | 159,2 | 303,2 | **2,402** | 34,2 |
| `maison` | 86,3 | 188,2 | 365,0 | **2,903** | 39,8 |

**L'écart est par pas : +0,50 ns (+21 %)**, et il l'est aux deux passages (+0,50 et +0,65). Sur un programme
de 112 pas, la part par échantillon ne pèse que **±0,05 ns par pas, moins de 10 % de l'écart du socle** :
réparer hors de la boucle de pas ne rendrait presque rien.

**Et l'ordonnée à l'origine, elle, n'est PAS résolue — le banc le dit lui-même.** Les deux passages lui
donnent des signes opposés (−6,1 puis +5,7 ns) et le contrôle de linéarité a refusé la droite de `mame` au
second. C'est la même maladie que le témoin « 68000 » de §4 sexies : une différence de grands nombres. Le
contrôle est dans l'outil pour que personne ne lise ces deux nombres-là comme une mesure ; **seule la pente
est publiable.**

### Ce qu'il reste à faire, et dans cet ordre

1. **Lire l'assembleur engendré** des deux boucles de pas — `g++ -S -O2` sur `dsp_maison::boucle<false>`
   contre `SCSPDSP::Run<0>` — **sur le chemin inévitable seulement**, maintenant que le banc dit lequel.
   C'était inutile avant : on aurait comparé deux pages en cherchant quoi.
2. **Deux hypothèses structurelles sont déjà mortes**, et par inspection, pas par raisonnement : le cœur du
   projet a bien ses **deux corps** (`template <bool RARES> void boucle()`), et son pas décodé est **plus
   petit** que celui de MAME — 16 octets contre 28 (`dsp_maison::Pas` contre `SCSPDSP::Inst`). Ce n'est donc
   ni la spécialisation qui manque, ni l'empreinte du programme en cache.
3. **Puis l'accès mémoire**, le second poste, avec le même banc pour juger.

La barre est posée d'avance, comme pour les trois lots du §7 : **WAV identique à l'octet**, empreinte
`9A2310F8AF000E09EB18BD114852B003` inchangée.

### Puis l'assembleur, et il fallait d'abord resserrer la question

Lu le 2026-10-28, `g++ -S -O2` sur les deux boucles de pas — `dsp_maison::boucle<false>` contre
`SCSPDSP::Run<0>` — **et seulement apres que le banc ait dit que c'etait le socle**. Lu avant, on aurait
compare deux pages de cent quarante instructions en cherchant quoi.

**Cinq hypotheses sont mortes, et aucune par raisonnement :**

| hypothese | ce qui l'a tuee |
|---|---|
| il manque la specialisation des deux corps | `dsp_maison` les a : `template <bool RARES> void boucle()` |
| le pas decode est plus gros, donc plus de cache | il est **plus petit** : 16 octets contre 28 (`Pas` contre `Inst`) |
| un `switch` compile en table de saut, donc un branchement indirect | **zero** saut indirect des deux cotes |
| une division, ou une multiplication lente | aucune des deux n'en a |
| le produit 64 bits est une divergence de ce portage | **MAME ecrit la meme ligne** : `s64 const v = (s64(X) * s64(Y)) >> 12` |
| `std::clamp` compile en branchements d'un cote et en `cmov` de l'autre | les deux emploient `cmov` : 25 contre 22 |

**Et ce qui reste se compte.** La boucle de pas, epilogue exclu :

| | instructions | **branchements** | mouvements |
|---|---|---|---|
| `SCSPDSP::Run<0>` | 108 | **16** | 37 |
| `dsp_maison::boucle<false>` | **145** | **31** | 36 |

**+34 % d'instructions et presque DEUX FOIS les branchements, pour le meme algorithme.** Voila le cout, et
il est la : le chemin inevitable d'un pas est plus branchu chez le coeur du projet. Les `cmpb` le disent
(9 contre 0) — ce sont les comparaisons d'octets de la chaine `IRA` et des deux `switch`.

### La mesure par retrait, et ce qu'elle vaut

Pour confirmer, une experience **jetable** : enlever une piece du chemin inevitable, mesurer le socle,
remettre. Chaque variante rend le DSP faux, donc rien n'en est garde — seul le chiffre l'est. Et chaque
variante est **encadree par une mesure de la reference**, parce qu'un premier essai a vu l'ecart de
reference bouger de +0,41 a +0,98 selon le moment : c'est pour ca que `--socle` existe, assez rapide pour
entrelacer.

600 000 echantillons, sept passages par mesure. « referme » est la part de l'ecart de reference qui
disparait :

| ce qu'on enleve | ecart | referme |
|---|---|---|
| la chaine d'entree `IRA` reduite a MEMS | +0,831 | −14 % |
| l'extension de signe de TEMP | +1,090 | −101 % |
| **le `switch` YSEL reduit a son cas 0** | **−0,014** | **103 %** |
| **le `switch` SHIFT reduit a son cas 0** | **−0,038** | **106 %** |
| le produit 64 bits ramene a 32 | +0,482 | 17 % |

Les deux `switch` du chemin inevitable **referment l'ecart a eux seuls**, chacun entierement — ce qui est
coherent avec les 31 branchements.

**MAIS IL FAUT DIRE CE QUE CETTE EXPERIENCE NE PEUT PAS FAIRE, et c'est important :** un retrait n'enleve
pas que du travail, il **expose de l'invariant**. Dans le socle, `YSEL` vaut zero a tous les pas, donc `y`
est `m_frc_reg`, donc une constante de boucle : en retirant le `switch`, on autorise le compilateur a la
hisser hors de la boucle, et peut-etre a simplifier le produit avec elle. Le retrait **situe la region, il
ne la chiffre pas** — « 103 % » n'est pas le cout du `switch`, c'est le cout du `switch` plus ce que son
absence permet. Les deux colonnes se lisent ensemble ou pas du tout.

### Ce qu'il reste a faire, et c'est maintenant precis

**La direction est nommee : resoudre a la DECODE ce qui se rebranche a chaque pas de chaque echantillon.**
`Decode` construit deja le `Pas` une fois par ecriture de programme ; la chaine `IRA` (trois comparaisons
d'octets et un `return`) et les deux `switch` a quatre cas s'y resoudraient une fois pour toutes — un
pointeur ou un index pour la source d'entree, un code deja reduit pour le decalage. 44 100 fois par seconde
par puce, c'est la que vont les branchements.

Et la barre ne change pas : **WAV identique a l'octet**, empreinte `9A2310F8AF000E09EB18BD114852B003`.

**Ce qui reste hors de portee**, et il vaut mieux l'ecrire : pourquoi 145 instructions au lieu de 108 pour
le meme algorithme ecrit de la meme facon, un compteur materiel le dirait en un passage — instructions
retirees, branchements mal predits, cycles. Ce poste n'en a pas. La question est maintenant assez etroite
pour qu'un profileur y reponde d'un coup, ce qui n'etait pas vrai ce matin.

### Ce que ce banc ne peut pas dire

Un programme où les 112 pas font la même chose n'a **ni les défauts de cache ni les dépendances entre pas**
du vrai programme. Il **situe** un coût, il ne le prédit pas — et le témoin le montre : son écart total
(+0,629 ns par pas, soit +15 %) est en dessous du **+19,8 %** mesuré dans le jeu (§4 sexies). Le banc
sous-estime. C'est pour cela que le témoin est dans la sortie : si son écart n'était pas du même ordre que
ceux de la table, c'est le banc qu'il faudrait croire le moins.


## 5. Ce que le programme DSP contient vraiment

Mesuré (`dspprog`, imprimé par `SCSPDSP::Start` sous `VF3_SND_PROF=1`), sur les 112 pas du programme de
Fighting Vipers 2 :

| contrôle | pas qui le posent |
|---|---|
| `IWT` (écriture MEMS) | 23 |
| `TWT` (écriture TEMP) | 39 |
| `FRCL` | **0** |
| `ADRL` | **0** |
| `YRL` | **0** |
| `NEGB` | **0** |
| `MRD` / `MWT` | 23 / 10 |
| accès mémoire (MRD ou MWT) | 33 — **tous sur un pas impair** |
| `EWT` (sortie EFREG) | 5 |
| `ZERO` / `BSEL` / `XSEL` | 36 / 39 / 37 |
| `YSEL == COEF` | 87 |
| `IRA` dans MEMS | 108 |

Deux choses se lisent là-dedans :

1. **Quatre contrôles ne sont jamais posés** — `FRCL`, `ADRL`, `YRL`, `NEGB`. Les tester à chaque pas de chaque
   échantillon, c'est quatre branches par pas pour rien, et ça garde en vie `FRC_REG`, `Y_REG` et `ADRS_REG`
   qui ne peuvent alors valoir que zéro.
2. **Tous les accès mémoire tombent déjà sur un pas impair.** Le `if (MRD && (step & 1))` de MAME, qu'on
   pourrait croire à optimiser en portant le test sur le bloc entier, ne ferait rien gagner ici : il n'y a aucun
   pas pair à éviter. *Mesuré avant d'y toucher — c'est précisément pour ça qu'on mesure.*


## 6. Chemins chauds, et ce que la mesure dit de chacun

| chemin | part | ce que la mesure dit |
|---|---|---|
| `SCSPDSP::Run` (ex-`Step`), 112 pas × 44,1 kHz × 2 chips | **62-69 %** | la cible ; ~16,3 cycles TSC par pas au départ |
| le 68000 de MAME, ses appels de bus virtuels (`m68000_bus::read/write/peek`) | 23 % | un appel indirect par accès mémoire ; `third_party`, à ne pas toucher sans LTO ou une interface non virtuelle |
| `UpdateSlot` × 32 voix | 6-13 % | monte avec le nombre de voix actives ; tableaux de pointeurs vers des locales (`addr[2]`, `slot_addr[2]`) et boucle de 2 tours sur `LPCTL` |
| le tour de boucle et le balayage des 10 timers | ~1,2 % | ne vaut pas une ligne |
| le mélange stéréo et le limiteur | ~1 % | ne vaut pas une ligne |
| `std::function` de l'hôte (`sample_index`, `time_is_zero`) à chaque accès registre | dans les 23 % | appel indirect, mesurable seulement en le retirant |
| `getenv` dans `midi_in_byte` | négligeable (quelques octets par image) | même motif que le `getenv` par lecture de CROM déjà corrigé ailleurs ; à nettoyer par propreté, pas pour la vitesse |

Rien de ce qui précède n'alloue dans le chemin temps réel, sauf `m_out.push_back` (deux flottants par
échantillon, capacité amortie) et `out.push_back` du mélange — les deux vecteurs gardent leur capacité d'une
image sur l'autre.


## 7. Ce qui a été changé, et ce que ça a donné

### Lot 1 — le corps du DSP spécialisé sur ce que le programme utilise

`SCSPDSP::Step` devient `Run<U>`, un patron sur le jeu de contrôles que le programme touche réellement
(`SCSPDSP::uses`, calculé par `Start` sur tout le programme et seulement **augmenté** par `Decode` quand un pas
en gagne un — un bit posé pour rien ne coûte que le corps lent). Pour un programme qui ne pose ni `FRCL`, ni
`ADRL`, ni `YRL`, ni `NEGB` — celui de Fighting Vipers 2 et celui de Virtua Fighter 3 — le corps compilé n'a
plus ces quatre tests, et `FRC_REG`, `Y_REG`, `ADRS_REG` se replient sur zéro. Au passage, le mot de `TEMP` que
`B` et `X` lisaient tous les deux est lu et étendu **une** fois.

### Lot 2 — les copies locales, contre l'aliasing de la RAM son

Un pas qui écrit dans la ligne de retard écrit à travers `ram`, et rien ne dit au compilateur que la RAM son
n'est pas **dans l'objet DSP lui-même** : sans copies, `DEC`, `RBL`, `RBP` et `LastStep` doivent être relus
après chaque écriture, et chaque mot de `TEMP` aussi. Le lot en prend des copies locales et passe par un
pointeur `__restrict` — une promesse vraie : la RAM son est celle du 68000, une allocation séparée.

### Lot 3 — les contrôles d'un bit groupés dans un mot

Un pas allait chercher **huit octets séparés pour tester huit bits**. `Inst::f` les met dans un `u16`, avec
`SHIFT` et `YSEL` en prime ; `Inst::mf` fait de même pour les quatre lus seulement dans le bloc mémoire. Le
choix de `Y` teste d'abord le cas `COEF`, celui de 87 pas sur 112. Les champs nommés d'origine restent dans
`Inst` : `Decode` et le rapport `dspprog` les lisent, et ils disent ce qu'ils sont.

### Ce que les trois lots donnent

Même scène, même machine, sondes éteintes. Le détail de chaque lot est dans
`docs/scsp_benchmark_baseline.md`.

| images | base | lot 1 | lot 2 | lot 3 |
|---|---|---|---|---|
| 0-500 | 1,60 | 1,41 | 1,38 | 1,40 |
| 500-1000 | 1,75 | 1,53 | 1,50 | 1,27 |
| 1000-1500 | 1,83 | 1,62 | 1,56 | 1,39 |
| 1500-2000 | 1,83 | 1,66 | 1,59 | 1,42 |
| 2000-2500 | 1,85 | 1,65 | 1,60 | 1,37 |
| 2500-3000 | 1,89 | 1,68 | 1,65 | 1,38 |
| 3000-3500 | 2,03 | 1,81 | 1,76 | 1,51 |
| 3500-4000 | 2,02 | 1,75 | 1,69 | 1,46 |
| **moyenne (ms)** | **1,85** | **1,64** | **1,59** | **1,40** |
| contre la base | — | −11,4 % | −14,1 % | **−24,3 %** |
| cycles / pas de DSP | 16,3 | 13,2 | 13,1 | **10,0** |
| part du DSP dans la carte son | 62-69 % | 57-66 % | 57-66 % | **52-60 %** |

La part de l'image passe de **10,6 % à 8,0 %**, et le DSP de deux tiers de la carte son à un peu plus de la
moitié.

**Le son n'a pas bougé d'un échantillon**, et c'est la condition pour garder les lots, pas un bonus. Vérifié
contre une construction du commit de référence `335c050`, sur les **deux** jeux que le portage sert :

| jeu | scène | images | WAV | verdict |
|---|---|---|---|---|
| `fvipers2` | `fv2-fight.txt` | 4200 | 12 879 504 octets | **identique à l'octet** |
| `vf3` | `vf3-match.txt` | 2400 | 6 841 492 octets | **identique à l'octet** |

### Un quatrième lot, qui ne gagnait rien, et qui a été défait

La boucle des deux adresses de `UpdateSlot` déroulée (MAME y promène `(addr1, cur_addr)` puis
`(addr2, nxt_addr)` à travers deux tableaux de pointeurs vers des locales). Bit pour bit identique, code plus
clair — et **moyenne 1,399 ms contre 1,400 ms**, soit zéro. Défait : une modification dans un fichier de
fidélité qui n'achète rien ne se garde pas. Le résultat négatif, lui, est consigné
(`docs/scsp_benchmark_baseline.md`) : les voix ne se gagnent pas là.


## 8. Ce qui n'a pas été fait, et pourquoi

- **Le 68000** — 23 % de la carte son au départ, **28 à 31 % après les lots**, puisque c'est le DSP qui a
  maigri — est dans `third_party` et ses gestionnaires sont ceux de MAME, inchangés par contrat. Son coût
  propre tient aux appels virtuels de bus, un par accès mémoire : on ne les retire pas sans changer l'interface
  du cœur ou activer LTO, ce qui dépasse une modification petite et réversible. C'est désormais le plus gros
  poste après le DSP.
- **SIMD** : la boucle du DSP est une chaîne de dépendances (`ACC` se nourrit de lui-même par `BSEL`), elle ne
  se vectorise pas.
- **Le remplacement du SCSP de MAME** par une implémentation autonome : le code n'a pas été entamé, mais
  **les deux préalables existent désormais**. Le harnais différentiel : `tools/son/comparer-son.cmd`. Et le
  contrat, en deux morceaux qui ne disent pas la même chose :
  - `docs/scsp_architecture.md` — **l'interface C++** entre `SoundBoard` et le chip, et ce qu'elle traîne
    de MAME ;
  - `src/sound/scsp_contract.h` — **le contrat matériel** : ce que le pilote attend de la puce, champ par
    champ, avec la provenance de chaque nombre et les écarts assumés marqués comme tels. Écrit le
    2026-10-25, et **vérifié** par `tools/son/test_scsp_contract.cpp` contre `scsp.cpp` et contre la ROM.
- ~~**Les mesures sous `--rec`**~~ : **faites le 2026-10-25**, voir §4 bis. La part du son passe de 9,6 % à
  **30,5 %**, la carte étant 4,3 fois plus rapide et le son inchangé. **Et dans l'hôte, mesuré le
  2026-10-26** (§4 ter) : le fil du son travaille **1,49 ms par image** et **le jeu ne l'attend jamais** —
  0,000 ms, maximum 0,016 jamais dépassé. Soit **8,5 % d'un cœur et 0,00 % de l'image**.
