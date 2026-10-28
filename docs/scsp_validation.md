# Valider le son — ce qui est en place, et ce qui manque

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [docs/son.md](son.md).

État au 2026-10-24. La partie « en place » a été exécutée et vérifiée ; la partie « manque » est une liste de
travaux, pas une promesse tenue.


## 1. La règle

**Pour une optimisation, l'identité exacte est la seule réponse acceptable.** Une optimisation qui change le
son n'est pas une optimisation, c'est une modification du portage, et elle se juge alors sur la fidélité et pas
sur la vitesse. Il n'y a pas de tolérance à élargir : si le WAV bouge d'un échantillon, on cherche pourquoi.

Les mesures plus souples (erreur max, RMS, corrélation, spectre, dérive de tempo, transitoires) ont leur place
ailleurs — quand on compare deux *implémentations* dont on n'attend pas l'identité, ou le portage à une autre
émulation. Elles ne remplacent jamais la comparaison stricte quand celle-ci s'applique.


## 2. Ce qui est en place, et ce que ça couvre

### Le son, octet par octet, contre un commit de référence

`tools/son/comparer-son.cmd <jeu> <script> <images> [commit]`

Construit (une fois) un arbre de travail du commit de référence dans `C:\Temp\fv2-son-ref`, fait jouer la même
scène aux deux `vf3run`, et compare les deux WAV avec `fc /b`.

Le 2026-10-24, chacun des quatre lots a été comparé à l'octet (`cmp`) contre un WAV de référence produit par la
construction d'avant les lots, sur 4200 images de `tools/scripts/fv2-fight.txt` sur `fvipers2.zip`
(12 879 504 octets à chaque fois) : **identique**, aux quatre lots. Puis le script lui-même a été lancé, contre
une construction du commit `335c050`, sur les **deux** jeux que le portage sert :
`fvipers2`/`fv2-fight.txt`/4200 images et `vf3`/`vf3-match.txt`/2400 images — **identique à l'octet** aux deux.

Ce que ça couvre, et c'est beaucoup : tout le chemin réel — 68000, pilote de son du jeu, 32 voix × 2 chips,
enveloppes, LFO, modulation, DSP, timers, interruptions, les deux SCSP et le mélange — sur une séquence que le
jeu lui-même produit. Un seul échantillon de travers et ça se voit.

Ce que ça ne couvre pas : les cas limites que *ce jeu, cette fois* ne traverse pas. Un bouclage à l'envers, un
`EGHOLD`, un DMA, une valeur réservée : si le pilote de son de Fighting Vipers 2 ne les demande jamais, le
harnais ne les regarde pas.

### Les deux corps du DSP, l'un contre l'autre et contre une référence

`make test` → `build/bin/test_scspdsp` (`tools/son/test_scspdsp.cpp`). Pas de ROM, pas de RAM de jeu, pas de
fenêtre : les programmes sont engendrés depuis des graines fixes, donc deux lancements font la même chose.

Le test porte une **référence naïve** du DSP : elle décode les champs de bits du MPRO **dans** la boucle, comme
MAME `master` et Supermodel le font encore, sans table décodée, sans contrôles groupés, sans patron et sans
copies locales de `DEC`/`RBL`/`RBP`. Elle teste donc exactement la machinerie que ce portage a ajoutée. Les
deux côtés reçoivent le même programme, les mêmes tables, la même RAM, les mêmes `MIXS` à chaque échantillon,
et après **chacun** des 64 échantillons on compare `EFREG`, `TEMP`, `MEMS`, `DEC` et les **524 288 mots** de la
ligne de retard.

Les seize cas, et ce que chacun attrape :

| cas | ce qu'il exerce |
|---|---|
| deux programmes sans aucun des quatre | `Run<0>`, le corps que les deux jeux prennent |
| le même, `uses` forcé à `USES_ALL` | **les deux corps doivent s'accorder sur le même programme** |
| les quatre ensemble, 48 puis 112 pas | `Run<USES_ALL>` sur ce que personne ne jouait |
| `FRCL`, `ADRL`, `YRL`, `NEGB` seuls | chacun isolé, et `uses` vaut exactement son bit |
| `RBL` 16k/64k, `RBP` 1/3 | l'adressage de la ligne de retard hors du cas par défaut |
| `IRA` hors plage au pas 20, puis au pas 0 | l'abandon du pas entier : ni `DEC` ni `MIXS` ne bougent |
| « les quatre mordent » | garde-fou : les quatre contrôles **changent** la RAM de retard (296 mots), donc le générateur les a bien posés et le cas n'est pas secrètement celui d'à côté |
| `Decode` ajoute un bit | un pas réécrit sans `Start` : `uses` gagne le bit et le corps générique prend le relais |
| `Start` recalcule `uses` | sur un objet réutilisé, `uses` **redescend** de 15 à 0 — la seule affirmation de `scspdsp.h` que rien ne tenait |

