# Reprise — Fighting Vipers 2, menu console

État au **2026-10-24**, branche `fv2`, poussée sur `mine` jusqu'à `7f732ee` ; **`origin` (l'amont
`mr-finster`) n'a pas de branche `fv2`** — rien de tout ça n'y est allé.

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
| la couleur des parties sans texture | | `docs/fv2-couleurs.md` |
| les deux Kuhn, et ce qui les separe | | `docs/fv2-deux-kuhn.md` |
| le nom sur l ecran de selection | | `docs/fv2-noms-selection.md` |
| les modes console, et la liste sur l'écran titre | `jouer-fv2-console.cmd` | `docs/fv2-console.md` |
| mesurer ce que fait Start | `mesurer-fv2-start.cmd` | `docs/fv2-etats.md` |
| les captures de la liste | `capturer-fv2-liste.cmd` | `docs/fv2-console.md` |
| construire la fenêtre aussi | `construire-hote.cmd` | |
| construire | `construire.cmd` | |
| vérifier | `verifier-vf3.cmd` | |
| où va le temps de la carte son | `VF3_SND_PROF=1` avec `VF3_PROF=1` | `docs/scsp_audit.md` |
| le son n'a pas bougé d'un octet | `tools\son\comparer-son.cmd` | `docs/scsp_validation.md` |
| les deux corps du DSP, testés | `make test` | `docs/scsp_validation.md` |

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

## Journal du 2026-10-23 — la page OPTIONS finie, et la fin du jeu

Tout est dans `docs/fv2-console-options.md` et `docs/fv2-etats.md` ; ceci est le fil, pas le détail.

### `ROUND TIME` ne faisait rien, et pourquoi

Le match **ne lit pas le réglage**. Il lit un mot déjà converti — `0x00581418` pour un joueur, `0x0058141C`
pour deux — et cette conversion n'a lieu **qu'une fois, à l'amorçage**, valeur × 64. Écrire le réglage après
coup ne pouvait donc pas se voir. `fv2_set_option` écrit maintenant les trois : la sauvegarde, la copie de
travail, et le cache quand il y en a un. Deux boots comparés sur tout `0x00580000`-`0x00582000` disent
lesquels sont cachés : les deux `ROUND TIME` et `ADVERTISE SOUND`, rien d'autre.

Le round de vérification a été joué de la touche au chrono, en partie cachée : deux rounds partis de 1280
images, les vingt secondes posées par la page.

### `VS DIAGRAM` sur de vrais chiffres

Le tableau lisait déjà la sauvegarde, mais sur rien : sans match mené à terme, tout est à zéro, et un tableau
de zéros ne dit pas si on lit au bon endroit. Quatre matchs joués en caché, chacun contre la sauvegarde du
précédent, l'ont rempli — et ont corrigé la description :

