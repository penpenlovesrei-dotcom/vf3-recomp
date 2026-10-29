# Le son de la liste des modes — le plan, avant d'écrire

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [scsp/docs/sound.md](son.md).

Écrit le 2026-10-21, après avoir **défait** une première version des bruitages posée le jour même.

Ce document n'est pas une description de ce qui existe : rien de tout ça n'est écrit. C'est la liste des
questions auxquelles il faut une réponse **mesurée** avant qu'une ligne soit tapée, et dans quel ordre.


## Pourquoi la première version a été défaite

Elle marchait sur l'essai qui l'avait accompagnée et pas à la manette. Trois rustines en une heure, chacune sur
un retour de Frédéric — « pas de bruitage », « la musique ne s'arrête pas », « ça grésille » — et à la fin
« beaucoup de bugs sonores et de comportement, je ne saurais les énumérer ».

Quatre trous nommés en relisant, dont deux certains :

1. **Échap par-dessus la liste tuait la musique pour de bon.** La liste se referme par quatre chemins ; trois
   remettaient la musique, le quatrième (`main.cpp`, le menu ouvert par-dessus) ne la remettait pas et jetait
   au passage le son qui attendait d'être joué.
2. **Start en plein combat coupait tout le son, définitivement.** La demande d'écran titre portait un `FF`, et
   en mode `GAME` le séquenceur d'attraction ne tourne pas : personne ne renvoie jamais de bloc de sons.
3. **Start répété hors de l'écran titre envoyait un `FF` par appui**, et `FF` relance un son encore en cours
   avant de le couper (mesuré sur la ROM en août) : deux appuis rapprochés font un hoquet.
4. Et dessous : la liste **gèle le PowerPC** pendant que la carte son tourne seule, le reset part dans le dos
   du jeu, la musique est remise à la main. Un état hybride pour lequel ce portage n'a pas été bâti.

La leçon n'est pas « il fallait mieux coder » : c'est qu'**un son ne se vérifie pas en regardant une image**,
et qu'une rustine par symptôme ne converge pas. D'où ce document.


## Ce qui est acquis, et qui ne sera pas remesuré

Tout ça vient du travail sur la ROM (`SupermodelSrc\Docs\MenuConsole.md` §5, `Src\Menu\GameFV2.h`) et a été
revérifié ici pendant la version défaite :

| | |
|---|---|
| le protocole | `A0 <banque> <index>` dans un seul port ; pas de DSB sur `fvipers2` |
| les banques | `0x10` musiques, `0x2B` et `0x30` voix, `0x2E` effets |
| ouverture et choix | `A0 2E 23` — 388 `sd_sign_16` |
| déplacement | `A0 2E 26` — 391 `sd_ketsu_00` |
| annulation | `A0 2E 27` — 392 `sd_sonic00_11` |
| arrêt | `FF`, le reset système MIDI, que le jeu envoie lui-même en tête de chaque bloc |
| le piège du reset | `FF` **relance** un son encore en cours avant de le couper |
| le délai d'arrêt | 80 images, pour que le reset survive au bip d'ouverture |
| le retour de la musique | 20 images après la fermeture, et **seulement sur annulation** |
| la dernière musique | se lit en retenant le dernier `A0 10 nn` que le jeu a demandé |
| faire tourner la carte seule | possible : son 68000 et son SCSP sont déjà sur leur propre fil |
| la cadence | 57,52 images par seconde, en accumulateur — pas « une par passe si c'est dû » |

Mesuré ici et gardé : la carte son tourne bien sous un jeu gelé (57,5 à 57,8 par seconde, file de 4 à 9,5 ko),
le `FF` arrête bien la musique (pic 6201 avant, 0 après), et la musique remise était bien `A0 10 07`.


## Les questions à trancher AVANT d'écrire

### 1. Faut-il geler le jeu sous la liste ?

C'est la question mère, et les trois autres en dépendent. Trois réponses possibles, et aucune n'est gratuite :

- **geler le PowerPC, faire tourner la carte son** (ce qui a été défait) : l'image reste l'écran titre, mais le
  jeu et son pilote de son sont dans deux temps différents ;
- **tout geler** (ce qui est en place aujourd'hui) : aucun hybride, et la liste est muette ;
- **ne rien geler** : le son redevient le chemin normal du portage, rien de spécial à écrire — mais l'écran
  titre s'en va au bout de 192 images et la liste se retrouve par-dessus une démonstration.

À mesurer pour trancher : *qu'est-ce que le jeu fait vraiment pendant que la carte son tourne seule ?* La
question n'a jamais été posée. Un relevé des octets MIDI que le jeu envoie, image par image, avant et après un
gel, la dirait.

### 2. Que doit-il se passer à chaque sortie de la liste ?

Il y a quatre sorties et il faut une ligne pour chacune, écrite avant d'être codée :

| sortie | la musique | le son d'accompagnement |
|---|---|---|
| un mode choisi (ARCADE, SURVIVAL) | ? | ? |
| une ligne sans levier | — (la liste reste) | ? |
| annulation (B) | ? | ? |
| Échap, le menu du portage par-dessus | ? | ? |

Le trou n° 1 ci-dessus est exactement une case vide de ce tableau.

### 3. Que doit faire Start **hors** de l'écran titre ?

Aujourd'hui : il demande l'écran titre, et rien d'autre. La version défaite y ajoutait un `FF`, ce qui a créé
le trou n° 2. Les cas à distinguer, parce qu'ils ne se ressemblent pas :

- depuis l'attraction (logo, démonstration, classements) : le séquenceur tourne, il renverra un bloc de sons
  au prochain changement d'écran ;
- depuis l'écran de sélection : à mesurer ;
- **depuis un combat** : le séquenceur ne tourne pas. Si on coupe, personne ne rallume. Et la partie court
  toujours sous l'écran titre — le trou connu, documenté dans `docs/fv2-etats.md`.

### 4. Comment chaque pas se vérifie-t-il, sans oreilles ?

La règle qui manquait. Pour chaque pas : **une mesure qui distingue le pas réussi du pas raté**, et pas
seulement « le programme n'a pas planté ».

Les instruments existent déjà dans l'hôte : `VF3_QUIT_AFTER_S` (finir à l'horloge), `VF3_KEYS_S` (envoyer une
touche à telle seconde, le seul moyen de piloter quelque chose dessiné sur un jeu gelé). Il manque :

