# Le son — la porte d'entrée

**Ce fichier n'explique rien. Il dit où est quoi.** Le sous-système sonore tient dans cinq endroits du dépôt
et rien ne les nommait ensemble ; il a fallu, le 2026-10-25, mesurer l'empreinte pour s'en rendre compte :

| | lignes | |
|---|---|---|
| `src/sound/` | 3 284 | dont **2 103 sont celles de MAME** (`scsp.cpp`, `scsp.h`) |
| `tools/son/` et `tools/ghidra/FV2Son*` | 2 572 | l'outillage de mesure et de lecture |
| les neuf documents ci-dessous | 4 955 | **plus gros que tout le code** |
| **en tout** | **10 811** | 1,8 % du dépôt, mais **11,2 %** hors `third_party/` |

(Comptés le 2026-10-25, et recomptés après l'écriture de ce fichier : la première version de ce tableau
donnait 3 130 lignes d'outillage parce qu'elle comptait tout `tools/ghidra/`, scripts du PowerPC compris.)

La documentation étant plus grosse que le code, l'essentiel de ce travail est de la **connaissance**, et une
connaissance qu'on ne retrouve pas est perdue. D'où ce fichier.

---

## 1. Par où commencer, selon ce qu'on vient faire

| je veux… | lire d'abord |
|---|---|
| comprendre ce que le son coûte et où | `scsp_audit.md` §4 et §4 bis |
| savoir ce que le pilote attend de la puce | **`src/sound/scsp_contract.h`** — le contrat matériel, vérifié par un test |
| savoir comment une implémentation se branche | `scsp_architecture.md` — l'interface C++ |
| remplacer le SCSP de MAME | `scsp_migration_plan.md`, qui énumère les préalables et dit lesquels existent |
| comprendre ce que le programme 68000 fait | `fv2-son-programme.md` — 3 062 lignes, les 168 fonctions lues |
| savoir si une mesure est fiable | `scsp_validation.md`, et la provenance notée à chaque chiffre |
| ne pas refaire une erreur déjà faite | `fv2-son-programme.md` garde les fausses pistes **à côté** des mesures qui les ont corrigées |

## 2. Le code

| | |
|---|---|
| `src/sound/soundboard.h`, `.cpp` | la carte son : un 68000, deux SCSP, la ROM d'échantillons banquée. **C'est ici qu'on entre** |
| `src/sound/scsp.h`, `.cpp` | **le chip, et c'est le code de MAME** (`license:BSD-3-Clause`, ElSemi et R. Belmont), modifié. Le dépôt est MIT : la licence est compatible, la frontière est ici |
| `src/sound/scspdsp.h`, `.cpp` | le DSP d'effets, réécrit par ce portage — les deux corps `Run<0>` et `Run<USES_ALL>`, et le gain mesuré de −24 % |
| `src/sound/scsp_contract.h` | **le contrat matériel** : 48 champs et les constantes du pilote, chaque nombre avec sa provenance, les écarts assumés marqués `DEVIATION` |
| `src/sound/sound_prof.h` | le profilage par parties (`VF3_SND_PROF=1`) |

## 3. Les outils

| | |
|---|---|
| `tools/son/extraire-son.py` | extrait la ROM du 68000 d'un `.zip` **et échange les octets** — sans quoi rien ne se désassemble. C'est le préalable de tout le reste |
| `tools/son/desas_son.py` | **cherche** dans les 512 Ko : `--cherche`, `--at`, `--fonctions`, `--carte`. Il sème les onze sauts calculés, sans quoi il ne voit que la moitié du programme |
| `tools/son/importer-son.cmd` | importe la ROM dans Ghidra, sans interface |
| `tools/ghidra/FV2SonSetup.java` | sème les points d'entrée **avant** l'analyse (en `-preScript`) |
| `tools/ghidra/FV2SonTablesDeSaut.java` | ouvre les quatre tables de saut — 41 fonctions à 90 |
| `tools/ghidra/FV2SonSautsIndirects.java` | résout les cinq derniers `jmp (An)` |
| `tools/ghidra/FV2SonDonnees.java` | déclare les données, et **recompte** ce qui reste dehors |
| `tools/ghidra/FV2SonListe.java` | sort les fonctions et leurs plages, sans rien changer — le témoin des trois précédents |
| `tools/son/comparer-son.cmd` | **le harnais différentiel** : deux WAV, même scène, comparaison à l'octet |
| `tools/son/audibilite.py` | si deux WAV diffèrent, **est-ce audible** : bandes de Bark, masquage, seuil absolu, plancher ambiant |
| `tools/son/comparer-madrs.cmd`, `mesurer-madrs.py` | le cas particulier du miroir MADRS |

## 4. Les sondes, et où chacune vit

Aucune n'est active par défaut. **Chaque mesure de ces documents nomme la sonde et la scène qui l'ont
produite** ; c'est la règle de ce sous-système, et elle vient d'y avoir été prise en défaut plusieurs fois.

