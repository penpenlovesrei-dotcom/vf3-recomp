# Les états de Fighting Vipers 2, et l'écran titre

Rien ici n'est neuf. Tout a été mesuré sur la ROM entre le 2026-08-14 et le 2026-08-20, et consigné dans
`Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md` (§6, 6 bis, 6 quater, 6 sexies) ; les adresses sont
dans `SupermodelSrc\Src\Menu\GameFV2.h`. Ce document les porte dans ce port pour qu'on cesse de les redécouvrir.

Rien n'est repris du code de Supermodel : ce sont des faits sur le jeu, pas du code.


## Le jeu dit lui-même où il en est

Quatre octets contigus, et c'est toute la machine :

```
MODE_MAIN       0x0058FD98   le mode
MODE_SUB        0x0058FD99   le sous-état du mode courant
MODE_REQUEST    0x0058FD9A   le mode demandé
SUBMODE_REQUEST 0x0058FD9B   le sous-état demandé
```

Les noms sont ceux de Sega, laissés en RAM : une suite contiguë de 80 chaînes à `0x00024494`, dont les 24
premières sont les modes. **Une table contiguë ne dit pas où ses tranches commencent** — le découpage par les
noms a été pris en défaut trois fois, et chaque tranche ci-dessous a fini par être mesurée à l'écran.

### Les 24 modes

```
 0 WARNING_INT     1 WARNING_DSP     2 ADV_INT         3 ADV_DSP
 4 SEL_INT         5 SEL_DSP         6 GAME_INT        7 GAME_DSP
 8 CONTINUE_INT    9 CONTINUE_DSP   10 INTRUDE_INT    11 INTRUDE_DSP
12 ENDING_INT     13 ENDING_DSP     14 NAME_INT       15 NAME_DSP
16 RANKING_INT    17 RANKING_DSP    18 OVER_INT       19 OVER_DSP
20 TEST_INT       21 TEST_DSP       22 RESULT_INT     23 RESULT_DSP
```

`_INT` installe l'écran, `_DSP` le déroule.

**`MODE_SUB` n'a de sens que pour trois modes : `GAME`, `SEL` et `ADV`.** Tous les autres laissent l'octet à ce
que le mode précédent y avait mis. Pour `CONTINUE`, `OVER`, `INTRUDE`, `ENDING` et `NAME`, il suffit de lire le
mode.

### Les douze sous-états de l'attraction (ADV), `0x0002455C`

```
0 DONT_RECYCLE_INT   1 DONT_RECYCLE_DSP   2 ADV_SEGA_INT     3 ADV_SEGA_DSP
4 FV2_TITLE_INT      5 FV2_TITLE_DSP      6 ADV_RATING_INT   7 ADV_RATING_DSP
8 ADV_REPLAY_WAIT    9 ADV_REPLAY_INT    10 ADV_REPLAY_DSP  11 ADV_DEMO_DSP
```

Douze, non dix : mesuré. Et la tranche commence à l'indice 50 de la table, non à 52 — les deux noms manquants
étaient **avant**, pas après, donc rien de ce qui suit ne glisse. Vérifié en gelant le jeu sous-état par
sous-état et en regardant :

| gelé sur | à l'écran |
|---|---|
| 3 (`ADV_SEGA_DSP`) | le logo SEGA, 192 images |
| 5 (`FV2_TITLE_DSP`) | **l'écran titre** |

### Les vingt sous-états d'un combat (GAME), `0x000244F4`

```
 0 ROUND_MASK_INT   1 ROUND_MASK_IN   2 ROUND_MASK_DSP  3 ROUND_MASK_OUT
 4 ROUND_INT        5 ROUND_DSP       6 SET_INT         7 SET_INT_POSE
 8 SET_DSP          9 READY_INT      10 READY_DSP      11 FIGHT_INT
12 FIGHT_DSP       13 JUDGE          14 JUDGE_DSP      15 REPLAY_INT
16 REPLAY_DSP      17 VIC_INT        18 VIC_DSP        19 NEXT_PROG_SEL
```

