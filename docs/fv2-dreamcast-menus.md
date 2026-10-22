# Relire nos menus au regard de ceux de la Dreamcast — le plan

Écrit le 2026-10-22, avant d'avoir lu quoi que ce soit sérieusement. Rien de ce document n'est une conclusion :
ce sont les questions à trancher, dans quel ordre, et par quelle mesure chacune se vérifie.

La version Dreamcast de Fighting Vipers 2 (2001, Sega / CRI) **est** le menu console que ce portage essaie de
refaire. Jusqu'ici on l'a utilisée comme une **source d'images**. Elle est surtout une source de **code** : ses
écrans, leur enchaînement, ce que chaque ligne déclenche, et ce qu'elle fait du jeu arcade qu'elle embarque.


## Pourquoi ce plan existe

Deux raisons, et la seconde est récente.

1. **La règle posée le 2026-10-22** : pour toute question en « pourquoi le jeu fait ça », on lit d'abord ce qui
   est déjà décompilé, on mesure ensuite. Elle a été payée par une soirée entière passée à retrouver par
   différence de RAM ce que `docs/fv2-flux-de-match.md` nommait déjà.
2. **Un premier coup de sonde dans le binaire Dreamcast a donné une conclusion fausse**, le 2026-10-22 : j'y ai
   lu « la ligne choisie est dessinée avec un autre motif », ce qui était un contresens sur deux points — le
   créneau de délai du `jsr` (voir plus bas) et l'identité des rangs 34-44, qui sont la **police** et pas des
   libellés. Un binaire qu'on sonde sans plan répond ce qu'on veut entendre.


## Ce qui est acquis, et qui ne sera pas remesuré

Tout vient de `Documents\fv2-arcade-notes\README.md`, section « Les ressources 2D de la Dreamcast ».

| | |
|---|---|
| le binaire | `Documents\fv2-arcade-notes\dreamcast\1ST_READ.BIN`, 1,3 Mo, SH-4, base `0x8C010000` |
| poser la ressource | `charge_ressource(enregistrement)` — `0x8C02A23A` |
| dessiner | `dessine(rang, x, y)` — `0x8C029598`, rang dans R4, x dans R5, y dans R6 |
| la nuance qui commande tout | `dessine` ne reçoit pas la ressource : elle travaille sur celle que le dernier `charge_ressource` a posée. **Toute lecture suit l'ordre d'exécution, pas le graphe d'appels.** |
| les motifs de la façade | entrée TEX 16, liste à `0x8C0E5808`, 96 enregistrements de 10 octets : `desc, x, y, larg, haut` |
| les libellés du menu | **rangs 10 à 18**, descripteur 18 |
| l'écran-titre | rangs 3 à 8, mosaïque 3×2 de 256×256, posée par `0x8C019CBC` |
| les outils | `desas_sh4.py` (Ghidra ne désassemble que 32,4 % de ce binaire), `tables_sh4.py`, `retrouver.py`, et Flycast piloté par GDB (`FV2 - Flycast avec GDB.cmd`) |

**Le piège du SH-4, payé le jour même** : `jsr` a un **créneau de délai**. L'instruction qui suit le `jsr` dans
le listing s'exécute **avant** le saut. Le `mov #N,r4` qui porte le rang est donc écrit *après* l'appel et
appartient *à* cet appel. Lire le listing de haut en bas sans ça décale tous les arguments d'un appel.

Et le relevé, lu correctement, de la première séquence sondée (`0x8C02C388`) :

```
rang 10  (ARCADE, 144x32)  en (99, 217)
rang 11  (V.S.,    64x32)  en (355, 217)
```

Deux libellés **côte à côte sur une même ligne** : ce n'est pas le menu vertical, c'est autre chose — un écran
de versus, ou une bannière. Ce qui est déjà un fait utile : **les mêmes libellés servent à plusieurs écrans.**


## La divergence qu'il faut trancher en premier

Les libellés de la Dreamcast, lus dans sa liste de motifs :

```
rang 10  ARCADE            144x32
rang 11  V.S.               64x32
rang 12  INTERNET          160x32
rang 13  RANDOM NETWORK    176x32
rang 14  SURVIVAL          176x32
rang 15  TRAINING VS       152x32
rang 16                    176x32
rang 17                    104x48
rang 18                    256x32
```

