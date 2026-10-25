# Le banc du son — la ligne de base, et chaque lot mesuré contre elle

Mesuré le 2026-10-24. Machine de Frédéric (Windows 11, g++ 16.1.0 MSYS2, `-O2 -g`), branche `fv2`, ligne de
base au commit `335c050`. **Un seul banc à la fois** : deux runs lancés ensemble se disputent les cœurs et les
chiffres ne veulent plus rien dire.


## Comment on refait ces chiffres

```bash
# construire
./construire.cmd                       # ou : make CXX=g++ build/bin/vf3run -j8

# le temps de chaque partie d'une image
VF3_PROF=1 build/bin/vf3run.exe "$ROMS/fvipers2.zip" \
    --script tools/scripts/fv2-fight.txt --frames 4200 --wav son.wav --quiet

# et, dedans, ou va le temps de la carte son
VF3_PROF=1 VF3_SND_PROF=1 build/bin/vf3run.exe ... --wav son.wav --quiet

# et : le son n'a pas bouge (contre un commit de reference, octet par octet)
tools\son\comparer-son.cmd fvipers2 tools\scripts\fv2-fight.txt 4200
```

`$ROMS` = `C:\Users\frede\Documents\SEGA Model 3\ROMs`. `VF3_PROF=1` imprime une ligne toutes les 500 images ;
`VF3_SND_PROF=1` ajoute la répartition dans la carte son (`src/sound/sound_prof.h`) et la composition du
programme DSP (`dspprog`, une ligne par écriture de programme).

**Les scènes.** Une seule a servi jusqu'ici, et il faut le dire :

| scène | fichier | ce qu'elle traverse |
|---|---|---|
| **Fighting Vipers 2, un round** | `tools/scripts/fv2-fight.txt` | démarrage (0-960), attraction et écran titre, pièce, sélection, chargement, intro, un round contre l'ordinateur avec jeu aléatoire (graine 20261019) |
| Virtua Fighter 3, un round | `tools/scripts/vf3-match.txt` | la même forme, sur l'autre jeu — utilisée pour la non-régression, **pas pour le temps** |

Ce qui manque et qui est demandé par le cahier des charges : **silence, musique seule, effets seuls, sound
test**. Le portage n'a pas de levier pour y aller sans main humaine (le menu test se traverse à la manette), et
`VF3_SCSP_SOLO` ne sépare que les deux chips, pas la musique des effets. Donc : tous les chiffres ci-dessous
sont ceux d'**une** scène, qui contient de la musique du début à la fin et des effets à partir de l'image
~1900. Rien n'y mesure le cas « rien ne joue ».

**Le limiteur de l'instrumentation.** Les portées de `sound_prof.h` coûtent deux `rdtsc` chacune. Vérifié sur
la scène entière : sondes éteintes contre sondes allumées, le temps de la carte son bouge de moins de 1 %, et
le WAV sort **identique à l'octet**. Le rapport imprime lui-même ce que ses `rdtsc` pèsent (~58 kcycles par
image). Les chiffres de temps ci-dessous sont tous pris **sondes éteintes**.


## La ligne de base, `335c050`

Budget d'une image à 57,52 Hz : **17,4 ms**.

| images | carte Model 3 | carte son | part de l'image |
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

Répartition dans la carte son, et le coût d'un pas de DSP (cycles TSC — pas des cycles de cœur : le TSC tourne
à la fréquence nominale, le cœur monte plus haut, donc ces nombres servent à **comparer entre eux**, pas à
convertir en nanosecondes) :

| | boucle+timers | 68000 | voix | DSP | mélange | cycles / pas de DSP |
|---|---|---|---|---|---|---|
| ligne de base | 1,1-1,4 % | 22,6-24,2 % | 5,7-13,3 % | **62,0-69,0 %** | 0,5-1,1 % | **16,2-16,4** |


## Chaque lot, mesuré seul, sur la même scène et la même machine

Tous les lots touchent **uniquement** `src/sound/scspdsp.{h,cpp}`. Après chacun, le WAV de 4200 images
(12 879 504 octets) a été comparé à celui de la ligne de base : **identique à l'octet**, à chaque fois. Ce n'est pas un
bonus, c'est la condition pour garder le lot.

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
| contre la base | — | **−11,4 %** | **−14,1 %** | **−24,3 %** |
| apport du lot | — | −11,4 % | −3,1 % | −12,0 % |
| cycles / pas de DSP | 16,3 | 13,2 | 13,1 | **10,0** |
| part du DSP | 62-69 % | 57-66 % | 57-66 % | **52-60 %** |

### Lot 1 — le corps du DSP spécialisé sur ce que le programme utilise

`Step` devient `Run<U>`, un patron sur l'ensemble des contrôles que le programme touche vraiment. Mesuré :
le programme de Fighting Vipers 2 (112 pas, et 127 pour celui du combat) ne pose **jamais** `FRCL`, `ADRL`,
`YRL` ni `NEGB`. Sans eux, quatre branches par pas disparaissent et `FRC_REG`, `Y_REG`, `ADRS_REG` se replient
sur zéro, ce qui en emporte d'autres. Le mot de `TEMP` que `B` et `X` lisaient tous les deux est lu une fois.

### Lot 2 — les copies locales, contre l'aliasing de la RAM son