Confirmés de bout en bout par la mesure. Le replay a ses propres états, donc **le jeu dit lui-même quand il a
fini de montrer**, que le joueur l'ait coupé ou non — ce qu'aucun chronomètre ne pouvait remplacer.


## « L'écran titre est-il à l'écran ? »

```
MODE_MAIN ∈ { 2 ADV_INT, 3 ADV_DSP }   et   MODE_SUB ∈ { 4 FV2_TITLE_INT, 5 FV2_TITLE_DSP }
```

Mesuré à l'écran, pas déduit des noms.

À ne pas confondre avec `InAttract` (`[0x00700000] == 0`, l'état de la tâche `entry1`), qui répond à une tout
autre question — « une partie tourne-t-elle » — et qui est vrai pendant l'avertissement, le logo, le titre, les
présentations, les démos et les classements. Les deux servent, à des choses différentes.


## Y aller : demander, ne pas forcer

```
MODE_REQUEST    = 3   ADV_DSP
SUBMODE_REQUEST = 4   FV2_TITLE_INT
```

**Écrit une fois, jamais tenu.** Deux images. Le répartiteur de `0x0000ABC4` recopie `MODE_REQUEST` dans le mode
*avant* de sauter : le mode n'est donc pas forcé, il est demandé, et c'est pour ça que ça marche. Tenir une
demande est ce qui a cassé l'écran de sélection et son chrono, deux fois.

Laisser passer **soixante images** avant de geler dessus : le gel coupe la voix de l'annonceur (à vingt images on
entend « Fighting Vipers » sans le « 2 »), et sur la première image du sous-état 5 le fondu n'est pas fini, le
logo SEGA s'y superpose encore. Trois cents sont déjà trop tard, l'écran est passé.

Ces soixante images ne sont **pas** un chronomètre déguisé : l'état dit *quel* écran, c'est lui le signal ; les
soixante n'écartent que le fondu.

### Depuis un combat, ce levier ne commande rien

Et l'autre non plus. Les deux écritures du séquenceur d'attraction —

```
[0x00580DFC] = 14      la dernière entrée réelle de la liste
[0x00580DD2] = 0x7FFF  son minuteur, expiré
```

— marchent **depuis l'attraction** et seulement là. Essayées depuis un combat, elles ne font rien, et la
relecture dit pourquoi : elles étaient encore intactes une seconde après. Le séquenceur d'attraction ne tourne
pas pendant un match, donc personne ne les lit. *Un essai négatif doit dire lequel des deux : le jeu a recouvert
nos valeurs, ou le jeu ne les lit pas.*

Forcer l'état de la tâche `entry1` fait dérailler le jeu — essayé, mesuré.

Reste la route du jeu lui-même : **finir le match sans le montrer ni l'entendre**, puis poser l'écran titre une
fois l'attraction revenue.

```
0x00581455 = MATCH COUNT − 1    la manche en cours devient la dernière
0x00581406 = 64                 le chrono au bord : une seconde affichée
vie du joueur 1 = 0             pour qu'au TIME UP l'issue ne se discute pas
```

Le chrono est posé **une fois par manche, jamais tenu** — rechargé à chaque image il n'expire jamais. Le numéro
de manche `0x00581456` dit quand une nouvelle commence, et c'est le seul moment où on le réécrit. La vie à zéro
parce qu'au TIME UP c'est le moins pourvu qui perd, et qu'un nul ne se compte probablement pas comme une manche
perdue : à zéro, il n'y a rien à supposer.

Le K.O., le GAME OVER et le décompte du `CONTINUE` se déroulent par la route du jeu, derrière l'image du menu,
le son coupé.

### Mesuré ici le 2026-10-21 : la demande, elle, passe de partout — mais elle ne sort pas de la partie

Ce qui précède porte sur les deux écritures du **séquenceur d'attraction**. La demande de mode est un autre
levier, et il n'avait jamais été essayé ailleurs que depuis l'attraction. Il l'est maintenant, avec
`vf3run --modes --console-start N`, qui fait passer un appui sur Start par la décision de l'hôte et dit ce
qu'elle a choisi, image par image.

