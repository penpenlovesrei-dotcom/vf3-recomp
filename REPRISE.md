# Reprise — Fighting Vipers 2, menu console

État au **2026-10-21**, branche `fv2`, rien de poussé depuis le tag `fv2-v0.2.0-alpha`.

Le journal du jour est en bas. Ce qui suit est l'état, pas le récit.


## La règle qui prime, posée le 2026-10-22

**Pour toute question en « pourquoi le jeu fait ça », ouvrir `docs/fv2-flux-de-match.md` AVANT de mesurer.**
C'est la décompilation du programme arcade : 759 lignes, les 81 gestionnaires d'état nommés par les tables du
jeu, les variables sous les noms de Sega. On ne mesure qu'ensuite, pour confirmer, ou pour ce qu'il ne couvre
pas — il dit lui-même ses limites (sa section 7) et marque ce qui a été **lu sans être vu tourner**.

Pourquoi cette règle existe : la soirée du 21 a été passée à retrouver par différence de RAM ce que ce
document nomme déjà.

| cherché à tâtons le 21 | déjà écrit dans le flux |
|---|---|
| « le bloc `0x00700500` dessine la démo » | `[0x0007511C] → 0x00700500` : **fighter 0** ; `0x00700200` = `CPU0`, `0x00700300` = `CPU1`, 23 créneaux |
| la routine `0x0003F874` vue sur l'écran titre | `FUN_0003F874` **est la tâche du combattant**, entrée par le `blrl` de l'ordonnanceur (`0x000391AC`) |
| « `0x0000B7C0` met fin au replay » | `fv2_adv_ADV_REPLAY_DSP`, **`0x0000B764`, 100 octets** — l'adresse tombe dedans |
| — | `fv2_adv_ADV_DEMO_DSP` demande le sous-mode 2 **quand `ADV_INFO SUBMODE` (`0x00580E08`) atteint 4** |

`SupermodelSrc\Docs\MenuConsole.md` reste la source des **leviers** — chacun y a été mesuré en jouant. Le flux
dit **pourquoi** ce sont les leviers, et jusqu'où ils portent. C'est l'absence de ce « jusqu'où » qui a rendu
le levier du séquenceur inerte en replay sans qu'on puisse le prévoir.


## Ce qui marche aujourd'hui

Tout est derrière un lanceur `.cmd` à la racine, et tous épinglent `VF3_DATA` sur le dossier du projet.

| | lanceur | où c'est écrit |
|---|---|---|
| le jeu, son et manettes | `jouer-fv2.cmd` | |
| les personnages cachés | `jouer-fv2-roster.cmd` | `docs/fv2-roster.md` |
| les portraits de la barre de sélection | `jouer-fv2-portraits.cmd` | `docs/fv2-portraits.md` |
| le pack de tuiles 2D | `jouer-fv2-hd2d.cmd`, `releve-tuiles-fv2.cmd` | `docs/fv2-hd-2d.md` |
| les textures 3D | `jouer-fv2-hd3d.cmd`, `dumper-textures-3d-fv2.cmd` | `docs/fv2-hd-3d.md` |
| les quatre costumes de Honey | `montrer-honey.cmd` | `docs/fv2-honey.md` |
| les modes console, et la liste sur l'écran titre | `jouer-fv2-console.cmd` | `docs/fv2-console.md` |
| mesurer ce que fait Start | `mesurer-fv2-start.cmd` | `docs/fv2-etats.md` |
| les captures de la liste | `capturer-fv2-liste.cmd` | `docs/fv2-console.md` |
| les ombres : ce qu'elles sont | `montrer-ombres-fv2.cmd` | `docs/fv2-ombres.md` |
| ce que coûte une image | `mesurer-budget-fv2.cmd` | `docs/fv2-ombres.md` |
| construire la fenêtre aussi | `construire-hote.cmd` | |
| construire | `construire.cmd` | |
| vérifier | `verifier-vf3.cmd` | |

Et le jeu tourne en **free play** : un seul octet de la NVRAM, `backup[0x17] = 27`.

**Start fait maintenant les deux choses demandées**, et la liste des sept modes est une surface à elle,
dessinée sur l'écran titre du jeu. La page du menu Échap existe toujours à côté, inchangée : ce n'est pas elle,
la liste.


## Ce qui a été fait le 2026-10-21

```
                        sur l'écran titre du jeu  ->  la liste des sept modes s'ouvre par-dessus
Start, mode console
                        partout ailleurs          ->  le jeu revient à son écran titre, par SA route
```

Dans les deux cas l'appui est **retenu** : le jeu ne le voit pas.

- La décision est `console_play::take_start` (`src/host/console_play.h`), donc l'hôte et `vf3run` la prennent
  de la même façon, et `vf3run --modes --console-start N` la **mesure** sans fenêtre.
- La surface est `src/host/fv2_mode_list.h`. Elle ne passe pas par `game_menu` : elle dessine
  `assets/menu-console/title_list.png` sur l'image que le jeu vient de produire. Géométrie entièrement
  reprise du relevé (`docs/fv2-etats.md`), plus les deux facteurs demandés à l'époque — grossie de 7/6,
  remontée de 50/1080. La ligne choisie **clignote** (alpha 1 → 0,5 sur 45 images) : c'est la réponse du
  propriétaire au fait qu'une seule version de chaque ligne existe. **L'image derrière passe à la moitié**
  pendant que la liste est là, comme sur la Dreamcast : mesuré sur deux captures de cette version, 255 devient
  128 sur les trois canaux, et la même mesure sur les deux captures d'ici donne 0,500.
- **On en ressort** : B à la manette (ou la touche de coup de pied, ou Retour arrière) referme la liste et
  laisse le jeu là où il était. A, Start, Entrée ou la touche de coup de poing choisissent.
- **La liste est muette**, et le son du jeu est en pause derrière elle, comme sous le menu Échap. Une première
  version des bruitages a été posée puis **défaite le jour même** : voir le journal du 2026-10-21 et
  `docs/fv2-console-son.md`, qui pose les questions auxquelles la prochaine devra répondre avant d'être écrite.
- ARCADE et SURVIVAL agissent. Les cinq autres lignes sont dessinées et ne font rien : RANDOM et V.S.
  demandent le chrono de l'écran de sélection, jamais mesuré ici ; NETWORK, TRAINING et OPTIONS n'ont aucun
  levier dans ce portage.