**Et le test a montré qu'il sait échouer**, ce qui est la seule chose qui rende un test vert intéressant. Quatre
fautes injectées puis défaites :

| faute injectée | cas en échec |
|---|---|
| `NEGB` ne nie plus, dans le corps générique | 5 |
| la boucle lit `TWT` à la place de `XSEL` | 12 |
| `SHIFT` lu avec un décalage de trop | 13 |
| `Start` ne remet pas `uses` à zéro | 1 (celui écrit pour ça) |

Une cinquième, l'échange de deux valeurs de l'énumération `F_XSEL`/`F_TWT`, **n'a rien fait échouer** — et à
raison : échanger deux étiquettes change l'écriture et la lecture ensemble, c'est un renommage, pas une faute.
Noté parce qu'une mutation qui ne mute rien n'est pas une preuve que le test est aveugle.

Ce que ce test ne dit pas : il ne redérive pas le comportement du chip depuis une documentation matérielle, et
`PACK`/`UNPACK` y sont **transcrits** plutôt que re-raisonnés (ce portage n'y touche pas). Un `IDENTIQUE` ici
veut dire « le portage calcule toujours ce que la boucle simple calcule », qui est la question posée, et pas
« le matériel fait ça ».

### Le reste de la carte, déjà en place avant ce travail

| | ce que ça compare |
|---|---|
| `verifier-vf3.cmd` | l'état du PowerPC image par image et les 8 Mo de RAM, cette branche contre `main`, sur Virtua Fighter 3 — le contrat de non-régression du portage |
| `tools/statecmp.py` | deux fichiers d'état (celui de Supermodel instrumenté et celui de `vf3run --state`) |
| `build/bin/vf3lock` | l'interpréteur contre le code recompilé |
| `VF3_SCSP_WLOG` | chaque écriture de registre SCSP, image par image — une trace d'accès déjà exploitable |
| `VF3_KEYON_LOG` | chaque voix démarrée, avec sa forme (TL, AR/D1R/D2R/RR, DL, EGHOLD, LPCTL, LSA/LEA/SA) et l'état du 68000 |
| `VF3_SCSP_SOLO=0\|1` | un seul des deux SCSP à la fois |
| `VF3_DUMP_SOUND_RAM` | la RAM son et les registres des deux chips, à des images données |
| `m_probe_*` / `energy_on` | l'énergie que chaque fente a envoyée, directe et vers le DSP, et l'énergie des sorties du DSP |
| `VF3_SCSP_MADRS_SEPARATE` / `_MIRROR`, `VF3_SCSP_MSLC_LATCH` | forcer un choix de fidélité dans un sens ou l'autre, pour comparer |

Ce dépôt est donc **bien outillé pour l'enquête** et peu outillé pour le test automatique — `make test` est le
premier, et il ne couvre qu'un bloc sur huit. C'est l'inverse de ce qu'on voudrait pour une réécriture, et
c'est le point à corriger avant d'en entamer une.


## 3. Ce qui manque

### a. Les scènes

Il n'y a **qu'une scène** pour le son : `fv2-fight.txt`. Il manque, et le cahier des charges les demande :

| scène | ce qu'il faudrait | pourquoi ça manque |
|---|---|---|
| silence | le jeu à un moment où rien ne joue | aucun moment de la scène actuelle n'est muet ; il faudrait le trouver ou l'imposer |
| musique seule | `VF3_VOLUMES=100,0` | **faisable tout de suite**, pas encore fait |
| effets seuls | `VF3_VOLUMES=0,100` | **faisable tout de suite**, pas encore fait ; attention, ces deux-là allument le niveleur d'effets (`m_group_gains`), donc ils ne mesurent pas le mélange d'origine |
| sound test | le menu de test de la borne | se traverse à la manette ; `console_play` n'a pas de levier pour y aller sans main humaine |
| Virtua Fighter 3, le temps | `vf3-match.txt` avec `VF3_PROF=1` | la scène existe et sert déjà à la non-régression ; son **temps** n'a pas été relevé |

### b. Les tests par bloc

**Un sur huit**, le DSP (`make test`, décrit plus haut). Les sept autres n'existent pas, et rien n'exerce le
SCSP lui-même en dehors du jeu. La liste de ce qu'il faudrait, par ordre de dépendance (le découpage est celui
de `docs/scsp_architecture.md` §3) :

1. **registres** — écrire chaque mot de 0x000 à 0xFFF et le relire ; les bits réservés ; la plage 0x7C0-0x7FF,
   où ce portage et MAME ne font pas la même chose (`docs/scsp_references.md`) ;
