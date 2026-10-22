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