Notre `title_list.png` a **sept bandes** : ARCADE, V.S., RANDOM, SURVIVAL, NETWORK, TRAINING, OPTIONS. La
Dreamcast a **neuf motifs**, et des lignes composées (`RANDOM NETWORK`, `TRAINING VS`) qui n'existent pas chez
nous. Deux lectures possibles, et une seule est vraie :

- notre image vient d'une **autre** version ou d'un autre écran que celui que ces rangs servent ;
- ou les rangs 10-18 ne sont pas tous du même menu, et la liste verticale n'en utilise qu'une partie.

**Une partie de la question est tombée le 2026-10-22**, et elle réduit la divergence : `NETWORK`, `TRAINING` et
`OPTIONS` **n'existent pas sur la borne**. Les 24 modes nommés par Sega n'en contiennent aucun, et les notes
arcade l'écrivent : « il n'y a pas de mode Training ». Ces lignes sont donc celles de la Dreamcast, et elles ne
peuvent pas être « câblées » — il n'y a rien à câbler. Soit ce portage les **construit** lui-même (une zone
d'entraînement à partir du décor `TEST` et des leviers du jeu, une page d'options par-dessus les
`GAME ASSIGNMENTS`), soit elles sortent de la liste.

Ce qui reste de la divergence est donc plus net : **notre image porte sept lignes dont trois que le jeu
dessous ne sait pas faire**, et la Dreamcast en a neuf, composées autrement. La question n'est plus « quel
levier pour ces lignes » mais « que doit contenir la liste d'un menu console posé sur une borne ».


## Les questions, dans l'ordre

### 1. Quelle est la machine à états des menus Dreamcast ?

Côté arcade on a la machine nommée par Sega — `MODE_MAIN` / `MODE_SUB` et les 24 modes
(`docs/fv2-etats.md`). La question est si le portage Dreamcast en a une à lui, par-dessus, ou s'il réutilise
celle du jeu.

**Mesure qui tranche** : poser un point d'arrêt GDB sur `dessine` dans Flycast, naviguer dans le menu à la
main, et relever qui appelle, avec quel rang, et depuis quelle pile. Les appelants distincts sont les écrans.

### 2. Comment ses écrans s'enchaînent

Ce que Frédéric a demandé explicitement, et ce qui manque le plus à notre menu : **les transitions**. Titre →
liste des modes → sélection → combat → retour ; et les chemins de retour (le bouton B), qui sont justement ce
que notre portage a eu le plus de mal à poser.

**Mesure** : la même session Flycast, en notant pour chaque transition l'ordre `charge_ressource` /
`dessine`, et ce que le jeu arcade embarqué fait au même moment (son mode, son sous-état).

### 3. Qui dessine la liste verticale, et comment la ligne choisie est marquée

Reprise de la question que j'ai mal lue. Le critère : une routine qui dessine **plusieurs rangs de la plage
10-18 à des `y` croissants et un `x` constant**. C'est ça, une liste verticale — pas deux rangs côte à côte.

**Mesure** : le même balayage que le premier coup de sonde, mais en respectant le créneau de délai, et confirmé
sous GDB.

Et la question qui en dépend, celle qui nous occupe depuis deux jours : **comment la ligne choisie est-elle
marquée ?** Un second motif, une couleur, un clignotement, un cadre ? Notre portage fait clignoter la ligne
(alpha 1 → 0,5 sur 45 images), ce qui vient du menu Supermodel du propriétaire, pas de la Dreamcast.

### 3bis. L'écran OPTIONS de la Dreamcast : **relevé le 2026-10-22**

Frédéric a fourni une capture Flycast de cet écran. Elle est mesurée ci-dessous, pas commentée à l'œil. Image
du jeu extraite de la fenêtre : 1878 × 1405, rapport 1,3367 — du 4:3. Toutes les fractions sont rapportées à
cette image.

#### Le fond : **aucun voile**

C'était la question posée. Les médianes de quatre zones sans interface, comparées à `option_bg.png` mis à la
même taille :