- un relevé des octets MIDI que le **jeu** envoie, image par image ;
- un relevé du pic de chaque image **des deux côtés** du gel ;
- et surtout un essai qui pose la question dans un état où la réponse est lisible. Le premier essai de la
  version défaite ouvrait la liste sur le **premier** écran titre, celui qui suit le logo SEGA, où il n'y a que
  la voix de l'annonceur — qui s'arrête toute seule. Le silence après le reset n'y prouvait rien : une musique
  arrêtée et une musique finie s'y ressemblent. Il a fallu refaire l'essai sur un écran titre venant d'une
  **démonstration** pour que la mesure veuille dire quelque chose.


## Étape 1, faite le 2026-10-21 : ce que le jeu dit vraiment à sa carte son

Relevé avec `VF3_MIDI_LOG=N` : tout bloc d'octets que **le jeu** envoie, avec le mode où il était et le pic de
l'image précédente ; plus une ligne toutes les N images, pour que le silence soit au dossier lui aussi. Rien
n'est modifié par ce relevé.

### Il parle très peu, sauf en combat

| état | blocs envoyés |
|---|---|
| avertissement | 1 (image 82) |
| attraction, logo, titre, démo | 4 en 1250 images |
| `SEL_INT` / `SEL_DSP`, la sélection | 3 |
| `GAME_DSP`, un combat | **55 en 1700 images** |

La boucle d'attraction entière, 73 secondes, tient en **six blocs**, un par changement d'écran. Un combat, lui,
parle sans arrêt.

### Ce qu'il envoie dans la boucle

```
image   82  mode 0            FF                                    le seul FF de toute la partie
image  997  ADV               A0 00 01
image  999  ADV_SEGA_INT      A0 00 01  A0 30 00                     la voix « SEGA »
image 1193  FV2_TITLE_DSP     A0 2B 00                               la voix « Fighting »
image 1247  FV2_TITLE_DSP     A0 2B 05                               la voix « Vipers 2 »
image 1385  ADV_DEMO_DSP      A0 10 07                               la musique de la démo
image 1257  SEL_INT           A0 2E 24  A0 00 01  A0 10 00  A0 2B 02  effet, silence, musique, voix
```

Trois choses en découlent, et aucune n'était prévue :

1. **Le jeu n'envoie jamais `FF`**, sauf une fois à l'amorçage. Sa commande de silence à lui est `A0 00 01`,
   et il la met en tête de ses blocs exactement là où le relevé d'août voyait `FF`. La ligne de ce document
   qui disait « le jeu l'envoie lui-même en tête de chaque bloc » était vraie du relevé d'août, pas d'ici.
2. **L'écran titre n'a pas de musique à lui** — deux voix, rien d'autre. La musique qu'on y entend est celle de
   la démonstration, laissée en route. C'est exactement ce que Frédéric a signalé.
3. **L'écran de sélection pose lui-même sa musique** (`A0 00 01` puis `A0 10 00`, image 1257). Donc remettre la
   musique après un mode choisi est non seulement inutile, c'est nuisible — et ça explique la mesure d'août où
   l'écran de sélection héritait de la musique de la démo.

### Les deux commandes d'arrêt, mesurées côte à côte

Fond : la musique de la démo à l'image 2000, pic moyen 6025 sur les 360 images suivantes.

| envoyé à l'image 2000 | pic moyen ensuite |
|---|---|
| rien (témoin) | **6025** |
| `A0 00 01` | **0** |
| `FF` | **0** |

Et le piège du reset, repris avec le bon instrument — un bip `A0 2E 23` sur fond silencieux, puis la commande
d'arrêt 40 images plus tard, relevé toutes les 4 images :

```
image   bip seul   bip + A0 00 01   bip + FF
 2040       3162             3162       3162
 2044       3119                0          0
 2072        865                0          0
 2076        100                0          0
```

**Ni l'une ni l'autre ne relance le bip** : les deux le coupent net. Le piège mesuré en août sur Supermodel ne
se reproduit pas sur la carte son de ce portage. Donc **le délai de 80 images ne sert à rien ici**, et l'arrêt
peut partir dès l'ouverture. (Mesuré sur ce portage, pas sur la vraie carte : c'est ce portage qu'on livre.)


## Étape 2 : la réponse à la question mère

**Geler le jeu et faire tourner la carte son est sans risque là où la liste s'ouvre.** Entre l'image 1247 et
l'image 1385 — tout l'écran titre — le jeu ne dit **rien** à sa carte. Il n'y a donc rien à rater pendant le
gel : ce n'est pas un dialogue interrompu, c'est un silence prolongé.

Ce qui avait cassé dans la version défaite n'était donc pas le principe, et c'est important de le dire
exactement : c'était **la cadence** (une image de son une passe sur deux, trente par seconde au lieu de 57,5)
et **les cas non traités** (quatre sorties, trois seulement remplies). Pas l'idée de faire tourner la carte.

