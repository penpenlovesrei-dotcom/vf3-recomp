# Reprise — Fighting Vipers 2, menu console

État au **2026-10-20**, branche `fv2`, 36 commits devant `mine/fv2`, rien de poussé depuis le tag
`fv2-v0.2.0-alpha`. Arbre propre.

Le journal du jour est en bas. Ce qui suit est l'état, pas le récit.


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
| les modes console | `jouer-fv2-console.cmd` | `docs/fv2-console.md` |
| construire | `construire.cmd` | |
| vérifier | `verifier-vf3.cmd` | |

Et le jeu tourne en **free play** : un seul octet de la NVRAM, `backup[0x17] = 27`.

**Les modes console sont aujourd'hui une page du menu de la touche Échap.** C'est tout ce qu'ils sont, et il
faut le dire ainsi : ce n'est pas le menu d'une console, c'est une page de réglages de ce portage.


## Ce qu'on vient de poser, et qui ne sert encore à rien

`docs/fv2-etats.md` — la machine à états du jeu, portée du travail fait sur la ROM en août.

```
MODE_MAIN 0x0058FD98   MODE_SUB 0x0058FD99   MODE_REQUEST 0x0058FD9A   SUBMODE_REQUEST 0x0058FD9B
```

Dans `src/machine/model3.h` : `fv2_mode()`, `fv2_submode()`, `fv2_on_title()`, `fv2_request_title()`.
Dans `tools/vf3run.cpp` : `--modes`, qui imprime le mode et son sous-état sous les noms de Sega à chaque
changement. **Personne ne les appelle.**


## Ce que Frédéric a demandé, et qui est la prochaine marche

Une seule touche, deux comportements selon où le jeu en est :

- **ailleurs qu'à l'écran titre** → Start ramène à l'écran titre ;
- **à l'écran titre** → Start ouvre le sous-menu, la liste des sept modes ;
- dans les deux cas Start est **retenu**, le jeu ne le voit pas.

Le menu Échap peut rester où il est.

### Le chemin est mesuré, et il est en deux morceaux

**Depuis l'attraction** — deux images :

```
MODE_REQUEST = 3 (ADV_DSP)   SUBMODE_REQUEST = 4 (FV2_TITLE_INT)      écrit UNE FOIS, jamais tenu
```

**Depuis un combat** — ce levier ne commande rien, et l'autre non plus. Il faut sortir de la partie par la
route du jeu : finir le match sans le montrer ni l'entendre (`0x00581455 = MATCH COUNT−1`,
`0x00581406 = 64` une fois par manche, vie du joueur 1 à zéro), puis poser l'écran titre une fois
l'attraction revenue. Tout est dans `docs/fv2-etats.md`.

### La surface

C'est la seule pièce sans antériorité à reprendre. `assets/menu-console/` porte les 45 images du propriétaire
(hors dépôt, `.gitignore`) ; `title_list.png` est la liste des sept modes, 709 x 897, et sa géométrie est
relevée — bandes, encre, placement sur l'écran titre — dans `docs/fv2-etats.md`.

**Une seule version de chaque ligne existe.** Aucun des 45 fichiers n'est une variante « sélectionnée », donc
la ligne choisie devra être marquée autrement.

Et ce qu'il ne faut pas refaire : la liste doit se poser **sur l'écran titre du jeu**, pas sur une illustration
qui le recouvre, et surtout pas être une page de plus du menu RmlUi — c'est exactement la faute commise deux
fois aujourd'hui.


## Ce qui reste ouvert, et qui bloque quoi

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
