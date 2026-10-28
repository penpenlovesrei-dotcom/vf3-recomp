# Sortir du SCSP de MAME — le plan, et ce qui est déjà fait

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [docs/son.md](son.md).

**C'est un plan.** Au 2026-10-27, les étapes 0, 2 et 3 sont faites. **Le SCSP du projet existe** —
`src/sound/scsp_maison.{h,cpp}` — et il rend le même son que celui de MAME **à l'octet** sur une scène
réelle. **Les sept blocs sont écrits**, et le dernier — le DSP d'effets — l'a été contre une spécification
mesurée d'abord (`tools/son/test_scsp_dsp.cpp`), faute de la moindre description de son jeu d'instructions.
Le cœur du projet ne contient donc **plus une ligne de MAME**.

**Au rejoueur, sur la scène entière de `fv2-fight.txt` (4200 images) : IDENTIQUES.** 3 219 865 échantillons
par puce, 208 360 interruptions, 69 311 armements de timer, 265 469 lectures de registre — tout concorde,
échantillon par échantillon, sur les deux puces.

**ET DE BOUT EN BOUT, LE JEU ENTIER : MÊME EMPREINTE.** `vf3run` sur les 4200 images, une fois avec chaque
cœur, donne le même WAV — `9A2310F8AF000E09EB18BD114852B003`, qui est **exactement la référence prise
avant tout ce travail** (commit de l'étape 3). Le rejoueur jugeait la puce sur une séquence figée ; ceci la
juge en place, avec le 68000 qui lui répond et le mélangeur de la carte derrière.

**La carte peut désormais tourner sur l'un ou l'autre** : `VF3_SCSP_COEUR=mame` (par défaut) ou `maison`.
Deux des quatre extras ont disparu en chemin — `MVOL` se **lit dans le registre**, la puce ne l'appliquant
pas elle-même, et `reg_word` n'était qu'un `read_reg` déguisé. Le **miroir MADRS** est porté par les deux
cœurs, chacun sur sa classe concrète, parce qu'il appartient au jeu et non à la puce.

Ce qui reste dû, et c'est nommé dans `soundboard.h` : **les gains par groupe** (le partage musique/effets du
menu), **la sonde d'énergie** et **le journal des key-on** ne vivent que dans le cœur de MAME. Les deux
derniers sont des diagnostics ; le premier compte pour le joueur, mais il est **inerte tant que les deux
volumes valent 1**. C'est pourquoi le cœur du projet est utilisable sans eux, et pourquoi le défaut reste
`mame` : rien ne change sans qu'on le demande.


## Pourquoi, et dans quel ordre les quatre objectifs se servent

Le cahier des charges les classe D > A > B > C : processeur, puis sortir de MAME, puis la fidélité, puis le
C++. Mais ils ne sont pas indépendants, et il faut dire comment :

- **D (processeur) ne demande pas A.** Démontré : les 24 % gagnés le 2026-10-24 l'ont été **dans** le code de
  MAME, sans rien changer à ce qu'il produit. Les deux tiers du coût étaient dans le DSP, et le DSP se gagne
  par la forme du code, pas par la propriété du code.
- **A (sortir de MAME) met B (la fidélité) en jeu**, et c'est le vrai coût du projet. Aujourd'hui, le code de
  MAME **est** la référence de fidélité de ce portage : chaque écart documenté (`docs/scsp_audit.md` §2) a été
  mesuré contre lui, jeu par jeu. Un cœur neuf ne garde cette fidélité que s'il est comparé échantillon par
  échantillon à celui-là — donc A ne peut pas avancer sans que l'ancien reste disponible.
- **La licence ne presse pas.** MAME est BSD-3-Clause, compatible avec le MIT de ce dépôt, et les obligations
  sont tenues (`docs/scsp_references.md`). Rien n'oblige à sortir ; l'intérêt est la maîtrise, pas la
  conformité.

Conséquence : **A se fait par blocs, derrière un sélecteur, et le dernier bloc retiré est le plus risqué**, pas
le premier venu.


## Étape 0 — faite le 2026-10-24

