# Le son de la liste des modes — le plan, avant d'écrire

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
