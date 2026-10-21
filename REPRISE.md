# Reprise — Fighting Vipers 2, menu console

État au **2026-10-21**, branche `fv2`, rien de poussé depuis le tag `fv2-v0.2.0-alpha`.

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
| les modes console, et la liste sur l'écran titre | `jouer-fv2-console.cmd` | `docs/fv2-console.md` |
| mesurer ce que fait Start | `mesurer-fv2-start.cmd` | `docs/fv2-etats.md` |
| les captures de la liste | `capturer-fv2-liste.cmd` | `docs/fv2-console.md` |
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
                        partout ailleurs          ->  le jeu est prié de revenir à son écran titre
```

Dans les deux cas l'appui est **retenu** : le jeu ne le voit pas. Et dans les deux cas **la musique s'arrête** —
le mode est demandé dans le dos du séquenceur d'attraction, et c'est lui qui envoie le bloc de sons à chaque
changement d'écran, donc sans ça la musique de la démonstration continue par-dessus l'écran titre.

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
- **Les bruitages sont ceux du jeu**, joués par sa propre carte son : `A0 2E 23` à l'ouverture et au choix,
  `A0 2E 26` au déplacement, `A0 2E 27` à l'annulation. Ce qu'il a fallu ajouter, c'est de **faire tourner la
  carte son pendant que le jeu est gelé** (`Game::sound_only_frame`), **à la cadence du jeu** : la première
  version demandait une fois par passe si une image de son était due, et comme une image fait 17,38 ms quand
  une passe en fait 16,67, ça n'en produisait qu'une passe sur deux — trente à la seconde au lieu de 57,5,
  la file se vidait, et ça s'entend comme un son qui ralentit et grésille. C'est un accumulateur maintenant.
- **La musique s'arrête** sous la liste : `FF`, le reset MIDI, envoyé 80 images après l'ouverture parce que
  `FF` relance un son encore en cours avant de le couper. Elle est remise **seulement si le joueur recule**,
  20 images après la fermeture : sur un mode choisi, le jeu change d'écran et demande la sienne.
- ARCADE et SURVIVAL agissent. Les cinq autres lignes sont dessinées et ne font rien : RANDOM et V.S.
  demandent le chrono de l'écran de sélection, jamais mesuré ici ; NETWORK, TRAINING et OPTIONS n'ont aucun
  levier dans ce portage.

### Ce que la mesure a dit, et qui n'était pas prévu

`mesurer-fv2-start.cmd`, cinq essais d'un seul appui chacun. La demande de mode commande l'écran **depuis
partout** — le logo SEGA, la démonstration, l'écran de sélection, et même en plein combat : l'écran titre
revient une image après.

**Mais elle ne termine pas la partie.** La tâche `entry1` (`0x00700000`) reste à sa valeur de jeu : `0x2A`
depuis un combat, `0x2D` depuis la sélection. L'écran titre revient par-dessus une partie qui court toujours,
donc `fv2_in_attract()` reste faux. Le tableau complet est dans `docs/fv2-etats.md`.

C'était écrit « ces leviers ne commandent rien depuis un combat » : c'était vrai des deux écritures du
séquenceur d'attraction, et faux de la demande de mode, qui n'avait jamais été essayée là.

### Les captures, et de quoi elles sont la preuve

`capturer-fv2-liste.cmd`, trois images dans `build\` :

| image | ce qu'elle montre |
|---|---|
| `liste-1-arcade.png` | les sept lignes sur le vrai écran titre, FREE PLAY lisible dessous |
| `liste-2-survival.png` | trois fois bas puis Entrée : la partie démarre, et l'écran de sélection dit SURVIVAL MODE |
| `liste-3-retour-au-titre.png` | Start pendant la démonstration : l'écran titre est revenu |

La deuxième est celle qui compte : elle va d'un bout à l'autre, sans main sur la manette.

### Deux crochets d'essai, sans lesquels rien de tout ça ne se vérifiait

La liste gèle le jeu, donc tout ce qui compte les images du jeu n'arrive plus — c'est le blocage de neuf
minutes de la veille. D'où, dans l'hôte :

```
VF3_QUIT_AFTER_S=N     finir après N secondes d'horloge
VF3_KEYS_S=k@sec,...   envoyer la touche k à telle seconde
```


## Ce qui reste ouvert, et qui bloque quoi

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