**En revanche, geler pendant un combat serait une autre affaire** : 55 blocs en 1700 images. Un menu de pause
qui gèle un combat couperait un dialogue en cours. C'est noté pour le jour où ce menu-là sera écrit ; la liste
des modes, elle, ne s'ouvre que sur l'écran titre.


## Étape 3 : le tableau des quatre sorties, à valider avant d'écrire

Chaque case est justifiée par une mesure ci-dessus, et aucune n'est un avis.

| sortie | la musique | le son |
|---|---|---|
| un mode choisi (ARCADE, SURVIVAL) | **rien** : l'écran de sélection pose la sienne, image 1257 | le son de choix |
| une ligne sans levier | — (la liste reste ouverte) | **rien** : il ne s'est rien passé |
| annulation (B) | remettre ce qui jouait, si on l'a arrêté | le son d'annulation |
| Échap, le menu du portage par-dessus | **remettre aussi** — c'était le trou n° 1 | rien |

Et l'arrêt lui-même : `A0 00 01`, la commande du jeu, **à l'ouverture et sans délai**, puisque le piège du
reset ne se reproduit pas ici. Pas de `FF` : le jeu ne s'en sert pas.

Pour « remettre ce qui jouait » : le dernier `A0 10 nn` que le jeu a demandé, qui se lit au passage dans les
octets qu'il envoie. Mesuré : c'était bien `A0 10 07`, la musique de la démo.


## L'ordre

Un pas, une mesure, une capture ou un relevé montré. Pas deux pas d'un coup — c'est ce qui a été payé deux fois
sur ce chantier.

1. **Relever ce que le jeu envoie**, image par image, dans les états qui comptent (attraction, titre, démo,
   sélection, combat) et sous un gel. Rien n'est écrit tant que ce relevé n'existe pas.
2. **Répondre à la question 1** avec ce relevé, et l'écrire ici.
3. **Remplir le tableau de la question 2**, case par case, avant de coder.
4. Alors seulement : les trois bruitages, **sans toucher à la musique**. Mesure : le pic monte sur le
   déplacement là où il était à zéro.
5. La musique, **si** l'étape 2 dit que c'est tenable. Mesure : la musique joue avant, ne joue plus après, et
   revient là où le tableau dit qu'elle revient.
6. `verifier-vf3.cmd` à chaque étape, et une partie jouée à la main par Frédéric avant de passer à la suivante.


## Étape 4, faite le 2026-10-22 : les trois bruitages, sans toucher à la musique

Frédéric a demandé de **sauter l'étape 3** — le tableau des quatre sorties — et de poser les bruitages. C'est
exactement l'étape qui n'en dépend pas : elle ne touche pas à la musique, donc aucune case de ce tableau ne
la commande.

### Ce qu'il a fallu d'abord : faire tourner la carte sous l'écran gelé

Une surface de la console gèle le PowerPC. Un PowerPC gelé ne lance aucune image de son, donc **rien ne sort**,
pas même une note qu'on lui tend. Il a donc fallu que l'hôte tourne la carte lui-même : `Game::sound_only_frame`,
qui est la moitié sonore de `finish_frame` sans rien toucher à l'état de la carte.

**Par un accumulateur, et c'est tout l'enjeu.** La version défaite lançait une image de son par passe, condition
sur 17,38 ms écoulées, ce qui donnait trente par seconde sur un écran à 60 Hz là où la carte en veut 57,5 : la
file se vidait et ça grésillait. Ici le temps est mesuré et les images dues sont faites, quel que soit le
nombre de passes — avec un plafond de quatre, pour qu'une fenêtre traînée ne déclenche pas un flot.

Et **seulement sous les surfaces de la console** : jamais sous le menu de la touche Échap ni sous une pause, qui
sont ceux du portage et ont toujours été muets.

### Les trois sons

Ceux du jeu, trouvés en août en envoyant des notes et en écoutant. Leurs numéros sont ceux de `GameFV2.h` :

| ce qui arrive | la note | le son |
|---|---|---|
| déplacement | `A0 2E 26` | `sd_ketsu_00`, n° 391 |
| choix, et ouverture | `A0 2E 23` | `sd_sign_16`, n° 388 |
| annulation | `A0 2E 27` | `sd_sonic00_11`, n° 392 |

Chaque surface dit **ce qui est arrivé** (`want_sound`) ; l'hôte, qui possède la carte, dit **à quoi ça
ressemble**. Aucune des deux n'a à connaître le métier de l'autre. Le drain se fait une fois par passe et non
là où les événements sont distribués, parce que ces blocs-là font `continue` sur chaque touche : une note
remise à l'intérieur ne serait partie qu'à l'événement suivant.

### La mesure, qui est la règle de ce chantier

`VF3_MENU_SOUND=1` relate ce que la carte produit sous la surface. Trois `Down` envoyés à l'horloge, liste
ouverte :

```
menu sound: 11.45 s,  660 images (57.6/s), pic 0
menu sound: 12.32 s,  710 images (57.6/s), pic 2880
menu sound: 12.33 s,  711 images (57.6/s), pic 4417
menu sound: 12.35 s,  712 images (57.6/s), pic 4289
menu sound: 12.37 s,  713 images (57.6/s), pic 3110
menu sound: 12.38 s,  714 images (57.6/s), pic 678
menu sound: 12.40 s,  715 images (57.6/s), pic 279
menu sound: 12.48 s,  720 images (57.6/s), pic 0
```

Les deux autres pressions donnent **la même bouffée, au même chiffre près**. Donc :

- **la cadence est 57,6/s**, celle de la carte, et non les 30/s de la version défaite ;
- **le pic monte sur le déplacement là où il était à zéro**, qui est mot pour mot la mesure que l'étape 4
  exigeait ;
- **le silence revient**, donc rien ne reste accroché.