| zone | capture Dreamcast | `option_bg.png` tel quel | rapport |
|---|---|---|---|
| haut-gauche | (39, 38, 37) | (35, 33, 32) | 1,11 — 1,16 |
| bord gauche | (41, 39, 38) | (36, 34, 33) | 1,15 |
| bas gauche | (44, 41, 40) | (38, 36, 35) | 1,14 — 1,16 |
| haut-droit | (49, 47, 46) | (39, 37, 36) | 1,26 — 1,28 |

Le fond du disque **est** `option_bg.png`, à la compression et au gamma de l'émulateur près. Il n'est pas
assombri du tout. À ne pas confondre avec la liste des modes, où la moitié mesurée s'applique à l'image vivante
du jeu : ici le fond est une image préparée, déjà sombre par construction.

#### Les deux polices, et elles sont deux

Mesurées en cisaillant chaque ligne et en gardant l'angle qui rend les jambages verticaux les plus nets :

| | pente |
|---|---|
| `OPTIONS`, le titre | **0,00°** |
| `SOUNDS`, `CONTROLLER` | **0,00°** |
| `MATCH COUNT`, `ROUND TIME`, `SURVIVAL TIME`, `ENERGY MAX -1P-`, `ENERGY MAX -2P-` | **−7,0° à −8,25°**, médiane ≈ 7,5° |

Donc : un gras étroit **droit** pour le titre et les trois sous-écrans, et une **oblique** de ~7,5° pour les
sept lignes de réglages.

Et la graisse, mesurée comme la médiane des suites d'encre sur une rangée à mi-hauteur, rapportée à la
hauteur de capitale :

| | graisse |
|---|---|
| `police.png` (le `I`, qui est un jambage pur : 6 px sur 31) | **0,194** |
| les lignes de réglages du disque (8 px sur 41) | **0,195** |
| le titre du disque (16 px sur 63) | **0,254** |

`police.png` et les lignes du disque tombent au millième. **C'est la même fonte**, sans la pente — ce que les
formes laissaient déjà penser, et que la graisse confirme.

Le titre, lui, est un gras étroit : largeur du mot `OPTIONS` = **4,143** capitales, quand `police.png` en vaut
**6,000**. On l'approche en condensant puis en épaississant **horizontalement** — pas avec une dilatation
carrée, essayée d'abord et écartée parce qu'elle arrondit les angles et ferme les contrepoinçons, alors que le
disque les a francs et ouverts ; un gras étroit porte de toute façon sa graisse sur les jambages verticaux.
L'épaississement horizontal ne touche pas la hauteur, d'où un système à deux inconnues :

```
6,000*cap*c + s = 4,143*cap      (la largeur)
0,194*cap*c + s = 0,254*cap      (la graisse)
```

soit `c = 0,6698` et `s = 0,1241*cap`. Rendu ainsi, le titre du portage mesure 4,129 capitales de large pour
une graisse de 0,258, contre 4,143 et 0,254 au disque. `tools/fv2/maquette_options.py` le fait.

#### La disposition

| | |
|---|---|
| titre `OPTIONS` | encre de `y = 0,0996` à `0,1438`, centré |
| les sept réglages | hauteur de capitale **0,0292**, pas **0,0584**, première ligne à `y = 0,2128` |
| libellés | **calés à gauche sur `x = 0,3136`** |
| valeurs | **centrées sur `x ≈ 0,665`** — et non alignées à droite |
| `SOUNDS` / `CONTROLLER` / `RESULT` | capitales plus hautes (0,0377), même pas de 0,0584, à partir de `y = 0,644` |
| `INITIALIZE` | `y = 0,848..0,873`, l'image peinte |
| le pied | deux lignes, `y = 0,902` et `y = 0,940` |

#### Comment la ligne choisie est marquée

**Par la couleur, et par le filet.** L'encre de la ligne choisie est `(252, 252, 10)` — jaune franc — quand
les six autres sont `(247, 246, 246)`, blanc. Et le filet rose `(254, 170, 255)` court sous elle, de
`x = 0,2913` à `x = 0,7295` : il déborde à peine des libellés et des valeurs, **il ne traverse pas l'écran**.
C'est exactement `option_cursor.png`, ergot compris.

