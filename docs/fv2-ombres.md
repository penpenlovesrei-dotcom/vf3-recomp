# Les ombres de Fighting Vipers 2 : ce qu'elles sont

Mesuré le 2026-10-22 dans `C:\Temp\fv2-ombres` (branche `fv2-ombres`), sur **l'image 3101** de
`tools/scripts/fv2-fight.txt` — décor 1, Picky contre Charlie, sans fenêtre (`vf3run`, rasteriseur logiciel).
Rien ici n'est déduit : chaque ligne renvoie à une mesure, et la section « ce qui n'est pas établi » dit où
s'arrête ce qui a été vu.

## La réponse

L'ombre d'un combattant est **un jeu de modèles 3D à part**, que le jeu bâtit **en RAM de polygones à chaque
image**, **aplatis** sur le plan du sol, **sans texture**, **sans éclairage**, et peints d'**une seule couleur
opaque** — celle du décor.

Ce n'est donc ni un disque texturé, ni une couche de polygones noirs translucides, ni le modèle du combattant
réutilisé avec une matrice d'aplatissement.

## Ce qui le dit, mesure par mesure

| ce qui a été mesuré | comment |
|---|---|
| **37 maillages** de l'image portent la couleur de sommet `#051A26` | `VF3_MESH_AUDIT=3101` |
| **les 37 sont en RAM de polygones** (adresse < `0x100000`), aucun en ROM | idem |
| **aucun n'est texturé** (`tex 0`), **aucun n'est éclairé** (drapeau `lighting` absent) | idem |
| **aucun n'est translucide** : alpha `FF`, ni `poly_alpha` ni `texture_alpha` | idem |
| **ils sont plats** : étendue verticale de **0,000 à 0,020** en espace caméra, pour une emprise au sol de 1,44 sur 1,52 | idem |
| **ils sont au niveau du sol** : y de −1,31 à −1,26, quand les corps sont de −0,29 à +0,35 | idem |
| **aucune de leurs adresses de modèle n'apparaît ailleurs** dans l'image | comparaison des adresses |
| **386 pixels sur 576** pris dans l'ombre de Charlie valent exactement `#051A26` | relevé sur le PNG |
| **peints en magenta, ces 37 maillages couvrent les deux ombres et rien d'autre** | `VF3_MARK_RGB=051A26` |
| **aucun polygone `layered` dans l'image** : 0 sur 485 | `VF3_MESH_AUDIT` |

La valeur des pixels est le témoin le plus net : si ces polygones étaient éclairés, la couleur à l'écran
vaudrait la couleur de sommet multipliée par `soleil + ambiante` et ne tomberait pas sur `#051A26` au bit près.
S'ils étaient translucides, le sol se mêlerait dedans. Ni l'un ni l'autre.

## Combien, et de quelle taille

L'image porte **485 maillages**. Les ombres s'y répartissent sur deux vues :

| | ombres | ce qui se tient au-dessus de leur emprise |
|---|---|---|
| vue 5 | 17 modèles, **3 324 sommets** | 52 modèles, 25 482 sommets |
| vue 9 | 19 modèles, **4 566 sommets** | 75 modèles, 18 924 sommets |

Donc **une ombre par partie du corps**, et de l'ordre du **cinquième à un septième** des sommets de ce qui se
tient dessus. C'est bien une version simplifiée du modèle du personnage — simplifiée en nombre de parties
(19 contre 75) autant qu'en sommets — et non un décalque aplati du modèle complet, puisque aucune adresse
n'est partagée.

Le plus gros maillage d'ombre fait 864 sommets, soit 144 polygones ; le plus petit 72 sommets, soit 12.

## Ce qui n'est pas établi

- **Une seule image, un seul décor, deux combattants.** Rien ne dit encore que `#051A26` est la couleur
  d'ombre de tous les décors, ni que le compte tient ailleurs. C'est la question 2.
- **Ce que sont les vues 5 et 9** n'a pas été cherché. Les deux portent des ombres ; pourquoi la scène est
  découpée ainsi n'est pas mesuré.
- **Le chemin GPU n'a pas été regardé.** `VF3_MARK_RGB` est dans le rasteriseur logiciel seulement. Rien ne
  laisse attendre une différence — les deux chemins sont comparés au démarrage — mais ce n'est pas mesuré.
- **Qui fabrique ces modèles dans la RAM de polygones**, et par quelle projection, n'a pas été cherché dans le
  code du jeu.
- **Les ombres des décors** (peintes dans les textures ou non) n'ont pas été regardées.

## Les instruments, tous éteints par défaut

Deux interrupteurs ajoutés à côté de ce qui existait, rien remplacé :

| | |
|---|---|
| `VF3_MESH_AUDIT=<image>` | tous les maillages de cette image : sommets, adresse, RAM ou ROM, texture, couleur, alpha des nœuds, les neuf drapeaux, et l'étendue en espace caméra (`src/video/r3d_scene.cpp`) |
| `VF3_MARK_RGB=<RRGGBB>` | tout maillage dont la couleur de polygone est celle-là est peint en magenta (`src/video/r3d_raster.cpp`) |

L'auditeur qui existait, `VF3_TRANS_AUDIT`, ne montre que les maillages translucides : il ne pouvait pas voir
ces ombres, qui sont opaques.

`montrer-ombres-fv2.cmd` refait les deux images et l'audit.

## Combien, et qui les produit

Mesuré sur **36 images de la boucle d'attraction** (`tools/scripts/fv2-ombres-attract.txt`, une image toutes
les 200 entre 1400 et 9000 — pas un crédit, pas une touche), **19 968 maillages** en tout, plus l'image 3101
du combat. Un maillage est compté comme ombre quand il est **plat, sans texture, sans éclairage, et bâti en
RAM de polygones** ; la couleur n'entre pas dans le test, pour qu'elle puisse être un résultat.

| image | décor | ombres | par amas | couleur |
|---|---|---|---|---|
| 3101, un combat | `09` | 37 | **17** (vue 5) + **20** (vue 9) | `#051A26` |
| 7001, une démonstration | `01` | 30 | **15** (vue 4) + **15** (vue 7) | `#302010` |
| 5001, une présentation de personnage | `15` | 16 | **16** (vue 5) | `#000000` |

**Qui** : **chaque combattant, dans sa propre vue.** Les amas se séparent exactement en deux quand les deux
combattants sont écartés, et chacun tient dans une seule vue du Real3D — vues 5 et 9 au combat, 4 et 7 (ou 8)
dans la démonstration. Le jeu donne donc une vue par combattant, et l'ombre suit son combattant.

