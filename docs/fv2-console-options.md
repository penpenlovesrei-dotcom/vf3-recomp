# OPTIONS : le plan

Écrit le 2026-10-22, avant d'avoir écrit une ligne. Ce n'est pas une description : ce sont les questions, dans
l'ordre, et la mesure qui tranche chacune.

`OPTIONS` est la dernière ligne de la liste qui ait un sens ici. `NETWORK` n'a rien derrière lui sur la borne ;
`OPTIONS`, si : les **treize réglages** du menu de test du jeu. L'inventaire le disait déjà — « le travail,
c'est la page, pas le mécanisme ». C'est à moitié vrai, et la moitié fausse est ce document.


## Ce qui est acquis

### Les treize réglages, mesurés en août

`MenuConsole.md` §9 : un guetteur imprimait tout octet qui changeait dans `0x120540`-`0x12055F` pendant qu'on
modifiait **un réglage à la fois** dans le menu de test.

| réglage | adresse | plage | usine |
|---|---|---|---|
| `MATCH COUNT 1P` | `0x12054C` | 2-5 | 2 |
| `MATCH COUNT VS` | `0x12054D` | 2-5 | 3 |
| `ROUND TIME 1P` | `0x120555` | 20-60 par 5 | 30 |
| `ROUND TIME VS` | `0x120556` | 20-60 par 5 | 45 |
| `DIFFICULTY` | `0x12054E` | 0-4 | 2 `NORMAL` |
| `ENERGY MAX 1P` | `0x120540` | 160-360 par 10, **16 bits gros-boutiste** | 210 |
| `ENERGY MAX VS` | `0x120542` | idem | 210 |
| `ADVERTISE SOUND` | `0x12054F` | 0/1 | `ON` |
| `SURVIVAL MODE` | `0x120550` | 0/1 | `OFF` |
| `COUNTRY` | `0x120557` | lecture seule | `JAPAN` |
| `VS FINISH` | `0x120558` | 0/1 | `OFF` |
| `SUPER K.O.` | `0x120554` | 0/1 | `ON` |
| `BIKE LOGO` | `0x12055A` | 0/1 | `ON` |

L'ordre à l'écran n'est pas l'ordre des adresses — `ROUND TIME` est neuf octets après `MATCH COUNT` alors
qu'il le précède. Et **rien ne borne une écriture directe** : le menu de test refuserait une valeur hors
plage, nous non. Les plages ci-dessus sont donc la seule garde.

`SURVIVAL MODE` (`0x120550`) est déjà employé par ce portage, à côté du drapeau de Survival.

### Le piège : ce bloc n'est qu'une copie de travail

Le jeu le charge depuis sa **sauvegarde** au démarrage. Le changer ne dure pas : seul le menu de test du jeu
recopie vers la sauvegarde, en sortant. Le menu d'août a contourné ça avec un fichier à lui, relu au
lancement et reposé **à l'instant où le jeu vient de charger son bloc**. La question 1 ci-dessous rend ce
contournement inutile : la sauvegarde est atteignable.

### Mais ce portage sait déjà faire mieux, pour l'autre jeu

Pour Virtua Fighter 3, la page « Arcade settings » n'écrit pas la copie de travail : elle écrit **la sauvegarde
elle-même**, un octet par réglage à partir de `0x1E000`, trouvés en changeant chacun dans le menu de test. Le
jeu les relit en démarrant, donc une modification **redémarre la partie** à la fermeture du menu, et elle
tient.

Et ce portage a déjà fait exactement ce travail-là pour Fighting Vipers 2 une fois : l'octet `0x17` de la
sauvegarde, le réglage COIN/CREDIT, trouvé en pilotant le menu de test du jeu depuis un script d'entrées puis
en comparant la sauvegarde avant et après (`docs/fv2-console.md`).


## Question 1 — où sont ces treize réglages dans la sauvegarde : **close le 2026-10-22**

`HANDOFF.md` §7 disait que la sauvegarde « ne contient pas ces valeurs telles quelles » et que personne ne
l'avait décodée. C'est fait, et il n'a pas fallu piloter le menu de test treize fois : il a suffi de regarder
**qui écrit la copie de travail au démarrage**.

`vf3run --watch 120540` donne `store32 00120540 = D200D2 at PC 0002026C (LR 00020F60)` — 210 et 210, les deux
`ENERGY MAX` à leur valeur d'usine, en un seul mot. `0x00020240` est un `memcpy` par mots (r3 destination, r4
source, r5 compte) : l'information est chez l'appelant, et il y en a deux.

| | r3 (vivant) | r4 (source) | compte |
|---|---|---|---|
| `0x00020D80` | `0x00120520` | **`0xFE0C0000`, la sauvegarde** | 30 demi-mots |
| `0x00020F38` | `0x00120520` | `0x00120D24`, **la table d'usine dans le programme** | 60 octets |

Et l'arbitre est `0x00020F98`, qui charge **quatre** blocs puis valide :

| bloc | vivant | sauvegarde | octets |
|---|---|---|---|
| 1 | `0x0011F024` | `0xFE0C1000` | 1604 |
| 2 | `0x0011F668` | `0xFE0C2000` | 376 |
| 3 | `0x0011F7E0` | `0xFE0C3000` | 3392 |
| 4 | **`0x00120520`** | **`0xFE0C0000`** | **60 — les réglages** |

Les quatre sont **contigus en RAM** (`0x0011F024` à `0x0012057C`) et sur **quatre pages séparées** de la
sauvegarde. La validité, c'est trois champs en tête du bloc 1 : le mot `0x0011F024` égal à `0x53454741`
(« SEGA »), le demi-mot `0x0011F028` égal à `0x8221` et `0x0011F02A` égal à `0x0312` — une estampille de
version. Si l'un manque, `0x00020F74` remet l'usine et `0x00021008` la **réécrit dans la sauvegarde**.

### La correspondance

Le chargeur lit un **mot** de la sauvegarde par **demi-mot** vivant (`lwbrx` puis `sthu`), donc chaque valeur
de 16 bits occupe quatre octets de la sauvegarde et seuls deux portent la donnée. Pour un octet vivant à
`0x00120520 + k` :

```
indice dans board.backup = 2k + 2   si k est pair
                           2k + 1   si k est impair
```

**Contrôle croisé immédiat** : l'octet des pièces déjà connu de ce portage, `0x17`, retombe sur `k = 11`, soit
`0x0012052B` — dans les `COIN ASSIGNMENTS`, à sa place. Il avait été trouvé à tâtons ; la formule le redonne.

### Les treize, mesurés

Les colonnes « vivant » et « usine » sont lues dans un dump de RAM ; la colonne « sauvegarde » est l'indice
dans `board.backup`, et sa valeur a été relue dans **votre** `backup-fvipers2` du 21 octobre. Les treize
concordent, et les valeurs d'usine sont exactement celles mesurées en août dans le menu de test — deux sources
indépendantes.