- **`0x020` est `DRAW`**, que la note d'août donnait pour nulle part ;
- le codage est **contigu** (deux octets gros-boutiens en `+0`/`+1` d'une case de huit), pas espacé comme
  celui des réglages ;
- `SKO 1` et `SKO 2` tombent presque sûrement dans les deux trous à `0x008` et `0x010`, et **restent vides**
  faute d'un Super K.O. joué. Une case vide dit ce qu'on sait ; un zéro prétendrait savoir.

Un match ARCADE n'écrit rien de tout ça : c'est un relevé de versus.

### `DISPLAY` : les deux lignes enjambées

`HD ART` est un vrai interrupteur. Aucun nouveau levier n'a été nécessaire : `load()` vide ce qu'il tient
avant de lire, donc charger depuis nulle part repose un pack, et le rendu GPU vérifiait déjà le nombre de
pages. `NONE` quand il n'y a rien à charger, et la ligne est alors enjambée.

`ENDING` fait deux choses. `ROLL NOW` donne le générique par-dessus l'écran titre ; `AFTER A MATCH` arme la
vraie fin. Et elle a demandé une vraie recherche.

### Ce que la fin du jeu est vraiment

**Les noms que Sega a laissés en clair l'ont dit.** `0x00024494` n'est pas une suite de chaînes mais une table
de quatre-vingts pointeurs, avec les gestionnaires à `0x00024354` au même indice. Les sept sous-états de la
fin : `ending_trans_init`, `ending_trans_wait`, `ending_replay_init`, `ending_replay_ctrl`,
`ending_replay_next`, `ending_final_mask1`, `ending_final_mask2`. **Trois sur sept sont des replays.** Le
générique alterne les crédits et les rediffusions des matchs gagnés ; les bandeaux « lieu » et « v.s. NOM »
sont ceux du replay, pas des restes.

Le levier : `fv2_game_NEXT_PROG_SEL` branche sur la fin quand `ADV_INFO_STATUS` porte le bit 3, « l'échelle
est finie » — bit que les notes d'été donnaient comme lu dans le code et jamais vu posé. Mais c'est le point
de branche de **chaque round**, donc le poser trop tôt le fait consommer par un round qui n'a rien à
rediffuser. La règle tient en une ligne : **le poser à la première image où le mot porte déjà `0x40000000`**,
le drapeau par lequel le jeu dit que le *match* est fini.

Joué de bout en bout : liste, `OPTIONS`, `DISPLAY`, `ENDING` sur `AFTER A MATCH`, `ARCADE`, un match — et la
fin sort par la branche du jeu, avec Cross River Damsite, la cage et les deux combattants.

### Deux erreurs de méthode, la même deux fois

La fin **alterne** toutes les 225 images : des crédits, puis un replay. Je m'en suis fait prendre deux fois.

1. En photographiant **toujours l'image 1900** pendant que je déplaçais le moment de la demande, j'ai cru
   trouver une frontière nette à l'image 1212 et j'ai posé une attente de quarante images dessus. Il n'y avait
   pas de frontière : je changeais seulement la phase dans laquelle tombait l'image 1900. L'attente a été
   retirée.
2. En photographiant le **second** passage de replay, j'ai cru que l'hôte ne chargeait pas les décors alors
   que `vf3run` les chargeait, et j'allais isoler le rendu processeur contre le rendu GPU. Il n'y avait aucun
   écart : l'image de `vf3run` était le premier passage, les miennes le second — et le second est un
   adversaire jamais affronté, donc un replay qui n'existe pas.

**Sur un phénomène qui alterne, comparer à décalage égal, jamais au même numéro d'image absolu.** Et le
balayage à l'image près (`VF3_KEYS=touche@image`, qui écrit la fenêtre trois images plus tard) est ce qui a
fini par trancher, parce qu'il ne dépend pas de la vitesse de la machine.

### Au passage

`F12` traverse maintenant les surfaces de la console, et les surfaces sont dessinées en un seul endroit : une
photo de la page est une photo de la page. Avant, elle recomposait la fenêtre sans elles et rendait l'écran
titre.

### Ce qui reste

- `SKO 1` et `SKO 2` : **faits**, par un Super K.O. joue a la main. `0x010` passe a 1 dans le bloc
  du vainqueur sans que KO, JUDGE ni DRAW ne bougent. Ni le script, ni trente mille images de jeu
  aleatoire, ni un combat entier contre le processeur n'avaient su le produire.
- `CONTROLLER`, vide, et `BGM SET`, les packs de musique du fork.
- Les combattants noirs du 2026-10-23 : la soumission tronquée après un redémarrage de l'attraction, cause
  toujours ouverte.

## Journal du 2026-10-24, la nuit — CONTROLLER, le menu de pause, et trois bugs à moi

### Ce qui s'ajoute au jeu

- **`CONTROLLER` n'est plus vide.** Un éditeur d'affectations : neuf lignes — les quatre directions, poing,
  garde, pied, Start, `SELECT` — sur deux colonnes, `1P` et `2P`. `A` ouvre une ligne et la touche ou le
  bouton suivant devient l'affectation, écrite dans `settings.ini`. Une dixième ligne, `TEST`, ouvre le
  **miroir** : les huit directions et les trois boutons allumés tant qu'on les tient, une diagonale s'allumant
  seule. Les icônes sont celles du jeu, `inputs.png`, qui ne servait nulle part.
- **Le menu de pause**, `src/host/fv2_pause.h`. Start pendant un match, dans la forme du disque : panneau
  violet sous sa barre `1P PAUSE`, à gauche pour le joueur 1 et à droite pour le 2, écran assombri. Une des
  quatre lignes a un levier qui marche d'ici ; les trois autres sont en sourdine et disent ce qu'elles
  attendent. Voir `docs/fv2-console.md`.
- **Start en combat ne déchire plus l'écran.** L'avertissement était écrit depuis le 21 sans que personne
  ait regardé : la demande d'écran titre déplaçait l'écran sans arrêter la partie, et le titre revenait
  haché. Hors attraction, cette couche ne fait plus rien et laisse l'appui au jeu.

### Trois bugs, tous de moi

- **`ADVERTISE SOUND` écrivait dans le nombre de rounds d'un match.** `0x0058142C` n'est pas son cache mais le
  **match point** ; mon relevé du matin avait pris une coïncidence pour une mesure (1 + 1 = 2 = `MATCH COUNT
  1P`). Toucher le réglage raccourcissait tous les matchs suivants. Retiré, et rien ne le remplace : le jeu
  recuit ce mot à chaque début de match.
- **Le miroir de `CONTROLLER` avalait ce qu'il devait refléter** — gauche et droite changeaient de joueur, le
  poing validait, le pied ressortait. Corrigé en supprimant ce qu'il fallait affecter, pas en le réaffectant.
- **Une capture d'affectation enfermait la manette** : seul Retour arrière annulait, donc à la manette tout
  bouton liait. Trouvé parce que le `p1_pad_kick` de Frédéric est revenu sur `start`.

### Ce qui a été lu dans Ghidra

- **Les quatre-vingts noms d'états de Sega**, avec leurs gestionnaires, dans `docs/fv2-etats.md` — c'est
  Frédéric qui a dit d'aller les lire. Les sept de la fin disent ce qu'elle est : **des rediffusions**.
- **La table des statistiques vivante** à `0x0011F7E0`, pas de `0xD4`, trouvée par l'écran `VS DIAGRAM` de la
  borne ; et `FUN_000701C8`, la routine qui range un match, avec le genre de fin à `0x00581452` et le
  vainqueur à `0x00581453`. Dans `docs/fv2-flux-de-match.md` §8.
- **`SKO 1` contre `SKO 2`** : la lecture est consignée **et marquée comme non convaincante**, Frédéric
  n'étant pas d'accord et son objection tenant. Un Super K.O. joué réglage éteint trancherait.

### Outillage

`VF3_OPTION_LOG`, `VF3_TIMER_LOG`, `VF3_LIVE`, une dizaine de routes dans `tools/scripts/`, et un **verrou sur
`verifier-vf3.cmd`** : deux tours lancés ensemble partageaient leurs fichiers et ont fabriqué un faux
`DIFFERENT` sur le contrat de fidélité.

### Ce qui reste ouvert

- **Sortir d'un combat proprement**, par la route du jeu. C'est ce qui bloque trois des quatre lignes du menu
  de pause, et c'est le vrai travail suivant.
- `CHANGE STAGE` est câblée sur le sélecteur que TRAINING utilise ; elle n'a pas été vue changer un décor en
  plein round, et c'est probablement l'autre levier qu'il faut.
- `BGM SET`, les packs de musique du fork.
- Les combattants noirs du 2026-10-23.

## Journal du 2026-10-24, matin — l'amont fusionné

`mr-finster/vf3-recomp` avait six commits et le tag `v0.1.6-alpha` d'avance. Tout est entré dans `fv2`.

### Ce que l'amont apporte

- **Vulkan d'abord sous Windows**, pour toutes les cartes, le Direct3D 12 de NVIDIA étant lui aussi bien plus
  lent ; et un réglage `Display > Graphics driver` pour revenir à l'autre.
- **Les sauvegardes écrites hors du fil du jeu** (`bg_saver`) : chaque vidage sur un disque dur coûtait des
  dizaines de millisecondes, donc un hoquet toutes les quelques secondes pendant que la comptabilité de la
  borne remuait la RAM de sauvegarde. La NVRAM part maintenant une fois par minute, et tout au moment de quitter.
- **Un septième nuanceur, `FS_COLOUR`** : l'image entière passée par les couleurs d'un moniteur d'arcade.
- Trois corrections du menu (un clic qui ferme le menu ne plante plus, `<` descend d'un cran, les bornes
  bouclent) et `docs/HANDOFF.md`.

### Les trois fichiers qui ont demandé une main

- **`.gitignore`** garde les deux listes : la nôtre, tout ce que ce portage laisse à côté de lui quand on le
  lance d'ici, et la leur, le cache de Wrangler et les portages voisins.
- **`README.md`** garde le nôtre : la section que l'amont ajoute pointe vers ce fork, elle n'a rien à dire ici.
- **`src/gpu/r3d_shaders.h`** est un fichier **généré**, et DXC n'est pas sur cette machine. Les deux côtés ne
  se marchaient pas dessus pour autant : le nôtre avait recompilé les six nuanceurs pour la lumière non bornée
  (`FS_POLY`), l'amont en ajoute un septième dont notre modification ne touche pas le code. Donc notre
  en-tête, les trois tableaux `fs_colour` de l'amont greffés devant la ligne d'empreinte, et le sha1 recalculé
  sur le `r3d.hlsl` fusionné. **À régénérer d'un coup** par `tools/gpu/shaders.sh` sur une machine qui a DXC et
  SPIRV-Cross, pour que l'en-tête redevienne la sortie franche du générateur.

### Vérifié

`verifier-vf3.cmd 2000` contre le nouveau `main` (`d30b722`), l'arbre de référence `C:\Temp\vf3up` avancé et
ses binaires refaits : **quatre IDENTIQUE** — l'état du PowerPC image par image, les 8 Mo de RAM, les images
rendues, l'image de l'hôte complet — et **2231 images identiques** entre l'interpréteur et le code recompilé.
Virtua Fighter 3 n'a pas bougé. Et `vf3run` dit enfin ce que Start en plein combat veut dire
(`start_for::pause`), que son `switch` laissait de côté depuis la construction du menu de pause.

## Les branches, posees le 2026-10-24

Frederic a essaye la version nue et elle tourne a pleine vitesse, la branche de travail non. Donc une
**copie de reference** existe desormais, et elle ne bouge plus :

| branche | ce qu'elle est | ce qu'on y fait |
|---|---|---|
| `main` | l'amont, `mr-finster/vf3-recomp`, en miroir | on l'avance par fusion depuis l'amont. **Jamais un commit de nous** |
| `fv2-stable` | la version nue et rapide : `889a691`, « Fighting Vipers 2 tourne », etiquetee `fv2-stable-1` | **rien**. On n'y pousse pas. Elle n'avance que le jour ou une version de travail est a la fois complete et rapide, et alors on l'etiquette `fv2-stable-2` |
| `fv2` | le travail en cours, tout ce qu'on ajoute | la branche de tous les jours |
| `fv2-<sujet>` | un arbre de travail par sujet (`git worktree list`) | une recherche a la fois, fusionnee dans `fv2` quand elle aboutit |

**Un pacte entre branches, posé le 2026-10-24.** `src/gpu/r3d_shaders.h` est un fichier **généré**, et aucune
machine ici n'a de DXC capable de SPIR-V. L'en-tête que portent `fv2` et `decor-test-dreamcast` est donc une
**greffe** — nos six nuanceurs recompilés avec l'écrêtage de lumière `665b40c`, plus les trois tableaux
`fs_colour` de l'amont — et non la sortie franche du générateur. Il se vérifie sans rien croire : le
générateur inscrit dans l'en-tête le sha1 du `r3d.hlsl` qui l'a produit, donc

```
python -c "import hashlib;print(hashlib.sha1(open('src/gpu/r3d.hlsl','rb').read()).hexdigest())"
grep -o 'source_sha1\[\] = "[0-9a-f]*"' src/gpu/r3d_shaders.h
```

doivent concorder, et `grep -c 'alignas(4) static const uint8_t r3d_'` doit rendre **21**. Conséquence :
**le jour où une branche modifie `r3d.hlsl`, les autres n'ont plus de blobs valides.** Celle qui y touche
prévient les autres. (C'est ainsi que la branche du décor Dreamcast a récupéré l'écrêtage de lumière qu'une
fusion d'amont lui avait pris : elle a repris notre en-tête, `2e8a285`.)

La copie de reference est a **`C:\Users\frede\fv2-stable`**, plus dans `C:\Temp` : un dossier temporaire
n'est pas un endroit pour ce qu'on ne doit pas perdre. Elle a ses propres `settings.ini`, `backup-fvipers2`
et `eeprom-fvipers2` (free play), et un raccourci sur le bureau, **Fighting Vipers 2 - version stable**,
qui pointe sur son `jouer-fv2.cmd`.

### Le rythme, mesure a decalage egal le 2026-10-24

Meme route, aucune entree, `--rec`, `VF3_PROF`, memes numeros d'image. La demonstration de l'attraction est
un vrai combat joue par la machine, donc elle mesure ce qu'un combat coute :

| fenetre d'images | `fv2-stable` (889a691) | `fv2` au 2026-10-24 |
|---|---|---|
| 1500 | 2,64 ms | 4,27 ms |
| 2000 | 3,12 | 10,03 |
| 2500 | 2,66 | 5,96 |
| 3000 | 2,86 | 6,36 |
| 3500 | 2,55 | 4,19 |
| 4000 | 2,94 | 9,25 |
| **moyenne** | **2,78 ms** | **6,68 ms** |

Et ce n'est **pas** la fusion de l'amont : les deux `vf3run` d'avant et d'apres la fusion donnent 16,74 et
16,24 ms sur l'attraction, donc a egalite. C'est une de nos additions entre `889a691` et aujourd'hui, et
elle n'est pas encore nommee. Une bissection a ete tentee et **son verdict est faux** : elle a juge
« rapide » le commit de fusion lui-meme, qui mesure 6,68 ms, donc son harnais mesurait un binaire perime.
A refaire en s'assurant que chaque etape reconstruit vraiment et que le juge imprime son chiffre.

Dans la fenetre, un combat coute encore plus que ces 6,68 ms — 15,5 ms avec le son, 16,37 sans — donc il
restera une seconde part a chercher dans l'hote une fois la premiere nommee. Le grésillement du son en
decoule : la file de SDL tombe a 1300 octets, soit 7 ms d'avance.

## Le jeu lent : un `getenv` par lecture de CROM

Trouve le 2026-10-24, apres que Frederic a signale des combats sous 50 images/s et un son qui gresille.

### La cause

`Model3::read32`, le chemin de lecture memoire, portait ma sonde `VF3_CROM_WATCH` **sans mise en cache** :

```cpp
if (const char *w = getenv("VF3_CROM_WATCH"))      // a CHAQUE lecture de mot dans la CROM
    if (a == uint32_t(strtoul(w, nullptr, 16)))
```

Des millions d'appels par image. Le reste du portage met partout `static const bool ... = getenv(...)` --
`VF3_DMA_LOG` le fait dans le meme fichier -- et je ne l'ai pas suivi a l'endroit le plus chaud. L'adresse
est maintenant lue une fois dans un `static`. Au passage, `VF3_BANK_LOG` dans `set_crom_bank` payait aussi
son `getenv` avant de tester si le banc avait change.

### La mesure, a decalage egal

Meme route, aucune entree, `VF3_PROF`, memes numeros d'image. Moyenne des six fenetres de la demonstration :

| | une image |
|---|---|
| `fv2-stable` (889a691) | **2,78 ms** |
| `fv2` avant la correction | **6,68 ms** |
| `fv2` apres | **2,81 ms** |

La branche de travail est revenue sur la version nue a 0,03 ms pres. **Les 3,9 ms etaient ce seul `getenv`.**

### Ce que cela corrige dans la lecture qu'on en faisait

- La version nue n'etait pas plus rapide **parce qu'elle a moins de choses**. Les portraits, le HD, le costume
  de Honey et le menu console ne coutent rien par image. Elle etait plus rapide parce qu'elle n'avait pas la
  sonde. Remonter les additions une par une depuis `fv2-stable` aurait coute des semaines pour aboutir a une
  ligne.
- Le gresillement du son en decoulait : la file de SDL tombait a 1300 octets, soit 7 ms d'avance, parce que le
  jeu ne tenait pas la cadence. Le banc de son ne coute que 1,6 ms par image, et le SCSP de l'amont n'y est
  pour rien -- ses dumps WAV ne montrent ni ecretage ni saut d'echantillon.
- La fusion de l'amont n'y etait pour rien non plus : 16,74 contre 16,24 ms sur l'attraction, a egalite.

### La lecon pour la suite

**Une sonde d'environnement dans un chemin chaud se lit une fois.** Et : chercher la cause dans le code
plutot que dans l'histoire. La bissection lancee ce jour-la a rendu un verdict faux -- elle a juge
« rapide » le commit de fusion, qui mesurait 6,68 ms -- la ou un diff des fichiers du chemin chaud entre
`fv2-stable` et `fv2`, puis `git diff -U0 | grep '^@@'` pour voir **dans quelles fonctions** les lignes
ajoutees tombaient, a donne `read32` en deux commandes.


## Journal du 2026-10-24, l'après-midi — le son : où va son temps, et un quart de moins

Deux commits, `1757e83` et `7f732ee`. Six documents dans `docs/scsp_*.md` ; ce qui suit est ce qu'il faut
savoir sans les ouvrir.

### Ce que la mesure a dit, et que personne n'avait demandé

Le profileur de `vf3run` donnait déjà le coût de la carte son **en bloc**. Il manquait la répartition dedans,
et c'était tout le sujet : `src/sound/sound_prof.h` (`VF3_SND_PROF=1`) la donne, en retirant le temps imbriqué,
ce qui n'est pas trivial — **les échantillons ne sont pas fabriqués dans une phase à eux**, le 68000 les
déclenche depuis l'intérieur de ses propres tranches chaque fois qu'il touche un registre de chip.

Sur `fv2-fight.txt`, 4200 images, `fvipers2` :

| | part propre de la carte son |
|---|---|
| **le DSP d'effets** | **62-69 %** |
| le 68000 | 22-24 % |
| les voix | 6-13 % |
| la boucle et les timers | ~1,2 % |
| le mélange | ~1 % |

**Les deux tiers sont le DSP d'effets.** Les voix, qu'on soupçonnerait d'abord, font un dixième. Et le
programme DSP lui-même, lu par la sonde `dspprog` : sur ses 112 pas, `FRCL`, `ADRL`, `YRL` et `NEGB` sont
posés **zéro fois**, et les 33 accès mémoire tombent **tous** sur un pas impair — donc porter le test
`step & 1` de MAME sur le bloc entier, qui ressemble au gain évident, n'aurait rien donné. Mesuré avant d'y
toucher, et c'est exactement pour ça qu'on mesure.

### Trois lots, et un défait

| | ms par image | cumul |
|---|---|---|
| base `335c050` | 1,85 | — |
| le corps du DSP spécialisé sur les contrôles utilisés (`Run<U>`) | 1,64 | −11,4 % |
| copies locales + `__restrict`, contre l'aliasing RAM son / objet DSP | 1,59 | −14,1 % |
| les huit contrôles d'un bit groupés dans un mot (`Inst::f`) | **1,40** | **−24,3 %** |

Le pas de DSP passe de 16,3 à 10,0 cycles, la part de l'image de 10,6 % à 8,0 %.

**Le quatrième lot a été défait.** La boucle des deux adresses de `UpdateSlot` déroulée : identique au bit,
code plus clair, **1,399 ms contre 1,400** — zéro. Une modification dans un fichier de fidélité qui n'achète
rien ne se garde pas. Le résultat négatif, lui, est consigné : les voix ne se gagnent pas là.

**Et aucun gain d'image n'est revendiqué.** Dans l'hôte la carte son est sur un fil à elle, une image en
retard : ces 0,45 ms sont du temps processeur en moins — chaleur, batterie, cœurs laissés au rendu — pas une
image qui arrive plus tôt. C'est dans `vf3run`, tout sur un fil, que le gain se lit.

Les 1,6 ms de l'entrée du `getenv` plus haut ne contredisent pas les 1,85 d'ici : ce n'était pas la même
scène. Celle-ci contient un combat, et la part des voix y monte.

### Le son n'a pas bougé d'un échantillon

C'est la condition pour garder un lot, pas un bonus. `tools\son\comparer-son.cmd` construit un arbre de
travail d'un commit de référence dans `C:\Temp\fv2-son-ref`, fait jouer la même scène aux deux `vf3run` et
compare les WAV **octet par octet** :

| jeu | scène | images | verdict |
|---|---|---|---|
| `fvipers2` | `fv2-fight.txt` | 4200 (12 879 504 octets) | **identique à l'octet** |
| `vf3` | `vf3-match.txt` | 2400 (6 841 492 octets) | **identique à l'octet** |

### Le trou que le premier lot a creusé, et qui est bouché

Spécialiser le corps du DSP l'a coupé en deux chemins. Les deux jeux prennent le maigre, vérifié à l'octet ;
**l'autre n'était vérifié par rien**. `make test` (`tools/son/test_scspdsp.cpp`) l'exerce : une référence qui
décode le MPRO **dans** la boucle, comme MAME et Supermodel le font encore, et seize cas qui comparent
`EFREG`, `TEMP`, `MEMS`, `DEC` et les 524 288 mots de la ligne de retard après **chacun** de 64 échantillons.
Onze cas atteignent `Run<USES_ALL>`, dont celui qui compte : le programme même des jeux avec `uses` forcé,
donc **les deux corps doivent s'accorder dessus**.

**Et le test a montré qu'il sait échouer** — quatre fautes injectées puis défaites : `NEGB` qui ne nie plus
(5 cas tombent), la boucle qui lit `TWT` là où elle veut `XSEL` (12), `SHIFT` lu un bit trop loin (13),
`Start` qui ne remet pas `uses` à zéro (1, le cas écrit pour ça). Une cinquième, l'échange de deux valeurs
d'énumération, n'a rien fait tomber — et à raison : c'est un renommage, pas une faute. Noté pour que personne
ne le lise comme un aveuglement.

### Ce qui a été lu à la source, et l'avertissement qui en sort

- **MAME `master` et Supermodel décodent tous deux le MPRO dans la boucle**, à chaque pas de chaque
  échantillon. La forme décodée de ce dépôt était déjà en avance sur les deux **avant** ces lots.
- **Le SCSP de MAME et celui de Supermodel viennent du même ElSemi.** Ce ne sont pas deux témoins, c'est un
  témoin lu deux fois : leur accord n'établit rien sur un YMF292-F réel. Toute question de fidélité tranchée
  par « MAME et Supermodel font pareil » est une question **non tranchée**.
- **MAME a cessé de mirrorer MADRS** (`#16527`, fusionnée le 2026-10-19) et rejoint le mode « séparé » d'ici
  côté écriture, par sa propre route et sur `doa`. Mais ses **lectures** de 0x7C0-0x7FF renvoient MADRS là où
  le mode séparé renvoie ce qui a été écrit, **et personne n'a regardé si l'un des deux jeux relit cette
  plage.** C'est décidable ici, et ça ne l'est pas encore.
- Côté licences, rien ne manquait : MAME est BSD-3-Clause, compatible avec le MIT du dépôt, et chaque fichier
  porte son avis. Supermodel, Mednafen, Yabause et Kronos sont GPL — **observation seulement**, jamais de
  copie ni de traduction.

### Ce qui reste ouvert

- **La part du son sous `--rec`** : tout a été mesuré sur l'interpréteur. Avec le recompilateur la carte
  descend, donc la part du son monte, et ces pourcentages n'y transposent pas. C'est la mesure qui dit si la
  suite vaut quelque chose.
- **Le temps de VF3** : jamais relevé proprement (le harnais en a craché au passage, sur des fenêtres où la
  machine bougeait — non retenu).
- **Les scènes qui manquent** : silence, musique seule, effets seuls, sound test. Les deux du milieu sont un
  mot (`VF3_VOLUMES`), avec la réserve qu'ils allument le niveleur d'effets.
- **Un test par bloc sur huit.** Les sept autres sont listés dans `docs/scsp_validation.md` §3b, et ce sont
  eux, avec le rejoueur à deux cœurs, le préalable à tout remplacement du SCSP de MAME. Sans eux, écrire un
  cœur autonome est un pari — et rien ne presse : MAME est BSD, et **son code est aujourd'hui la référence de
  fidélité de ce portage**.
- **Le miroir de MADRS gardé pour VF3**, qui diverge maintenant de MAME : indécidable ici, il n'y a pas de
  Model 3 sur ce bureau. Consigné comme hypothèse.
- Et le rappel d'échelle : dans `vf3run` sur l'interpréteur, la carte Model 3 fait 13,8 ms contre 1,40 pour le
  son. **Si l'objectif est des images, le son n'est pas là qu'il faut regarder.**

## Journal du 2026-10-24, le soir — Start, le choix de décor, et une lenteur sans propriétaire

### Ce qui s'ajoute au jeu

- **Start n'ouvre plus le menu qu'en combat.** La règle est de Frédéric, posée en deux temps, et le second
  corrige le premier : `FIGHT_INT` et `FIGHT_DSP` ouvrent le menu de pause, **et eux seuls**. La présentation
  (`ROUND_MASK_*`, `ROUND_*`, `SET_*`, `READY_*`) voit l'état **coupé**, par la demande de sous-état que les
  gestionnaires du jeu se passent entre eux. Partout ailleurs — `JUDGE`, le replay, `VIC`, `NEXT_PROG_SEL`,
  l'écran de choix — la touche va au jeu, intacte. `7e4f6a3`.
- **Le versus a un choix de décor**, `src/host/fv2_stage_list.h`, la quatrième surface du portage. `V.S.`
  ouvre une page de treize entrées au lieu de lancer, et c'est le décor choisi qui lance le match. `ef4c2c7`.

### Ce qui a été lu dans le code de Sega

- **`STAGE_SEL` est une souche vide sur la borne.** Les deux états que Sega nomme `STAGE_SEL_INT` et
  `STAGE_SEL_DSP` partagent une adresse, `0x0000BBB4`, qui tient une seule instruction : `b 0x0000bad8`, la
  sortie du répartiteur. Le jeu d'arcade n'a jamais eu de choix de décor. `83f443b`.
- **Le répartiteur recopie `SUBMODE_REQUEST` dans `MODE_SUB` à chaque tour**, ce qui est la preuve directe de
  ce sur quoi la coupure d'état repose.
- **Le sélecteur d'août commande bien un versus**, contrairement à ce que ce portage avait écrit. La phrase
  « atteint seulement par un rematch passant par l'écran de choix » était vraie du premier combat d'une route
  solo et fausse d'un versus. Corrigé dans `console_play.h` avec les trois mesures.

### LA LENTEUR DU SOIR N'EST PAS DE NOUS, et elle n'est pas corrigée

Frédéric : « il fait ralentir le jeu à 45fps ». Ce n'était **pas** la page de choix du décor, et c'est mesuré :

| | une image, en combat |
|---|---|
| `fv2` le matin, après la correction du `getenv` (`62a856c`) | **2,96 ms** — 57,5 images/s |
| `fv2` le soir | **15,0 à 19,6 ms** — 42 à 45 images/s |
| `fv2` **moins deux commits de son** | **60 images/s** |

Les deux commits sont **`1757e83`** et **`7f732ee`**, un audit du DSP du SCSP : 149 lignes dans
`src/sound/scspdsp.cpp`, qui tourne 128 pas par échantillon à 44,1 kHz, soit 5,6 millions de pas par seconde.
Le reflog de ce dépôt les date du 2026-10-24 à **17:13:58** et **17:23:07**, faits **dans ce dossier**, sur
`fv2`, entre deux de mes commits.

Vérifications faites au passage, parce que la page de décor était accusée à tort : la tenue du sélecteur ne
coûte rien (**zéro repatch sur 1500 images**), et le coût est le même avec un décor tenu et avec celui du jeu
(15,0 contre 15,7 ms).

**Ce qui est proposé et pas fait** : annuler le **code** de ces deux commits — `scspdsp.cpp`, `scspdsp.h`,
`scsp.cpp`, `sound_prof.h` — et **garder tout leur savoir**, les six documents `docs/scsp_*.md` et le banc
d'essai `tools/son/test_scspdsp.cpp`, qui ne coûtent rien à l'exécution. Frédéric a demandé une pause avant
de décider.

**Et une erreur à moi, consignée parce qu'elle a accusé quelqu'un.** J'ai désigné la session « gestion des
Ombres » comme autrice de ces deux commits, sur la seule foi d'un dossier de lancement et d'un horaire
voisin. Elle a vérifié et démontré le contraire : elle travaille dans `C:\Temp\fv2-ombres`, sur la branche
`fv2-ombres`, qui ne contient pas ces commits, et elle n'a jamais touché à `src/sound/`. **L'auteur reste
inconnu**, et Frédéric dit que personne ne travaille sur le son en ce moment. Une coïncidence prise pour une
mesure, le jour même où il me l'avait reproché deux fois.

### L'état de la machine à la pause

- **Le binaire qui tourne n'est pas la branche.** `build/bin/vf3.exe` du 20:06 est l'essai construit **sans**
  les deux commits de son, donc rapide. Toute reconstruction ramènera les 45 images par seconde tant que la
  question n'est pas tranchée.
- **L'arbre est propre** et le travail non commis d'une autre session dans `src/sound/soundboard.cpp` — une
  sonde `VF3_SCSP_DSPLOG` — a été copié avant d'être touché puis remis intact.
- Le projet Ghidra `FV2-claude` est ouvert sur `foo.bin`.

### Ce qui reste ouvert

- La lenteur ci-dessus, non corrigée, sans propriétaire.
- La réverbération de Fighting Vipers 2, jamais comparée à une référence (`docs/fv2-console-son.md`).
- Sortir d'un combat proprement, qui bloque trois des quatre lignes du menu de pause.
- `BGM SET`, et les combattants noirs du 2026-10-23.