Rien ne clignote. Le clignotement de `fv2_mode_list.h` vient du menu Supermodel du propriétaire, et cet écran-ci
ne le fait pas.

#### Et ce ne sont pas les treize réglages de la borne

Le point qui compte pour `docs/fv2-console-options.md`. L'écran du disque montre **sept** réglages —
`DIFFICULTY`, `MATCH COUNT`, `ROUND TIME`, `SURVIVAL TIME`, `ENERGY MAX -1P-`, `ENERGY MAX -2P-`,
`SUPER K.O.` — puis trois sous-écrans (`SOUNDS`, `CONTROLLER`, `RESULT`), `INITIALIZE`, et un encart d'aide en
bas à droite qui explique l'entrée courante. Ce n'est pas la liste des `GAME ASSIGNMENTS` : la borne sépare
`MATCH COUNT 1P` et `VS`, le disque n'en montre qu'un ; la borne a `ADVERTISE SOUND`, `VS FINISH`, `BIKE LOGO`
et `COUNTRY`, que le disque ne montre pas.

Une **hypothèse**, pas un fait : `SURVIVAL TIME` vaut 180 sec. sur la capture, et l'un des octets non nommés du
bloc de la borne, `0x12054A`, porte `0x00B4` = 180. À vérifier avant d'y toucher.

Et un indice sur la plage de `DIFFICULTY` : la capture montre `VERY EASY`, ce qui va dans le sens de 0 à 4 =
`VERY EASY` … `VERY HARD`, avec 2 = `NORMAL` comme l'usine.

### 4. Ce que chaque ligne déclenche

Les cinq lignes de notre menu qui ne font rien : `RANDOM`, `V.S.`, `NETWORK`, `TRAINING`, `OPTIONS`. La
Dreamcast, elle, les fait toutes. La question est ce qu'elle écrit dans le jeu arcade pour chacune — c'est
exactement la nature des leviers que `MenuConsole.md` a trouvés à tâtons.

**Mesure** : point d'arrêt sur les écritures de la zone des réglages (`0x00120540`-`0x0012055F` côté arcade) et
sur la machine à modes, pendant qu'on choisit chaque ligne.

### 5. Où la Dreamcast diffère de l'arcade, et ce que ça impose à notre portage

La Dreamcast **émule ou porte** le jeu arcade ; nos leviers sont ceux de l'arcade. Là où elle fait autrement,
il faut savoir si c'est parce que son jeu est différent, ou parce qu'elle a trouvé un meilleur levier.

Le cas déjà connu : la Dreamcast a `INTERNET` et `RANDOM NETWORK`, qui n'existent pas sur la borne.


## L'ordre, et ce qui l'arrête

Un pas, une mesure montrée, rien d'écrit dans le code du portage avant la question 3.

1. **Lire ce qui existe déjà** : `fv2-arcade-notes\README.md` en entier (1900 lignes), et les sections 2D en
   particulier. Ne rien sonder avant.
2. **Monter la session Flycast + GDB** et vérifier qu'un point d'arrêt sur `dessine` se déclenche. Sans ça,
   rien de la suite ne se mesure.
3. Questions 1 et 2, qui vont ensemble : les écrans et leurs transitions.
4. Question 3 : la liste verticale et la marque de la ligne choisie.
5. **Trancher la divergence des libellés** — et seulement là, décider si notre image et nos sept lignes
   tiennent.
6. Question 4, ligne par ligne, du plus simple au plus obscur.
7. Alors seulement : ce qu'on change dans le portage, et dans quel ordre.

## Ce que ce plan ne fait pas

- **Aucune ligne du code Dreamcast ne passe dans le dépôt.** Les adresses, les tables et les mesures sont des
  faits sur le jeu ; le code, non. Même règle que pour Supermodel et MAME.
- **Rien ne change dans le portage avant la question 3 close.** Notre menu marche ; on ne le défait pas pour
  suivre une lecture en cours.
- **On ne conclut pas d'un coup de sonde.** Celui du 2026-10-22 a produit une affirmation fausse en trois
  minutes. Une conclusion sur ce binaire demande deux lectures indépendantes, ou une lecture plus une mesure
  sous GDB.