| | |
|---|---|
| mesurer | `src/sound/sound_prof.h` : où va le temps de la carte son, vérifié sans effet sur ce qu'elle produit |
| cartographier | `docs/scsp_audit.md` : pièces, provenance, licences, interfaces, chemins chauds |
| sourcer | `docs/scsp_references.md` : ce qui a été lu à la source, et l'avertissement sur la non-indépendance des émulateurs |
| le harnais | `tools/son/comparer-son.cmd` : le son contre un commit de référence, octet par octet |
| gagner | trois lots sur `scspdsp`, **−24,3 %** sur la carte son, WAV identique à l'octet sur les deux jeux |
| le lot défait | un quatrième, sur les voix, qui ne gagnait rien : défait, et le résultat négatif consigné |
| tester | `tools/son/test_scspdsp.cpp` (`make test`) : les deux corps du DSP contre une référence qui décode dans la boucle, seize cas, et quatre fautes injectées pour montrer qu'il sait échouer |

Ce que l'étape 0 n'avait pas fait a été fait depuis, et il faut le lire avant la suite. Elle laissait un seul
test de bloc sur sept ; **les sept en ont un** maintenant, les six derniers écrits le 2026-10-26 : les
registres (1), les fentes (2), l'enveloppe (3), les LFO (4), le DSP (5), les timers et les interruptions (6)
et le mélange (7). Ce qui manque encore : **une seule scène de mesure**, le démarrage et l'attract.

**Les trois mesures qui manquaient sont faites** :

| où | part du son | ce que ça dit |
|---|---|---|
| `vf3run --rec`, la carte dans la boucle d'image | **30,5 % de l'image** (§4 bis, 2026-10-25) | la carte Model 3 y est 4,3 fois plus rapide et le son, lui, ne change pas. C'est là que sa marge compte |
| l'hôte, la carte sur son fil, machine de bureau | **8,5 % d'un cœur, 0,00 % de l'image** (§4 ter, 2026-10-26) | le jeu ne l'attend jamais |
| l'hôte, **portable AMD à deux cœurs** | **17,4 % d'un cœur, 0,00 % de l'image** (§4 quater, 2026-10-26) | deux fois plus cher, et l'attente reste nulle. La cadence y est tenue par les téléversements de la photo, pas par le son |

Le DSP fait la moitié du son dans les trois. Ce qui reste non mesuré : la **concurrence** sur deux cœurs — la
sonde compte l'attente bloquante, pas le coût de partager le cache et l'ordonnanceur ; il faudrait la même
scène son coupé. Et toujours une seule scène, le démarrage et l'attract.


## Étape 1 — le reste du gain processeur, s'il en faut

Dans l'ordre de ce que la mesure dit, après l'étape 0 : DSP 52-60 %, 68000 28-31 %, voix 7-17 %.

1. **Le DSP, une forme compilée par programme.** Le programme est fixe entre deux écritures de MPRO : 112 pas
   rejoués 44 100 fois par seconde. La forme décodée (`Inst`) et les contrôles groupés (`Inst::f`) sont déjà
   au-delà de MAME et de Supermodel, qui décodent dans la boucle. La suite est d'émettre, à chaque `Start`, une
   suite d'opérations sans branche de décodage — pas un JIT, une liste de micro-opérations choisies par pas.
   À mesurer avant d'y croire : le plancher est la chaîne de dépendance de `ACC`, qui ne se vectorise pas.
2. **Le 68000**, et c'est le mur : son coût propre tient aux appels virtuels de bus, un par accès mémoire, et
   ses gestionnaires sont ceux de MAME par contrat. Deux voies honnêtes, aucune petite : activer LTO sur
   l'ensemble, ou donner au cœur une interface de bus non virtuelle (ce qui le modifie, donc sort du contrat
   « inchangé »).
3. **Les voix** : le lot 4 a montré que ce n'est pas la boucle d'adresses. Restent les tables de panoramique
   (deux `int[0x10000]`, 256 Ko chacune), l'enveloppe et le LFO. **Non mesuré dans le détail.**

