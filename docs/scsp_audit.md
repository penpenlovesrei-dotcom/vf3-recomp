# Audit du sous-système sonore — ce qu'il y a, ce qu'il coûte

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
- **Le remplacement du SCSP de MAME** par une implémentation autonome : rien n'a été entamé. Le contrat
  d'interface et le harnais différentiel sont les préalables ; le harnais existe maintenant
  (`tools/son/comparer-son.cmd`), l'interface non.
- **Les mesures sous `--rec`** et dans l'hôte : non faites. La part du son y est différente et ce document ne
  prétend pas la connaître.