### Ce qui n'est toujours pas fait

L'étape 3 et l'étape 5 : **la musique n'est pas touchée**. Elle ne s'arrête pas à l'ouverture, elle n'est pas
remise à la sortie, et `A0 00 01` n'est envoyé nulle part. Le tableau des sorties reste à remplir, et il lui
faut maintenant six cases de plus que lorsqu'il a été écrit : se déplacer dans `OPTIONS`, franchir une porte,
remonter d'un écran, changer une valeur, `INITIALIZE`, et sortir d'un écran enfant.


## Ce que Start fait vraiment à la démonstration — mesuré le 2026-10-23

Frédéric a corrigé le cadrage de tout ce document : « *ce n'est pas que la musique qui doit être arrêtée
quand on passe de la démo à l'écran titre en appuyant sur Start, c'est toute la démo qui est coupée, dont la
musique. Au sortir de l'écran titre, la démo et sa musique doivent recommencer au début.* »

Mesuré, témoin compris, avec `VF3_MIDI_LOG` et `VF3_RAMLOG=580DFC:1,58FD99:1`.

### La première moitié est déjà vraie, et ce n'est pas nous qui la faisons

| image | sous-état | ce que le jeu envoie |
|---|---|---|
| 1385 | `0B` démo | `A0 10 07`, la musique de la démo |
| **1450** | | **Start : le portage demande l'écran titre** |
| 1453 | `02` | `A0 00 01  A0 30 00` — **son propre silence**, puis la voix « SEGA » |
| 1455 | `05` titre | `A0 2B 00`, « Fighting » |
| 1523 | `05` titre | `A0 2B 05`, « Vipers 2 » |

Le pic avant l'image 1453 est 6816 — la musique de la démo — et zéro après. **La démo est bien coupée, musique
comprise, trois images après Start, et c'est le jeu lui-même qui le fait** en redémarrant sa boucle. Il n'y a
donc rien à écrire pour ça, et l'étape 5 de ce plan avait mal posé la question : elle parlait d'arrêter une
musique là où le jeu coupe une démonstration.

### La seconde moitié n'est pas un défaut — **corrigé le 2026-10-23**

J'avais consigné ici un « vrai défaut ». Il n'y en a pas : Frédéric a répondu que **l'alternance entre le
replay et la démo à la sortie de l'écran titre est normale**. C'est ce que fait la borne, et ce que faisait
déjà le jeu avant qu'on y touche.

Ce que la mesure avait vu est donc exact mais mal interprété :

```
témoin (sans Start)   01 02 03 04 05(titre) 0B(démo)
avec Start à 1450     01 02 03 04 05(titre) 0B(démo) 00 01 02 03 04 05(titre) 08 09 0A(replay)
```

Le deuxième passage par l'écran titre donne le replay parce que c'est **au tour** du replay, pas parce que
notre redémarrage aurait dérangé quelque chose. Et `0x00580DFC`, l'index de la liste, vaut bien 0 dans les
deux cas : il n'avait rien à voir avec ce choix non plus.

### Ce qui décide, puisqu'on l'a trouvé en chemin

La table de ce document disait déjà où regarder — « *`a FV2_TITLE_DSP = B ou = 8` : vers la démonstration, ou
vers le replay* » — et la règle d'ouvrir les documents avant de mesurer a tenu : il a suffi de deux
instructions à trouver, pas d'une soirée.

`vf3run --watch 58FD98` donne les deux sites : `0x0000B480` écrit `0B`, `0x0000B4C4` écrit `08`. Et le test qui
choisit est juste avant :

```
0000B450  lis   r3,0x0029
0000B454  lbz   r3,10517(r3)      ; l'octet 0x00292915
0000B458  cmpwi r3,1
0000B45C  bne   0x0000B4B8        ; different de 1 -> REPLAY, qui ecrit 08
                                  ; egal a 1      -> DEMO,   qui ecrit 0B
```

**Un seul octet, `0x00292915`, commande l'alternance.** À 1 c'est la démonstration, sinon c'est le replay.

Et il **bascule tout seul, à l'entrée de l'écran titre** (sous-état 4), ce qui est l'alternance même.
`VF3_RAMLOG=292915:1,58FD99:1`, les deux passages d'un même run :

```
image 1193  octet = 01  sous-etat 04   premier titre   -> 1386  sous-etat 0B  la demo
image 1647  octet = 00  sous-etat 04   second titre    -> 1850  sous-etat 08  le replay
```

Le jeu le retourne lui-même à chaque fois qu'il arrive sur son titre. Notre redémarrage n'y touche pas et n'a
pas à y toucher.

Et un voisin, pour le dossier : juste au-dessus, `0x0000B444` lit l'octet 55 du bloc des réglages — soit
`0x00120557`, **`COUNTRY`** — et s'il vaut 1 (`U.S.A.`) part sur le sous-état 6, `ADV_RATING_INT`, l'écran de
classification. La région commande donc un écran d'attraction de plus, ce qui n'était écrit nulle part.

### Ce que ça laisse à faire

**Rien sur ce chemin.** Start coupe la démo, le jeu fait son ménage, et ce qui repart ensuite est ce qui devait
repartir. L'étape 5 garde son seul vrai sujet : **la musique qui continue de jouer sous la liste ouverte**.


## Deux défauts entendus, et pourquoi la question était mal posée — 2026-10-23

Frédéric a joué et rapporté deux choses : « *si on interrompt la démo ou le replay en appuyant sur start,
l'écran titre apparaît mais avec le son du logo SEGA* », et « *les bruitages du menu ne se jouent pas en
direct mais en décalé* ». Les deux sont réels, et aucun n'était dans mes mesures — parce que mes mesures
lisent ce que la carte **produit**, pas ce qui sort des haut-parleurs.

