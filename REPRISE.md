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
