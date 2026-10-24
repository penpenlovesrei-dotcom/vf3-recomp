# Sortir du SCSP de MAME — le plan, et ce qui est déjà fait

**C'est un plan.** Au 2026-10-24, l'étape 0 est faite et rien d'autre. Le cœur du son est toujours celui de
MAME, modifié.


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

Ce que l'étape 0 **n'a pas** fait, et il faut le lire avant la suite : aucun test par bloc n'existe, il n'y a
qu'une scène de mesure, rien n'est mesuré sous `--rec` ni dans l'hôte, et le corps générique du DSP
(`Run<USES_ALL>`) n'est exercé par rien (`docs/scsp_validation.md` §4).


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

1. **Les tests par bloc** de `docs/scsp_validation.md` §3b, écrits avant les blocs qu'ils testent. Y compris,
   d'abord, celui qui manque déjà : un programme DSP qui pose `FRCL`, `ADRL`, `YRL` et `NEGB`.
2. **Le sélecteur et le rejoueur** : les deux cœurs dans le même binaire, la même séquence d'accès rejouée dans
   les deux (la trace de `VF3_SCSP_WLOG` existe, le rejoueur non), et la comparaison des PCM, des états
   observables, des interruptions et des timers — pas seulement du WAV.
3. **L'interface** de `docs/scsp_architecture.md` §2, posée **d'abord sur le cœur de MAME** : un adaptateur qui
   présente l'ancien cœur à travers le contrat neuf. C'est le geste qui décide si le contrat tient, et il ne
   coûte rien à défaire.

L'ordre importe : (3) avant (2), parce qu'un rejoueur écrit contre l'ancienne interface sera à refaire.


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
| 8 | le repli reste utilisable pendant la validation | le sélecteur de l'étape 2, et l'ancien cœur derrière |

**État au 2026-10-24 :** 1 ✔, 2 non revérifié depuis les lots (les lots ne touchent pas le PowerPC, et
`verifier-vf3.cmd` ne regarde pas le son — à lancer quand même), 3 ✔ sur les deux jeux, 4 ✔ (aucun écart),
5 ✔, 6 ✔, 7 ✔, 8 sans objet (rien n'a été remplacé).
