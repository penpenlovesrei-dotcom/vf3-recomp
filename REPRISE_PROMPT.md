# Prompt de reprise — Fighting Vipers 2, menu console, session du 2026-10-20

Colle ce texte au début d'une nouvelle session.

---

## Le contexte

`C:\Users\frede\Desktop\fv2-recomp` — portage de jeux Sega Model 3 (Virtua Fighter 3 Step 1.0, **Fighting
Vipers 2** Step 2.0), dérivé de `mr-finster/vf3-recomp`. Le programme PowerPC est recompilé (SLJIT) avec un
interpréteur en repli ; la carte, le Real3D, le 68000/SCSP et l'hôte SDL3 sont le code propre du projet.
**Réponds en français.**

Branche `fv2`, 36 commits devant `mine/fv2`, rien de poussé depuis le tag `fv2-v0.2.0-alpha`.

**Lis `REPRISE.md` à la racine avant tout.** Il porte l'état complet, le journal du jour, et où sont les
sources d'origine. Les documents du chantier sont dans `docs/` ; les trois qui comptent pour la suite sont
`fv2-etats.md`, `fv2-menu-console-inventaire.md` et `fv2-console.md`.

## Ce qui est demandé

Une seule touche, deux comportements :

- **ailleurs qu'à l'écran titre** → Start ramène le jeu à son écran titre ;
- **à l'écran titre** → Start ouvre le sous-menu, la liste des sept modes (ARCADE, V.S., RANDOM, SURVIVAL,
  NETWORK, TRAINING, OPTIONS) ;
- dans les deux cas Start est **retenu**, le jeu ne le voit pas.

Le menu de la touche Échap peut rester où il est.

## Ce qui est déjà posé et qui ne sert encore à rien

`src/machine/model3.h` : `fv2_mode()`, `fv2_submode()`, `fv2_on_title()`, `fv2_request_title()`.
`tools/vf3run.cpp` : `--modes`, le journal des états sous les noms de Sega.

```
MODE_MAIN 0x0058FD98   MODE_SUB 0x0058FD99   MODE_REQUEST 0x0058FD9A   SUBMODE_REQUEST 0x0058FD9B
```

Écran titre = mode `ADV` (2 ou 3) et sous-état 4 ou 5. Y aller depuis l'attraction =
`MODE_REQUEST = 3`, `SUBMODE_REQUEST = 4`, **écrit une fois, jamais tenu**, deux images. Depuis un combat ces
leviers ne commandent rien : il faut d'abord sortir de la partie par la route du jeu (`docs/fv2-etats.md`).

## La faute à ne pas refaire, commise DEUX fois le 20/10

**Le menu console n'est pas une page du menu de la touche Échap.** `menu.open(console_page())` ouvre le widget
RmlUi des réglages de ce portage : quoi qu'on y mette, c'est le menu Échap qui s'affiche. Frédéric l'a rejeté
deux fois dans la même journée.

La liste des modes doit être **une surface à elle**, dessinée par-dessus l'image du jeu, sur l'écran titre
d'origine — pas sur une illustration qui le recouvre, pas dans le menu de réglages.

Et la vérification doit porter sur ce qui est en cause : constater que le jeu gèle ne distingue pas les deux
menus, puisque les deux gèlent le jeu.

## Les images

`assets/menu-console/`, 45 fichiers, **hors dépôt** (`.gitignore`). `title_list.png` est la liste des sept
modes, 709 x 897 RGBA ; ses bandes, l'encre de chaque ligne et son placement sur l'écran titre sont relevés
dans `docs/fv2-etats.md`. **Une seule version de chaque ligne existe** : aucun fichier n'est une variante
« sélectionnée », donc la ligne choisie devra être marquée autrement.

## Les règles de travail, non négociables

