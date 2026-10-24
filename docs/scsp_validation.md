# Valider le son — ce qui est en place, et ce qui manque

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

Ce dépôt est donc **bien outillé pour l'enquête** et peu outillé pour le test automatique. C'est l'inverse de
ce qu'on voudrait pour une réécriture, et c'est le point à corriger avant d'en entamer une.


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

Aucun. Il n'y a pas de cadre de test unitaire dans le dépôt, et rien n'exerce le SCSP en dehors du jeu. La
liste de ce qu'il faudrait, par ordre de dépendance (le découpage est celui de `docs/scsp_architecture.md` §3) :

1. **registres** — écrire chaque mot de 0x000 à 0xFFF et le relire ; les bits réservés ; la plage 0x7C0-0x7FF,
   où ce portage et MAME ne font pas la même chose (`docs/scsp_references.md`) ;
2. **PCM** — 8 et 16 bits, les quatre `LPCTL`, `SA`/`LSA`/`LEA`, l'interpolation, les bords de la RAM
   (`read_byte`/`read_word` masquent à 0xFFFFF et renvoient 0 au-delà : à exercer) ;
3. **enveloppes** — les quatre états et leurs transitions, `EGHOLD`, `KRS`, `LPSLNK`, le key-off en pleine
   attaque, les rates 0 et 1 (« infinity » dans les tables) ;
4. **pitch, LFO, modulation** — `OCT`/`FNS` aux extrêmes, les quatre formes, et la modulation par anneau avec
   la profondeur de ce portage ;
5. **DSP** — `PACK`/`UNPACK` sur toute la plage 24 bits, la saturation du shifter, l'accumulateur, les quatre
   `SHIFT`, les retards, et les quatre contrôles que le programme de ces jeux ne pose jamais (`FRCL`, `ADRL`,
   `YRL`, `NEGB`) — **ceux-là sont désormais les plus importants à tester**, parce que le corps spécialisé
   (`Run<0>` / `Run<USES_ALL>`) fait que *le chemin qui les gère n'est plus celui que ces jeux empruntent*.
   Tant qu'aucun test ne l'exerce, `Run<USES_ALL>` n'est vérifié par rien ;
6. **timers et interruptions** — A/B/C et leurs périodes, `SCIEB`/`SCIPD`/`SCILV`, le décodage de priorité, le
   DMA, et l'ordre relatif des accès du 68000 ;
7. **les deux chips et le mélange** — `DISDL`/`EFSDL`, le panoramique, `MVOL`, `DAC18B` ;
8. **reset et répétabilité** — deux runs identiques donnent le même PCM (c'est déjà vrai de fait : les WAV de
   deux runs se comparent à l'octet, et la seule source aléatoire, `machine_rand`, a une graine fixe).

### c. Le harnais à deux cœurs

`comparer-son.cmd` compare deux **constructions**. Pour une réécriture, il faut comparer deux **cœurs dans le
même binaire** : la même séquence d'accès rejouée dans les deux, et la comparaison des PCM, des états
observables, des interruptions et des timers, pas seulement du WAV final. `VF3_SCSP_WLOG` donne déjà la trace
d'accès ; il manque le rejoueur.


## 4. Le trou que ce travail a creusé, et qu'il faut nommer

Le lot 1 du 2026-10-24 a coupé le corps du DSP en deux chemins (`docs/scsp_audit.md` §7). Pour les deux jeux de
ce portage, c'est le chemin maigre qui tourne, et il est vérifié à l'octet. **L'autre chemin ne l'est pas**, et
il ne le sera par rien tant qu'aucun test ne pose `FRCL`, `ADRL`, `YRL` ou `NEGB`.

Le risque est borné — `Run<USES_ALL>` est le code d'avant, à quatre `if constexpr` près, et un `uses` mal
calculé ne peut que choisir le corps **lent**, jamais le mauvais — mais il est réel pour un troisième jeu.
C'est le premier test à écrire si ce portage en sert un jour un autre.