| Start à l'image | le jeu était | ce qui a été décidé | ce qui est arrivé |
|---|---|---|---|
| 1100 | `ADV_DSP` 3, le logo SEGA | demander le titre | écran titre à **1101**, au lieu de 1193 |
| 1255 | `ADV_DSP` 5, l'écran titre | ouvrir la liste | **rien d'écrit** ; la suite image pour image comme le témoin |
| 1450 | `ADV_DSP` 11, la démonstration | demander le titre | écran titre à **1451** |
| 2100 | `SEL_DSP` 1, l'écran de sélection | demander le titre | écran titre à **2101** |
| 3100 | `GAME_DSP` 12, en plein combat | demander le titre | écran titre à **3101** |

Les trois derniers sont neufs. La demande commande donc **l'écran** depuis l'attraction, depuis la sélection
et depuis un combat.

**Et elle ne termine pas la partie.** C'est le même essai poussé d'un cran : la RAM relue après coup dit que la
tâche `entry1` (`0x00700000`, « une partie tourne-t-elle ») est restée à sa valeur de jeu.

```
depuis un combat     entry1 = 0x2A avant, 0x2A après   (relu à l'image 4000)
depuis la sélection  entry1 = 5 avant,    0x2D après   (relu à l'image 2600)
```

L'écran titre revient donc par-dessus une partie qui, elle, court toujours. `fv2_in_attract()` reste faux, et
tout ce qui s'y accroche — l'armement de Survival le premier — le voit. Sortir vraiment d'un combat reste la
route du jeu décrite juste au-dessus, qui n'est pas câblée.

*Un essai négatif doit dire lequel des deux ; un essai positif doit dire jusqu'où il porte.*


## Les tâches qui dessinent l'attraction — relevé du 2026-10-21

Mesuré parce que demander l'écran titre laissait une scène sale derrière elle. Les blocs de tâches sont à
`0x00700000`, **un tous les `0x100`** ; l'ordonnanceur lit deux mots de chacun soixante fois par seconde : le
**bit 0 du premier mot** dit si la tâche tourne, le mot suivant est sa routine.

Relevé aux trois moments de la boucle, par vidage de la RAM (`vf3run --ram-at`) :

| bloc | écran titre | démonstration | replay |
|---|---|---|---|
| `0x00700200` | `00000000 00062144` | `00000000 00062144` | `00000001 00062390` |
| `0x00700300` | `00000000 000621B0` | `00000000 000621B0` | `00000001 00062404` |
| `0x00700400` | `00000000 00015A4C` | `00000000 00015A4C` | `00000001 00015A74` |
| `0x00700500` | `00000000 0003F874` | `01018001 00040134` | `01010041 00040134` |

**Sur l'écran titre, toutes sont éteintes.** La démonstration en fait tourner une, le replay quatre. Le reste
de la région qui diffère est en flottants — des coordonnées, pas des blocs de tâches.

`0x00700000` et `0x00700100` sont `entry1` et `entry2`, les tâches d'une partie ; forcer `entry1` fait dérailler
le jeu (essayé en août). Elles sont éteintes dans l'attraction de toute façon.

### Qui éteint ces tâches, et quand — relevé au `--watch`

`vf3run --watch 700200 --modes` nomme l'instruction qui écrit, sans rien décompiler :

```
store32 00700200 = 0 at PC FFF018DC (LR FFF0153C)     l'amorce, au démarrage
store32 00700200 = 0 at PC 0000779C (LR 000076AC)     idem
store32 00700200 = 0 at PC 000390F0 (LR 00038C38)     idem
store32 00700200 = 0 at PC 0000B240 (LR 00039068)     entre les images 998 et 999
```

La quatrième est la seule qui compte : elle tombe **entre le sous-état 1 (`DONT_RECYCLE_DSP`) et le sous-état
2 (`ADV_SEGA_INT`)**, c'est-à-dire au **redémarrage de la boucle d'attraction**. `LR 0x00039068` est le site
d'appel de la machine à modes (`0x00039064` est `bl 0x0000ABC4`).

**Le jeu n'éteint donc jamais ces tâches pendant une démonstration ni pendant un replay.** Il attend que la
boucle reparte. C'est pour ça qu'un effacement à la main ne tient pas en plein replay : ce qui les rallume,
c'est le replay lui-même, qui n'est pas fini.