### 1. La voix du logo arrivait sur l'écran titre

Elle était dans mon propre relevé depuis la veille, sans que je l'entende : à l'image 1453, trois images après
Start, le jeu envoie `A0 00 01  A0 30 00` — son silence, **puis la voix « SEGA »**. Le portage saute l'image du
logo et demande le titre ; il ne sautait pas sa voix. **Sauter une image et garder son son, c'est sauter à
moitié.**

Corrigé en retirant les notes de la banque `0x30` **sur la seule route où le logo est sauté** — `skip_logo > 0`
dans `console_play`, c'est-à-dire exactement là et nulle part ailleurs. Les voix du titre (`A0 2B 00`
« Fighting », `A0 2B 05` « Vipers 2 ») ne sont pas touchées : cet écran-là est bien montré.

Mesuré, liste ouverte juste après : le pic passe de **8190, 10588, 10396, 8128… soutenu** à **2585 qui retombe
à 200 en 0,13 s** — ce qui reste étant la fin de « Fighting », qui doit rester.

### 2. Les bruitages étaient en retard, et ce n'était pas la carte

La carte les faisait à l'heure : mesuré à 57,6 images par seconde, le pic monte sur l'image de la touche. Ce
qui était en retard, c'est **la file audio de SDL**, dans laquelle le jeu avait pris de l'avance avant de
geler. Un bip placé derrière une seconde de retard sort une seconde plus tard, et rien ne vide ce retard
pendant le gel, puisque la pompe ne fabrique que ce que le temps réel demande.

Corrigé en **vidant la file à l'entrée de la surface** : le menu part d'un silence, et ses sons sont immédiats.

### 3. Et la musique : la question était bien mal posée

Mesuré sur la route que Frédéric décrit — Start coupe la démo, la liste s'ouvre sur l'écran titre qui suit :

```
aucune ligne « the console's music » : rien ne jouait, donc rien n'a ete arrete
```

**Il n'y a pas de musique à arrêter sous la liste.** Le jeu l'a déjà coupée en redémarrant sa boucle, trois
images après Start. Tout l'échafaudage « arrêter, puis remettre à la sortie » répond à une question qui ne se
pose pas sur ce chemin.

Il est écrit et mesuré, et il **se tait** : il suit la musique dans les octets du jeu (`A0 10 nn` la pose,
`A0 00 01` l'efface), et si rien ne joue il ne fait rien et le dit dans le journal. Il resterait utile le jour
où la liste s'ouvrirait sur un écran où une musique tourne. Sur la boucle d'attraction telle qu'elle est, il
ne se déclenche jamais.


## L'écran de victoire : un son qui naît et qui enfle — RÉSOLU le 2026-10-24

Frédéric : « ce son qui débute à l'écran WIN et qui augmente pour être très fort, **n'est pas normal** ».
Il a raison, et ce n'est ni un jingle fort ni un volume mal réglé. Voici tout ce qui est mesuré, tout ce qui
est éliminé, et où ça s'arrête.

### Le phénomène, chiffré

Mesuré avec `VF3_MIDI_LOG=1`, qui donne par image le mode, le sous-état et le pic du son fabriqué, sur de
vraies parties de Frédéric menées jusqu'au bout. Le sous-état est **18, `VIC_DSP`** (`docs/fv2-etats.md`) —
la victoire de *match*, pas la fin de round.

À `--volume 25`, donc gain exactement 1 et **limiteur hors circuit**, le niveau brut du banc de son :

| état | images | pic brut | % du plein régime |
|---|---|---|---|
| **`VIC_DSP` (18)** | 256 | **13805** | **42,1 %** |
| `JUDGE_DSP` (14) | 384 | 4107 | 12,5 % |
| `FIGHT_DSP` (12) | 2003 | 3942 | 12,0 % |
| `REPLAY_DSP` (16) | 510 | 3441 | 10,5 % |
| attraction | 1478 | 4117 | 12,6 % |

Tout le jeu vit entre 10 et 13 % ; la victoire est à 42 %. Et elle **n'y arrive pas d'un coup** : le son naît
à 2,3 % à l'entrée de l'état et monte à 36 % en moins de deux secondes — **24 dB** — puis plafonne autour de
33–41 % jusqu'à ce que le jeu envoie son propre silence (`A0 00 01`). Une commande `A0 11 0D` arrive au milieu
de la montée ; rejouée seule hors de cet état (`VF3_MIDI_SEND=A0110D@1500`), elle ne produit rien.

### Pourquoi ça s'entend si mal

`LEVEL = 4.0` dans `src/host/main.cpp` relève la sortie du banc de **12 dB**, au motif écrit dans son
commentaire qu'elle « culmine bien en dessous du plein régime » — vrai de Virtua Fighter 3, pour qui il a été
choisi. À 42 % de brut, la victoire demande 168 % du plein régime, et le limiteur
(`CEILING = 0.891`, soit −1 dBFS = **29196**, attaque instantanée, retour en 0,1 s) retire 4,7 dB en continu.
Mesuré : **120 images collées exactement sur 29196**. Un limiteur qui travaille à fond pendant deux secondes,
c'est ce qu'on entend.

Le limiteur n'est donc pas le défaut, c'est le révélateur. Baisser `LEVEL` pour FV2 **masquerait** la montée
au lieu de l'enlever : c'est explicitement ce que Frédéric a refusé, et il avait raison.

### LE COUPABLE EST NOMMÉ : deux voix, c0/s18 et c0/s19

Les deux chips portaient déjà la mesure (`scsp.h` : `m_probe_direct`, l'énergie que chaque emplacement a
envoyée) et **rien ne l'allumait**. Elle est maintenant câblée : `SoundBoard::energy_on` / `energy_take`, et
`VF3_ENERGY_LOG=N` dans l'hôte imprime toutes les N images les voix qui sonnent, leur énergie totale, et les
six plus fortes avec leur part. Sur une partie de Frédéric :

```
 image | voix | total     | les plus fortes
  4724 |   13 |  2.98e+09 | c0/s13 31% c0/s17 16% c1/s24 14%
  4748 |    5 |  2.12e+10 | c1/s12 39% c0/s18 32% c0/s19 20%
  4796 |    4 |  5.34e+10 | c0/s19 56% c0/s18 40%
  4892 |    4 |  7.28e+10 | c0/s19 61% c0/s18 37%
  4940 |    4 |  1.49e+11 | c0/s19 57% c0/s18 42%
```

**Les emplacements 18 et 19 du premier chip portent 95 à 99 % de l'énergie, et leur total est multiplié par
cinquante — 17 dB — en une seconde et demie.** Ce n'est pas un empilement de voix : c'est une **paire** qui
s'emballe pendant qu'elle joue. Deux voix dont la sortie croît sans qu'on leur redonne d'ordre, c'est la
signature d'une modulation croisée : la modulation lit `m_RINGBUF`, qui contient les sorties récentes, donc
une voix peut se moduler avec le son de l'autre — une boucle de réinjection, dont la profondeur est le gain.

### Quatre hypothèses mesurées et tombées

1. **Les trois changements du SCSP de l'amont** (profondeur FM doublée, bit bas de `PLFOS` retiré, minuteries
   recalées). Deux parties comparées, banc d'avant et banc d'après : pic 29196 des deux côtés, 182 contre 181
   images au-dessus de 20000. **Cette comparaison ne vaut rien** et c'est une erreur à ne pas refaire : les
   deux parties étaient à volume plein, donc **toutes deux contre le plafond du limiteur**, où un écart de
   10 dB dans le signal brut donne le même chiffre. L'amont n'est ni accusé ni innocenté ; il faut refaire la
   comparaison à `--volume 25`. Le binaire est prêt : `vf3-son-avant.exe` dans le dossier de travail.