**Combien** : **une par partie du corps**, 15 à 20 selon le combattant — et **le compte ne bouge pas d'une
image à l'autre pour un même combattant** : 15 maillages / 2 682 sommets d'un côté, 15 / 2 802 de l'autre,
inchangés sur les treize images échantillonnées du décor `01`. Ce n'est donc pas une ombre recalculée pose par
pose avec un nombre variable de morceaux : c'est un jeu de pièces fixe, que le jeu replace.

**La couleur appartient à la scène** : trois situations, trois couleurs, et jamais deux dans la même image.
`#051A26` bleu nuit sur le décor `09`, `#302010` brun sur le décor `01`, `#000000` sur la présentation. La
marque magenta, refaite sur le décor `01` avec `VF3_MARK_RGB=302010`, tombe là encore sur les deux ombres et
nulle part ailleurs.

**Aucun objet du décor n'a d'ombre de cette sorte.** Sur les 19 968 maillages des 36 images, les seuls qui
portent la signature sont les amas des combattants. Rien sous les grillages, les bâtiments ou les objets.

**Ce que le décor a à la place : ses textures.** À l'image 3101, **424 maillages sur 485 sont texturés et
portent la couleur de sommet blanche** (`#FFFFFF`) ; 16 sont texturés et colorés, 45 non texturés et colorés —
dont les 37 ombres. Ce qui assombrit un décor vient donc de sa texture et de la lumière, pas d'un polygone
d'ombre posé devant.

### Ce que cette mesure ne dit pas

- **Trois décors seulement** (`09`, `01`, `15`), et le `15` est une présentation de personnage, pas une arène.
  Rien ne dit que les 23 autres décors tiennent la même règle.
- **La règle de reconnaissance est la mienne**, pas celle du jeu : plat + sans texture + sans éclairage + en
  RAM. Elle attrape bien les ombres sur ces images, et elle a laissé de côté deux maillages plats et sombres
  **en ROM** sur le décor `15` (`#002628`, `#001406`) qui n'ont pas été identifiés.
- **D'où vient la couleur** — une table par décor, une couleur de sommet écrite par la tâche qui bâtit
  l'ombre — n'a pas été cherché dans le code du jeu.
- **Le chemin GPU** n'est toujours pas regardé.

## Ce que coûte une image aujourd'hui

Mesuré le 2026-10-22 **dans la fenêtre**, sur la machine de Frédéric, par `mesurer-budget-fv2.cmd`
(`VF3_PACE_LOG=1`, voie PC) : une partie jouée à la main, de l'écran titre jusqu'à un combat. Les lignes sont
dans `log.txt`. Une course sans fenêtre ne peut pas répondre à cette question : le gouverneur y est éteint et
rien n'est présenté à l'écran.

```
i7-11800H, 8 coeurs (16 fils), RTX 3070 Laptop, ecran 60 Hz, vsync on
fenetre 1366 x 768, rendu GPU (Direct3D 12), mouvement lisse, une image par rafraichissement
```

### Le fait principal

**L'émulation seule dépasse le temps d'une image du jeu.** En combat, mesuré et stable sur une vingtaine de
lignes :

| | |
|---|---|
| une image du jeu (le PowerPC) | **17,6 à 18,1 ms** |
| le temps dont elle dispose à 57,524 Hz | **17,38 ms** |
| images par seconde réellement tenues | **43**, au lieu de 57,5 |
| une image affichée | **4,4 à 4,8 ms** |
| échelle de rendu | **1x (682 x 384)**, baissée par le gouverneur |

Le gouverneur a bougé **une fois**, et il ne l'a plus jamais défait :

```
governor: pictures take 4.3 ms, over a frame's time: the render scale lowered to 1x for now
```

À ce moment l'image du jeu coûtait 13,3 ms. Le budget d'une image affichée est `1000/60 − émulation x 0,959`,
soit **moins de 4 ms** — et il devient **négatif** quand l'image du jeu atteint 17,9 ms. Le gouverneur a donc
coupé l'échelle de moitié, et le jeu tourne quand même sous sa vitesse nominale.

### Où part le temps d'une image affichée

Sur 4,5 ms, en combat, à 1x :

| | |
|---|---|
| les téléversements | 1,25 à 1,32 ms |
| les triangles préparés (processeur) | 1,06 à 1,18 ms |
| l'entre-deux du mouvement lisse (processeur) | 0,61 à 0,66 ms |
| le rendu GPU | 1,34 à 1,47 ms, dont **0,60 à 0,67 de préparation** |

Donc **environ 0,75 ms à attendre la carte graphique**, et tout le reste sur le processeur. La RTX 3070 ne
fait presque rien ; c'est le processeur qui tient la laisse, et d'abord l'émulation du PowerPC.

### Ce que ça dit pour la suite

Il n'y a **pas de budget disponible** sur cette machine en combat : l'image est déjà en retard, l'échelle est
déjà tombée à 1x, et le gouverneur ne la remonte pas. Toute technique ajoutée se heurterait à lui — et sa
deuxième règle dit exactement ce qui arriverait d'un coût posé sur le processeur : quand l'essentiel d'une
image est processeur, il **garde l'échelle et ne fait rien**, parce que la baisser n'y changerait rien.

### Ce que cette mesure ne dit pas

- **Ce n'est pas la charge de la machine.** C'était l'explication que je tenais, et elle est fausse : la
  mesure a été refaite machine au calme, les autres sessions fermées, et elle donne **les mêmes chiffres**
  — une image du jeu de 17,7 à 18,3 ms, 42 à 44 images par seconde, et le gouverneur qui coupe l'échelle à
  1x au même endroit (3,9 ms contre 4,3). Hors fenêtre, en revanche, la charge se voit : les mêmes 1300
  images ont pris 17,9 s puis 20,9 s pour un calcul identique (même empreinte de RAM,
  `47062ED87FD9228A`).
- **Pourquoi ces 43 images par seconde n'est pas établi.** Le propriétaire dit que ce n'est pas son
  ordinaire. La seule piste restante n'a pas été instruite : son exécutable à lui n'est pas celui-ci — sa
  copie de travail porte du travail non commité sur `model3.cpp`, `main.cpp` et `tex3d_hd.*` — et la
  comparaison des deux sur le même travail a été lancée puis arrêtée. **On fait avec**, décision du
  propriétaire.
- **Une seule séance, une seule fenêtre.**
- **Un seul décor**, celui du premier adversaire.
- Que ces 43 images par seconde soient l'ordinaire de cette machine ou non **n'est pas établi ici** : c'est le
  propriétaire qui le sait.
- Le coût **du 3D seul** n'a pas été isolé de celui de l'émulation : les deux chiffres sont donnés côte à
  côte, pas séparés par une expérience.

## Ce qu'on remplacerait, et ce que chaque voie demande

> **Écrit avant la lecture du jeu, et dépassé par elle.** Gardé parce que les mesures qu'il porte
> tiennent toujours ; pour le classement des voies, voir « a / b / c, repris après la lecture du jeu »
> en fin de document.