### La route de Start vers l'écran titre, et les trois essais qu'il a fallu

C'est le morceau qui a coûté la soirée, et le résultat tient en deux lignes : **depuis l'attraction, Start
demande à la boucle de recommencer, puis demande l'écran titre une fois le ménage fait.**

```
MODE_REQUEST = 3, SUBMODE_REQUEST = 0   écrit UNE FOIS : la boucle redémarre, le jeu range sa scène
puis, arrivé au logo SEGA (sub 2 ou 3)  la demande d'écran titre
```

Mesuré : **écran titre en cinq images, à 0 modèle 3D**, depuis une démonstration comme depuis un replay —
exactement l'écran titre où le jeu arrive seul.

Les deux routes écartées restent au dossier, parce qu'elles ont chacune appris quelque chose :

| route | ce qu'elle donne |
|---|---|
| la demande de mode seule | une image, mais **397 modèles** laissés sur l'écran titre : les démos suivantes sortent noires ou vides |
| le levier du séquenceur (§6 bis) | propre depuis une démonstration, **inerte depuis un replay**, et un replay de 77 s derrière |
| **le redémarrage de la boucle** | **cinq images et 0 modèle, de partout** |

Pourquoi : la demande de mode change ce qui est **affiché**, elle n'arrête pas ce qui **dessine**. Le jeu
n'éteint ses tâches de dessin qu'au redémarrage de sa boucle (`0x0000B240`, entre les sous-états 1 et 2), et
c'est exactement ce que le replay demande quand il se termine (`0x0000B7C0` écrit `SUBMODE_REQUEST = 0`).
Toute la chaîne est relevée dans `docs/fv2-etats.md`, au `--watch`, sans rien décompiler.

**Ce qui reste vrai et non résolu** : depuis un **combat**, la demande de mode ramène l'écran titre mais ne
termine pas la partie — `entry1` (`0x00700000`) reste à `0x2A`. L'écran titre revient par-dessus une partie qui
court toujours.

### Les captures, et de quoi elles sont la preuve

`capturer-fv2-liste.cmd`, trois images dans `build\` :

| image | ce qu'elle montre |
|---|---|
| `liste-1-arcade.png` | les sept lignes sur le vrai écran titre, FREE PLAY lisible dessous |
| `liste-2-survival.png` | trois fois bas puis Entrée : la partie démarre, et l'écran de sélection dit SURVIVAL MODE |
| `liste-3-retour-au-titre.png` | Start pendant la démonstration : l'écran titre est revenu |

La deuxième est celle qui compte : elle va d'un bout à l'autre, sans main sur la manette.

### La sonde, et ce qu'elle a permis

`jouer-fv2-console-sonde.cmd` lance le jeu avec `VF3_PIC_LOG` et `VF3_MIDI_LOG` : **une ligne par image
dessinée** dans `log.txt` — modèles 3D, vues, mode, sous-état — et tout ce que le jeu dit à sa carte son. C'est
elle qui a nommé le défaut que quatre essais scriptés n'avaient pas su reproduire :

```
écran titre où le jeu arrive seul     0 modèle,  8 vues
écran titre demandé par Start       397 modèles, 10 vues
```

Repères pour la prochaine fois : un écran titre sain est à **0 modèle sur 8 vues**, une démonstration autour de
**400 à 520**.

### Deux crochets d'essai, sans lesquels rien de tout ça ne se vérifiait

La liste gèle le jeu, donc tout ce qui compte les images du jeu n'arrive plus — c'est le blocage de neuf
minutes de la veille. D'où, dans l'hôte :

```
VF3_QUIT_AFTER_S=N     finir après N secondes d'horloge
VF3_KEYS_S=k@sec,...   envoyer la touche k à telle seconde
```

Et trois autres, posés dans la soirée, qui ont fait tout le reste du travail :

```
VF3_PIC_LOG=1          chaque image dessinée : modèles, vues, mode, sous-état
VF3_MIDI_LOG=N         ce que le JEU dit à sa carte son, plus une ligne toutes les N images
VF3_MIDI_SEND=h@F      poser une question à la carte son à l'image F ; la réponse se lit dans les pics
```


## Les ombres, le 2026-10-22 : ce qui est acquis

Rien n'a été ajouté au rendu. Le dossier complet est `docs/fv2-ombres.md` (619 lignes, le **mesuré** et le
**lu** tenus séparés, avec les limites de chaque mesure). Branche `fv2-ombres`, trois commits, non poussée.

**Le portage n'a pas de code d'ombre, et n'en a pas besoin : le jeu dessine les siennes, en polygones.**

- **La géométrie est une donnée de la ROM**, dessinée par les auteurs : un jeu de pièces simplifiées par
  combattant (15 à 20 selon le personnage), téléversé une fois en RAM de polygones par `FUN_00056B50`, un bloc
  par image à partir de l'image 547, depuis le mot `0x010000`. Rien ne la calcule.
- **La couleur vient d'une table indexée par décor** (`0x00111318`). `FUN_000224EC` en écrit l'entrée 16 à
  chaque image dans la table de couleurs de la RAM de polygones ; les polygones d'ombre portent **l'index 16**,
  pas une couleur. C'est pourquoi la teinte change avec le décor sans que la géométrie bouge.
- **L'aplatissement est une vraie projection**, bâtie par `FUN_00012D68` depuis les **angles** de la lumière
  (pas son vecteur) : tourner de l'azimut, échelle `(1, 0, 1/sin(élévation))`, basculer, retourner. La
  direction extraite des matrices de la scène et celle lue dans le jeu tombent sur le même angle — **30,7°**
  de la verticale au décor 9.
- **Les angles sont ceux du décor**, recopiés dans chaque combattant par `FUN_000717FC` — sauf aux décors
  **3, 10 et 11**, où ils sont calculés depuis la **position** du combattant : ces trois-là ont une lumière
  locale.
- **`STAGE_TEST__ROB_LIGHT` n'est pas une lumière** : c'est la **fiche du décor**, prise dans la table de
  pointeurs `0x0008D118`. L'octet `+0x6E` y commande l'ombre. Sur les **vingt-sept décors** : 0 à 12 les
  arènes, chacune sa couleur ; **13 à 17 sans ombre du tout** ; 18 à 25 en noir ; 26 traité à part.
- **Reconnaître une ombre est désormais le critère du jeu** : `Mesh::colour_index == 16`. Vérifié sur l'image
  de référence, 37 sur 37, et rien d'autre dans l'image n'est indexé.

Trois interrupteurs de mise au point, **éteints par défaut**, et les cinq contrôles passent avec eux :

```
VF3_MESH_AUDIT=<image>|<a>-<b>[/<n>]   tous les maillages : drapeaux, couleur, index de couleur,
                                       memoire, matrice et determinant, et le soleil de chaque vue