2. **L'accumulation dans les registres d'effet du DSP.** `EFREG[EWA] += SHIFTED >> 8` dans
   `src/sound/scspdsp.cpp`, et `EFREG` n'est remis à zéro qu'à l'initialisation du chip, jamais entre deux
   échantillons. Les vider à chaque échantillon (interrupteur d'essai) **ne change rien du tout** : pic
   13805 et moyenne 8234, contre 13805 et 8238 sans. Donc non seulement ce n'est pas la cause, mais **le DSP
   ne contribue pratiquement rien** à ce son. L'interrupteur a été retiré. (Le code vient de MAME et la copie
   de référence `fv2-stable` a la même ligne.)
3. **Des voix qui s'empilent sans se libérer.** `VF3_KEYON_LOG` : 81 voix démarrées pendant les 256 images de
   `VIC_DSP`, réparties sur des emplacements différents, échantillons variés. Ordinaire.
4. **Les enveloppes.** Le journal des voix imprime maintenant leur forme (`tl`, `ar`, `d1r`, `d2r`, `rr`,
   `dl`, `eghold`, `lpctl`, `lsa`, `lea`) :

   | | voix | `rr`=0 | `d2r`=0 | boucle | `tl` moyen |
   |---|---|---|---|---|---|
   | `VIC_DSP` | 81 | 8 | 81 | 81 | 49 |
   | `FIGHT_DSP` | 1153 | 36 | 1153 | 1153 | 58 |
   | attraction | 693 | 0 | 648 | 693 | 51 |

   Tout le jeu joue en boucle sans décroissance de maintien : c'est sa manière. Les voix de la victoire sont
   un peu plus fortes (`tl` 49 contre 58, quelques dB) et un peu plus nombreuses à ne jamais relâcher. Rien
   qui approche 24 dB.

### Ce qui a été essayé comme correction, et retiré

Rendre à Fighting Vipers 2 les profondeurs de MAME, en ne laissant celles de Supermodel qu'à Virtua Fighter 3
— elles sont, de l'aveu du commentaire, « as Supermodel's, **set by ear for VF3** », et doubler la profondeur
d'une boucle de réinjection est exactement ce qui fait passer une résonance bornée à une montée jusqu'à
saturation. Construit, joué : **le défaut est toujours là**. Le changement a donc été **retiré** plutôt que
gardé — un changement de son non mesuré ne reste pas dans l'arbre. Le raisonnement, lui, reste valable et
mérite d'être reprisé avec la bonne mesure (point 1 ci-dessus).

### Où reprendre

La question est maintenant étroite : **deux emplacements, 18 et 19 du premier chip, pendant `VIC_DSP`.**

1. Lire leurs registres — `SoundBoard::reg_word(0, 0x000 + slot * 0x20 ...)` donne les mots d'un emplacement —
   et regarder `MDL`, `MDXSL`, `MDYSL` : est-ce que 18 et 19 se modulent l'une l'autre, ou elles-mêmes ?
2. Refaire la comparaison avant/après fusion **à `--volume 25`**, la seule qui ait un sens.
3. Si c'est bien une paire en réinjection, le gain de la boucle est dans `smp >>= 0x18 - (MDL & 0xE)` et dans
   l'indexation de `m_RINGBUF` (`SCSP_FM_DELAY`) : c'est là qu'il faut mesurer, pas à l'oreille.

### Les outils laissés en place