| réglage | vivant | k | sauvegarde | usine |
|---|---|---|---|---|
| `ENERGY MAX 1P` | `0x120540` (16 bits) | 32 | `0x42`, `0x43` | `00D2` = 210 |
| `ENERGY MAX VS` | `0x120542` (16 bits) | 34 | `0x46`, `0x47` | `00D2` = 210 |
| `MATCH COUNT 1P` | `0x12054C` | 44 | `0x5A` | 2 |
| `MATCH COUNT VS` | `0x12054D` | 45 | `0x5B` | 3 |
| `DIFFICULTY` | `0x12054E` | 46 | `0x5E` | 2 `NORMAL` |
| `ADVERTISE SOUND` | `0x12054F` | 47 | `0x5F` | 1 `ON` |
| `SURVIVAL MODE` | `0x120550` | 48 | `0x62` | 0 `OFF` |
| `SUPER K.O.` | `0x120554` | 52 | `0x6A` | 1 `ON` |
| `ROUND TIME 1P` | `0x120555` | 53 | `0x6B` | 30 |
| `ROUND TIME VS` | `0x120556` | 54 | `0x6E` | 45 |
| `COUNTRY` | `0x120557` | 55 | `0x6F` | 0 `JAPAN` |
| `VS FINISH` | `0x120558` | 56 | `0x72` | 0 `OFF` |
| `BIKE LOGO` | `0x12055A` | 58 | `0x76` | 1 `ON` |

### La vérification de bout en bout

Lire la table ne prouve pas qu'écrire marche. Deux runs, un témoin et un essai, la sauvegarde réelle posée
octet par octet par `VF3_POKE` et cinq réglages changés dans l'essai :

| réglage | témoin | essai | attendu |
|---|---|---|---|
| `MATCH COUNT 1P` | 02 | **05** | 05 |
| `ROUND TIME 1P` | 1E | **3C** | 3C |
| `ENERGY MAX 1P` | 00D2 | **0168** | 0168 |
| `DIFFICULTY` | 02 | **04** | 04 |
| `BIKE LOGO` | 01 | **00** | 00 |

Cinq sur cinq. **Écrire l'octet dans la sauvegarde fait charger la valeur au jeu** : `OPTIONS` sera durable,
par le même mécanisme que la page « Arcade settings » de Virtua Fighter 3, et la copie de travail n'a pas à
être touchée.

Un piège pour la prochaine fois : `VF3_POKE` lit ses valeurs en **décimal** (`%lu`), et `sscanf` **s'arrête à
la première valeur illisible** — un `D2` écrit en hexadécimal fait perdre silencieusement tout ce qui suit. Le
premier essai a conclu « le jeu ignore la sauvegarde » pour cette seule raison.

### Ce qui reste du bloc

Le bloc fait 60 octets et treize réglages n'en occupent que quinze. `0x120544`, `0x120546`, `0x120548` et
`0x12054A` portent `0x0078`, `0x0078`, `0x0045`, `0x00B4` (120, 120, 69, 180) et ne sont nommés par rien :
d'autres énergies ou d'autres durées. On n'y touche pas.

### 2. Avec quoi dessine-t-on la page ?

**La question que l'image n'a pas résolue.** `assets/menu-console/option_bg.png` est un **fond** — le logo
assombri — sans un seul libellé. Les treize noms et leurs valeurs doivent donc être du **texte**, et ce
portage n'a pas de moteur de texte en dehors de RmlUi, qui est celui du menu de la touche Échap.

Trois voies étaient ouvertes : un document RmlUi à elle, la police du jeu par le générateur de tuiles, ou la
police de la Dreamcast à sortir de `TEX_DAT.AFS`.

**Tranchée le 2026-10-22, et par un fichier plutôt que par un arbitrage** : Frédéric a déposé `police.png`
**parmi les images du menu**, dans `texture_remplacement_2d/fvipers2/`, et non parmi les tuiles nommées par
leur empreinte. C'est donc une quatrième voie, qui est la bonne : sa propre feuille de glyphes, dessinée
directement comme `fv2_mode_list.h` dessine déjà la liste des modes. Ni RmlUi, ni le générateur de tuiles, ni
l'archive du disque.

### Ce que porte `police.png`, mesuré