Rien ici n'est une recommandation : c'est le classement de ce que les trois mesures autorisent à dire, et de
ce qu'il faudrait mesurer ensuite. Le choix est au propriétaire.

### a. Mieux dessiner l'ombre que le jeu dessine

| | |
|---|---|
| ce que ça demande | reconnaître les polygones d'ombre |
| ce que la mesure dit | on sait les reconnaître : la signature **plat + sans texture + sans éclairage + en RAM** les attrape sur 36 images de l'attraction et sur l'image 3101 du combat, et la marque par couleur les isole exactement |
| ce qui reste inconnu | le coût ; et surtout **le contrôle du démarrage**, qui compare l'image du GPU à celle du processeur. Un adoucissement posé sur un seul des deux chemins les ferait diverger |

Le contrôle en question n'est pas théorique : il tourne à chaque lancement et il est déjà bavard — mesuré ce
soir, `1 049 087 pixels de 3D, 68 401 pixels diffèrent (1 dessiné d'un seul côté, 6 de plus de 8, couleurs de
40 au plus)`, et le portage garde la version GPU. Une option allumée d'un seul côté changerait ce bilan.

### b. Remplacer l'ombre du jeu par une ombre calculée

| | |
|---|---|
| ce que ça demande | cacher celle du jeu, et en calculer une |
| ce que la mesure dit | **la cacher est le point facile** : 15 à 20 maillages reconnaissables par combattant, qu'il suffit de ne pas dessiner. Et **chaque combattant est dans sa propre vue**, donc chacun a son éclairage de vue |
| ce qui reste inconnu | **la direction du soleil de ces vues n'a jamais été relevée** — ni si elle correspond à l'ombre que le jeu dessine. Et le coût d'une seconde passe de géométrie sur les ~20 000 sommets d'un combattant |

C'est la mesure suivante la moins chère, et elle décide de cette voie à elle seule : relever `sun.xyz`,
`sun.w` et l'ambiante des vues qui portent les combattants, et les comparer à l'orientation de l'ombre que le
jeu pose au sol. L'auditeur existe déjà, il ne lui manque que l'éclairage de la vue.

### c. Éclairer la scène autrement

Une idée reçue tombe ici, et elle est mesurée. On dit de ce jeu que sa couleur est **peinte dans les
sommets**, donc qu'un éclairage ajouté éclairerait deux fois. À l'image 3101 :

| | |
|---|---|
| texturés, couleur de sommet **blanche** | **424 maillages sur 485** |
| texturés et colorés | 16 |
| non texturés et colorés | 45, dont **37 sont les ombres** |

Donc l'immense majorité de la scène est **texture x (soleil·normale + ambiante)** : l'éclairage du Real3D est
réel et il travaille. Ce qui manque n'est pas la lumière, ce sont **les matières** — la texture *est* la
couleur finale, sans albédo ni rugosité à en extraire.

L'occlusion ambiante en espace écran, elle, a déjà son entrée : le nuanceur **écrit déjà la profondeur comme
une couleur** (`o.depth`, couche opaque de `r3d.hlsl`).

## Le contrat de fidélité

Il ne se discute pas, il se lit : le portage a déjà le patron, et c'est celui du pack de textures 3D.

```
bool hd_3d = false;                                     faux par defaut            (main.cpp:458)
flag("hd_3d", hd_3d);                                   lu de settings.ini         (main.cpp:503)
if (hd_3d) fprintf(f, "hd_3d = %d\n", int(hd_3d));      ECRIT seulement si allume  (main.cpp:562)
else if (a == "--hd-3d") arg_hd_3d = 1;                 et son contraire --no-hd-3d (main.cpp:1145)
if (cfg.hd_3d) { ... }                                  rien ne tourne sans lui    (main.cpp:1347)
```

Toute ombre ajoutée suit ce patron : **une option, éteinte par défaut**, dont rien ne tourne tant qu'elle
n'est pas demandée, et `verifier-vf3.cmd` passe sans elle — ses cinq contrôles IDENTIQUE et ses 2231 images
en pas à pas. C'est ce qui a été vérifié ce soir pour les deux interrupteurs de mesure, qui suivent la même
règle.

## Ce que le jeu en dit lui-même

Tout ce qui précède a été *mesuré*. Ce qui suit est *lu* — dans les noms de mise au point de Sega
(`C:\Temp\fv2-ghidra\symbols_dbg.csv`, 745 adresses tirées du programme), dans la RAM du jeu aux images 3101
et 7001, et dans le décompilateur (projet `FV2`, `fv2_prog.bin`). La lecture confirme les mesures et donne ce
qu'aucune mesure ne pouvait donner.

### La couleur d'ombre : une table par décor, et le code qui la pose

`FUN_000224EC` :

```c
if ((DAT_00591d14 & 0x4000000) == 0) {
    p = FUN_000225b8(PTR_ROB0_INFO__FLAG_INFO_0007511c);   // la tache du combattant 0
    SHADOW_COLOR__B0 = p[0];  SHADOW_COLOR__B1 = p[1];
    p = FUN_000225b8(PTR_ROB1_rob_flag__BIT_00075120);     // celle du combattant 1
    SHADOW_COLOR__B2 = p[0];  SHADOW_COLOR__B3 = p[1];
    SHADOW_COLOR__B16 = *(u32 *)(&DAT_00111318 + STAGE_TEST__STAGE * 4);
}
FUN_000333f4(0x98001000, &SHADOW_COLOR__B0, 0x20);
```

- `0x98000000` est **la RAM de polygones** du Real3D (`src/machine/model3.cpp`, le `case 0x98`). Ces couleurs
  sont donc écrites dans la **table de couleurs** où les en-têtes de polygones vont chercher la leur par
  index (`src/video/r3d_scene.cpp`, `ci = colour_table + ((h[4] >> 8) & 0xFFF)`).
- **Les entrées 0 à 3 viennent des tâches des deux combattants**, pas d'une palette de mise au point, ce que
  leurs valeurs changeantes laissaient croire.
- **L'entrée 16 est la couleur du décor**, prise dans la table `0x00111318` indexée par `STAGE_TEST__STAGE`.

Lue dans la RAM, cette table donne **les treize décors d'un coup** :

| décor | couleur | décor | couleur | décor | couleur |
|---|---|---|---|---|---|
| 0 | `#000000` | 5 | `#37170B` | 10 | `#00061B` |
| 1 | `#302010` | 6 | `#1B1B14` | 11 | `#000B13` |
| 2 | `#3B444B` | 7 | `#354049` | 12 | `#201711` |
| 3 | `#0A0A1A` | 8 | `#214B68` | 13 et au-delà | `#000000` |
| 4 | `#1F1529` | 9 | `#051A26` | | |