VF3_MARK_RGB=<RRGGBB>                  les maillages de cette couleur peints en magenta
VF3_DMA_LOG=<image>|0                  chaque transfert du Real3D, avec son PC et son LR
```

**Ce qui n'est pas fait** : aucune décision sur ce qu'on ferait des ombres. Les trois ambitions (mieux dessiner
celle du jeu, la remplacer par une ombre calculée, éclairer autrement) sont instruites dans le document, avec
le budget en main — et le budget dit qu'il n'y en a pas, voir ci-dessous.

## Les ombres, le 2026-10-23 : la voie b tourne

Le portage dessine maintenant ses propres ombres, avec les **vrais modèles** des combattants (≈ 24 000
sommets) et non les pièces simplifiées de la ROM. Branche `fv2-ombres`. Détail et mesures :
`docs/fv2-ombres.md`.

Ce qui marche, vu à l'image : **l'auto-ombrage** (le dos et les jambes s'assombrissent du côté opposé au
soleil), **l'ombre portée sur le sol** avec la silhouette exacte — on distingue les deux chaussures, le vélo
de Charlie — et les ombres du jeu retirées à leur place. Allumé par `VF3_SHADOWS=1`, **éteint par défaut**, et
jamais avant que le contrôle de démarrage GPU/processeur ait comparé l'image telle que le jeu la dessine.

**Trois choses qu'il a fallu comprendre, et qui coûteraient une journée à retrouver.**

1. **Le décor n'est pas dessiné dans un seul espace.** Même caméra, même cadre, même matrice racine, mais des
   unités différentes par couche : au barrage la couche lointaine est **vingt-deux fois** plus grande et plus
   loin que les combattants. Le facteur n'est écrit nulle part ; `R3dGpu::measure_scales` le mesure sur le
   sol. Mais le plan le plus large d'une couche lointaine n'est pas forcément son sol — au barrage c'est
   l'eau sous le grillage — donc **seules les couches à l'échelle 1 reçoivent l'ombre**. `VF3_SHADOW_FAR=1`
   pour voir l'autre choix.
2. **Rien sous le sol ne prend d'ombre.** L'ombre du jeu est un quadrilatère posé *dans* le plan du sol, elle
   ne le traverse jamais ; une ombre venue d'une carte, si. Sans cette règle elle tombait sur la structure
   sous les ouvertures du grillage — 1 102 pixels sur 13 415 à l'image 3300.
3. **Les arènes jouables sont les décors 0 à 12.** `VF3_FORCE_STAGE=<n>` écrit `STAGE_TEST__STAGE`
   (0x0058FD9F) : 13, 14 et 20 ne chargent aucun décor. Les cinq fiches qui coupent l'ombre (13 à 17) ne sont
   donc pas des arènes.

Les réglages, tous **éteints par défaut** :

```
VF3_SHADOWS=0/1          les ombres du portage
VF3_SHADOW_DARK=<0..1>   ce que l'ombre prend de l'ambiante en plus du soleil (0 : la lumiere du jeu
                         intacte ; il faudrait ~0,80 pour egaler la noirceur de l'ombre du jeu)
VF3_SHADOW_FAR=1         appliquer quand meme l'echelle mesuree aux couches lointaines
VF3_FORCE_STAGE=<0..26>  forcer le decor
VF3_SHADOW_MAP=<image>   la carte ecrite dans build/shadow_map.png
VF3_GPU_SHOT=<image>     l'image du rendu GPU dans build/gpu_shot.png
VF3_SHADOW_DEBUG=1..6    1 la lecture de la carte, 2 l'endroit lu, 3 le viewport, 4 et 5 la place dans
                         l'espace de la vue, 6 la place une fois les unites de la couche retirees
VF3_SHADOW_NONE=1        les ombres du jeu retirees et la notre non appliquee : le sol eclaire, pour
                         mesurer ce qu'une ombre enleve
VF3_SHADOW_ALL=1         tous les viewports dans la carte, pour voir lesquels partagent l'espace
VF3_SCALE_LOG=1          les echelles mesurees des qu'une bouge
VF3_SHADOW_WHY=1         combien d'images n'ont pas trouve leur lumiere, et pourquoi
VF3_SHADOW_FAIL=<n>      faire echouer la lumiere une image sur n, pour exercer le filet
```

**Deux pièges payés cher.** `r3d.hlsl` et `r3d_gpu.cpp` doivent bouger ensemble : un échantillonneur de
différence et Direct3D 12 refuse le pipeline en silence, l'hôte retombe sur le rendu processeur, et on mesure
une image qui n'est pas celle qu'on croit. Et `2>/dev/null` fait taire le journal : sous Windows `isatty()`
est vrai pour `NUL`, donc l'hôte ne fait pas son `freopen` — rediriger vers un vrai fichier, le journal est
alors dans `$VF3_DATA/log.txt`.

**Les décors 3, 10 et 11 sont faits** : la direction de projection n'est plus prise au soleil des vues mais
**lue dans les matrices des ombres du jeu**, par combattant (`S·Sᵀ = I − N·Nᵀ + w·wᵀ`, donc `d = N + w`). Les
vues sont groupées par direction et chaque groupe a sa carte, deux au plus. Au barrage les deux combattants
donnent la même direction à quatre décimales : une seule carte, aucun coût ajouté. Aux trois décors à lumière
locale, deux cartes — et elles sont plus fines, chacune ne couvrant qu'un combattant (remplissage 9,1 % et
10,4 % contre 4,1 %). `VF3_SHADOW_MAP` écrit les deux.

**Ce qui reste.** Pas d'option d'hôte sur le patron de `hd_3d` : seule la variable d'environnement allume les
ombres. Et `VF3_FORCE_STAGE` ne recharge pas la mémoire de textures, donc les arènes atteintes en forçant
sortent avec les textures d'un autre décor — sans effet sur la géométrie des ombres, mais les captures de ces
décors ne valent pas pour juger à l'œil.

## L'ombre du jeu qui revenait, le 2026-10-23 : trouvee et corrigee

Frederic l'a vu en jouant : par moments l'ancienne ombre reapparaissait. C'etait dans le code et pas dans le
hasard. L'ombre du jeu n'est retiree que lorsque notre lumiere a pu etre trouvee (`r3d_gpu.cpp`, la boucle de
trace : `if (shadows && light_ok && colour_index == 16) continue;`), donc **toute image ou `fit_light`
echoue montre l'ombre du jeu**. Compte avec `VF3_SHADOW_WHY=1` : **17 images sur 660** d'un combat, **223 sur
3180** de la boucle d'attraction.

Trois causes, mesurees et nommees une par une :

1. **Le sens des normales des quadrilateres d'ombre.** Il venait d'un soleil pris dans la scene — la premiere
   vue qui en avait un — qui pouvait etre presque dans le plan du sol : les normales s'annulaient alors en
   s'additionnant. Elles sont maintenant alignees sur la premiere, et le sens est fixe par le cote ou se
   tiennent les combattants.
2. **La couche de reference.** C'etait « la plus proche des pieds en proportion » ; c'est maintenant **la plus
   haute au-dessous d'eux**. L'ancien critere lachait des qu'un combattant quittait le sol : c'etait les 159
   images de la boucle d'attraction.
3. **Les images sans aucun triangle d'ombre.** Les modeles sont dans la scene mais rien d'eux n'arrive au
   rastériseur — hors champ. Elles etaient comptees comme des echecs alors que **le jeu n'y dessine pas
   d'ombre non plus** : il n'y a rien a remplacer. Elles sont desormais rangees a part.

Et un filet : quand une image echoue malgre tout alors que des triangles d'ombre sont bien la, **la lumiere de
l'image precedente est reprise** (`R3dGpu::kept`), lachee apres 30 images sans ombre pour qu'un decor n'herite
jamais de la lumiere du precedent.

Apres les trois : **zero image perdue**, sur 780 images d'un combat et 3300 de la boucle d'attraction — il ne
reste que les images sans ombre du tout. Le filet ne se declenche donc plus jamais, et c'est pour cela que
`VF3_SHADOW_FAIL=<n>` existe : il fait echouer la lumiere une image sur n, pour l'exercer. Essaye a 2, la
capture ne differe d'une image normale que de **13 pixels sur 1 049 088**, et l'ombre du jeu ne revient pas.

## Virtua Fighter 3, le 2026-10-23 : le meme code, et un refus mesure

Le code des ombres n'a plus **une ligne propre a un jeu**. Les deux choses qui l'etaient ont ete remplacees
par des mesures internes, meilleures dans les deux jeux :

1. **Reconnaitre une ombre** : plus `colour_index == 16` mais **`Model::flattened`**, le determinant nul de la
   matrice. Sur Fighting Vipers 2 les deux criteres designent exactement les memes 37 maillages.
2. **Le signe de la direction** : plus le soleil de la vue mais **l'ecart entre un combattant et son ombre**.

Ce qui a permis de mesurer Virtua Fighter 3 au lieu d'en parler. Essaye, il clignotait. Trois mesures :

| mesure | Fighting Vipers 2 | Virtua Fighter 3 |
|---|---|---|
| ecart de direction entre images | 0,18° | **5,55°**, pire 123,7° |
| ecart des pieces d'une MEME image | **0,00°** | 6,71°, pire 61,3° |
| residu de l'ajustement `I − N·Nᵀ + w·wᵀ` | **0,000000** sur 16 000 pieces | **0,069551** sur 64 000 |

**Les matrices d'ombre de Virtua Fighter 3 ne sont pas des projections planes** — son sol a du relief. Toute
la methode repose dessus, donc elle n'a pas de sens sur ce jeu. Le residu est devenu le critere, et il decide
sans nommer de jeu : une vue qui n'est pas une projection plane refuse l'image, et le jeu dessine ses propres
ombres. Seuil 0,005, pose entre un millionieme et sept centiemes.

Verifie : sur Virtua Fighter 3, meme image ombres allumees ou eteintes, **identique a l'octet**. Sur Fighting
Vipers 2, rien ne bouge.

Une tentative a noter : extraire la direction **piece par piece** (chaque piece rendant son propre plan) fait
tomber l'ecart entre images de 5,55° a 1,15° et supprime les retournements. Elle est gardee, elle est juste,
mais elle ne suffit pas — le modele lui-meme est faux sur ce jeu.

**Le refus a ete juge a l'essai**, pas seulement sur les chiffres. `jouer-vf3-ombres.cmd force`
(`VF3_SHADOW_FORCE=1`) passe outre : a l'arret l'ombre est bonne, en mouvement non. Frederic, manette en main :
« clignotements, changements de direction, je ne vois pas d'amelioration ». Le refus reste. Sans argument, le
lanceur montre le comportement commite, c'est-a-dire les ombres du jeu.

**Une reserve.** Au premier combat arcade de Virtua Fighter 3, sur la plage, le sol n'est pas dessine du tout
dans le rendu 3D ; les ombres du jeu y sont invisibles aussi. Autre question.

## Le sol qui changeait de couche, le 2026-10-24

Vu en jouant au decor de Honey (**decor 11**), mesure sur 857 images de cette partie : le portage changeait
d'avis sur l'emplacement du sol **une image sur trois**, entre deux couches a un facteur 180 l'une de l'autre
(vp13 a −1,2 : 532 fois ; vp12 a −220 : 255 fois). Tout en depend — les unites de chaque couche, et ce qui
compte comme « sous le sol » — donc l'ombre au sol apparaissait et disparaissait.

**Cause** : la regle etait « la plus haute surface plate AU-DESSOUS des combattants », avec une tolerance de
2 %. Le sol de ce decor, a −1,15, est d'un cheveu au-dessus du point le plus bas d'un combattant (−1,30, un
pied qui traverse le sol) : il etait ecarte, et il ne restait que la couche lointaine a −220.

**Correction** : le sol est la surface plate **la plus proche des pieds**. Aucune tolerance a regler.
Resultat : 1 changement sur 480 images au decor 11 et au barrage, zero image perdue partout, et la regression
que l'ancienne regle evitait (159 images quand les combattants quittent le sol) n'est pas revenue.

**La sonde qui a permis tout cela** : `jouer-fv2-ombres.cmd journal` (`VF3_SHADOW_WHY=1`) ecrit dans
`log.txt`, a chaque changement de decor, son numero, le nombre de cartes, la direction de chaque lumiere et
l'echelle de chaque couche. C'est ce qui transforme « ca ne marche pas sur ce decor » en un tableau.

**Mesure utile au passage** : le decor du premier combat **ne depend pas du combattant choisi**. Les douze
positions du curseur de selection donnent toutes le decor 9. Le decor suit l'adversaire, et le premier
adversaire est fixe. Et `VF3_FORCE_STAGE` change la fiche et la geometrie mais **pas les textures**, qui
restent celles du decor deja charge.

## La lumiere de chaque arene, le 2026-10-24

Mesuree en forcant `STAGE_TEST__STAGE` sur les treize arenes, une partie par decor. Le detail et les reserves
sont dans `docs/fv2-ombres.md` ; l'essentiel :

| decor | nom | lumiere de l'arene | angle | chute hors du ring |
|---|---|---|---|---|
| 0 | `TEST` | `[-0,2070 0,8310 0,5163]` | 32,0° | 0 |
| 1 | `WESTERN` | `[-0,1099 0,8117 0,5736]` | 33,8° | **0,2** |
| 2 | `AC_CARRIER` | `[0,3250 0,9055 -0,2729]` | 26,6° | 0 |
| 3 | `ROOM_00` | a l'aplomb (`w = 0`) | 0,0° | **0,25** |
| 4 | `GREAT_BM` | *illisible, l'arene ne charge pas* | | 99,99 |
| 5 | `FURNACE` | a l'aplomb (`w = 0`) | 0,0° | 99,99 |
| 6 | `STORE` | `[0,4403 0,8521 -0,2829]` | 32,7° | **1,0** |
| 7 | `HANGAR` | `[0,3813 0,9224 0,0621]` | 22,5° | **0,2** |
| 8 | `CAPSULE` | `[-0,1815 0,8288 0,5293]` | 32,1° | **1,0** |
| **9** | `DAM` — **le barrage** | `[0,1198 0,8424 0,5254]` | **30,6°** | 99,99 |
| 10 | `TOWER` | `[-0,2059 0,8296 0,5190]` | 32,1° | 99,99 |
| **11** | `BUILDING` — **celui de Honey** | `[-0,1994 0,5780 0,7913]` | **52,7°** | 99,99 |
| 12 | `MUSEUM` | `[-0,2059 0,8296 0,5190]` | 32,1° | 99,99 |

**Temoin** : le barrage ressort a 30,6°, mesure a 30,7° par un tout autre chemin. **`TOWER` et `MUSEUM`
portent la meme lumiere** au dix-milliemes (captures differentes, donc deux arenes bien chargees).
**`GREAT_BM`** ne charge pas ainsi : 6 pieces aplaties au lieu de 36, lumiere refusee sur 644 images, sol
retourne. **La chute** (fiche `+0x34`, `CONFIG__STAGE_H`) dit ce qu'il y a sous le ring ; le demi-cote
(`+0x30`) vaut 5,0 partout. **Ce que ce champ commande n'est pas etabli** : j'avais ecrit que les petites valeurs voulaient dire « un
sol juste sous le ring », c'etait une interpretation et elle est fausse — a `STORE`, qui porte 1,0, un
combattant pose a y = -0,95 tombe hors du champ.

**Comment c'est pris** : plus par `VF3_FORCE_STAGE`, qui n'ecrit que le NUMERO et que le jeu reecrit —
en forcant le 11, la sonde lisait encore la fiche du barrage. Maintenant par la **table des fiches** :
`charger_fiche_decor` prend `((void **)0x0008D118)[numero]`, tout premier combat est le numero 9, et pointer
cette entree sur une autre fiche charge cette arene par le chemin du jeu. Le journal nomme l'arene venue et
les treize adresses valent `0x7516C + n x 0x70`. C'est `VF3_STAGE=<0..26>`.

**Ce que ce chemin ne donne pas** : les textures (le ring porte celles du barrage ; forcer le chemin long via
`0x0058FDA0` n'a rien change, image identique au pixel), et la lumiere **par combattant**, puisque le numero
reste 9 — la branche locale des numeros 3, 10 et 11 n'est pas empruntee. Ce tableau donne la lumiere **rangee
dans la fiche**. Ce que l'ancienne mesure garde : aux numeros 3, 10 et 11 le jeu prend **deux** lumieres tres
ecartees, et une lumiere a l'aplomb met l'ombre SOUS le combattant. Au decor 11, 82 % des pixels assombris
sont sur les corps ; en vraie partie, medianes **9,7°** et **16,3°**.

**La lumiere appartient a l'arene, pas a la camera.** La direction sort de `S = C.P.R`, donc dans l'espace
de la camera, et elle bouge avec elle : sur 720 images au barrage, **0,27° de derive par image et 16,3° au
pire**, onze images au-dela de 5°. Ramenee avant la camera par le `root` de la vue (`R^T.d`) : **0,038° et
4,4° au pire, zero au-dela de 5°**. Les treize relevees dans cet espace sont dans `docs/fv2-ombres.md` ; les
deux vues de combattant s'y accordent a quatre decimales partout, et quatre valeurs tombent sur des axes
exacts (`[0 1 0]` a `ROOM_00` et `FURNACE`, z nul a `HANGAR`, x nul a `WESTERN`).

**`VF3_SHADOW_TABLE=1`** prend la lumiere dans cette table au lieu de la lire a chaque image. L'arene est
reconnue par sa fiche (`0x0058FD90`, indice = soustraction sur `0x0007516C`), -1 ailleurs donc VF3 garde sa
lecture et son refus. Mesure au barrage : la table donne `[0,1215 0,8422 0,5253]`, la lecture des matrices a
la meme image donne la meme chose, et **l'image rendue est identique au bit**. Elle remplace la **lecture**,
pas la dependance : quelle vue porte les combattants vient toujours des pieces aplaties, et le sol de leurs
quads. L'effet attendu aux arenes a lumiere locale n'est pas demontre — il faudra y jouer. Interrupteur
eteint par defaut.

**Un ring out, mesure** : `ring_out` est le **bit 15** de `rob_flag` (`0x00700500` / `0x00707500`), lu dans la
table de noms de Sega a `0x0007A1FC` et recoupe deux fois. C'est **la chute, pas la poussee de cote** : la
place du combattant est aux flottants `+0x10 / +0x14 / +0x18` ; teleporte a x = 20 hors d'un ring de 5, le
jeu continue ; descendu a y = -4, il pose `ring_out` tout seul a l'image suivante. Poser le bit a la main ne
fait rien, le jeu l'efface en trois images. Pendant la chute le jeu **aplatit encore 29 pieces sur 36**.
`VF3_EJECT=<image>[,x[,y]]`.

## A faire en reprenant

1. **Charger `WESTERN` avec ses vraies textures.** `VF3_STAGE` charge la fiche, la lumiere et la geometrie
   d'une arene par le chargeur du jeu, mais **pas sa memoire de textures** : le ring sort habille de celles
   du barrage et les captures sont illisibles. La piste du saut `+0x66` de `charger_fiche_decor` est
   ecartee (tenir `0x0058FDA0` au-dessus de 0x81 force le chemin long : image identique au pixel). Il faut
   trouver ce qui commande le televersement. Sans ca, rien ne peut etre juge a l'oeil.
2. **Puis le ring out sur `WESTERN`** : ejecter (`VF3_EJECT=3300,6,-0.2`), lire la peinture avec
   **`VF3_GPU_SHOT`** (pas les captures de script, voir ci-dessous), et nommer laquelle des trois regles
   refuse l'ombre a l'ejecte — la couche hors echelle 1, le « rien sous le sol », ou la vue qui ne projette
   plus.

**Attention a l'instrument** : `VF3_SHADOW_DEBUG` peint dans le nuanceur du GPU, les captures de script
sortent du rastériseur processeur. La peinture n'y apparait jamais. Un tableau de comptages de pixels tire
de ces captures a ete produit puis **retire** : il comptait les couleurs d'images ordinaires.

## Ce qui reste ouvert, et qui bloque quoi

- **Le jeu tient 43 images par seconde en combat, au lieu de 57,5, et on ne sait pas pourquoi.** Mesuré dans
  la fenêtre sur un i7-11800H et une RTX 3070 : une image du jeu coûte **17,7 à 18,3 ms** quand elle en vaut
  17,38, le gouverneur a déjà baissé l'échelle à **1x**, et une image affichée ne coûte que 4,5 ms dont
  **0,75 ms d'attente de la carte** — c'est donc le processeur, et d'abord l'émulation. Refait machine au
  calme : **les mêmes chiffres**. Le propriétaire dit que ce n'est pas son ordinaire. La seule piste non
  instruite est que son exécutable à lui porte du travail non commité (`model3.cpp`, `main.cpp`,
  `tex3d_hd.*`). **Ça bloque toute idée d'ajouter quoi que ce soit au rendu.**

- **Sortir vraiment d'un combat.** L'écran revient, la partie non (ci-dessus). La route du jeu — finir le match
  sans le montrer ni l'entendre, `0x00581455 = MATCH COUNT−1`, `0x00581406 = 64` une fois par manche, vie du
  joueur 1 à zéro — est relevée dans `docs/fv2-etats.md` et **n'est pas câblée**.

- **Le second joueur n'a jamais été introduit.** `0x0058145A == 3` commande le cut-in, le compteur de victoires
  d'affilée et la moitié à deux de `NEXT_PROG_SEL`. C'est un trou de **banc d'essai**, pas de connaissance : il
  faut une pièce et un Start du joueur 2 au bon moment. Le fermer servirait la boucle Versus, la vraie
  condition de déblocage de Honey, et `VERSUS` dans le menu.
- **La fin, l'entrée du nom, le classement et le Super K.O.** sont nommés par la table de Sega et jamais
  atteints ici.
- **Ce qui *décide* de reconstruire un combattant** — le test à `0x00060288` — est inconnu. Ce qui le
  *commande* est connu et employé (`fv2_redo_fighter_model`). Ça ne bloque rien.
- **Les branches de Survival** n'ont jamais été regardées tourner, bien que le mode marche.

Les leviers des autres lignes du menu sont mesurés et inventoriés dans `docs/fv2-menu-console-inventaire.md` :
`CHANGE YOUR VIPER`, `REMATCH`, `CHANGE STAGE` (`0x0007111C`), `OPTIONS` (`0x00120540`–`0x0012055F`), `STATS`,
`SOUNDS`, `DISPLAY`, `RANDOM`, `VERSUS`.


## Où sont les sources d'origine

Elles ne sont pas dans ce dépôt et ne doivent pas y entrer.

```
Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md     3689 lignes -- TOUT le relevé sur la ROM
Documents\SEGA Model 3\SupermodelSrc\Src\Menu\GameFV2.h      les adresses et la géométrie
Documents\SEGA Model 3\SupermodelSrc\Docs\TuilesHD.md        le pack de tuiles
Documents\SEGA Model 3\SupermodelSrc\Docs\Tex3DHD.md         les textures 3D
Documents\SEGA Model 3\ROMs\fvipers2.zip                     la ROM
C:\Users\frede\Desktop\Nouveau dossier (2)\fvipers2          les images du menu, d'où assets/ vient
```

`MenuConsole.md` a une table des matières : `grep -n "^#"`. Les sections qui comptent pour la suite sont
**§6** (savoir où en est le jeu), **§6 bis** (sortir d'un combat), **§6 quater** (les états nommés par Sega),
**§6 sexies** (le menu sur le vrai écran titre), **§13** (les pages du menu et leur géométrie).

**Rien ne doit être repris du code de Supermodel** : il est sous GPL. Les adresses et les mesures sont des
faits sur le jeu, pas du code — c'est ce qui a été porté, et rien d'autre.


## Les règles de travail, non négociables

- **Aucune prédiction.** Tout se lit dans le code ou se mesure. S'il faut décompiler, on décompile.
- **Le code dit ce qui PEUT arriver, la mesure dit lequel est arrivé.**
- **Montrer toutes les captures, sans verdict.** C'est Frédéric qui tranche.
- **Toujours un lanceur `.cmd`**, jamais une ligne de commande à recopier.
- **Ne jamais filtrer la sortie de compilation.**
- **Ajouter à côté, ne jamais remplacer ce qui marche.** Lire avant d'écrire.
- **Aucun binaire de Sega, aucune ROM, aucune image tirée du jeu dans le dépôt.**
- **Pousser sur `mine` uniquement** (`penpenlovesrei-dotcom/vf3-recomp`), jamais sur `origin`, qui est l'amont
  de `mr-finster`.
- **Rien n'est livré sans `verifier-vf3.cmd`** : cinq contrôles IDENTIQUE et le pas à pas interpréteur contre
  code recompilé sur 2231 images.

Réponses courtes. Dire ce qui est mesuré et ce qui ne l'est pas.


## Les pièges de ce portage

- **Une seule image de la RAM qui diffère de l'instruction d'où son code natif a été bâti éteint le
  recompilateur en entier.** Les correctifs doivent arriver avant `rebuild_from_ram_if_stale()`, ou provoquer
  une reconstruction.
- **Le programme de FV2 est un gzip à l'offset 0 de la CROM**, déplié en RAM 0 par l'amorce du jeu. Tous les
  correctifs sont donc des correctifs de RAM, et chacun relit avant d'écrire.
- **`board.ram` garde les mots de 32 bits dans l'ordre de l'hôte** : l'octet d'adresse A est à l'indice
  `A ^ 3`. `word(ram, a)` rend la valeur gros-boutiste directement.
- **`tools/vf3run` ne charge JAMAIS la NVRAM** : il part toujours d'une carte jamais réglée. Aucun réglage
  d'exploitant ne peut s'y prouver.
- **`vf3run` ne rend l'image que sur les marques `shot` d'un script** (et `--hd-dump`, qui force le rendu).
- **Les heredocs bash mangent les apostrophes et les échappements.** Écrire les fichiers avec l'outil Write.
- **Les fichiers du dépôt sont en CRLF.** Un script d'insertion qui cherche `\n` ne trouve rien.
- **Le lien échoue en « Permission denied »** si le jeu ou le vérificateur tourne encore.

---

# Journal du 2026-10-21, le soir

## Trois rustines sonores, et ce qu'elles ont coûté

Le son de la liste a été posé en trois fois dans la même heure, chaque fois sur un retour de Frédéric — « pas
de bruitage », « la musique ne s'arrête pas », « ça grésille » — et la quatrième fois il a écrit : « beaucoup
de bugs sonores et de comportement, je ne saurais les énumérer ».

Tout a été **défait**. Le grésillement était bien de moi (la carte son avancée une passe sur deux : trente
images de son par seconde au lieu de 57,5), mais le fond était la méthode : une rustine par symptôme ne
converge pas, et **un son ne se vérifie pas en regardant une image**. `docs/fv2-console-son.md` tient la place
du son : ce qui est acquis, les quatre questions à trancher avant d'écrire, le tableau des quatre sorties de la
liste à remplir case par case, et l'ordre des pas. Rien de sonore n'est écrit.

L'étape 1 de ce plan a quand même été faite, et elle a retourné trois idées reçues : **le jeu n'envoie jamais
`FF`** (sa commande de silence est `A0 00 01`), **l'écran titre n'a pas de musique à lui** (ce qu'on y entend
est celle de la démonstration, laissée en route), et **l'écran de sélection pose la sienne** — donc remettre
l'ancienne après un mode choisi est nuisible. Tout est dans `docs/fv2-console.md`.

## Le vrai défaut était ailleurs, et la sonde l'a nommé

« Après être sorti du menu, les deux démos sont buggées : personnages très sombres ou absents. » Quatre essais
scriptés n'ont pas su le reproduire — les démos y étaient **identiques au bit près** avec et sans la liste.
C'est en instrumentant **sa** partie (`VF3_PIC_LOG`) que la réponse est tombée en deux lignes : l'écran titre
du jeu porte 0 modèle 3D, celui que Start demandait en portait 397.

La demande de mode change ce qui est **affiché** et n'arrête pas ce qui **dessine**. De là, trois essais
mesurés, deux impasses, et la bonne route — le redémarrage de la boucle — trouvée en regardant *comment le jeu
lui-même sort d'un replay*. Aucun décompilateur : `vf3run --watch` nomme l'instruction qui écrit, et c'était
assez.

## Ce que la soirée confirme sur la méthode

- **Quatre essais scriptés qui ne reproduisent rien ne prouvent rien** : ils ne couvraient pas le chemin qu'il
  prenait, lui. Instrumenter sa partie a coûté dix minutes et a donné la réponse.
- **Demander le levier au jeu plutôt que de l'inventer.** Les deux leviers qui marchent ici sont ceux que le
  jeu s'envoie à lui-même. Les deux qui ont échoué étaient les nôtres.
- **Ne pas lancer le jeu avant d'avoir dit quoi regarder.** Fait deux fois, reproché une fois. Depuis, les
  lanceurs sont donnés, pas déclenchés.

---

# Journal du 2026-10-21

## Start, et la liste

Les deux chemins demandés hier sont posés, et la liste est enfin une surface à elle. Ce qui a changé par
rapport aux deux essais ratés n'est pas l'intention, c'est le **lieu** : `fv2_mode_list::render` dessine entre
`compose()` et le menu, sur l'image du jeu, et ne touche jamais à `game_menu`.

Le reste de la journée a tenu dans trois choses.

**Lire avant d'écrire, pour de bon.** La géométrie, le facteur 7/6, la remontée de 50/1080 et surtout la façon
de marquer la ligne choisie étaient déjà dans `SupermodelSrc\Src\Menu\GameFV2.h` et
`Src\OSD\SDL\MenuOverlay.cpp`. La première version d'ici éteignait les six autres lignes — une invention,
alors que la réponse existait : c'est la ligne choisie qui **clignote**, les autres restent pleines. Rien du
code n'a été repris ; les nombres, si.

**Un essai positif doit dire jusqu'où il porte.** La demande de mode ramène l'écran titre depuis un combat —
ce qu'on croyait impossible. Mais la relecture de `entry1` dit que la partie, elle, court toujours. Une mesure
juste peut porter une conclusion trop large : « ça marche » voulait dire « l'écran revient », pas « on est
sorti de la partie ».

**La vérification doit porter sur ce qui est en cause.** La veille, le contrôle ne distinguait pas les deux
menus. Aujourd'hui `liste-2-survival.png` ne peut pas se confondre : la liste s'ouvre sur l'écran titre du
jeu, son curseur descend de trois lignes, la ligne choisie lance la partie, et l'écran de sélection dit
SURVIVAL MODE. Il a fallu deux crochets pour ça (`VF3_QUIT_AFTER_S`, `VF3_KEYS_S`), parce que tout ce qui
compte les images du jeu s'arrête quand la liste est là.

---

# Journal du 2026-10-20

## Deux fois la même faute, et ce qu'elle apprend

Start a été branché sur `open_console()`, qui appelle `menu.open(console_page())` — **le widget RmlUi du menu
de la touche Échap**, à une autre page. Frédéric l'avait rejeté une première fois quelques heures plus tôt
(« ton menu n'est pas du tout intégré au jeu, il est dans le menu de la touche échape »), j'en étais convenu,
et je l'ai livré une seconde fois sous un autre commit.

Ce qui l'a laissé passer est plus utile que la faute : **la vérification ne pouvait pas distinguer les deux
menus.** Elle constatait que le mode console gelait le jeu au bout de 75 secondes, là où le mode PC atteignait
l'écran de sélection. Or les deux menus gèlent le jeu. Le contrôle ne portait sur rien de ce qui était en
cause, et il a été présenté comme un progrès.

`f328e77` est annulé par `ceb9a8a`. Start ne prend plus rien, dans aucun mode. Ce qui avait été mesuré dans le
commit annulé est gardé dans son message : ouvrir un menu sur Start gèle le jeu à cette image, un Start de
script doit être lu comme celui d'un joueur, et une course scriptée avec le jeu gelé se bloque sans limite de
temps venue de l'extérieur.

## Les états étaient déjà analysés, et pas ici

Frédéric : « il y a des états qui ont déjà été analysés lors du travail sur la ROM, trouve-le. »

C'est `Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md`, 3689 lignes, 194 Ko, du 14 au 20 août. Ce
portage n'en avait rien. La question du jour — « comment savoir que l'écran titre est à l'écran » — y a une
section entière, **§6 sexies**, et la réponse y est mesurée, pas déduite.

Porté dans `docs/fv2-etats.md` (`e3c1938`), puis lu par le port (`d1e0ee8`) :

- les quatre octets contigus de la machine à états, et les 24 modes que Sega a nommés en RAM, 80 chaînes d'un
  tenant à `0x00024494` ;
- l'écran titre est à l'écran quand le mode est `ADV` et le sous-état `FV2_TITLE_INT` ou `FV2_TITLE_DSP` —
  mesuré en gelant le jeu sous-état par sous-état et en regardant l'écran. **Pas** la même question que
  `fv2_in_attract()`, qui demande si une partie tourne et qui est vrai de l'avertissement, du logo, du titre,
  des présentations, des démos et des classements ;
- pour y aller, le mode et le sous-état sont **demandés**, pas forcés : le répartiteur de `0x0000ABC4` recopie
  `MODE_REQUEST` dans le mode avant de sauter. Deux images. Écrit une fois — tenir une demande est ce qui a
  cassé l'écran de sélection et son chrono, deux fois ;
- depuis un combat les deux leviers connus ne commandent rien, et l'essai négatif dit lequel des deux :
  **les valeurs étaient encore intactes une seconde après**, donc le séquenceur d'attraction ne tourne pas
  pendant un match et personne ne les lit.

Trois leçons de méthode y sont écrites noir sur blanc, et chacune a coûté une séance là-bas : une table
contiguë ne dit pas où ses tranches commencent (le découpage par les noms a été pris en défaut **trois fois**) ;
une mesure juste peut porter une explication fausse, et l'explication se propage plus loin que la mesure ; un
essai négatif doit dire lequel des deux — le jeu a recouvert nos valeurs, ou le jeu ne les lit pas.

## La mesure, faite ici

`vf3run --modes` a été ajouté : le mode et son sous-état sous les noms de Sega, à chaque changement. C'est
l'équivalent du `-menu-mode-log` d'en face, et c'est l'instrument auquel toutes les questions de *quand*
devraient être accrochées, au lieu de compter des images depuis un événement approximatif.

```
     1  WARNING_INT    0
   225  WARNING_DSP    0
   997  ADV_INT        0 DONT_RECYCLE_INT
   998  ADV_DSP        1 DONT_RECYCLE_DSP
   999  ADV_DSP        2 ADV_SEGA_INT
  1001  ADV_DSP        3 ADV_SEGA_DSP
  1193  ADV_DSP        4 FV2_TITLE_INT    <- ECRAN TITRE
  1194  ADV_DSP        5 FV2_TITLE_DSP    <- ECRAN TITRE
  1386  ADV_DSP       11 ADV_DEMO_DSP
```

**Le logo SEGA tient 1001 à 1193, soit 192 images** — le chiffre relevé il y a deux mois sur un autre
émulateur. Deux mesures indépendantes qui tombent sur un nombre que ni l'une ni l'autre ne cherchait : c'est
le témoin le plus fort de la journée.

L'écran titre tient de 1193 à 1385. Capture prise à 1255 — soixante images après, la règle établie là-bas pour
laisser passer le fondu et la voix de l'annonceur, le gel coupant la voix (à vingt images on entend « Fighting
Vipers » sans le « 2 »).

## La liste des modes, mesurée

`assets/menu-console/title_list.png`, 709 x 897, RGBA : ARCADE, V.S., RANDOM, SURVIVAL, NETWORK, TRAINING,
OPTIONS. Les sept bandes relevées ici retombent à une image près sur celles du relevé d'août, et les colonnes
de l'encre aussi. Les étiquettes sont peintes **calées à gauche** — `V.S.` a son milieu à 0,17 quand `NETWORK`
l'a à 0,50 — donc chacune doit être glissée pour amener son encre au milieu du bloc.

## Vérification

`verifier-vf3.cmd` : les cinq contrôles IDENTIQUE, et 2231 images en pas à pas entre l'interpréteur et le code
recompilé.