- **Aucune prédiction.** Tout se lit dans le code ou se mesure. S'il faut décompiler, on décompile.
- **Le code dit ce qui PEUT arriver, la mesure dit lequel est arrivé.**
- **Montrer toutes les captures, sans verdict.** C'est Frédéric qui tranche.
- **Toujours un lanceur `.cmd`**, jamais une ligne de commande à recopier.
- **Ne jamais filtrer la sortie de compilation.**
- **Ajouter à côté, ne jamais remplacer ce qui marche.** Lire avant d'écrire.
- **Aucun binaire de Sega, aucune ROM, aucune image tirée du jeu dans le dépôt.**
- **Ne rien reprendre du code de Supermodel ni de MAME** — ils sont sous GPL. Les adresses et les mesures sont
  des faits sur le jeu, pas du code.
- **Pousser sur `mine` uniquement**, jamais sur `origin` (l'amont de `mr-finster`).
- **Rien n'est livré sans `verifier-vf3.cmd`** : cinq contrôles IDENTIQUE et 2231 images en pas à pas.
- Réponses courtes. Dire ce qui est **mesuré** et ce qui ne l'est pas.

## Les pièges, payés

1. **Une seule image de la RAM qui diffère de l'instruction d'où son code natif a été bâti éteint le
   recompilateur EN ENTIER.** Les correctifs arrivent avant `rebuild_from_ram_if_stale()`, ou provoquent une
   reconstruction.
2. **Le programme de FV2 est un gzip à l'offset 0 de la CROM**, déplié en RAM 0 par l'amorce. Tous les
   correctifs sont des correctifs de RAM, et chacun relit avant d'écrire.
3. **`board.ram` garde les mots de 32 bits dans l'ordre de l'hôte** : l'octet d'adresse A est à l'indice
   `A ^ 3`.
4. **`tools/vf3run` ne charge JAMAIS la NVRAM.** Aucun réglage d'exploitant ne peut s'y prouver.
5. **`vf3run` ne rend l'image que sur les marques `shot` d'un script** (et `--hd-dump`).
6. **Tenir une demande de mode casse le jeu** — l'écran de sélection et son chrono, deux fois. Une demande
   s'écrit une fois.
7. **Les heredocs bash mangent les apostrophes et les échappements** : écrire les fichiers avec l'outil Write.
8. **Les fichiers du dépôt sont en CRLF** : un script d'insertion qui cherche `\n` ne trouve rien.
9. **Le lien échoue en « Permission denied »** si le jeu ou le vérificateur tourne encore.
10. **Une taille de PNG identique ne prouve rien**, et deux empreintes différentes non plus. Il faut regarder
    l'image.

## Les sources d'origine, hors dépôt

```
Documents\SEGA Model 3\SupermodelSrc\Docs\MenuConsole.md    3689 lignes -- TOUT le relevé sur la ROM
Documents\SEGA Model 3\SupermodelSrc\Src\Menu\GameFV2.h     les adresses et la géométrie
Documents\SEGA Model 3\ROMs\fvipers2.zip                    la ROM
```

`grep -n "^#" MenuConsole.md` donne sa table des matières. §6 (savoir où en est le jeu), §6 bis (sortir d'un
combat), §6 quater (les états nommés par Sega), §6 sexies (le menu sur le vrai écran titre), §13 (les pages du
menu et leur géométrie).

## Ce qui reste ouvert

- **Le second joueur n'a jamais été introduit** (`0x0058145A == 3`). Trou de banc d'essai, pas de
  connaissance. Le fermer servirait la boucle Versus et la vraie condition de déblocage de Honey.
- La fin, l'entrée du nom, le classement et le Super K.O. : nommés, jamais atteints.
- Ce qui *décide* de reconstruire un combattant (`0x00060288`) : inconnu. Ce qui le *commande* est connu et
  employé.
- Les branches de Survival n'ont jamais été regardées tourner.

## Par où commencer

1. Lire `REPRISE.md`, puis `docs/fv2-etats.md`.
2. Brancher Start sur les deux chemins — `fv2_on_title()` décide lequel.
3. La surface : `title_list.png` par-dessus l'image du jeu. C'est la seule pièce sans antériorité à reprendre.