Les deux valeurs mesurées à l'écran — `#051A26` au décor 9, `#302010` au décor 1 — sont celles de la table,
à l'octet près. Et tout décor au-delà de 12 donne du noir, ce qui explique le `#000000` de l'écran de
présentation (qui annonce le décor `0x15`).

### La lumière : elle n'est PAS par décor

C'est la réponse à « comment la lumière est gérée dans tous les décors », et elle est négative.

`ROB_LIGHT` est posé **une fois**, à partir de constantes fixes (désassemblé à `0x00018F30`) :

```
lfs  f2, 0x13C8(0x11)  ->  0.6    ->  ROB_LIGHT__ambient
lfs  f1, 0x1504(0)     ->  1.0    ->  ROB_LIGHT__ambient_fog  et  __intensity
lfs  f1, 0x13CC(0x11)  -> 30.0    ->  ROB_LIGHT__distance
lis/subi               -> 0x9A00  ->  ROB_LIGHT__xang  ET  __yang
```

Aucun index de décor nulle part. Puis, **à chaque image**, `FUN_0001A124` :

| | |
|---|---|
| `ROB_LIGHT__yang` | `= (champ +0x0C d'une tâche) + 0x7038` — **l'azimut suit ce champ**, pas le décor |
| `ROB_LIGHT__xang` | forcé à `0xC000` (distance 2,0) ou `0x9A00` (distance 30,0) selon des bits de mode — **deux élévations fixes** |
| le vecteur | une position (`r14+0x94..0x9C`) **moins** le point à `distance` dans la direction `xang`/`yang` |
| puis | recopié dans `STAGE_TEST__VEC_X/Y/Z`, `__AMBIENT`, `__AMB_FOG`, `__INTENSITY`, et le brouillard depuis `ROB_LIGHT__red/green/blue/den/atn/ofs` |

Ce qui se vérifie dans la RAM : au combat (décor 9) `xang 0x9A00`, `yang 0x6D58`, `distance 30`, `ambient 0,6`,
`intensity 1,0` ; dans la démonstration (décor 1) `xang 0x9A00`, `yang 0x9A00`, **les mêmes** ambiante,
intensité et distance. Le vecteur mesure 29,79 pour une `distance` de 30 : **`ROB_LIGHT` est une position de
lumière ponctuelle**, pas une direction de soleil à l'infini.

**Donc : l'ambiante et l'intensité sont les mêmes dans tous les décors.** Ce qui change d'un décor à l'autre,
c'est la couleur de l'ombre, et seulement elle. Ce qui change en cours de partie, c'est l'azimut de la
lumière, qui suit un champ de tâche — le même mécanisme qui fait tourner le décor d'une manche à l'autre
(`fv2_game_SET_INT`, `docs/fv2-flux-de-match.md`).

### Ce que la lecture n'a pas encore donné

- **La routine qui bâtit la géométrie des ombres** n'est pas trouvée. On sait où va la couleur, pas qui
  aplatit les modèles.
- ~~Le transfert ne porte que 32 octets~~ — **faux, et démenti par la mesure** : `VF3_DMA_LOG` montre
  `dma src 003C2D04 dst 98001000 len 32 words`, soit **128 octets**, les entrées 0 à 31. Le `0x20` passé
  à `FUN_000333F4` est un compte de **mots**, pas d'octets, et l'entrée 16 est donc bien transmise.
- **`ADV_INFO__SHADOW_ROB`** vaut `0x00707500` aux deux images lues — un pointeur dans le bloc du joueur 2,
  jamais suivi.
- Ce que vaut le champ `+0x0C` de la tâche dont dépend l'azimut n'a pas été regardé.

## La routine qui bâtit les ombres : il n'y en a pas

Cherchée, et la réponse est négative — ce qui est la réponse la plus utile de la soirée.

**La géométrie d'ombre est une donnée de la ROM.** Elle n'est calculée par rien : elle est dessinée par les
auteurs, rangée dans la CROM, et téléversée une fois dans la RAM de polygones pendant l'amorce.

### Comment on le sait

`VF3_DMA_LOG`, ajouté à `src/machine/model3.cpp`, journalise chaque transfert du Real3D avec son PC et son LR.
Sur la course de combat (3105 images) :

| | |
|---|---|
| transferts vers la RAM de polygones **depuis la RAM** | 37 996 |
| transferts vers la RAM de polygones **depuis la ROM** | **450**, un par image à partir de l'image 547 |
| ce que les transferts venus de la ROM couvrent | les mots **010000 à 058FC6** |

Les 36 modèles d'ombre relevés par l'audit tombent tous dans cet intervalle — ceux de la vue 9 entre `011350`
et `01A409`, ceux de la vue 5 entre `04F0D2` et `050815`. Et l'un d'eux se lit en clair dans le journal :

```
f556  src FFA87278  dst 98045278  (mot 01149E)  848 mots
```

`01149E` est le modèle d'ombre de 588 sommets de la vue 9.

### La chaîne, lue dans le décompilateur

```
FUN_00056B50          _DAT_00580D44 = 0x10000          le pointeur d'allocation en RAM de polygones
                      nombre d'entrees lu en ROM
                      pour chaque entree non vide ->
FUN_00056814(i)       lit (longueur, source) dans une table de la ROM
                      relocalise les noeuds de culling sur la nouvelle base    (FUN_000569A0)
                      les envoie en RAM de culling                             (0x8C......)
                      avance le pointeur d'allocation
                      puis envoie le modele en RAM de polygones  ->
FUN_00056A98          decoupe en tranches de 0x8000 mots  ->
FUN_000333F4          l'aide de copie vers le Real3D -- la meme que pour SHADOW_COLOR
```

**`_DAT_00580D44 = 0x10000`** est le témoin le plus net : le code part du mot `0x010000`, et la mesure trouve
les transferts venus de la ROM à partir du mot `010000`. Deux relevés indépendants sur le même nombre.

### Ce qui fait qu'un modèle est une ombre

Puisque rien ne la fabrique, ce qui distingue une ombre tient en trois choses, toutes déjà établies :

1. **Ses polygones portent un index de couleur, pas une couleur** — et l'index tombe sur `SHADOW_COLOR[16]`,
   la couleur du décor, que `FUN_000224EC` réécrit à chaque image dans la table de couleurs de la RAM de
   polygones.
2. **Ils sont sans texture et sans éclairage** (mesuré : drapeau `lighting` absent, `tex 0`).
3. **Ils sont posés à plat au sol** par leur matrice — et c'est là, dans la matrice, que se trouve la dernière
   question ouverte.

C'est aussi pourquoi ces modèles sont en **RAM de polygones** et non laissés en ROM : un modèle de la RAM de
polygones peut prendre sa couleur par index dans une table que le jeu réécrit, un modèle de la ROM non.