- `VF3_ENERGY_LOG=N` — quelle voix fait le son, par image (`energy_on` / `energy_take`, `soundboard.h`).
- `VF3_KEYON_LOG=fichier` — chaque voix démarrée, **avec sa forme** d'enveloppe et de boucle.
- `VF3_MIDI_LOG=N` — ce que le jeu envoie au banc, avec son état et le pic de l'image précédente.
- `tools/scripts/fv2-vic-banc.txt` — une route qui **n'aboutit pas** : elle entre bien en combat dans
  `vf3run` (crédits payés, V.S. choisi) et le chrono du round forcé à zéro (`VF3_POKE` sur `0x00581404`) fait
  bien finir le round, mais le jeu reste ensuite dans `JUDGE_DSP` et `VIC_DSP` n'est jamais atteint. Et
  `vf3run` tourne à 14 images par seconde sur ce jeu : six minutes par essai. **Un banc hors-jeu pour cet
  état reste à trouver** ; en attendant, la seule route qui y mène est une partie jouée à la main.

### Ce que MAME et Supermodel font de ce défaut (relevé le 2026-10-24)

Frédéric a demandé si le défaut existait déjà dans les émulateurs et comment ils l'avaient résolu. **Oui, et
sur ce jeu précisément.**

- **Supermodel a porté le même problème, et l'a nommé.** Son option `LegacySoundDSP` est documentée comme
  « legacy SCSP **DSP** implementation for games that do not play well with the newer one (**e.g. Fighting
  Vipers 2**) », et elle se règle **par jeu** — Fighting Vipers 2 et Sega Rally 2 sont les exemples cités. Le
  « nouveau » moteur est celui de MAME, porté chez eux en r814 ; l'ancien est celui d'ElSemi. Donc quand le
  DSP de MAME est entré, ce jeu a eu besoin qu'on revienne à l'ancien.
- **Puis ils l'ont corrigé pour de bon.** Le `Src/Sound/SCSP.cpp` actuel a supprimé le chemin d'ElSemi, et son
  en-tête dit pourquoi : Fighting Vipers 2 « used to need ElSemi's older DSP register handling, which has been
  removed now that **DSP memory access follows the manual** ».
- **Trois écarts avec notre modulation**, chez eux tirés du manuel (table 4.17) et non de l'oreille :

  | | Supermodel (manuel) | nous (MAME + amont) |
  |---|---|---|
  | quand la modulation s'applique | `MDL` **5 à F** seulement ; 0 à 4 = aucune | dès que `MDL`, `MDXSL` **ou** `MDYSL` est non nul |
  | le décalage | `>> (0x19 - MDL)` | `>> (0x18 - (MDL & 0xE))` |
  | le décalage d'adresse obtenu | **borné à 11 bits signés** | **non borné** |

  Le troisième est le plus suspect pour ce qu'on mesure : un décalage non borné lit hors de l'échantillon
  prévu, et c'est le genre de chose qui fait grossir une voix en bruit.