Aucune de ces trois n'est nécessaire : à 1,40 ms par image sur un budget de 17,4, et sur un fil à part dans
l'hôte, la carte son n'est plus un problème. L'étape 1 est à faire **si** quelqu'un en a besoin, pas parce
qu'elle est là.


## Étape 2 — les préalables de la réécriture

Rien ne s'écrit avant ces trois-là, et c'est le gros du travail :

0. **Le contrat matériel**, qui n'était pas dans cette liste et aurait dû l'être : ce que la puce doit faire,
   par opposition à la façon dont on la branche. Écrit le 2026-10-25 — `src/sound/scsp_contract.h`, 48 champs
   et les constantes du pilote, chaque nombre portant sa provenance et les écarts assumés marqués comme tels
   — et **vérifié** par `tools/son/test_scsp_contract.cpp` : 55 contrôles contre `src/sound/scsp.cpp` et
   contre la ROM du pilote, avec un compte qui doit fermer pour qu'aucun champ ne soit silencieusement sauté.
1. **Les tests par bloc** de `docs/scsp_validation.md` §3b, écrits avant les blocs qu'ils testent. Le premier
   des sept existe depuis le 2026-10-24 (`make test` : le DSP, ses deux corps et les quatre contrôles que
   les jeux ne posent jamais) — ~~les six autres non~~ **et les six autres depuis le 2026-10-26** : registres,
   fentes, enveloppe, LFO, timers et interruptions, mélange. **Le contrat ci-dessus est testé lui aussi**, par
   le même `make test`, qui enchaîne désormais neuf tests.
2. ~~**Le sélecteur et le rejoueur**~~ — **faits le 2026-10-26.**
   - `src/sound/scsp_select.h` : `scsp::make(nom, ...)`, la porte par laquelle un second cœur entrera.
     Aujourd'hui `mame` est la seule entrée, et le fichier le dit plutôt que de faire semblant.
   - `src/sound/scsp_trace.h` : le format de trace, l'enregistreur et le rejoueur. **Le format est neuf, et
     `VF3_SCSP_WLOG` ne pouvait pas servir de départ** : il note l'image et non l'échantillon — donc
     l'instant d'une écriture dans la trame, qui est ce qui décide où tombent les interruptions — et il
     laisse de côté la puce 1, les lectures, le MIDI entrant, les timers qui se déclenchent et **les
     écritures de la RAM son**, sans lesquelles un rejeu n'a aucun échantillon à jouer.
   - `tools/son/rejouer.cpp` et `rejouer.cmd` : le rejeu et la comparaison du PCM, des interruptions, des
     armements de timer, des lectures de registre et du MIDI sortant — la première divergence, située à
     l'échantillon.
   - L'enregistrement passe par **une seule porte**, `SoundBoard::tete[n]` : un décorateur de `scsp::core`
     plutôt qu'une sonde posée aux huit sites d'appel, pour qu'un neuvième ajouté un jour ne puisse pas
     échapper à la trace en silence.

   **Ce qui a été vérifié**, et dans cet ordre, parce qu'un contrôle ne vaut rien sans celui d'avant :
   | | |
   |---|---|
   | la trace est **complète, sur la scène entière** | le rejeu retrouve le PCM que la carte a produit, **identique à l'octet sur 3 219 865 échantillons par puce** (4 200 images), pour les deux puces. C'est ce qui établit qu'aucun état n'entre par une porte que la trace ne note pas. **Établi d'abord sur 300 images seulement, et c'était trop court** : sur la scène entière ce contrôle échouait (échantillons 772 067 et 1 149 006), parce que le **miroir MADRS** se posait hors de l'interface et n'entrait donc pas dans la trace. Trouvé, nommé et refermé le 2026-10-28 — en-tête de trace version 2, porte unique dans `scsp_select.h` ; `docs/scsp_validation.md` §3 c porte toute la mesure |
   | le comparateur **sait échouer** | un seul échantillon modifié dans la référence, et il le nomme : « échantillon 100000 droite » |
   | `mame` contre `mame` | identiques sur les cinq flux |
   | la carte n'a **pas changé** | `vf3run` 1200 images contre la construction de `9753fde` : **même empreinte MD5**. Le détour par `tete[n]` ne coûte rien au son |