### Ce qui reste ouvert

- **Qui pose la matrice qui aplatit l'ombre**, et si `ROB_LIGHT` y entre. C'est le travail par image sur les
  nœuds et les matrices, pas sur la géométrie.
- Les adresses des tables de la ROM (`0xFFDC5000` pour le compte, et les deux tables indexées) sont les
  déplacements du décompilateur lus comme des adresses absolues ; elles n'ont pas été relues dans la ROM.

## La matrice qui aplatit, et oui, ROB_LIGHT y entre

C'était la dernière question ouverte. Elle est tranchée, et par deux chemins qui se rejoignent au
cinq-millième.

### C'est la matrice qui aplatit, pas le modèle

L'audit imprime maintenant, pour chaque modèle, sa matrice et le **déterminant** de sa partie linéaire
(`VF3_MESH_AUDIT`, `src/video/r3d_scene.cpp`). À l'image 3101 :

| | |
|---|---|
| les modèles du **corps** | `det 1.000000` — des rotations pures |
| les modèles d'**ombre** | `det 0.000000` — des matrices **singulières** |

Le modèle d'ombre de la ROM n'est donc pas plat : c'est la matrice qui l'écrase. Et **chaque membre a son
orientation** — 17 matrices distinctes pour les 20 pièces d'ombre de la vue 9 — donc l'ombre suit bien la
pose, membre par membre.

### La direction de l'aplatissement, extraite des matrices

Une matrice d'ombre vaut `S = C · P · R` : la caméra, la projection, la rotation du membre. `C` et `R` étant
orthonormales, **`S · Sᵀ = C · P · Pᵀ · Cᵀ` ne dépend plus du membre**. Mesuré sur les 37 pièces :

```
S.S^T identique d'un membre a l'autre : ecart maximum 0.00014
```

Une seule projection, donc, partagée par toutes les pièces des deux combattants. En écrivant
`P = I − k·Nᵀ` (la forme d'une projection sur un plan de normale `N` le long d'une direction `d`, avec
`k = d/(d·N)`), l'ajustement donne :

```
residu final 0.000019      k.N = 1.0000
verticale du monde, vue camera   N = [0.0000  0.9994  0.0345]
direction de projection          d = [0.2673  0.8443  0.4645]
```

Le résidu dit que c'est **exactement** une projection sur un plan, pas un écrasement approximatif. Et `N`
redonne le léger tangage de la caméra, que la première mesure avait vu comme une « épaisseur » de 0,02.

### Et cette direction est celle de ROB_LIGHT

```
direction de projection, tiree des matrices     d = [0.2673   0.8443   0.4645 ]
soleil que le Real3D recoit pour le decor       s = [0.26734  0.84427  0.46448]
```

**Les mêmes cinq décimales.** La direction selon laquelle le jeu aplatit ses ombres est le vecteur de lumière
du décor, celui que `FUN_0001A124` calcule depuis `ROB_LIGHT` et que `FUN_0000619C` normalise et écrit dans la
vue du Real3D.

La projection fait **30,7°** avec la verticale du monde.

Un détail qui a failli m'égarer : les vues des deux combattants ont **chacune leur propre soleil**, à 36° et
40° de la direction de projection. C'est attendu, et c'est même une confirmation : `ROB_LIGHT` est une
**position** à distance 30, donc chaque combattant la voit sous un autre angle, alors que l'ombre est
projetée selon la direction du décor.

Au passage, l'ambiante et l'intensité relevées dans **toutes** les vues valent `0.6000` et `1.0000` — les deux
constantes lues dans l'initialisation de `ROB_LIGHT`, retrouvées à l'autre bout de la chaîne.

### Ce qui reste ouvert

- **Par quel chemin la direction arrive dans la matrice n'est pas lu.** La géométrie le prouve au
  cinq-millième, mais les références absolues du décompilateur ne montrent qu'un seul lecteur de
  `STAGE_TEST__VEC` (`FUN_0000619C`, qui sert la vue). Le code qui bâtit la matrice d'ombre y accède donc
  autrement — par un pointeur, ou depuis la même valeur calculée — et ce chemin n'a pas été trouvé.
- La hauteur du plan de projection (le sol) n'a pas été cherchée dans le code.

## Par quel chemin la lumière arrive dans la matrice

C'était la dernière inconnue, et la réponse explique pourquoi je ne la trouvais pas : **le jeu n'utilise pas
le vecteur, il utilise les angles.** Le Real3D reçoit un vecteur normalisé ; l'ombre, elle, est bâtie depuis
`LIGHTANG_X` et `LIGHTANG_Y`. Même lumière, deux expressions, et une seule référence absolue au vecteur --
d'où mon impasse.

### La chaîne, maillon par maillon

```
FUN_00012D68(param_1, dst)        batit la matrice de projection :
    uVar1 = ADV_INFO__LIGHTANG_X              elevation
    uVar2 = ADV_INFO__LIGHTANG_Y              azimut
    si (ADV_INFO__STATUS & 0x20) == 0   ->    les angles PROPRES au combattant,
                                              a r14+0x6E18 et r14+0x6E1A
    si elevation > 0x8000 : elevation = 0x4000          (ecretee)
    d = sin(elevation) ; si d < 0.0001 : d = 0.0001     (plancher)
    tourner de  azimut                        FUN_00002470
    echelle (1.0, 0.0, 1/d)                   FUN_000025E8   <- Y ECRASE
    basculer de -(elevation - 90 deg)         FUN_000023F0
    tourner de -azimut                        FUN_00002470
    ranger a dst                              FUN_000020D4

FUN_0006B6D4(partie)              dessine une partie du corps, puis :
    FUN_00006B3C(noeud du membre)                       le membre
    si *(char *)(STAGE_TEST__ROB_LIGHT + 0x6E) == 0 : return     <- l'interrupteur d'ombre
    FUN_00002078(r14 + 0x1B6E)                          charge la matrice de projection
    FUN_000021C0(&UNK_005819BC)                         la multiplie par celle du membre
    ... puis le noeud de l'ombre
```

`DAT_00001504` vaut 1,0 et `DAT_00001500` vaut 0,0 : l'échelle `(1, 0, 1/sin)` est bien l'écrasement, et le
`1/sin(élévation)` est l'étirement que donne une lumière basse. C'est la projection faite à la main, telle
qu'on l'écrivait en 1998.

### Ce que vaut r14, et comment on le sait

`r14` est **la structure du combattant**. Ce n'est pas une déduction : tracé à l'exécution, l'appel
`FUN_00002078(r14 + 0x1B6E)` reçoit `007072B8` et `0070E2B8`, soit exactement `0x00700500 + 0x6DB8` et
`0x00707500 + 0x6DB8` -- les deux pointeurs rangés en `[0007511C]` et `[00075120]`, ceux-là mêmes dont
`FUN_000224EC` tire les couleurs d'ombre par combattant.