### Comment la boucle repart : la liste du séquenceur

`vf3run --watch 580DFC` montre le séquenceur marcher :

```
store8 00580DFC = 0 at PC 00072EB4 (LR 00072E7C)      posé sur l'écran titre
store8 00580DFC = 1..F at PC 00073178 (LR 00072E7C)   un pas à la fois, pendant la démonstration
   5866  ADV_SEGA_INT                                 sorti de la liste : la boucle repart
```

La liste va de 0 à 15, et c'est `0x00073178` qui l'avance. Écrire 14 et vider le minuteur fait franchir la fin
au pas suivant — le levier de §6 bis, et on voit ici pourquoi il marche.

**Et pourquoi il ne marche pas depuis le replay** : pendant le replay (`sub 10`), plus aucune écriture de
`0x00580DFC` n'apparaît. Le séquenceur n'avance plus sa liste là ; le replay court sur sa propre durée. Pousser
la liste n'a donc rien à pousser.

### Ce qui met fin au replay — et la route propre, enfin

Relevé en surveillant le mot de la machine à états (`vf3run --watch 58FD98`). Le mode et son sous-état sont
réécrits à chaque image par la machine à modes, mais **`SUBMODE_REQUEST` (`0x0058FD9B`) n'est écrit qu'une
poignée de fois**, et ces écritures-là *sont* la boucle d'attraction :

```
a WARNING_DSP       = 0, = 1                    la boucle demarre
a DONT_RECYCLE_*    = 2, = 3                    vers le logo SEGA
a ADV_SEGA_DSP      = 4, = 5                    vers l ecran titre
a FV2_TITLE_DSP     = B  ou  = 8                vers la demonstration, ou vers le replay
a ADV_REPLAY_DSP    = 0 par 0x0000B7C0, puis 1  <- LE REPLAY DEMANDE LE REDEMARRAGE DE LA BOUCLE
```

**La façon dont le jeu sort d'un replay n'est pas de demander l'écran titre : c'est de demander que la boucle
recommence** (`DONT_RECYCLE_INT`). Et ce redémarrage est précisément l'endroit où il éteint les tâches qui
dessinent (`0x0000B240`, entre les sous-états 1 et 2).

D'où la route propre, qui marche de partout dans l'attraction et en deux temps :

1. `MODE_REQUEST = 3`, `SUBMODE_REQUEST = 0` — écrit **une fois** : la boucle redémarre et le jeu fait **son**
   ménage ;
2. une fois arrivé au logo SEGA (sous-état 2 ou 3), là où la scène est vide, demander l'écran titre.

Mesuré avec la sonde d'images, depuis une démonstration **et** depuis un replay :

```
appui image 1450 (demo)    sub 0, 1, 2 puis ECRAN TITRE a l image 1455 : 0 modele
appui image 2100 (replay)  sub 0, 1, 2 puis ECRAN TITRE a l image 2105 : 0 modele
```

**Cinq images, et 0 modèle** — le même écran titre que celui où le jeu arrive seul. Contre 397 modèles pour la
demande directe, et 400 images d'attente pour le levier du séquenceur, qui lui ne commandait rien depuis un
replay.

### Ce que ça donne, et où ça bute

Éteindre le bit 0 avant de demander l'écran titre **range presque tout, depuis la démonstration** : l'écran
titre passe de **397 modèles à 4-19**, contre 0 pour celui où le jeu arrive seul.

**Mais pas depuis le replay** : relu dix images après la demande, les trois tâches `0x00700200`, `0x00700300`
et `0x00700400` sont **rallumées par le jeu**. Un effacement écrit une fois ne tient pas, et le tenir est
justement ce qu'il ne faut pas faire.

Donc la vraie sortie n'est pas d'éteindre ces bits à la main : c'est de trouver **ce que le jeu exécute** quand
son séquenceur installe l'écran titre, et qui arrête ces tâches pour de bon. C'est la prochaine mesure, et elle
demande de décompiler cette branche du séquenceur.


