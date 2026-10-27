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


## 4 quinquies. Ce que le cœur du projet coûte, comparé à celui de MAME

Mesuré le 2026-10-27 par `tools/son/rejouer`, qui rejoue la **même trace** d'accès dans les deux cœurs : le
travail est identique a l'accès près, ce qui est la seule façon de les comparer honnêtement. Trois passages
chacun, sur la trace de la scène entière (3 219 865 échantillons, soit 73 secondes d'audio).

| étape | `maison` | ce qui a changé |
|---|---|---|
| première version | 47,80 s | le lecteur de champs cherchait **par comparaison de chaînes** dans la table du contrat, à chaque champ, chaque fente, chaque échantillon |
| positions résolues une fois | 22,93 s | la table du contrat reste la source unique, lue **une fois** à la construction |
| mot d'indicateurs | 22,48 s | les champs d'un bit d'un pas de DSP tiennent dans un entier lu une fois, au lieu de quinze booléens |
| corps spécialisé | **21,88 s** | les quatre contrôles que ce jeu n'emploie jamais sortent des 112 pas par échantillon |
| | **20,80 s** | le cœur de MAME, pour comparaison |

**Le chiffre qui compte, et ce n'est pas le plus flatteur.** Le temps absolu est dominé par l'analyse de la
trace — 81 Mo de texte, analysés deux fois dans un passage à deux cœurs — et elle est identique dans les
deux. L'écart **par cœur** est donc de 0,54 s pour 73 secondes d'audio, soit **0,74 % d'un cœur par puce**.
Sur les 8,5 % que le son coûte au bureau (§4 ter), le cœur du projet ferait passer le son à environ 10 % :
**le gain processeur va dans le mauvais sens**, et c'est la priorité absolue du cahier des charges.

Trois choses à en retenir plutôt qu'une :

1. **La justesse n'a jamais été payee par la vitesse.** La première version, 2,27 fois plus lente, était
   déjà identique à l'octet. Les quatre étapes ci-dessus n'ont rien changé au son — le rejoueur l'a
   vérifié après chacune.
2. **La source unique des positions de champs n'a pas été abandonnée pour aller plus vite.** Elle est lue
   une fois au lieu d'être cherchée un million de fois : un champ faux dans le contrat casse toujours les
   tests.
3. **Ce chiffre n'est pas celui de la carte.** Il ne pourra l'être que quand le cœur y sera branché, et
   `VF3_SND_PROF` le mesurera alors à l'intérieur, part par part. En attendant, 0,74 % par puce est une
   borne mesurée sur un banc, pas la part du son dans une image.

Et une taille, qui n'est pas une mesure de performance mais qui se dit : 1 759 lignes pour le cœur du projet
(`scsp_maison` et `dsp_maison`) contre 2 540 pour celui de MAME (`scsp` et `scspdsp`), à comportement
identique.


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