- **Et MAME a corrigé un défaut d'adressage du DSP que nous gardons actif.**
  [mamedev/mame#16527](https://github.com/mamedev/mame/pull/16527), « Don't mirror MADRS at 0x7c0-0x7ff,
  fixes corrupted and then silent sound in doa ». Or `src/sound/scsp.h` garde ce miroir **par défaut** :

  ```cpp
  const bool m_madrs_mirror = [] { const char *e = getenv("VF3_SCSP_MADRS_SEPARATE"); return !(e && *e == '1'); }();
  ```

  Le comportement corrigé de MAME n'est donc accessible que par une variable d'environnement, et ce portage
  fait tourner par défaut ce que MAME a réparé — dans la zone même que Supermodel dit avoir dû mettre en
  conformité avec le manuel.

**La première chose à essayer est donc `VF3_SCSP_MADRS_SEPARATE=1`** : une variable sur le binaire existant,
et c'est la correction de MAME. Tout ceci est aussi dans l'issue ouverte à cette occasion,
[penpenlovesrei-dotcom/vf3-recomp#1](https://github.com/penpenlovesrei-dotcom/vf3-recomp/issues/1) — les
issues du fork étaient désactivées et ont été activées pour elle.

Sources : le `LegacySoundDSP` des configurations publiées de Supermodel, la documentation du cœur
Libretro-Supermodel (moteurs « MAME-derived » et « ElSemi »), l'en-tête de `Src/Sound/SCSP.cpp` de
`trzy/Supermodel`, et la demande d'intégration MAME ci-dessus.

### RÉSOLU : le miroir de `MADRS` écrasait les lignes à retard du DSP sur une seule adresse

La cause est le miroir de la table `MADRS` du DSP sur `0x7C0-0x7FF`, et **le commentaire qui l'accompagnait
dans `scsp.h` la contenait déjà sans que personne l'ait rapprochée de ce défaut** :

> « VF3's sound program sets its reverb's addresses, then clears that range, which **with the mirror leaves
> them all at one place: the reverb short** […] kept apart, VF2's way, they **rang on for several** »

Avec le miroir, **toutes les entrées de `MADRS` tombent au même endroit**, donc chacune des lignes à retard du
DSP lit et écrit le même tampon. Sur l'écran de victoire, cette boucle se nourrit d'elle-même : le son naît,
grossit de 24 dB et sature. Les deux voix nommées par la sonde d'énergie, `c0/s18` et `c0/s19`, sont les deux
prises qui se retrouvaient à la même adresse.

**La mesure, à contexte égal** — même code, même volume, deux parties de Frédéric, avec le miroir et sans :

| état | pic AVEC | pic SANS | moyenne AVEC | moyenne SANS |
|---|---|---|---|---|
| **`VIC_DSP`** | **29196** | **16506** | **22831** | **6532** |
| `FIGHT_DSP` | 16980 | 16290 | 6714 | **6979** |
| `JUDGE_DSP` | 16913 | 18386 | 8109 | 7654 |
| `REPLAY_DSP` | 14023 | 14749 | 6575 | **6923** |
| `SELECT` | 11101 | 10708 | 5800 | 5858 |

L'écran de victoire passe de 22831 de moyenne à 6532 et quitte le plafond du limiteur ; **tous les autres
états restent où ils étaient**, la moyenne du combat montant même légèrement. Frédéric, qui a joué les deux :
« le bug a été corrigé, même si je trouve que les sound effects sont plus bas ». Ce n'est pas une perte de
niveau — la mesure dit le contraire — mais la réverbération, qui n'est plus écrasée en un point et retrouve
ses vraies lignes à retard : les frappes ont une queue au lieu d'un claquement concentré.

### La correction est PAR JEU, parce que les deux mesures sont bonnes

L'amont a mesuré que le miroir est **juste pour Virtua Fighter 3** : les échos du crédit et de Start y meurent
en une seconde, comme la référence, alors que sans lui ils sonnent plusieurs secondes. Nous venons de mesurer
qu'il est **faux pour Fighting Vipers 2**. Aucune des deux mesures n'annule l'autre, donc le réglage suit le
jeu, comme `driver_known` le fait déjà :

- `SoundBoard::madrs_mirror`, posé depuis le set par l'hôte et par `vf3run` (`board.set == "vf3"`), appliqué
  au chip à sa création via `scsp_device::set_madrs_mirror`.
- `VF3_SCSP_MADRS_SEPARATE=1` et `VF3_SCSP_MADRS_MIRROR=1` forcent l'un ou l'autre, pour comparer.
- `verifier-vf3.cmd 2000` : quatre `IDENTIQUE` et 2231 images identiques — Virtua Fighter 3 ne bouge pas.

**Un piège de mesure rencontré en route, à ne pas refaire** : la première construction a échoué
(`m_madrs_mirror` était privé) et le vérificateur a tourné ensuite sur les **anciens** binaires, rendant
quatre `IDENTIQUE` qui ne portaient sur rien. Même défaut que la bissection du matin : un harnais qui mesure
un binaire périmé répond toujours « tout va bien ». Depuis, le script s'arrête sur une erreur de compilation
avant de vérifier quoi que ce soit.

### Ce qui reste ouvert

- **Personne n'a jamais mesuré la bonne longueur de réverbération de Fighting Vipers 2** contre une
  référence. L'amont a comparé VF3 à Supermodel ; nous avons comparé FV2 à lui-même. Supermodel dit avoir mis
  les accès mémoire de son DSP en conformité avec le manuel, et c'est cette référence qu'il faudrait écouter.
- Les trois écarts de modulation avec Supermodel (table 4.17) restent, et le **bornage du décalage d'adresse
  à 11 bits** reste une correction tirée du manuel que ce portage n'a pas.
- `LEVEL = 4.0`, les 12 dB choisis pour Virtua Fighter 3, s'appliquent toujours aussi à ce jeu. Ils ne
  saturent plus, mais ils n'ont jamais été mesurés pour lui.

## Le menu qui grésille : 56 ms de musique jetés à chaque ouverture (2026-10-28)

Frédéric : « parfois le son du menu principal grésille, ou plutôt a un écho ».

### Ce que les mesures ont écarté d'abord

Trois routes jouées cachées, avec `VF3_MENU_SOUND=1`, qui imprime la profondeur de la file du
périphérique image par image :

| route | file (images) | cadence |
|---|---|---|
| la liste sur l'écran titre, 65 s | min 2,7 — médiane 3,0 — max 3,4 | 57,6/s |
| la même, **tous les cœurs chargés** | min 3,0 — médiane 3,3 — max 3,6 | 58,1/s |
| le retour au menu depuis un match | médiane 3,0 | — |

**La pompe ne décroche pas**, même sous charge, et l'enregistrement du son (`VF3_SOUND_WAV`, nouveau)
ne montre ni rupture d'échantillon ni bloc répété. Ce n'était donc ni une famine de la file ni un
doublement de l'audio.

### Ce qu'elles ont trouvé

L'ouverture d'une surface **vidait la file du périphérique**, et c'est du son que le joueur n'a pas encore
entendu. La ligne était là depuis le jour où les bips arrivaient « en décalé », et le chiffre n'avait
jamais été regardé. Mesuré sur la route arcade :

```
the console: MENU_IN -> MENU (the game is home)
the console's sound: 3.2 frames (56 ms) were queued and are thrown away
```

**3,2 images, 56 ms** — ce n'est pas un retard accumulé, ce sont les trois images d'avance que le
régulateur du jeu tient exprès. Autrement dit, **56 ms de musique étaient coupés au milieu d'eux-mêmes à
chaque fois qu'un menu s'ouvrait** : un saut dans une note tenue, ce qui s'entend exactement comme un
grésillement ou un écho.

### La correction

L'excédent seulement est vidé — au-delà du double de ce que le périphérique veut — et les trois images
habituelles sont laissées tranquilles. La pompe ne fabrique plus que ce qui manque réellement
(`max(0, QUEUED - file)`) au lieu d'amorcer trois images par-dessus une file déjà pleine. Le cas que la
première version soignait vraiment, une file dans laquelle le jeu a couru loin devant, reste soigné :
c'est celui de l'avance rapide, huit images par passe.

Un bip arrive donc 56 ms après la touche, soit trois rafraîchissements — personne n'entend ça.

**Après** : plus une seule ligne « thrown away », et la file ne descend plus jamais à zéro à l'ouverture
(min 2,5 images sur 106 relevés, contre 0,0 à chaque ouverture avant). Les trois routes de
`verifier-console.cmd` passent toujours.