2. **PCM** — 8 et 16 bits, les quatre `LPCTL`, `SA`/`LSA`/`LEA`, l'interpolation, les bords de la RAM
   (`read_byte`/`read_word` masquent à 0xFFFFF et renvoient 0 au-delà : à exercer) ;
3. **enveloppes** — les quatre états et leurs transitions, `EGHOLD`, `KRS`, `LPSLNK`, le key-off en pleine
   attaque, les rates 0 et 1 (« infinity » dans les tables) ;
4. **pitch, LFO, modulation** — `OCT`/`FNS` aux extrêmes, les quatre formes, et la modulation par anneau avec
   la profondeur de ce portage ;
5. **DSP** — ✔ **fait** (`make test`) : les deux corps, les quatre contrôles que ces jeux ne posent jamais,
   l'adressage de la ligne de retard, l'abandon du pas, `uses` ajouté par `Decode` et recalculé par `Start`.
   Reste dehors : `PACK`/`UNPACK` sur toute la plage 24 bits pour eux-mêmes (le test les transcrit, donc une
   faute partagée passerait), et la saturation du shifter aux bords exacts plutôt qu'au hasard des graines ;
6. **timers et interruptions** — A/B/C et leurs périodes, `SCIEB`/`SCIPD`/`SCILV`, le décodage de priorité, le
   DMA, et l'ordre relatif des accès du 68000 ;
