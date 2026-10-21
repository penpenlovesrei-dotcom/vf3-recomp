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