| sonde | ce qu'elle donne | dans |
|---|---|---|
| `VF3_SND_PROF=1` | le coût du son par parties : boucle, 68000, voix, DSP, mélange | `sound_prof.h` |
| `VF3_PROF=1` | le coût par carte, toutes les 500 images (`prof …: board … sound …`) | `tools/vf3run.cpp` |
| `VF3_SND_HOST=N` | **ce que le son coûte à l'hôte** : le travail du fil, et surtout **l'attente du jeu** — ce n'est pas le chiffre de `vf3run` | `src/host/main.cpp` |
| `VF3_SCSP_DSPLOG=f` | **chaque écriture** dans les registres du DSP, du tampon FM, du mot `0x6` et des mots `SA` d'une fente | `soundboard.cpp` |
| `VF3_FICHES=f` | les fiches de voix vivantes, avec canal et catégorie — ce qui a tranché « quelle catégorie est la musique » | `tools/vf3run.cpp` |
| `VF3_STAGE=n` | **force l'arène** (1-12), en tenant les deux leviers du jeu | `tools/vf3run.cpp` |
| `VF3_VOLUMES=m,e` | les volumes musique et effets, séparément | `tools/vf3run.cpp` |
| `VF3_SCSP_WLOG=f` | la trace des écritures de registres, pour un futur rejoueur | `soundboard.cpp` |
| `VF3_KEYON_LOG=1` | les allumages de note | `soundboard.cpp` |
| `VF3_SCSP_SOLO=n` | n'écouter qu'une fente | `soundboard.cpp` |
| `VF3_SCSP_MADRS_MIRROR`, `VF3_SCSP_MADRS_SEPARATE` | forcer l'un ou l'autre côté de la question `0x7C0-0x7FF` | `soundboard.cpp` |
| `VF3_SCSP_MSLC_LATCH` | le verrou de `MSLC` | `scsp.h` |
| `VF3_DUMP_SOUND_RAM=f`, `VF3_MIDI_LOG`, `VF3_MIDI_SEND` | la RAM sonore, le MIDI reçu, le MIDI injecté | `tools/vf3run.cpp` |
| `VF3_ENERGY_LOG`, `VF3_TIMER_LOG`, `VF3_MENU_SOUND` | côté hôte | `src/host/main.cpp` |

## 5. Les tests

`make test` construit et lance les deux, et ils ne demandent ni ROM ni fenêtre pour l'essentiel :

| | |
|---|---|
| `tools/son/test_scspdsp.cpp` | les deux corps du DSP contre une référence qui décode dans la boucle. 16 cas, dont les quatre contrôles qu'aucun jeu ne pose |
| `tools/son/test_scsp_contract.cpp` | **le contrat contre ce qu'il décrit** : 48 champs contre les macros de `scsp.cpp`, les constantes contre la ROM. 55 contrôles, et **un compte qui doit fermer** pour qu'aucun champ ne soit silencieusement sauté |

La seconde partie du test du contrat s'efface, **avec une ligne visible**, quand `build/fv2-son-68000.bin`
n'est pas là — elle vient d'une ROM que le dépôt ne porte pas.

## 6. Les neuf documents

| | lignes | |
|---|---|---|
| `scsp_audit.md` | 330 | ce qu'il y a, ce que ça coûte, et où va le temps |
| `scsp_benchmark_baseline.md` | 174 | la ligne de base, et chaque lot mesuré contre elle |
| `scsp_architecture.md` | 174 | l'interface C++ d'un cœur autonome |
| `scsp_migration_plan.md` | 124 | le plan pour sortir du SCSP de MAME, et ce qui est fait |
| `scsp_validation.md` | 167 | ce qui valide quoi, et ce qui manque |
| `scsp_references.md` | 162 | les sources lues, et la règle : **jamais le consensus entre émulateurs comme preuve** |
| `fv2-son-programme.md` | 3 062 | le programme 68000, lu — les 168 fonctions, la carte de la RAM, les erreurs gardées |
| `fv2-console-son.md` | 643 | le son de la liste des modes |
| `son.md` | 119 | ce fichier |

## 7. Pourquoi ce n'est pas un dépôt à part

La question a été posée le 2026-10-25 et tranchée **non, pas encore**, pour une raison et une seule :
**la preuve vit dans le jeu.** Les chiffres de ces documents — la part du son sous `--rec`, les trois arènes
qui chargent leur propre programme DSP, les 993 écritures du tampon FM toutes protégées par `STWINH`, les
1 807 allocations de voix qui ont tranché le partage musique/effets — sortent tous de `vf3run` pilotant le
vrai jeu avec les vraies ROM. Un dépôt séparé porterait le code et les documents **sans pouvoir refaire ni
recontrôler un seul de ces nombres**. Le test du contrat lit `scsp.cpp` d'un côté et la ROM du pilote de
l'autre ; le harnais différentiel compare la sortie de *ce* portage sur *ces* jeux.

Et il n'y a encore rien d'autonome à extraire : la puce est toujours celle de MAME.

**Le moment de séparer est nommé** : quand le cœur autonome existera et passera le harnais différentiel.
Alors il ne dépendra plus du jeu, la frontière de licence sera propre, et la séparation ne coûtera rien.