Un pas qui écrit dans la ligne de retard écrit à travers `ram`, et le compilateur n'a aucun moyen de savoir que
la RAM son n'est pas **dans l'objet DSP lui-même** : sans copies, `DEC`, `RBL`, `RBP` et `LastStep` doivent être
relus après chaque écriture, et chaque mot de `TEMP` aussi. Le lot prend une copie locale des quatre et passe
par un pointeur `__restrict` — une promesse vraie : la RAM son est celle du 68000, une allocation séparée.

### Lot 3 — les contrôles d'un bit groupés dans un mot

Un pas allait chercher **huit octets séparés pour tester huit bits**. `Inst::f` les met dans un `u16`, avec
`SHIFT` et `YSEL` en prime ; `Inst::mf` fait de même pour les quatre lus seulement dans le bloc mémoire (un
tiers des pas). Le choix de `Y` teste d'abord le cas `COEF`, qui est celui de 87 pas sur 112. Les champs
nommés d'origine restent dans `Inst` : `Decode` et le rapport les lisent, et ils disent ce qu'ils sont.


## Le lot qui n'a rien donné, et qui a été défait

**Lot 4 — la boucle des deux adresses de `UpdateSlot`, déroulée.** MAME y promène `(addr1, cur_addr)` puis
`(addr2, nxt_addr)` à travers deux tableaux de pointeurs vers des locales, ce qui garde `addr1` et `addr2` en
mémoire pour toute la fonction. Écrite à plat, avec `LPCTL`, `LSA` et `LEA` lus une fois, le code est plus clair
et le WAV reste identique à l'octet.

**Et il ne gagne rien :** moyenne **1,399 ms** contre **1,400 ms** sans lui — soit zéro, dans le bruit.

| images | lot 3 | lot 3 + lot 4 |
|---|---|---|
| 0-500 | 1,40 | 1,32 |
| 500-1000 | 1,27 | 1,26 |
| 1000-1500 | 1,39 | 1,35 |
| 1500-2000 | 1,42 | 1,40 |
| 2000-2500 | 1,37 | 1,37 |
| 2500-3000 | 1,38 | 1,45 |
| 3000-3500 | 1,51 | 1,52 |
| 3500-4000 | 1,46 | 1,52 |
| **moyenne** | **1,400** | **1,399** |

**Défait.** Une modification dans un fichier de fidélité qui n'achète rien de mesurable ne se garde pas : elle
écarte le fichier de MAME sans raison, et quelqu'un la relira un jour en croyant qu'elle servait. Le résultat
négatif, lui, se garde — il dit que les voix ne se gagnent pas là, et que le prochain qui voudra les accélérer
doit chercher ailleurs (les tables de panoramique de 256 Ko, l'enveloppe, le LFO).


## Virtua Fighter 3 : vérifié, pas chiffré

Le harnais a été lancé sur `vf3.zip` (`tools\son\comparer-son.cmd vf3 tools\scripts\vf3-match.txt 1200`, puis
2400 images) : **son identique à l'octet** dans les deux cas. Son programme DSP a aussi 0 pas qui posent
`FRCL`, `ADRL`, `YRL` ou `NEGB`, donc il prend le corps maigre lui aussi.

Le harnais imprime au passage le temps des deux constructions, et sur les 1000 premières images de VF3 il
donne 1,34 puis 2,03 ms pour la référence contre 1,08 puis 1,15 ms pour cette branche. **Ce n'est pas une
mesure retenue** : deux fenêtres de 500 images sur le démarrage, où le temps de la carte Model 3 varie lui
aussi de 6,77 à 5,20 ms entre les deux runs — la machine bougeait. L'ordre de grandeur va dans le même sens que
celui mesuré sur Fighting Vipers 2, et c'est tout ce qu'on peut en dire. **Le temps de VF3 reste à mesurer
proprement.**


## Ce que ces chiffres ne disent pas

1. **Rien sur le temps d'une image dans le jeu.** Dans l'hôte, la carte son tourne sur **un fil à elle**, une
   image en retard (`src/host/main.cpp`). Ses 0,45 ms économisées sont du temps processeur en moins — chaleur,
   batterie, cœurs laissés au rendu — **pas** une image qui arrive plus tôt. Dans `vf3run`, tout est sur un
   fil, et là le gain se lit directement.
2. ~~**Rien sur la version recompilée.**~~ **Mesuré le 2026-10-25** (`docs/scsp_audit.md` §4 bis), et la
   prévision de cette ligne était juste : avec `--rec` la carte passe de 13,77 à **3,23 ms** et le son reste
   à **1,42**, donc sa part va de 9,6 % à **30,5 %**. Les pourcentages de ce document ne s'y transposent
   effectivement pas. Le partage interne au son, lui, est identique dans les deux modes, et les deux WAV
   sont identiques à l'octet.
3. **Rien sur une autre machine.** Un seul poste, un seul compilateur, un seul niveau d'optimisation.
4. **Rien sur les scènes qui manquent** (silence, musique seule, effets seuls, sound test) — voir plus haut.
5. **Rien sur la fidélité au matériel.** Le WAV identique à l'octet dit que *ce portage n'a pas changé*, ce qui
   est exactement ce qu'on demande à une optimisation. Il ne dit rien sur ce que fait un YMF292-F réel.