La matrice d'aplatissement appartient donc **au combattant**, à `+0x6DB8`, et c'est `FUN_00012D68` qui
l'écrit (996 écritures sur la course, toutes depuis `PC 000020E0`, `LR 00012E4C`).

### La vérification, par deux routes qui ne se parlent pas

À l'image 3101, `ADV_INFO STATUS` vaut 0 : le bit `0x20` est clair, donc ce sont les angles du combattant qui
servent.

| | |
|---|---|
| lu dans la RAM, `combattant + 0x6E18` | `xang 2A2A`, soit **59,29°** d'élévation |
| donc la projection fait, de la verticale | **30,71°** |
| mesuré dans les matrices de la scène (section précédente) | **30,7°** |

Et l'octet d'ombre, `ROB_LIGHT + 0x6E` (`000755CA`), vaut **1** pendant le combat.

Les deux combattants portent les mêmes angles (`xang 2A2A`, `yang 9393`), ce qui est cohérent avec le fait
mesuré que `S·Sᵀ` est identique pour les 37 pièces : **une seule projection, pour tout le monde.**

### Ce qui reste ouvert

- **Qui écrit `combattant + 0x6E18`**, les angles propres au combattant, et avec quoi. `FUN_0001A124` tient
  `ROB_LIGHT__xang`/`__yang` ; que ce soit la même valeur recopiée n'a pas été vérifié -- à l'image 3101,
  `ROB_LIGHT__xang` vaut `9A00` et le combattant `2A2A`, donc **ce n'est pas une simple recopie**.
- Le rôle de `param_1` de `FUN_00012D68`, et celui de `ADV_INFO__STATUS & 0x20` (quel mode prend les angles
  globaux plutôt que ceux du combattant).

## Les trois lectures qui ferment le dossier

### 1. D'où viennent les angles du combattant

`FUN_000717FC`, appelée par combattant (`r14` = le combattant) :

```c
if (STAGE_TEST__STAGE == 3) { ... angle calcule depuis la POSITION du combattant ... }
else if (decor < 3 || decor > 11 || decor < 10) {
    combattant[0x6E18] = STAGE_TEST__XANG;    // le cas ordinaire : les angles DU DECOR,
    combattant[0x6E1A] = STAGE_TEST__YANG;    // recopies tels quels
    return;
}
... decors 10 et 11 : angle calcule depuis la position, lui aussi ...
```

**Les angles d'ombre d'un combattant sont ceux de son décor, recopiés** — sauf sur les décors **3, 10 et 11**,
où ils sont calculés depuis la position du combattant (`r14+0x94` et `r14+0x9C`, son x et son z). Ces trois
décors ont donc une **lumière locale**, et l'ombre de chacun s'écarte selon l'endroit où il se tient.

Mesuré, et la recopie se voit :

| image | décor | `STAGE_TEST` | les deux combattants |
|---|---|---|---|
| 3101, un combat | `09` | XANG `2A2A`, YANG `9393` | `2A2A` / `9393` |
| 7001, une démonstration | `01` | XANG `2800`, YANG `0000` | `2800` / `0000` |

Et **l'élévation change avec le décor** : `2A2A` = 59,29° au décor 9, `2800` = 56,25° au décor 1, soit 30,7°
et 33,8° de la verticale.

### 2. Ce que choisit `ADV_INFO STATUS & 0x20`

Le bit marque une **scène de présentation de l'attraction** : `FUN_00072E8C`, qui pose `ACTIVE_ROB` et
`SHADOW_ROB`, le met. Quand il est mis, `FUN_00012D68` prend les angles **globaux**
`ADV_INFO__LIGHTANG_X/Y` au lieu de ceux du combattant.

Mesuré à l'image 5001, une présentation de personnage :

```
ADV_INFO STATUS 00000020      le bit est mis
ADV_INFO LIGHTANG_X 8800  LIGHTANG_Y 5900
```

Et c'est là que l'écrêtage de `FUN_00012D68` prend son sens : `if (0x8000 < elevation) elevation = 0x4000`.
`0x8800` dépasse, donc l'élévation retenue est **0x4000 = 90°** : dans ces scènes **l'ombre tombe exactement
à la verticale**, sans étirement. Au combat, le bit est clair aux deux images lues.

### 3. Les vingt-sept décors, et leur ombre

`FUN_0001809C` prend la fiche du décor dans une table de pointeurs à `0x0008D118`, indexée par
`STAGE_TEST__STAGE`, et la range dans `STAGE_TEST__ROB_LIGHT`. **Ce pointeur n'est donc pas une lumière :
c'est la fiche du décor** — ce qui explique qu'il vaille `0007555C` au décor 9 et `000751DC` au décor 1. Et
l'octet d'ombre, `+0x6E`, est **un champ de cette fiche**.

Lue en entier, la table donne les vingt-sept décors (0 à 26 ; la 28e entrée vaut `FFFFFFFF`) :

| décors | octet `+0x6E` | couleur | |
|---|---|---|---|
| **0 à 12** | 1 | chacun la sienne | les treize arènes |
| **13 à 17** | **0** | — | **pas d'ombre du tout** |
| 18 à 25 | 1 | `#000000` | ombre noire |
| 26 | 0 | — | le décor que `FUN_0001809C` traite à part |

Ma lecture précédente — « au-delà de 12 la table est vide » — était donc incomplète : chaque décor a son
réglage, et cinq d'entre eux **coupent l'ombre**.

## a / b / c, repris après la lecture du jeu

Le premier classement a été écrit quand on ne savait que ce que la mesure montrait. Depuis, le programme a
été lu, et ce qu'on y a trouvé change ce que chaque voie veut dire. Rien ici n'est une recommandation : le
choix est au propriétaire.

### Ce que la lecture a changé

L'ombre du jeu n'est pas une approximation d'époque. C'est **une projection planaire exacte depuis une lumière
nommée**, dont on connaît la direction, le plan, la couleur, et jusqu'à l'interrupteur par décor. Il n'y a donc
rien à « corriger » dans sa géométrie : elle est juste.

Ce qui lui manque tient en deux choses, et une seule est un défaut.

**Un défaut mesuré : l'ombre est opaque, donc elle efface le sol.** Dans l'ombre de Charlie, 386 pixels sur
576 valent exactement la couleur de la table : la texture du sol **n'y paraît pas du tout**. Une ombre devrait
assombrir le sol, pas le repeindre. C'est le seul endroit où le jeu fait moins bien que ce que le matériel
permettait.

**Une limite, qui n'est pas un défaut : la projection est sur un plan.** L'ombre ne monte pas sur les murs,
ne tombe pas sur l'autre combattant, et le combattant ne s'ombre pas lui-même. C'était le choix de 1998, et
il est cohérent.