| | |
|---|---|
| image | 1024 × 384, RGBA, glyphes **blancs sur transparent** |
| grille | **16 colonnes × 6 rangées**, cases de **64 × 64** |
| correspondance | **ASCII `0x20` à `0x7F`**, dans l'ordre, une case par code |
| cases vides | exactement deux : `0x20` (espace) et `0x7F` — ce qui confirme la correspondance |
| position | les glyphes sont **centrés dans leur case**, horizontalement *et verticalement* |
| capitales et chiffres | tous exactement de `y = 16` à `y = 46`, le seul écart étant la queue du `Q` (13 à 49) |
| chasses de l'encre | de 6 px (`'`) à 41 px (`W`), médiane 22 |

Et la trouvaille qui compte, parce qu'elle décide de ce qu'on peut écrire avec cette feuille : **elle n'a pas
de ligne de base**. Chaque glyphe est centré dans sa case, verticalement aussi. `O` occupe `y = 16..46`, `o`
occupe `20..42`, le point `29..33`, la virgule `26..37`, les deux-points `21..41` — tous de milieu 31. Une
fonte pose ces quatre-là sur une même ligne ; celle-ci les empile autour d'un centre.

Trois conséquences, et elles sont nettes :

- un rendu proportionnel doit passer par **la boîte d'encre de chaque case**, pas par la case, puisque les
  glyphes ne sont pas calés à gauche. D'où la table des 96 boîtes dans `src/host/fv2_texte.h`, relevée par
  `tools/fv2/mesurer_police.py` ;
- **verticalement, il n'y a rien à calculer** : on dessine la case entière, et les glyphes gardent l'alignement
  que la feuille leur donne déjà. Pour une page en **capitales et chiffres** — ce que sont les treize réglages
  et leurs valeurs — c'est *exact*, puisque les 35 partagent la même boîte ;
- en revanche les **minuscules et la ponctuation sont fausses** sur une ligne de texte ordinaire : le `o` flotte
  4 px au-dessus du `O`, et le point se lit comme un point médian — `SUPER K.O.` sort en `SUPER K·O·`. Ce n'est
  pas un défaut du dessin, c'est la feuille qui est ainsi. La corriger demanderait de **choisir** une ligne de
  base que la feuille ne donne pas, c'est-à-dire d'inventer : à Frédéric de dire s'il la veut, et laquelle.

Rien dans ce portage ni dans le travail d'été ne se servait de cette feuille : la seule fonte du portage est
celle de RmlUi.

### 3. Qu'est-ce qu'une modification doit faire ? — **tranchée le 2026-10-22, et pas comme prévu**

Le plan disait : « pour Virtua Fighter 3, elle redémarre la partie à la fermeture du menu. C'est le
comportement à reprendre, pas à réinventer. » **C'était faux**, et c'est Frédéric qui l'a vu en jouant : le
redémarrage passe l'écran au noir et laisse la fenêtre muette plusieurs secondes pendant que la carte réamorce.
Il l'a d'abord pris pour un plantage, et il avait raison de le prendre pour tel — sur un écran d'options de
console, ça n'a aucun sens.

La bonne réponse est d'écrire **les deux copies** :

| | ce qu'elle apporte |
|---|---|
| la **sauvegarde** (`board.backup`, par la formule ci-dessus) | le réglage **dure** : le jeu le relira au prochain démarrage |
| la **copie de travail** (`0x00120520 + k`, directement en RAM) | le réglage **prend effet tout de suite** : c'est celle-là, et elle seule, que le jeu lit en tournant |

Écrire la seule sauvegarde oblige à redémarrer pour être lu ; écrire la seule copie de travail ne survit pas à
la partie suivante. Les deux ensemble, et il n'y a plus rien à redémarrer.

Mesuré : deux `Right` sur `ROUND TIME 1P` et la sortie de la page, sans redémarrage — l'octet `0x6B` de la
sauvegarde écrite sur disque passe de 40 à 50, et la page affiche la nouvelle valeur parce qu'elle lit la copie
vivante.

**Ce qui reste à vérifier**, et qui demande de jouer : chaque réglage ne prend effet qu'au moment où le jeu le
relit. `ROUND TIME` et `MATCH COUNT` sont a priori lus au début d'un match, `ENERGY MAX` aussi ; mais rien ne
dit qu'aucun des treize n'est verrouillé plus tôt. Un réglage qui ne bougerait qu'après un redémarrage doit
être relevé ici plutôt que supposé.


## L'ordre

1. ~~**Question 1** : les treize adresses dans la sauvegarde.~~ **Close le 2026-10-22**, relevé ci-dessus,
   vérifiée par un essai contre témoin.
2. ~~**Question 2**.~~ **Close le 2026-10-22** : `police.png`, mesurée ci-dessus.
3. La page, les treize lignes, les plages comme garde.
4. `verifier-vf3.cmd`, et une partie jouée.

## Ce que ce plan ne fait pas

- **Il n'écrit pas la copie de travail** si la sauvegarde est atteignable. Un réglage qui ne survit pas au
  redémarrage n'est pas un réglage.
- **Il ne borne rien au hasard** : les plages sont celles du §9, mesurées.
- **Il ne touche pas au menu de la touche Échap**, qui garde sa page de réglages du portage.


## Ce que le travail d'août avait déjà appris — relu le 2026-10-22

Frédéric a signalé qu'« il y avait un sous-menu avant cet écran ». Le travail d'été est dans
`Documents/SEGA Model 3/Sauvegardes/2026-08-20_23h04/Docs/MenuConsole.md`, et il dit beaucoup plus que ça.

### La forme d'OPTIONS en août

Ce n'était pas une page plate de treize lignes. `OPTIONS` était **une page de seize lignes** : les treize
`GAME ASSIGNMENTS`, un `INITIALIZE`, et deux entrées menant ailleurs —

- **`SOUNDS`** : le test des bruitages, celui des vingt musiques, les deux volumes et le choix du pack ;
- **`STATS`** : le VS DIAGRAM, dessiné à partir des compteurs de la sauvegarde.

Et la **Dreamcast a exactement cette forme** : sept réglages, puis `SOUNDS`, `CONTROLLER`, `RESULT`, puis
`INITIALIZE`. Les trois sources concordent — la page porte des réglages *et* des portes.

Notre page n'a que les treize et `INITIALIZE`. Ce qui manque, ce sont les portes, et elles ne mènent nulle
part aujourd'hui : la couche sonore a été défaite (`docs/fv2-console-son.md`), et le VS DIAGRAM n'est pas
dessiné ici.

### La densité, que le propriétaire avait mesurée avant moi

Son relevé des choses en suspens, mot pour mot : « *densité d'`OPTIONS` : seize lignes ne peuvent pas être
grosses. Deux sorties : sortir les treize `GAME ASSIGNMENTS` sur leur propre page, ou passer à deux
colonnes.* » C'est précisément le problème rencontré ici, où le pas des lignes a dû être resserré sous celui
de la Dreamcast parce qu'elle en tient sept et nous treize. **Sortir les treize sur leur propre page le
résout des deux côtés à la fois** : la page des réglages respire, et `OPTIONS` redevient une courte liste de
portes, comme sur le disque.

### Trois règles de dessin, payées en août

1. **Le filet de sélection s'ancre par le bas.** `option_cursor.png` porte un ergot vertical qui *monte
   au-dessus* du trait, donc le trait visible est en bas de l'image. Placée par le haut, elle souligne la
   ligne suivante. **Ce portage avait exactement ce défaut**, et je l'avais « corrigé » en déplaçant une
   constante jusqu'à ce que ça tombe bien — ce qui n'est pas la même chose : à une autre taille il serait
   revenu. Corrigé le 2026-10-22, le filet est maintenant suspendu à la position voulue de son trait.
2. **Un bloc se centre sur la largeur qu'il *peut* occuper, jamais sur celle qu'il occupe.** Sinon la page
   glisse sous l'œil dès qu'une valeur s'allonge — mesuré en août sur `DIFFICULTY` passant de `HARD` à
   `VERY HARD`. D'où son `SetValueRoom`, où chaque page déclare sa largeur réservée : neuf caractères pour
   `OPTIONS` (`VERY HARD`), quatorze pour `SOUNDS` (`BRAIN CLEANSER`). **Ce portage ne le fait pas** : chaque
   valeur est centrée sur la sienne, donc elle se recentre quand on la change. La Dreamcast, elle, centre bien
   ses valeurs — la tension est réelle et n'est pas tranchée.
3. **Une page trop longue cherche de la place avant de rétrécir** : on élargit la bande (jusqu'à 0,20 en haut
   et 0,96 en bas) avant de réduire le texte. Sans cet ordre, toute augmentation de la police est annulée par
   le resserrement, « deux effets qui s'annulent exactement, et une correction qui semble ne rien faire ».

### Et une chose qu'on a faite mieux

Son relevé portait aussi : « *police italique : la référence Dreamcast est en italique, notre atlas ne l'est
pas. Un second atlas le permettrait.* » Il n'en a pas fallu un second : la pente est mesurée à 7,5° et posée
par cisaillement au dessin (`src/host/fv2_texte.h`).


## L'arborescence, refaite sur celle d'août — le 2026-10-22

Demandé par Frédéric : refaire l'arborescence des sous-menus d'OPTIONS en prenant pour exemple août 2026.
Elle a été relue dans `Src/OSD/SDL/Main.cpp` de la sauvegarde du 20 août (`OptionsEntries` et `OptionLines`),
pas reconstituée de mémoire.

```
OPTIONS
├─ les treize réglages        nom à gauche, valeur centrée
├─ DISPLAY                    → un écran           (demi-ligne de blanc avant)
├─ SOUNDS                     → un écran
├─ CONTROLLER                 → un écran
├─ STATS                      → un écran
└─ INITIALIZE                 une action, en image  (1,2 ligne de blanc avant)
```

Les quatre portes sont dessinées **en gras et centrées**, comme en août et pour la raison qu'il donnait :
elles mènent quelque part plutôt qu'elles ne règlent quelque chose. Le gras est l'épaississement du titre
sans la condensation — la feuille n'a qu'une graisse, c'est ainsi qu'elle en gagne une seconde. Les deux
blancs sont les siens. `B` **remonte d'un écran** au lieu de viser la liste des modes en dur : c'est sa pile
de retour.

La Dreamcast a les mêmes portes à deux noms près — `SOUNDS`, `CONTROLLER`, `RESULT`, puis `INITIALIZE`.
`DISPLAY` et `STATS` sont du fork, `RESULT` est du disque ; on suit août, c'est ce qui était demandé.

**Ce qu'il y a derrière chaque porte, honnêtement : rien encore**, sauf `CONTROLLER`, qui était déjà
titre-et-rien-d'autre en août — « *LA PAGE CONTROLLER, VIDE POUR L'INSTANT. Demandée le 2026-08-18 : le
titre et rien d'autre* ». Pour celle-là l'arbre est donc complet. Les trois autres portaient : `DISPLAY`
`WIDESCREEN`, `HD ART`, `FPS COUNTER`, `ENDING` ; `SOUNDS` le test des bruitages, les vingt musiques, les
deux volumes et le choix du pack ; `STATS` le VS DIAGRAM. Chacune affiche aujourd'hui son titre et, en
sourdine, ce qu'elle tiendra.

### La densité, annoncée en août et mesurée ici

Le relevé d'août disait : « *seize lignes ne peuvent pas être grosses* ». La page en porte **dix-huit**.
Mesuré sur une capture de 1440 × 1080 :

| | la Dreamcast | cette page |
|---|---|---|
| hauteur de capitale | 0,0292 | **0,0176** — 60 % |
| pas des lignes | 0,0584 | **0,0361** — 62 % |

C'est exactement ce qu'il avait prévu. **Tranché le jour même par Frédéric** : la première de ses deux
sorties, les treize réglages sur leur propre page.

### L'arborescence telle qu'elle est

```
OPTIONS
├─ GAME ASSIGNMENTS  → les treize réglages, puis INITIALIZE
├─ DISPLAY           → un écran
├─ SOUNDS            → un écran
├─ CONTROLLER        → un écran (titre seul, comme en août)
└─ STATS             → un écran
```

`OPTIONS` fait cinq lignes, `GAME ASSIGNMENTS` en fait quatorze. **`INITIALIZE` a suivi les réglages**, et
c'est une divergence délibérée d'avec août et le disque, qui le gardent tous deux au pied d'`OPTIONS` : il
ne remet que ces treize valeurs, donc sur une page qui ne les montre plus ce serait un bouton dont on ne peut
pas voir l'effet. Eux pouvaient l'y laisser, les réglages y étaient.

Et **une page courte ne s'étire pas** : le pas est celui du disque tant que les lignes le permettent, et le
bloc se centre dans sa bande. Sans quoi `OPTIONS`, avec ses cinq lignes, aurait eu un pas trois fois celui de
`GAME ASSIGNMENTS` et les deux écrans n'auraient pas eu l'air du même menu.

### Ce que ça a rendu, mesuré

| | capitale | pas | % du disque |
|---|---|---|---|
| dix-huit lignes | 0,0176 | 0,0361 | 60 % |
| quatorze lignes | **0,0231** | **0,0472** | **79 %** |
| la Dreamcast | 0,0292 | 0,0584 | 100 % |

Il reste 21 % d'écart, et il vient de ce que le disque n'affiche que **sept** réglages là où la borne en a
treize. Les réduire voudrait dire en cacher, ce qui n'est pas la même chose que les ranger.


## Les pages derrière les portes, portées — le 2026-10-22

Frédéric a dû le demander deux fois : la première livraison était la charpente, avec quatre portes qui ne
menaient nulle part. Les voici remplies, ligne pour ligne d'après `Src/OSD/SDL/Main.cpp` du 20 août.

### `DISPLAY` — `DISPLAY_LINES`

| ligne | ici |
|---|---|
| `WIDESCREEN` | **la forme de l'image**, qui défile sur les cinq du portage (`FIT`, `4:3`, `16:10`, `16:9`, `21:9`). Août n'en avait que deux, d'où son `ON`/`OFF` ; la ligne est la même, elle en fait plus |
| `HD ART` | `ON` ou `NONE`. **Enjambée** : le pack de textures 3D est lu une fois au démarrage et rien ne sait le relâcher |
| `FPS COUNTER` | **réel**, le compteur du portage |
| `ENDING` | **enjambée** : la fin à la Dreamcast est du fork, rien de tel ici |

### `SOUNDS` — `SOUND_LINES`

| ligne | ici |
|---|---|
| `SE` | **réelle depuis le 2026-10-23** : les 256 index de la banque `0x2E`, joués à chaque pas. Les trois seuls nommés s'affichent par leur nom, les autres par le numéro du SOUND TEST du jeu (index + 353) |
| `SE VOLUME` | **réelle**, jauge de vingt segments |
| `BGM` | **réelle depuis le 2026-10-23** : les vingt musiques par leur nom, jouées à chaque pas, arrêtées en quittant la page. Août l'affichait `BGM` et l'aiguillait sur `BGM TEST`, deux lignes `VOLUME` étant indiscernables |
| `BGM SET` | **enjambée** |
| `BGM VOLUME` | **réelle**, jauge de vingt segments |
| `INITIALIZE` | **réelle**, remet les deux volumes à fond |

Les quatre lignes enjambées de `SOUNDS` demandent toutes de **tendre une note à la carte son**, c'est-à-dire
la couche défaite le 2026-10-21 et qui attend que `docs/fv2-console-son.md` soit validé. Elles sont dessinées
et passées, jamais retirées : « *une ligne avec rien à basculer est enjambée plutôt que retirée* ».

Les jauges sont `option_vol_up_on.png` et `option_vol_up_off.png`, vingt segments, **dans leurs couleurs à
lui** : les teinter au jaune de la ligne choisie ne disait rien, le segment allumé étant déjà jaune.

### `CONTROLLER`

Titre et rien d'autre, comme en août où elle fut demandée ainsi le 18. **Complète.**

### `STATS` — le VS DIAGRAM

Le tableau du jeu, `vs_global.png`, avec les chiffres posés dans ses cases : quatorze personnages, un `TOTAL`
en jaune, et huit colonnes. La grille est relevée sur l'image par ses filets blancs — `x` de 327 à 2219 en
huit parts égales, `y` de 99 à 1391 — ce qui retombe exactement sur la mesure d'août.

Les compteurs viennent de la sauvegarde, dans le même codage espacé que les réglages, par la disposition de
son §12 : `bloc = base + n × 0x1A8`, `KO` à +0x000, `JUDGE` à +0x018, puis `FIGHT`, `WIN` et `LOSE` par
adversaire tous les 8 octets — **seize adversaires et non quatorze**, parce que la rangée fait 0x80 et que Del
Sol est l'adversaire 14. `DRAW` n'est rangé nulle part : c'est `FIGHT − WIN − LOSE`, calculé au dessin, ce
qui explique qu'aucune recherche en mémoire ne l'ait jamais trouvé.

**Une seule chose a dû être retrouvée** : ses offsets sont dans *son* vidage de NVRAM, pas dans le `backup`
de ce portage. L'empreinte qu'il imprime les relie — à son `0x188` il lit `210 210 120 120 69 180`, qui est
le bloc `GAME ASSIGNMENTS`, et la même suite est à `0x42` ici. Donc **le nôtre est le sien moins `0x146`**, ce
qui place le premier bloc à `0x3006` — quelques octets dans la troisième page de la sauvegarde, qui est
précisément là où commence le troisième bloc. Deux contrôles indépendants qui concordent.

**Non confirmé sur des chiffres vivants** : tous les compteurs sont à zéro dans une sauvegarde où aucun match
n'a jamais été joué. Il faudra en jouer un pour le dire.

`SKO 1` et `SKO 2` restent **vides**, et c'est voulu : la liste des points en suspens d'août les donne
« *affichés à zéro sans qu'on sache où ils vivent — le seul chiffre du tableau qu'on ne puisse pas
justifier* ». Une case vide le dit ; un zéro le prétendrait.


## `SOUNDS` remplie — le 2026-10-23

Les deux essais de la page marchent. Ce qui les bloquait n'était pas le tableau des sorties mis de côté — ils
ne touchent pas à la musique du jeu — mais le fait qu'une note tendue à la carte **sous un écran gelé** ne
s'entendait pas. C'est fait depuis la veille, donc ils sont faisables.

| ligne | ce qu'elle fait |
|---|---|
| `SE` | parcourt les 256 index de la banque `0x2E` et joue à chaque pas |
| `BGM` | parcourt les vingt musiques par leur nom et joue à chaque pas |

Les noms et la numérotation sont les tables mesurées d'août : sur la banque `0x10` le numéro du SOUND TEST est
l'index plus dix, sur `0x2E` l'index plus 353. **Vérifié ici deux fois** dans le relevé MIDI : l'attraction
envoie `A0 10 07`, qui est le n° 17 `ATTRACT`, et le replay `A0 10 11`, qui est le n° 27 `DEMO`.

Et une décision à nous : **une musique d'essai est arrêtée en quittant la page**, par la commande de silence du
jeu. Sans ça un essai suivait le joueur dans la partie, et ce n'est plus un essai.

`BGM SET` reste enjambée : les packs de musique sont du fork, et ses propres notes disent que le remplacement
y a été branché sans jamais être entendu.

`DISPLAY` est laissée à ses quatre lignes d'août, dont deux vivantes. Lui en ajouter (échelle de rendu, choix
du rasteriseur, mouvement fluide) serait inventer une page qui n'est pas la sienne ; les deux enjambées le
sont pour une raison dite à leur ligne.


## Pourquoi `ROUND TIME` ne prenait pas, et ce qu'il fallait écrire en plus — 2026-10-23

Frédéric a mis `ROUND TIME 1P` à 60 et le round a duré 30. Mesuré, et la réponse n'est pas celle que
`docs/fv2-console-options.md` supposait depuis la veille.

**Le match ne lit pas le réglage.** Il lit un mot **déjà converti** : `0x0000C2EC` choisit entre `0x00581418`
(un joueur) et `0x0058141C` (deux) selon `0x0058145A`, et pose le résultat dans le chrono `0x00581404`.

**Et la conversion n'a lieu qu'une fois, à l'amorçage.** `VF3_RAMLOG` montre `0x00581418` déjà égal à `0x780`
à l'**image 223**, en mode 0, et il ne bouge plus de toute la partie. Le calcul est en `0x00038DB8` :
`rlwinm ...,6`, soit **la valeur fois 64** — le jeu compte 64 images par seconde pour ce chrono, pas 60.
30 × 64 = 1920, et 60 × 64 = 3840.

Écrire le réglage seul ne pouvait donc **rien** faire après l'amorçage, et c'est exactement ce qu'il a vu. La
version qui redémarrait la partie le cachait sans le résoudre.

### Quels réglages sont ainsi mis en cache

Pas deviné : deux amorçages ont été vidés, l'un avec les réglages poussés et l'autre non, et **tout
`0x00580000`–`0x00582000` comparé**. Deux endroits seulement bougent :

| réglage | son cache | la forme |
|---|---|---|
| `ROUND TIME 1P` | `0x00581418` | valeur × 64 |
| `ROUND TIME VS` | `0x0058141C` | valeur × 64 |
| `ADVERTISE SOUND` | `0x0058142C` | valeur + 1 |

Les **énergies n'ont rien bougé** : elles sont lues vives et n'ont besoin de rien.

### La correction

`Model3::fv2_set_option` écrit maintenant **trois** choses : la sauvegarde, la copie de travail, et le cache
quand il y en a un (`fv2_recook_option`).

**Vérifié**, en deux moitiés qui se rejoignent.

*Que le cache soit relu* : en posant `0x00581418` à `0xF00` à partir de l'image 1500 — donc bien après
l'amorçage — le round suivant part à **3840 images, soixante secondes**, contre 1920 au témoin. Le cache est
donc relu à chaque match, et l'écrire suffit.

*Que la page écrive bien ce mot-là* : `VF3_OPTION_LOG=1` fait dire à `fv2_recook_option` ce qu'elle pose et où.
La route est `tools/scripts/fv2-console-options-timer.txt` — Start à l'image 1200 tombe sur l'écran titre et
ouvre la liste ; de là le jeu est gelé, donc la suite se pilote à l'horloge (`VF3_KEYS_S`), seule chose qui
atteigne une surface dessinée par-dessus un jeu arrêté :

```
VF3_HIDDEN=1 VF3_OPTION_LOG=1 VF3_SCRIPT=tools/scripts/fv2-console-options-timer.txt
VF3_KEYS_S=Down@30,Down@31,Down@32,Down@33,Down@34,Down@35,Return@37,Return@39,Down@41,Down@42,Left@44,Left@45,Right@47,Right@48
```

Six *bas* mènent à OPTIONS, deux *valider* ouvrent la page puis GAME ASSIGNMENTS, deux *bas* tombent sur
`ROUND TIME 1P`, et les pas donnent :

```
option 120555 = 55, cooked 00581418 = 3520 (55 s)
option 120555 = 50, cooked 00581418 = 3200 (50 s)
option 120555 = 55, cooked 00581418 = 3520 (55 s)
option 120555 = 60, cooked 00581418 = 3840 (60 s)
```

Les deux moitiés se recouvrent sur `3840` : la page pose le mot que le match relit.

**À savoir pour le prochain essai** : si le réglage est déjà au maximum, un pas vers la droite ne change rien
et n'écrit rien — c'est ce qui s'est passé au premier essai, où la sauvegarde portait déjà 60. Faire descendre
la valeur avant de la remonter, sinon on lit un silence pour une panne.

### Le round de vérification

Les deux moitiés se recouvrent, mais aucune des deux n'est un match. Celui-là a été joué, en partie cachée, du
premier appui jusqu'au chrono : `tools/scripts/fv2-console-options-round.txt` ouvre la liste à l'image 1200,
`VF3_KEYS_S` mène à `ROUND TIME 1P` et le fait **descendre de 60 à 20**, ressort de la page par deux retours
arrière, l'appui de l'image 1260 rouvre la liste, *valider* prend ARCADE, et le poing de l'image 1500 choisit
le combattant. `VF3_TIMER_LOG=1` suit alors `0x00581404`.

La sauvegarde a été copiée dans un dossier de données à part, pour que celle du propriétaire ne bouge pas :
elle portait 60, donc l'amorçage avait cuit `3840`. Le journal donne :

```
option 120555 = 20, cooked 00581418 = 1280 (20 s)
...
timer f 1701: 1280 left (20.0 s), the round starts from 20 (1P) / 45 (2P)
timer f 3401: 1280 left (20.0 s), the round starts from 20 (1P) / 45 (2P)
timer f 4842: 0 left (0.0 s)
```

**Deux rounds partis de 1280**, les vingt secondes posées par la page, et non les 3840 de l'amorçage. Le
premier s'est terminé avant la fin du temps (remise à 1280 à l'image 3401), le second a expiré à l'image 4842.

Deux choses se lisent en passant :

- **`0x00581404` compte des images, pas des secondes.** 1280 pour vingt secondes : soixante-quatre par
  seconde, le même compte que la conversion. Les `5, 4, 3, 2, 1` relevés dans `fv2-flux-de-match.md` étaient
  ses cinq dernières images, pas des secondes.
- **`ROUND TIME VS` est resté à 45**, la valeur de la sauvegarde que rien n'a touchée. L'écriture ne déborde
  pas sur le réglage voisin.

## VS DIAGRAM : les vrais chiffres

Le tableau était construit et lisait déjà la sauvegarde, mais sur rien : dans une sauvegarde où aucun match
n'a été mené à terme, tous les compteurs sont à zéro, et un tableau de zéros ne dit pas si on lit au bon
endroit. Quatre matchs joués en partie cachée, chacun contre la sauvegarde que le précédent avait écrite,
l'ont rempli — et ont corrigé la description au passage.

### Un match ARCADE n'écrit rien de tout ça

Le round de vérification de la section précédente a bougé **quatorze octets**, tous ailleurs. Le tableau est
un relevé de **versus** : c'est un versus qu'il faut. Les routes sont dans `tools/scripts/` :
`fv2-console-vs-diagram.txt` (personne ne bouge), `fv2-console-vs-decide.txt` (le joueur 1 frappe sans
arrêt), `fv2-console-vs-juge.txt` (il frappe huit secondes puis laisse le chrono finir) et
`fv2-console-vs-sko.txt` (poing et pied alternés). Toutes passent par `ROUND TIME VS` au minimum, pour que les
rounds expirent vite.

### Ce que chaque match a déplacé

| match | ce qui s'est passé | ce qui a bougé dans la table |
|---|---|---|
| 1 | quatre rounds, personne ne bouge → quatre nuls | `FIGHT` 0 → 4 et `0x020` 0 → 4, **dans les deux blocs** |
| 2 | trois rounds décidés au KO | `FIGHT` 4 → 7, `LOSE` 0 → 3 d'un côté, `WIN` 0 → 3 de l'autre, `KO` 0 → 3 ; **`0x020` reste à 4** |
| 3 | frappes puis chrono, à santé inégale | `FIGHT` 7 → 10, un `WIN` et un `LOSE`, **`0x018` 0 → 1**, `0x020` 4 → 6 |
| 4 | poing et pied alternés | `FIGHT` 10 → 13, `LOSE`/`WIN` +3, `KO` 3 → 6 ; **rien à `0x008` ni `0x010`** |

Cinq octets bougent par match, jamais un de plus : bloc 10 à `0x4096` et bloc 11 à `0x423E`, chacun portant
l'autre comme adversaire, et chaque paire symétrique — le `LOSE` de CHARLIE contre EMI en face du `WIN` d'EMI
contre CHARLIE. La base, le pas et la case par adversaire tombent juste **en même temps**, ce qu'aucun
compteur isolé n'aurait pu montrer.

### Trois choses apprises

**`0x020` est DRAW**, que la note d'août donnait pour nulle part. Le match 1 l'a mis à 4 avec quatre nuls ;
le match 2 l'a laissé à 4 en ajoutant trois combats décidés — un total de combats aurait suivi `FIGHT`
jusqu'à 7. Et il s'accorde avec `FIGHT - WIN - LOSE` sur chaque ligne mesurée : 10 − 0 − 4 = 6, 10 − 4 − 0 = 6.
C'est le chiffre stocké qui est lu maintenant, étant le compte du jeu lui-même plutôt qu'une soustraction
entre trois de ses autres.

**Le codage est contigu**, pas espacé comme celui des réglages : deux octets gros-boutiens en `+0` et `+1`
d'une case de huit, donc six octets sur huit inutilisés. Un compte de 4 n'a bougé que l'octet de `+1`.

**SKO 1 et SKO 2 sont presque trouvés, et restent vides.** Les scalaires vont de huit en huit : `0x000` KO,
`0x008`, `0x010`, `0x018` JUDGE, `0x020` DRAW. Trois des cinq sont mesurés, ce qui fixe le pas, et l'écran
nomme exactement cinq compteurs de cette espèce — KO, SKO 1, SKO 2, JUDGE — dans cet ordre. Les deux trous
sont donc presque sûrement SKO 1 à `0x008` et SKO 2 à `0x010`. **Presque n'est pas mesuré** : le quatrième
match est allé les chercher et n'a produit que des KO ordinaires. Il faut un vrai Super K.O. avant de
remplir ces deux cases ; d'ici là une case vide dit ce qu'on sait, un zéro prétendrait savoir.

### Ce que la page montre

Photographiée sur la sauvegarde des quatre matchs (F12, qui traverse maintenant les surfaces de la
console) :

```
CHARLIE   FIGHT 13   WIN 0   LOSE 7   DRAW 6   KO 0   JUDGE 0
EMI       FIGHT 13   WIN 7   LOSE 0   DRAW 6   KO 6   JUDGE 1
TOTAL           26         7        7       12      6         1
```

Les lignes tombent sur les bons personnages, et le total est la somme des colonnes — chaque combat y compte
deux fois, une par participant, puisque les deux blocs l'enregistrent.

## `DISPLAY` : `HD ART` et `ENDING`

Les deux lignes enjambées de cette page sont branchées. Elles ne l'ont pas été de la même façon : l'une
n'attendait qu'un levier, l'autre a demandé une mesure.

### `HD ART` : un vrai interrupteur

La ligne ne faisait que rapporter, parce que le pack de textures 3D était lu une fois au démarrage et que
rien ne savait le relâcher. Il n'a pas fallu de nouveau levier : **`load()` vide ce qu'il tient avant de
lire**, donc charger depuis nulle part est la façon de reposer un pack, et le rendu GPU vérifie déjà le
nombre de pages au lieu de le supposer — son propre commentaire prévoyait qu'un pack arrive plus tard.

- `NONE` quand il n'y a rien à charger, et la ligne est alors enjambée comme toute ligne sans réglage.
- `ON` / `OFF` sinon, et le réglage suit dans `settings.ini`.

**Mesuré** avec un pack de quarante PNG tirés du dump : allumée, la page dit `ON` et le journal rapporte
*40 textures loaded, 0 left out, in 1 page*. Éteinte, *the 3D texture pack put down*, la page refermée, le jeu
a rendu sept cents images de plus sans une erreur — c'est là que le tampon du GPU est refait, donc c'est là
que ça devait casser si ça devait casser.

### `ENDING` : la fin du jeu, qu'on peut enfin regarder

Août en faisait un interrupteur pour ses propres illustrations, dont rien n'existe ici. Mais **le jeu a une
fin** — le générique, mode 12 — que personne n'avait jamais vue tourner : y arriver demande de finir l'échelle,
et les notes la rangent parmi les états jamais atteints. La ligne n'est donc pas son interrupteur mais un
moyen de la regarder : *valider* la demande au jeu et la page s'efface.

Elle se termine toute seule : le générique dure une centaine de secondes, puis le jeu repasse par sa boucle
d'attraction et revient à son écran titre. Il n'y a rien à remettre en place.

**La moitié seulement en sort, et la ligne est gardée quand même**, parce que la moitié qui sort est le
générique lui-même.

La fin **alterne** : environ 225 images de cartons sur le viper, qui se dessinent exactement comme il faut, et
environ 225 d'un passage en 3D — 279 modèles sur 16 vues, 383 plus tard — qui se dessine comme un **sol blanc
nu avec une silhouette grosse comme le pouce**, sous des bandeaux 2D restés de la démonstration (« Old
Armstone Town », « v.s. GRACE »). Rien du décor n'est là.

### Une frontière qui n'en était pas une

Il faut raconter l'erreur, parce qu'elle est instructive et qu'elle a tenu une demi-journée.

En poussant `MODE_REQUEST` une image à la fois et en photographiant **toujours à l'image 1900**, le générique
paraissait cassé demandé de 1201 à 1210, entier à partir de 1212, et partiel à 1211 — une frontière nette à
dix-huit images après `FV2_TITLE_DSP`, qu'on pouvait expliquer par un écran titre qui n'a pas fini de
s'installer. Une attente de quarante images a été posée là-dessus.

**C'était un artefact de l'échantillonnage.** Puisque la fin alterne toutes les 225 images, déplacer la
demande déplace *la phase dans laquelle tombe l'image 1900* : à 1201 elle tombait sur un passage 3D, à 1250
sur un carton. Photographiées **au même décalage depuis `ENDING_INT`** — +300 et +650 — une demande à 1201 et
une à 1250 donnent les deux mêmes images, aux mêmes tailles d'octet près. **Le moment de la demande ne change
rien du tout.** L'attente a été retirée.

La leçon, pour la prochaine fois : comparer des images prises au même instant d'un phénomène périodique, et
non au même numéro d'image absolu. Le « 1211 partiel » qui semblait si parlant n'était qu'une transition de
phase prise au vol.

### Pourquoi il n'y a pas de décor : la fin est faite de rediffusions

Sega a laissé les noms de ses états en clair, et ce sont eux qui répondent. Les sept sous-états de la fin
(`docs/fv2-etats.md`, qui dit aussi comment les lire) :

```
ending_trans_init   ending_trans_wait   ending_replay_init
ending_replay_ctrl  ending_replay_next  ending_final_mask1  ending_final_mask2
```

**Trois sur sept ne parlent que de replays.** Le générique n'est pas un film : c'est le défilé des crédits
alternant avec **les rediffusions des combats que le joueur vient de gagner**. Les bandeaux que j'avais pris
pour des restes de démonstration — « Old Armstone Town », « v.s. GRACE » — sont les surimpressions du replay
lui-même, qui annonce son lieu et son adversaire, exactement comme dans l'attraction.

Donc, demandée depuis l'écran titre, la fin n'a **aucun match à rediffuser**. Le sol reste nu parce qu'il n'y
a pas de décor à charger : il n'y a pas de combat.

### Les trois images qui le prouvent

La même image de la fin, au même décalage depuis `ENDING_INT`, demandée de trois façons :

| comment on y entre | ce qui sort | poids |
|---|---|---|
| `MODE_REQUEST = 12` depuis l'écran titre | un sol blanc nu, une silhouette grosse comme le pouce | 16 Ko |
| `MODE_REQUEST = 12` **depuis un combat** | les **deux combattants** du combat, texturés, avec leurs ombres — toujours sur un sol nu | 28 Ko |
| la **branche du jeu lui-même** | la rediffusion entière : *Cross River Damsite*, la cage, les deux combattants, « v.s. PICKY » | 324 Ko |

La deuxième ligne est la charnière : demander le mode depuis un combat fait apparaître les combattants, parce
qu'ils sont chargés — et pas le décor, parce que le replay n'a pas été préparé.

La troisième se fait sans rien forcer du rendu. `fv2_game_NEXT_PROG_SEL` branche sur la fin quand
`ADV_INFO_STATUS` porte le bit 3, « l'échelle est finie » — un bit que les notes d'été donnaient comme *lu dans
le code, jamais vu posé*. Il suffit de le poser : l'octet bas de `0x005813F0` vaut `00` juste avant la branche,
donc on n'écrase rien. Le jeu prend alors sa propre route, avec tout ce qu'elle fait en plus du mode —
`FUN_0000DD70`, l'arrêt de sept tâches, le brouillard effacé, le compteur d'écran à `0x140` — et la fin sort
entière.

```
VF3_POKE=5813f0=8 VF3_POKE_FRAMES=7170-7178
vf3run ... --script <la route d un combat> --modes
   7178  ENDING_INT   19      au lieu de CONTINUE_INT