3. ~~**L'interface** de `docs/scsp_architecture.md` §2, posée **d'abord sur le cœur de MAME**~~ — **faite le
   2026-10-26.** `src/sound/scsp_core.h` est l'interface telle que le §2 l'avait dessinée (trois méthodes de
   `sink`, le temps **poussé** par `run(n, out)`, l'unité l'échantillon, aucune allocation) ;
   `src/sound/scsp_mame.{h,cpp}` est l'adaptateur qui présente l'ancien cœur à travers elle ; et
   `tools/son/test_scsp_core.cpp` est ce qui décide que le contrat tient — il pilote MAME **uniquement** par
   `scsp::core`, et **écrit ses registres par la table de champs du contrat**, de sorte qu'un champ faux y
   coupe le son au lieu d'attendre d'être trouvé dans un jeu.

   **Les trois endroits où l'ancien cœur et la forme neuve ne coïncident pas** sont nommés dans
   `scsp_mame.h` plutôt que masqués : le temps est **tiré** chez l'ancien (l'adaptateur répond lui-même à la
   question « quelle heure est-il »), la sortie est en flottants (la reconversion est **exacte**, les
   diviseurs étant des puissances de deux), et **la sortie MIDI est en bits là où le contrat demande des
   octets** — l'adaptateur ne la franchit pas, il **compte les bits** pour que le manque soit visible.

   **Et la carte est branchée dessus**, le 2026-10-26 aussi : `soundboard.cpp` ne parle plus aux puces que
   par l'interface — registres, temps, timers, MIDI — et n'atteint l'ancienne classe, par `->device()`, que
   pour ce qui n'est pas l'affaire de la puce (les gains par groupe, la sonde d'énergie, le miroir MADRS).
   Ces `device()`-là sont ce qui doit partir en dernier.

   **Au harnais : IDENTIQUE A L'OCTET**, sur `fv2-fight.txt`, 4 200 images, `--rec`, et sur **trois arènes**
   (2, 7 et 9) — mêmes longueurs, mêmes empreintes MD5. Celle de l'arène 9 est `9a2310f8af00…`, c'est-à-dire
   **exactement la référence prise le matin même avant tout ce travail** : c'est aussi l'arène que le jeu
   choisit seul, ce qui ferme la boucle.

   **Trois choses que le harnais a trouvées et que le raisonnement seul n'aurait pas données :**

   - **le §2 se trompait sur les interruptions.** Il proposait `irq(int level)`, 0 pour rien, « pas une
     ligne à la fois ». Or la puce éteint **quatre lignes distinctes** indépendamment et peut en éteindre
     une pendant qu'une autre est levée : un seul numéro ne peut pas dire laquelle est retombée.
     `scsp_core.h` porte donc `irq(level, on)`, et dit pourquoi. **C'est exactement ce que l'étape 3 existe
     pour découvrir.**
   - **vider le tampon de l'ancien cœur en entier perdait des échantillons.** `run()` copiait `2n` valeurs
     puis appelait `clear()` : tout ce que l'ancien cœur avait fabriqué au-delà partait. Le harnais l'a vu —
     une paire perdue à l'image 1006, et tout le son d'après identique **une fois décalé d'un échantillon**.
     Un `clear()` de trop fait passer un cœur fidèle pour infidèle.
   - **et une hypothèse à moi, fausse, écartée par la mesure** : j'avais cru que remettre les *deux* puces à
     l'heure à chaque accès (au lieu de celle qu'on touche) expliquait l'écart. Corrigé, l'empreinte n'a pas
     bougé d'un bit. La correction est juste — l'ancien chemin n'avançait qu'une puce — mais **elle
     n'expliquait rien**, et c'est la mesure qui l'a dit.

   - **et la paire manquante était une convention de comptage.** L'`update()` de l'ancien cœur fait
     `n = idx - m_written + 1` : il fabrique les échantillons d'indice `m_written` **jusqu'à `idx` inclus**,
     donc à l'horloge `idx` il en a fait `idx + 1`. La nouvelle interface dit « `run(n)` fait n
     échantillons », ce qui est plus propre — mais il fallait que l'adaptateur absorbe l'écart. Il le fait
     en annonçant un indice **en retard d'un** sur son compteur, et la carte vise `cible + 1`. Sans ça le
     tout premier échantillon de la partie n'était jamais fabriqué : **une paire sur 3 219 865**, que ni le
     code ni l'oreille n'auraient montrée, et que le harnais a trouvée en une comparaison.

   Reste ouvert de cette étape : les horloges du récepteur et de l'émetteur MIDI perdent la précision du
   cycle en passant par une interface comptée en échantillons (`soundboard.cpp`, `Sink::timer`). Ça ne
   s'entend pas sur cette scène — l'empreinte est identique — mais ça reste une perte de précision écrite
   dans le code plutôt que corrigée.