## La route fixe de l'arcade : une table, relevée le 2026-10-22

Cherchée parce que le mode Training avait besoin de choisir son arène, et trouvée sans décompilateur : le
`--watch` nomme qui écrit l'index de scène, et les douze instructions qui précèdent disent d'où il sort.

```
000716B8  stb r0,-609(r9)      r9 = 0x00590000, donc 0x0058FD9F : l'index de scene
000716B4  mr  r0,r30           et r30 vient de :

000715F0  lbz    r9,5213(r11)  [0x0058145D], qui choisit la route
000715F8  addi   r11,r11,16640 la table, 0x000B4100
000715FC  addi   r0,r8,-1      le numero du combat, moins un
00071600  rlwinm r0,r0,3,0,28  fois 8
00071604  rlwinm r9,r9,1,0,30  la route, fois 2
0007160C  lbzx   r30,r11,r9    <- L'ARENE
00071614  lbz    r3,1(r9)      et l'octet suivant : l'adversaire
```

**Huit octets par combat, quatre paires (arène, adversaire), la paire choisie par `0x0058145D`.** Relevé dans
un jeu en marche :

```
+00 : 09 0A 00 09 00 09 00 09      combat 1 : arene 9,  adversaire 0x0A
+08 : 01 0B 00 09 00 09 00 09      combat 2 : arene 1,  adversaire 0x0B
+10 : 07 00 0B 06 00 09 00 09
+18 : 02 02 0A 07 00 09 00 09
```

« 9 puis 1 pour le second adversaire » : c'est exactement ce que `docs/fv2-flux-de-match.md` avait mesuré de
l'autre côté, sans savoir d'où ça venait.

**Et ce n'est pas le même levier que `CHANGE STAGE`.** La fonction `0x0007111C`, que le menu d'août patche en
`li r3,<scène> ; blr`, ne commande **pas** l'arène du premier combat : patchée et tenue, le jeu a choisi 9
quand même. Elle n'est atteinte que par un rematch **passant par l'écran de sélection**. Pour un combat qui
démarre, c'est la table.

Les douze arènes, numérotées par le propriétaire et mesurées à l'écran : 1 Old Armstone Town, 2 Deck of the
Dark Royal, **3 Marble Arena**, 4 The Great B.M, 5 Terror Furnace, 6 Serpent's Cage, 7 Hanger of the Dark
Royal, 8 Secret Chemical Plant, 9 Cross River Damsite, 10 Neo Armstone City Tower, 11 The Skies of Armstone,
12 Great Museum.


## La liste des modes, et sa géométrie

`assets/menu-console/title_list.png`, 709 x 897, RGBA, sept lignes : ARCADE, V.S., RANDOM, SURVIVAL, NETWORK,
TRAINING, OPTIONS. **Une seule version de chaque ligne** — aucun des 45 fichiers n'est une variante
« sélectionnée », donc la ligne choisie doit être marquée autrement.

L'image a été recadrée sur son encre (709 x 897 à partir de 1027 x 1024) ; sans ce recadrage le vide comptait
dans la hauteur et le bloc entier se décalait.

Les sept bandes sont coupées **au milieu des blancs**, pas en sept parts égales : les étiquettes font de 114 à
118 pixels, et des parts égales rogneraient les plus hautes.

```
0, 124.5, 254, 381.5, 511.5, 640.5, 769, 897        (sur 897)
```

Les libellés sont peints **calés à gauche** — `V.S.` a son milieu à 0,17 quand `NETWORK` l'a à 0,50 — donc chaque
ligne doit être glissée pour amener son encre au milieu du bloc.

| ligne | milieu de l'encre | demi-largeur | colonnes |
|---|---|---|---|
| ARCADE | 0,4006 | 0,3977 | 2..565 |
| V.S. | 0,1728 | 0,1643 | 6..238 |
| RANDOM | 0,4379 | 0,4320 | 4..616 |
| SURVIVAL | 0,4732 | 0,4673 | 4..666 |
| NETWORK | 0,5035 | 0,4908 | 9..704 |
| TRAINING | 0,4746 | 0,4718 | 2..670 |
| OPTIONS | 0,4281 | 0,4196 | 6..600 |