```

### Demandée depuis l'écran titre, elle ne peut que le générique

Elle montre **les cartons**, qui sont le contenu propre de la fin, et le jeu se rend la main tout seul. Les
rediffusions, elle ne peut pas : il n'y a rien à rediffuser quand on arrive de l'écran titre, et un replay
ne s'invente pas.

**La vraie fin du jeu est à portée, et elle y est maintenant.**

### Le levier : où poser le bit 3

`fv2_game_NEXT_PROG_SEL` est le point de branche de **chaque round**, pas seulement de chaque match. C'est
toute la difficulté : poser le bit trop tôt fait prendre la branche à un round intermédiaire, qui n'a encore
rien enregistré à rediffuser. Mesuré sur un combat dont le vrai point de branche tombe à l'image 7178 :

| posé à | ce que fait le jeu |
|---|---|
| 7178 | **trop tard** — la branche a déjà lu le bit, le jeu va à `CONTINUE` |
| 7177, 7176 | la fin, entière |
| 6922, une seule fois | la fin, entière — le bit **tient** 256 images, rien n'a besoin de le maintenir |
| 3000, en plein combat | la fin à l'image 4740, **à la fin du premier round**, et **nue** |
| 1250, sur l'écran titre, avant même le match | pareil : nue, à 4740 |

Les deux dernières lignes sont la leçon. Le bit survit à tout — l'écran de choix, le début du match — mais il
est consommé par le premier `NEXT_PROG_SEL` venu, et celui d'un round sans match fini n'a pas de replay.

**Le jeu dit lui-même quand le match est fini** : `0x40000000` dans le même mot, le bit que sa propre question
« un round de plus ? » va lire. D'où la règle, et elle est simple :

> poser le bit 3 à la première image où `ADV_INFO_STATUS` porte déjà `0x40000000`.

Une écriture, jamais tenue, comme toutes les demandes de ce portage. Le jeu réécrit le mot entier à chaque
image depuis `0x000122C4` en conservant sa valeur, donc une écriture posée avant une image vaut pour les
lectures de cette image-là.

### Ce que la ligne `ENDING` fait maintenant

Deux choses, que gauche et droite choisissent :

| valeur | ce que *valider* fait |
|---|---|
| `ROLL NOW` | le générique tout de suite, par-dessus l'écran titre. Les crédits sont entiers, les passages de replay sont nus — il n'y a pas de match à rediffuser |
| `AFTER A MATCH` | **arme**. Au premier match qui se termine, le jeu prend sa propre branche et donne sa fin entière, replays sur leurs décors |

Armée, la ligne le dit (`>`) et la page reste ouverte : il y a une partie à aller jouer. L'armement tient
jusqu'à ce qu'un match se termine.

### Le tour de vérification

Joué dans l'hôte, en partie cachée, de bout en bout : la liste, `OPTIONS`, `DISPLAY`, `ENDING` mis sur
`AFTER A MATCH`, *valider*, sortie de la page, puis `ARCADE` et un match. Le journal :

```
the console's list: OPTIONS chosen
the console's list: ARCADE chosen
the console ENDING: the match is over -- the game's own ending asked for     image 6258
image 6835 mode 12 sub 19     ENDING_INT, par la branche du jeu
image 6847 mode 13 sub 19     ENDING_DSP
image 7073 : 336 modeles      le passage de replay, garni
```

**336 modèles** dans le passage 3D, là où la demande nue n'en donnait que les restes.

### Et le décor est bien là — pour le match qu'on a joué

Photographié dans l'hôte, à l'image près, la fin montre **Cross River Damsite** : le barrage, l'eau, la cage,
les deux combattants en plein échange, « v.s. EMI ». C'est la rediffusion du match qui vient d'être joué, sur
son décor, en écran large.

**Mais la fin rediffuse l'échelle entière, et l'échelle n'a qu'un barreau.** Les passages se suivent :

| passage | modèles | ce qu'on voit |
|---|---|---|
| le premier | 334 à 338 | le match joué, décor compris |
| le suivant | 279 | le sol nu, « Old Armstone Town », « v.s. GRACE » |

Le second n'est pas un défaut du portage : c'est un adversaire que le joueur n'a jamais affronté, donc un
replay qui n'existe pas. Une vraie partie d'arcade en remplirait huit.

**Et c'est ce qui explique ma longue erreur de lecture.** Quatre photos de suite sont tombées sur le second
passage ou sur un carton, et j'en ai conclu que l'hôte ne chargeait pas les décors alors que `vf3run` les
chargeait — deux moteurs, deux résultats, une belle fausse piste. Il n'y avait aucune différence de moteur :
l'image de `vf3run` était le premier passage, les miennes le second. **Même leçon qu'à la section
précédente, au même endroit : sur un phénomène qui alterne, une image prise au hasard ne prouve rien.**

### Le tour de vérification

Joué dans l'hôte, en partie cachée, du premier appui jusqu'au générique. La liste s'ouvre à l'image 1200, la
page mène à `DISPLAY` puis à `ENDING`, *valider* la demande, et la page s'efface. Le jeu reprend à l'image 1200,
et — ce tour ayant été joué pendant que l'attente de quarante images était encore en place, dà déduite depuis :

```
image 1241 mode 12 sub 5     ENDING_INT, quarante-sept images apres FV2_TITLE_DSP
image 1253 mode 13 sub 5     ENDING_DSP
```

Les photographies montrent les cartons entiers et qui avancent — *Chief Programmer HIDEYA SHIBAZAKI* puis,
vingt secondes plus tard, *Character Designers KENTAROW NISHIMURA...* — sur le viper, en écran large. **Les
deux sont tombées sur des cartons**, ce qui est précisément le piège décrit plus haut : elles ne disent rien
des passages 3D, qui eux restent au sol nu.

Et elle rend la main toute seule, ce qui était la condition pour n'avoir rien à remettre en place :

```
image 7167 mode 18 sub 5     OVER_INT : le generique est fini
image 7360 mode  3 sub 1     la boucle d attraction reprend
image 7558 mode  3 sub 5     l ecran titre
image 7777 mode  3 sub 10    la rediffusion, comme si de rien n etait
```

Le jeu passe donc par son GAME OVER avant de revenir à l'attraction. Six mille trois cents images entre
la demande et le retour, un peu moins de deux minutes.