L'ordre importait : (3) avant (2), parce qu'un rejoueur écrit contre l'ancienne interface aurait été à
refaire. Il a été respecté — l'interface le 2026-10-26 au matin, le rejoueur le soir, et le rejoueur ne
connaît que `scsp::core`.

**L'étape 2 est donc close, et rien ne bloque plus l'étape 3.**


## Étape 3 — les blocs, un par un

Le découpage et l'ordre sont dans `docs/scsp_architecture.md` §3. Pour chaque bloc, la même boucle :

```
écrire le test  ->  écrire le bloc  ->  le harnais dit « identique »  ->  mesurer le coût  ->  garder
                                     \-> sinon : comprendre l'écart, pas élargir la tolérance
```

Et une règle sur l'ordre : **commencer par les blocs dont la fidélité est vérifiable sans le jeu** (registres,
PCM, PACK/UNPACK, le shifter du DSP), finir par ceux dont le seul juge est le jeu lui-même (les tables de temps
des enveloppes, la profondeur de modulation, le miroir de MADRS). Ces derniers portent les écarts mesurés de ce
portage, et ce sont eux qui peuvent casser le son sans qu'un test le voie.


## Critères d'acceptation

Repris du cahier des charges, et rendus vérifiables ici :

| | critère | comment on le voit |
|---|---|---|
| 1 | ça compile dans les configurations servies | `make build/bin/vf3run build/bin/vf3lock build/bin/vf3` sans erreur |
| 2 | les tests existants passent, ou l'écart est expliqué et isolé | `verifier-vf3.cmd` dit IDENTIQUE partout |
| 3 | les scènes sonores de référence se refont | `tools/son/comparer-son.cmd` sur les deux jeux |
| 4 | les écarts audio sont mesurés, classés, documentés | pour une optimisation : **aucun écart**, WAV identique à l'octet. Pour un cœur neuf : la liste des écarts, chacun avec sa mesure (erreur max, RMS, corrélation, spectre) et sa cause |
| 5 | le coût processeur est mesuré contre la ligne de base | `VF3_PROF=1`, même scène, même machine, et la répartition avec `VF3_SND_PROF=1` |
| 6 | aucun gain annoncé sans chiffres refaisables | les commandes sont dans `docs/scsp_benchmark_baseline.md` |
| 7 | les dépendances et les licences sont documentées | `docs/scsp_audit.md` §2, `docs/scsp_references.md`, `third_party/README.md` |
| 8 | le repli reste utilisable pendant la validation | le sélecteur de l'étape 2, et l'ancien cœur derrière. Depuis le 2026-10-28 **chaque cœur s'inscrit dans son propre `.cpp`** : le sélecteur ne nomme plus aucune classe concrète, donc ce qui est lié décide ce qui existe, et `make test-sans-mame` construit les sept tests de bloc **sans une ligne du cœur de MAME** — le dernier préalable d'un dépôt séparé |

**État au 2026-10-24 :** 1 ✔, 2 non revérifié depuis les lots (les lots ne touchent pas le PowerPC, et
`verifier-vf3.cmd` ne regarde pas le son — à lancer quand même), 3 ✔ sur les deux jeux, 4 ✔ (aucun écart),
5 ✔, 6 ✔, 7 ✔, 8 sans objet (rien n'a été remplacé).