Et sa place sur l'écran titre, en fractions de la hauteur, relevée sur une capture 3840 x 2160 :

```
haut   0.26157      hauteur 0.35557      milieu 0.5
```

Grossie de `7/6` (le bloc occupe la hauteur que sept lignes prendraient, pour six dessinées) et remontée de
`50/1080`, les deux à la demande. Gardé comme un facteur appliqué à la mesure plutôt qu'en remplaçant celle-ci
par le chiffre obtenu : les deux donnent la même image, mais l'un garde la mesure lisible et dit ce qui a été
ajouté par-dessus.

L'échelle verticale mesurée est 1,0013 et l'horizontale 1,1216 : cet écran est une image 4:3 montrée large, et
l'étirement est celui de son affichage, pas celui du jeu. **Seul le placement vertical est pris de là**, la
largeur suit les proportions de l'image.


## Trois fautes, déjà payées là-bas

- **Le style, pas le fond.** Le menu posé sur l'écran titre avait été basculé sur un style qui laisse voir le
  jeu — ce qui contournait toute la branche où vivent les libellés peints, et les lignes retombaient à la police
  du menu. Le remède n'est pas un style de plus, c'est une **soustraction** : même style, moins son illustration.
- **Le masque des boutons.** Ce chemin n'ouvre pas le menu tout de suite, et c'est l'ouverture qui retenait les
  entrées. Sans elle, Start arrivait au jeu, qui prenait son crédit et ouvrait l'écran de sélection pendant que
  le voyage attendait un écran titre qui ne viendrait jamais.
- **Deux changements livrés d'un coup**, sur un mécanisme dont le document disait lui-même qu'il n'avait jamais
  été essayé. Frédéric n'avait aucun moyen de savoir lequel des deux cassait quoi, et il a fallu tout défaire.


## Le replay qui suit Start : les combattants sont sombres — 2026-10-23

Rapporté par Frédéric : « *à la relance du replay, l'éclairage des personnages est anormalement sombre* ».
Reproduit sans fenêtre, avec témoin, et **isolé**.

### Ce n'est pas un manque de modèles

Le journal des images le dit tout de suite : le replay d'après dessine **457 à 532 modèles dans 16 à 18 vues**,
autant que le témoin. Rien ne manque — tout est là et mal éclairé. C'est un défaut distinct de celui des
modèles restés sur l'écran titre, même s'il vient du même geste.

### Ce n'est pas le menu non plus

Trois captures, toutes dans le sous-état `0A` :

| | comment on y arrive | les combattants |
|---|---|---|
| témoin | le jeu seul, replay naturel à l'image 6284 | **éclairés** |
| essai | Start coupe la démo, **la liste s'ouvre et on en ressort par B** | **sombres** |
| essai sans liste | Start coupe la démo, **et rien d'autre** | **sombres** |

La troisième est celle qui tranche : sans qu'aucun menu se soit ouvert, le défaut est là. **La cause est le
redémarrage de la boucle d'attraction par Start**, pas la surface de la console. Le menu n'y est pour rien.

Les décors, eux, sont normalement éclairés dans les trois. Ce sont **les modèles des combattants seuls** qui
sont sombres — des silhouettes bleutées.

### Où ça rejoint ce qui était déjà écrit

Le § « Ce que ça donne, et où ça bute » dit que la route propre range tout depuis la démonstration « mais pas
depuis le replay », où le jeu rallume les trois tâches qu'on avait éteintes, et conclut qu'il faut trouver
**ce que le jeu exécute quand son séquenceur installe l'écran titre**. Ce défaut-ci ajoute un symptôme à cette
même question : ce n'est pas seulement ce qui reste à l'écran, c'est aussi **une lumière qui n'est pas
reposée**.

### La mesure suivante

Trouver qui écrit les paramètres d'éclairage des modèles, et comparer ce qu'ils valent dans le témoin et dans
l'essai à la même image du replay. La reproduction tient sans fenêtre, donc elle ne coûte rien à répéter :
`tools/scripts` n'a pas encore ces trois scénarios, ils sont dans le bac à sable de la session.