7. **les deux chips et le mélange** — `DISDL`/`EFSDL`, le panoramique, `MVOL`, `DAC18B` ;
8. **reset et répétabilité** — deux runs identiques donnent le même PCM (c'est déjà vrai de fait : les WAV de
   deux runs se comparent à l'octet, et la seule source aléatoire, `machine_rand`, a une graine fixe).

### c. Le harnais à deux cœurs

`comparer-son.cmd` compare deux **constructions**. Pour une réécriture, il faut comparer deux **cœurs dans le
même binaire** : la même séquence d'accès rejouée dans les deux, et la comparaison des PCM, des états
observables, des interruptions et des timers, pas seulement du WAV final.

**Fait le 2026-10-26** : `tools/son/rejouer` et `tools/son/rejouer.cmd`, sur le format de
`src/sound/scsp_trace.h` et le sélecteur de `src/sound/scsp_select.h`. `VF3_SCSP_WLOG` n'a pas servi de
départ : il note l'image et non l'échantillon, et laisse de côté la puce 1, les lectures, le MIDI entrant,
les timers et les écritures de RAM. Le rejeu retrouve le PCM de la carte **à l'octet** sur 229 991
échantillons, ce qui établit que la trace est complète — le contrôle à passer avant que comparer deux cœurs
veuille dire quelque chose.

**Et il faut dire jusqu'où, parce que ce n'était pas jusqu'au bout.** Ces 229 991 échantillons sont
**300 images**, la valeur par défaut de `rejouer.cmd`. Lancé le 2026-10-28 sur la scène entière — 4 200
images, 3 219 865 échantillons par puce — **ce contrôle échouait** :

| puce | première divergence | ce que le rejeu rendait | ce que la carte avait produit |
|---|---|---|---|
| 0 | échantillon **772 067** gauche (image ~1 007) | 0 | −4 |
| 1 | échantillon **1 149 006** droite (image ~1 499) | −992 | −976 |

**Trouvé et refermé le même jour.** Ce qui suit est la mesure qui l'a nommé, gardée parce que la façon dont
cette divergence se cachait est le plus instructif de l'affaire.

### Ce que la mesure disait avant de savoir

1. **Ce n'était pas une divergence entre les deux cœurs.** Le second contrôle, `mame` contre `maison` sur
   la même trace de 4 200 images, disait **IDENTIQUES** sur les cinq flux. L'identité des cœurs tenait donc
   sur la scène entière ; c'était **la complétude de la trace** qui ne tenait que sur 300 images.
2. **L'étendue, et c'est elle qui a orienté.** Non pas un échantillon isolé : **84,7 % des valeurs après la
   première divergence**, jusqu'au bout, d'amplitude minuscule — écart médian 12 sur 131 072, maximum 156,
   et **tous multiples de 4** (la sortie 18 bits dont les deux bits bas tombent). Donc un état, pas un
   accident de calcul. Et la puce 1 était **identique à l'octet** sur les 1 050 premières images alors que
   la puce 0 divergeait : la cause dépendait de ce que le pilote écrivait, puce par puce.
3. **Trois pistes écartées par la mesure, pas par le raisonnement** : un chemin d'écriture de la RAM son qui
   manquerait à la trace (il n'y en a qu'un, `Bus` cas 0x0/0x2, et il appelle `ram_ecrite`) ; les gains par
   groupe (inertes, les deux volumes valant 1, et `m_group_gains` reste faux) ; la ROM d'échantillons
   (les SCSP ne jouent que depuis leur RAM, le 68000 y recopie — `soundboard.h`).

### La cause, et elle était écrite d'avance

`scsp_core.h` la nommait depuis le début, dans sa section **« AND WHAT IS DELIBERATELY ABSENT »** : quatre
choses de ce portage ne passent pas par l'interface, et la carte les posait **directement sur la classe
concrète**. Le miroir MADRS est la seule des quatre **qui change le son**, et il change **par jeu** : vrai
pour Virtua Fighter 3, **faux pour Fighting Vipers 2**. Il se posait dans `SoundBoard::reset`, par un
`dynamic_cast`, **après** la création de l'enregistreur — donc la trace ne pouvait pas le voir, et un rejeu
le laissait à son défaut, `true`.

**Le contrôle décisif n'a demandé aucun code** : la même scène réenregistrée avec `VF3_SCSP_MADRS_MIRROR=1`,
qui met la carte dans le réglage que le rejeu supposait, se rejouait **identique à l'octet sur les deux
puces**. C'est ce qui a nommé le coupable au lieu de le soupçonner.

Et cela explique pourquoi 300 images passaient : la première écriture dans `0x7C0-0x7FF` arrive à
l'échantillon **29 211**, mais elle vaut **zéro** — or le miroir ne distingue les deux réglages que quand
une valeur non nulle y va et que le programme DSP s'en sert. Le contrôle ne mentait pas ; il était trop court.

### Ce qui a été refermé

| | |
|---|---|
| **une seule porte** | `scsp::regle_miroir_madrs` et `scsp::miroir_madrs` dans `scsp_select.h` — dont l'en-tête nommait déjà ce manque. Le `dynamic_cast` ne vit plus dans `soundboard.cpp` |
| **l'ordre** | le miroir se pose **avant** que l'enregistreur existe, et l'enregistreur le **relit de la puce** au lieu qu'on pense à le lui dire |
| **la trace le porte** | en-tête **version 2** : `# scsp-trace 2 puce=N horloge=H ram=B miroir=0|1`, et le rejeu le pose avant la première ligne |
| **une trace v1 est refusée** | avec un message qui dit quoi faire, et non devinée — supposer est précisément ce qui a coûté cette divergence |

**Vérifié le 2026-10-28, scène entière**, et les trois contrôles comptent ensemble :

| | |
|---|---|
| `tools/son/rejouer.cmd fvipers2 tools/scripts/fv2-fight.txt 4200 mame maison` | **TOUT CONCORDE** — les quatre contrôles, 3 219 865 échantillons par puce, les deux puces |
| `make test` | les dix tests, **tous les cas passent**, les deux cœurs là où le test prend un nom de cœur |
| `vf3run --frames 4200 --rec --wav`, chaque cœur | même empreinte **`9A2310F8AF000E09EB18BD114852B003`** — le correctif ne change pas une valeur du son |

La complétude de la trace est donc établie sur la scène entière, et plus seulement sur 300 images.

**La leçon, et elle est générale :** *un état qui n'arrive pas par un appel n'entre pas dans une trace
d'appels.* Les trois autres extras du portage ne changent pas le son au réglage par défaut, et c'est la
seule raison pour laquelle ils ne sont pas dans l'en-tête ; si l'un d'eux se mettait à compter, il faudrait
l'y mettre aussi.


## 4. Le trou que ce travail a creusé, et qui est bouché

Le lot 1 du 2026-10-24 a coupé le corps du DSP en deux chemins (`docs/scsp_audit.md` §7). Pour les deux jeux de
ce portage, c'est le chemin maigre qui tourne, et il est vérifié à l'octet ; l'autre n'était vérifié par rien,
et c'était le premier test à écrire si ce portage servait un jour un troisième jeu.

**Il est écrit** (`tools/son/test_scspdsp.cpp`, §2) : `Run<USES_ALL>` est exercé par onze des seize cas, sur des
programmes qui posent les quatre contrôles ensemble puis chacun seul, et sur le programme même des jeux avec
`uses` forcé — ce dernier cas étant celui qui dit que **les deux corps s'accordent**. Quatre fautes injectées
dans le corps générique ont bien fait tomber le test.

Ce qui reste du trou, et il est petit : le test compare le portage à une transcription de la même sémantique,
pas à une documentation matérielle. Il attraperait un `if constexpr` mal câblé, un bit de `Inst::f` mal lu, un
`uses` périmé, une copie locale décalée — c'est-à-dire tout ce que ce lot pouvait casser. Il n'attraperait pas
une erreur que MAME aurait déjà, ni une faute recopiée à l'identique dans la référence.