### Ce que coûte la surface

Mesuré sur l'image 3101 en comptant les pixels marqués : **1 775 pixels d'ombre, soit 0,93 % de l'image**,
dans une bande de 342 x 35. Tout traitement qui ne touche que l'ombre travaille donc sur moins d'un
centième de l'écran.

### Ce que la carte permet vraiment

Une idée reçue à écarter avant de classer : **le portage tourne déjà en Direct3D 12.** Relevé dans le journal
d'une partie jouée ici :

```
renderer: gpu
SDL_GPU: direct3d12, NVIDIA GeForce RTX 3070 Laptop GPU
```

SDL_GPU est une couche mince sur Direct3D 12, Vulkan et Metal. « Le faire en DirectX 12 » n'est donc pas un
autre chemin : c'est **le chemin actuel**. La question utile est ce que SDL_GPU laisse passer. Lu dans
l'en-tête installé (`SDL3/SDL_gpu.h`) :

| | |
|---|---|
| nuanceurs de calcul | **oui** |
| cibles de profondeur, formats `D16`, `D24`, `D32_FLOAT` | **oui** |
| échantillonneur de comparaison (`enable_compare`, `compare_op`) | **oui** — le filtrage PCF matériel, la primitive des ombres douces |
| lancer de rayons, structures d'accélération, nuanceurs de maillage | **aucun** |

Et le portage **fait déjà** ce qu'une carte d'ombre demanderait : il écrit la profondeur **comme une couleur**
(`o.depth` dans `r3d.hlsl`) et la relit dans une passe suivante (`odepth.Load`) pour les couches translucides.

**Donc : tout ce qui améliore les ombres, sauf le lancer de rayons, se fait dans le chemin existant, sans une
ligne de Direct3D 12 ni de Vulkan.** Seul le lancer de rayons demanderait un troisième chemin de rendu --
Windows seulement en D3D 12, sans les Macs en Vulkan -- avec le contrôle GPU/processeur du démarrage à
repenser.

### a — mieux dessiner l'ombre que le jeu dessine

Elle se sépare en trois, de la plus petite à la plus grande.

| | ce que c'est | ce qu'il faut | ce qui coince |
|---|---|---|---|
| **a1** | **multiplier au lieu de peindre** : même géométrie, même couleur, mais l'ombre assombrit le sol au lieu de le remplacer | un changement de mélange, rien d'autre | **l'union**. Les 37 pièces se chevauchent ; multipliées pièce par pièce, elles assombriraient deux fois aux recouvrements, ce que l'opacité empêche aujourd'hui |
| **a2** | **adoucir les bords** : dessiner l'union dans un tampon, la flouter, la composer | le tampon, un flou sur 0,93 % de l'image | le chemin CPU et le chemin GPU doivent rester d'accord quand l'option est éteinte |
| **a3** | le contact | rien | il est déjà exact : la projection tombe sur le plan |

**a1 est le plus petit changement pour le défaut le plus visible**, et il force la question de l'union, sur
laquelle a2 s'appuie ensuite. Et l'union, le matériel sait déjà la faire : c'est le **stencil `layered`**, que
le portage reproduit (`r3d_raster.cpp`, `r3d_gpu.cpp`) et qui est à **zéro** dans toutes les images
regardées — donc disponible.

Reconnaître les polygones à traiter ne demande plus rien : `mesh.colour_index == 16`.

### b — remplacer l'ombre du jeu par une ombre calculée

Maintenant qu'on sait que celle du jeu est juste, b ne gagne que ce qu'une projection plane ne sait pas
faire : l'ombre sur autre chose que le sol (murs, cage, l'autre combattant), l'auto-ombrage, et une pénombre
qui s'ouvre avec la hauteur.

| | |
|---|---|
| cacher celle du jeu | **gratuit** : ne pas dessiner les maillages d'index 16 |
| la lumière | **on l'a**, et par son nom : angles par décor, ou calculés depuis la position aux décors 3, 10 et 11 |
| l'API | **tout est là** : cible de profondeur, texture de profondeur échantillonnable, échantillonneur de comparaison pour le PCF, nuanceurs de calcul. Rien à écrire hors de SDL_GPU |
| le coût | une passe de profondeur depuis la lumière, ~20 000 sommets par combattant, **sur la carte** — la seule part qui dort (0,75 ms d'attente sur 4,5) |
| ce qu'on y gagne en plus | la carte d'ombre voit les **vrais modèles** des combattants, pas les pièces simplifiées de la ROM : la silhouette est la bonne, et l'ombre peut tomber sur la cage, sur l'autre combattant, et sur le combattant lui-même |
| ce qui coince | la lumière est **ponctuelle et par combattant** aux décors 3, 10 et 11 : deux cartes d'ombre, pas une. Et les décors 13 à 17, où le jeu **coupe** l'ombre : respecter son choix, ou non, est une décision |

### c — éclairer la scène autrement

Cette voie avait été expédiée en deux lignes de jargon. En clair.

Pour chaque pixel, le jeu calcule :

```
couleur = texture x ( soleil·normale x intensite + ambiante )
```

L'**ambiante** vaut **0,6**, mesuré dans toutes les vues. C'est la lumière que reçoit une surface qui ne voit
pas le soleil -- et c'est une **constante** : le dessous d'une plateforme, le fond d'une cage, un recoin de mur
et un sol en plein air reçoivent exactement les mêmes 60 %.

**Ce qui est faisable, et c'est une seule chose : faire varier ce 0,6 selon l'enfermement du pixel.** C'est
l'occlusion ambiante. Ce n'est pas inventer une matière, c'est affiner un terme que le matériel applique déjà,
et son entrée existe (la profondeur est déjà écrite comme une couleur).

**Ce qui n'a aucune base : l'illumination globale et les trajets de lumière.** Pour calculer un rebond, il faut
savoir ce que la surface *fait* de la lumière qu'elle reçoit -- mate, brillante, métallique, et de quelle
couleur de base. Le jeu n'a qu'une texture, et cette texture est l'**apparence finale** voulue par le
dessinateur : en 1998, avec une seule lumière directionnelle, les reflets et les ombrages se peignaient dedans.
Une lumière calculée posée par-dessus éclairerait donc ce qui est déjà éclairé, et laisserait briller des
reflets peints là où rien n'arrive. Dans un texel, on ne peut pas séparer la matière de l'éclairage peint.

Et le prix : l'occlusion ambiante touche **toute l'image**, là où a2 touche **0,93 %**. Deux ordres de
grandeur, pour un effet moins visible que le défaut que a1 corrige.

### Ce qui vaut pour les trois

Le contrat de fidélité ne change pas : **une option, éteinte par défaut**, sur le patron de `hd_3d`, et
`verifier-vf3.cmd` qui passe sans elle. Et le budget ne change pas non plus : **rien côté processeur**,
0,75 ms d'attente côté carte — donc toute voie qui coûte doit coûter **sur la carte**.

## Le décor n'est pas dessiné dans un seul espace, le 2026-10-23

Mesuré en peignant, image par image, le viewport qui dessine chaque pixel puis la position de ce pixel dans
l'espace de sa caméra (`VF3_SHADOW_DEBUG=3` puis `=4`, `=5`).

Au barrage, à l'image 3101, cinq viewports dessinent quelque chose :

| viewport | ce qu'il dessine | boîte de ses sommets | part de l'image |
|---|---|---|---|
| vp5 | combattant de gauche | x −1,77..−0,49 y −1,34..−0,46 z −6,34..−5,66 | 2,2 % |
| vp9 | combattant de droite | x 0,64..1,88 y −1,35..0,37 z −6,26..−5,38 | 3,9 % |
| vp12 | le décor | x −1099..1071 y −32..813 z −1455..−49 | 67,4 % |
| vp13 | le sol proche, les cordes | x −3,20..6,88 y −7,19..2,10 z −11,50..−2,26 | 26,5 % |

Les cinq partagent **le même cadre** (−0,4142 0,4142 −0,3207 0,3207), **la même cota** (423,3625), la même
fenêtre et **la même matrice racine** (indice 1, échelle 1, place 1,98 −1,31 −5,67). Même caméra, donc. Mais
vp12 dessine sa géométrie **vingt fois plus grande et vingt fois plus loin** : une dalle au pied du combattant
est à `[19,6 −27,5 −129,4]` dans vp12, le sol juste en dessous à `[0,98 −1,37 −5,76]` dans vp13.

Ce facteur n'est **écrit nulle part** : l'échelle des nœuds de culling vaut 1,0000 partout, le déterminant des
matrices des modèles vaut ≈ 1. Il est dans les coordonnées des sommets. Le brouillard est réglé pour ces
unités-là (densité 0,0005 : à z −1000 il vaut 0,5, à z −5 il ne vaut rien) — **c'est ainsi que le jeu embrume
le fond et laisse les combattants nets**.

Et c'est possible parce que `r3d_raster.cpp` **vide le tampon de profondeur une fois par priorité** : les
couches ne se trient pas entre elles par la profondeur, mais par l'ordre de tracé. Le barrage met pourtant
vp12 et vp13 dans la **même** priorité 1 — là, la couche lointaine se trie derrière simplement parce qu'elle
est à z −1000.

### Mesurer le rapport, et pourquoi on ne s'en sert pas

`R3dGpu::measure_scales` le mesure sur la seule surface que les couches pourraient partager, le sol : la
direction vient des ombres du jeu (qui sont posées dessus), puis, par viewport, le plan parallèle portant le
plus d'aire. Au barrage : vp13 à −1,508 avec 98 % de l'aire, vp12 à −33,506 avec 64 % → **rapport 22,2**.

Deux pièges mesurés, et c'est le second qui tranche :

1. Une couche peut porter plusieurs surfaces plates (une marche, une bordure, le sol hors du ring). Sur un
   décor de la boucle d'attraction, trois couches qui partagent manifestement un espace (profondeurs −9..−2,
   −28..−5, −25..−13) montrent chacune une surface plate plus basse que le sol, qui lue seule donnerait 1,8 ou
   1,6. La profondeur tranche : une couche rejoint celle des combattants tant qu'elle **commence à moins du
   double** de la profondeur déjà atteinte. (Au barrage, la lointaine commence à 49 quand le reste finit à 11.)
2. **Le plan le plus large d'une couche lointaine n'est pas forcément son sol.** Au barrage, c'est **l'eau
   sous le grillage**, vue à travers les ouvertures. La caler sur la passerelle remonte cette eau au niveau de
   la passerelle, et l'ombre tombe alors **dans le vide entre les barres** : mesuré à l'image 3101, environ la
   moitié des pixels de sol touchés. Rien dans la scène ne dit à quelle profondeur est cette eau.

Donc : le rapport est mesuré, mais **seules les couches à l'échelle 1 — celles des combattants — reçoivent
l'ombre**. `VF3_SHADOW_FAR=1` l'applique quand même, pour comparer. Ce que la mesure rapporte quand même :
elle nomme les couches qui sont ailleurs, et le plan de sol qu'elle trouve sert d'arrêt — **rien sous le sol
ne prend d'ombre**, ce que l'ombre du jeu obtient gratuitement en étant un quadrilatère posé *dans* le sol.

### La densité

Ambiante **0,600** et soleil **1,000** sur tous les viewports (barrage et décor 5). L'ombre multiplie le
soleil : un pixel à l'ombre garde donc les 0,6. Mesuré au décor 5 (sol plein), sur les pixels de sol que
chaque ombre couvre :

| | luminosité | écart |
|---|---|---|
| sol éclairé | 187,3 / 165,3 | — |
| l'ombre du jeu, sur ses pixels | 29,7 | **−84 %** |
| la nôtre, `VF3_SHADOW_DARK=0` | 101,9 | −38 % |
| `=0,35` | 69,7 | −58 % |
| `=0,70` | 37,6 | −77 % |

Il faudrait environ **0,80** pour retrouver la noirceur du jeu — en perdant ce qui fait l'intérêt de la
nôtre, la texture qui se lit dessous. Le réglage vaut **0 par défaut** : la lumière du jeu n'est pas touchée.

### Les décors 13 à 17 : il n'y a rien à voir

`VF3_FORCE_STAGE=<n>` écrit `STAGE_TEST__STAGE` (0x0058FD9F) à chaque image. Essayé : **5, 10 et 12 chargent
de vraies arènes** ; **13, 14 et 20 n'en chargent aucune** — sol noir sous le ciel du décor précédent. Les
arènes jouables sont donc les décors **0 à 12**, et toutes ont l'octet d'ombre à 1. Les cinq fiches qui le
mettent à 0 ne sont pas des arènes : respecter leur choix n'a aucune conséquence visible.

### Deux pièges d'outillage, payés cher

- **`r3d.hlsl` et `r3d_gpu.cpp` bougent ensemble.** Un nuanceur qui déclare deux échantillonneurs contre un
  côté C++ : Direct3D 12 refuse le pipeline (`0x80070057`), l'hôte retombe silencieusement sur le rendu
  processeur, et on mesure une image qui n'est pas celle qu'on croit.
- **`2>/dev/null` fait taire le journal.** Sous Windows `isatty()` est vrai pour `NUL`, donc l'hôte ne fait
  pas son `freopen` vers `log.txt` et tout se perd. Rediriger l'erreur vers un **vrai fichier** ; le journal
  atterrit alors dans `$VF3_DATA/log.txt`.
