# Le programme son, lu dans son code

Commencé le 2026-10-24. **C'est un début, pas un relevé complet** : le programme fait 512 Ko et ce qui suit
en couvre une poignée de fonctions. Chaque ligne dit où elle a été lue.

Pourquoi ce document : la règle qui prime du dépôt dit d'ouvrir la décompilation **avant** de mesurer, et pour
le PowerPC elle existe (`docs/fv2-flux-de-match.md`). Pour le son il n'y avait rien, et ça se payait —
`docs/fv2-console-son.md` est 641 lignes de questions, et `soundboard.cpp` porte une structure **devinée**
derrière un garde-fou (`driver_known`) parce que personne ne l'avait lue. Deux de ses trois hypothèses sont
maintenant **confirmées par le programme lui-même**.


## 1. L'image, et l'ordre des octets tranché par la mesure

Le programme est **un seul EPROM**, 524 288 octets, mappé à `0x600000` par la carte
(`src/sound/soundboard.h`). `epr-20600a.21` pour Fighting Vipers 2, `epr-19231.21` pour Virtua Fighter 3.

L'ordre des octets est le seul piège, et il a été **tranché, pas supposé** : `romset.cpp` pose la région SOUND
avec `stride 1, chunk 1, swap false`, donc les octets du fichier y entrent tels quels ; puis
`SoundBoard::Bus::word` lit `v[a] | v[a+1] << 8`, soit **l'octet bas en premier**, alors que le 68000 est
big-endian. Les vecteurs de reset le disent sans appel :

| | SSP | PC de reset |
|---|---|---|
| échangé (octet bas en premier) | `00002000` | **`00600120`** — pile en haut des 8 Ko, code dans la ROM |
| big-endian direct | `00000020` | `60002001` — impair, hors de la carte |

Et après l'échange, les vecteurs 2 et 3 tombent à `006005AC` et `006005B0`, dans la ROM eux aussi. Quatre
vecteurs sains contre un faux.

`tools/son/extraire-son.py` fait l'extraction et l'échange, et **refuse d'écrire** si le PC de reset ne tombe
pas dans la ROM programme — pour qu'une image fausse ne se désassemble pas pendant une heure.

**Les deux jeux donnent le même SSP et le même PC de reset** (`00002000` / `00600120`) : c'est le même pilote,
ce qui appuie l'idée que `driver_known` parle d'une seule famille de pilote et pas de deux.


## 2. Les points d'entrée, que le 68000 porte lui-même

Pas besoin d'une carte du recompilateur comme pour le PowerPC : un 68000 porte ses entrées dans sa table
d'exceptions. Elle compte **72 longs** ici — pas 256 : le PC de reset est `0x600120`, donc la table s'arrête
là où le code commence, et lire au-delà lit du code comme des vecteurs (la première passe de
`FV2SonSetup.java` s'y est fait prendre et a posé trois fonctions en pleine autre fonction).

| vecteur | cible | |
|---|---|---|
| reset | `00600120` | |
| bus error | `006005AC` | |
| address error | `006005B0` | |
| **IRQ 1** | **`00600780`** | le tour du pilote : les 32 fentes des deux chips |
| **IRQ 2** | **`006032D2`** | |
| **IRQ 3** | `006005BE` | |
| IRQ 4-7, et une cinquantaine d'autres | `006005B4` | un gestionnaire fourre-tout commun |

Trois niveaux d'interruption servis, et **seules les interruptions du premier SCSP atteignent le 68000**
(`soundboard.cpp` : le second a `h.irq` vide).


## 3. Les fiches de voix à `0x400` — ce que `group_of()` devinait, et ce qui est confirmé

`soundboard.h` dit : « VF3's sound driver keeps a record a voice, 32 bytes at 0x400 + slot * 0x20 in each
SCSP's RAM: the slot's register address, then the channel the voice plays for. » C'était une déduction. Voici
le programme, dans `snd_irq1` :

```
00600780  movem.l   d2/d6-d7/a1/a4,-(a7)
00600784  move.l    $c10.w,$c14.w
0060078A  move.w    #$3c6,$10041a.l      ; SCSP 0 +41A : l'interruption acquittee/rearmee
00600792  move.w    #$80,$100422.l       ; SCSP 0 +422
0060079A  move.b    $c01.w,d7            ; un drapeau global en RAM son
0060079E  andi.b    #$12,d7
006007A2  bne.b     $6007d2              ; deja en cours -> sortie
006007A4  ori.b     #$2,$c01.w
006007AA  clr.b     $c30.w
006007AE  lea.l     $400.w,a4            ; <<< LES FICHES, chip 0
006007B2  moveq     #$1f,d7              ; 32 fentes
006007B4  clr.l     $c10.w
006007B8  bsr.b     $6007de
006007BA  lea.l     $200400.l,a4         ; <<< LES FICHES, chip 1 (0x400 de SA RAM)
006007C0  moveq     #$1f,d7
006007C2  move.l    #$200000,$c10.w      ; c10 = le decalage du chip courant
006007CA  bsr.b     $6007de
...
0060084C  dbra      d7,$6007de           ; et la routine finit par
00600848  lea.l     $20(a4),a4           ; <<< UN PAS DE 0x20
```

**Établi :** les fiches sont à `0x400` de la RAM de chaque chip, **32 fiches de `0x20` octets**, parcourues
dans l'interruption. Le pas de `0x20` et le compte de 32 sont dans le code, pas déduits ; et les globales du
pilote commencent juste au-dessus, à `0xC01`, `0xC10`, `0xC14`, `0xC30` — soit `0x400 + 32 × 0x20 = 0xC00`.
La géométrie ferme.

**Le premier long de la fiche est l'adresse des registres de la fente**, confirmé par `0x6006DC` :

```
006006DC  movea.l   $0(a4),a1            ; a1 <- le long a +0 de la fiche
006006E0  move.b    #$1f,$b(a1)
006006E6  move.b    #$10,$0(a1)          ; ecrit DANS les registres de la fente
```

Donc `group_of()`'s `r[0] != (n ? 0x30 : 0x10)` et `r[1] != slot * 0x20` testent **le mot haut et le mot bas
de ce long** : `0x00100000 + fente*0x20` pour le chip 0, `0x00300000 + fente*0x20` pour le chip 1. Les deux
tests sont **justes**, et ils sont ce que le programme écrit.

**Ce qui n'est PAS encore établi :** le troisième test, `(r[2] & 0xFF) >= 0x0F` — l'octet `+5` de la fiche lu
comme « le canal pour lequel la voix joue », avec la musique sur `0x00-0x0E` et les effets sur `0x0F-0x12`.
L'octet `+4` est bien un champ de drapeaux (le programme teste ses bits 0, 2 et 7, et écrit `#$ff` dans le
mot `+4`), mais **la routine qui pose un numéro de canal en `+5` n'a pas été trouvée** — elle est du côté du
protocole MIDI (`A0 <banque> <index>`), pas dans l'interruption. **Tant qu'elle n'est pas lue, le partage
musique/effets reste une déduction.**

Les autres champs vus en passant, sans être compris : `+2` (le mot bas du long, utilisé comme `fente*4` pour
indexer une table à `0xF00`), `+8` (un décalage de rotation), `+9`, `+B`, `+D`, `+E` (un long), `+10` (un
compteur décrémenté à chaque interruption, et à zéro il appelle `0x6006DC`). Et une table parallèle à
`0x1000`, pas de `0x20` (`clr.w $c00(a4)`).


## 4. Une question neuve : le pilote écrit dans `+0x600`, que le portage efface

Trouvé en cherchant, pas en lisant. La routine par fente finit par :

```
00600838  lsr.b     #$1,d6               ; d6 = fente*2
0060083A  lea.l     $100600.l,a1
00600840  adda.l    $c10.w,a1            ; 0x100600 ou 0x300600
00600844  add.w     d2,(a1,d6.w)         ; ACCUMULE dans +0x600 + fente*2
```

Le pilote **accumule**, par fente et pour les deux chips, dans la fenêtre de registres à `+0x600`. Et ce que
le portage fait de cette plage (`scsp.cpp`, `w16`/`r16`) :

```cpp
else if (addr < 0x700)
    m_RINGBUF[(addr - 0x600)/2] = val;     // le tampon circulaire FM de MAME
```

Or `DoMasterSamples` **réécrit ce tampon à chaque échantillon** : pour chacune des 32 fentes,
`m_RBUFDST = m_RINGBUF + m_BUFPTR` puis `*m_RBUFDST = ...`, avec `m_BUFPTR` qui tourne sur 64 — sauf pour une
fente dont `STWINH` est posé, qui n'écrit pas. Les entrées 0 à 31, exactement là où le pilote écrit, sont donc
**détruites dans l'échantillon qui suit**, à moins que `STWINH` ne les protège.

**Ce qui est établi :** le pilote écrit là, 32 fois par chip, à chaque interruption ; le portage y met le
tampon FM de MAME ; le chemin audio l'écrase.

**Ce qui ne l'est pas, et qu'il ne faut pas conclure :** si ça s'entend. Le pilote **ne relit jamais** cette
plage par adressage absolu (une seule occurrence dans toute l'image, celle de l'écriture ci-dessus), donc il
n'y a aucune preuve qu'il attende d'y retrouver quelque chose. Et personne n'a établi ce que le matériel fait
de `0x600-0x6FF` : MAME y met le tampon FM, aucune source consultée le 2026-10-24 ne le confirme
(`docs/scsp_references.md`).

**La mesure qui trancherait**, et elle est petite : `VF3_SCSP_WLOG` ne journalise aujourd'hui que les
écritures sous `0x430` (`soundboard.cpp`). L'élargir à toute la fenêtre de registres montrerait, dans
l'ordre, les écritures en `0x600+` **et** celles de `STWINH` (bit 9 du mot `0x6` de chaque fente), donc si le
pilote protège ce qu'il vient d'écrire.


## 5. La plage `0x7C0-0x7FF` : le mécanisme est tranché, l'ampleur ne l'est pas

**Le programme efface bien `0x7C0-0x7FF`, et il le fait APRÈS avoir chargé MADRS.** Mesuré le 2026-10-24
avec une sonde écrite pour ça (`VF3_SCSP_DSPLOG`, `soundboard.cpp`), sur les deux jeux.

Ce que la sonde dit, et c'est tout ce qu'elle dit :

| | Fighting Vipers 2 | Virtua Fighter 3 |
|---|---|---|
| écritures dans les registres DSP | 33 370 | 14 702 |
| dont MPRO / COEF / MADRS / `7C0-7FF` | 25 179 / 5 007 / 1 840 / **1 344** | 11 568 / 1 745 / 749 / **640** |
| valeurs écrites dans `7C0-7FF` | **`0000`, les 1344** | **`0000`, les 640** |
| séquences (image × chip) avec un effacement | 41 | 20 |
| écritures MADRS **non nulles** avant l'effacement | **878** | **176** |
| écritures MADRS non nulles après | 53 | 19 |

Les 32 mots de la plage sont écrits d'un bloc, et le journal montre la boucle passer sans rupture de `0x7B8`,
`0x7BC`, `0x7BE` à `0x7C0`, `0x7C2`, `0x7C4` : **un seul effacement de 64 mots, de `0x780` à `0x7FF`**. Puis,
dans la même image, deux à quatre adresses réelles sont réécrites.

### Ce que ça fait, et pourquoi c'est une déduction et pas une mesure

`scsp.cpp` (`w16`) avec le miroir : une écriture en `0x7C0 + k` va dans `MADRS[k]`. Les 32 mots valant zéro,
**MADRS est remis à zéro en entier**, après le chargement. Donc, avec le miroir, les 878 adresses de
Fighting Vipers 2 et les **176 de Virtua Fighter 3** sont écrasées, et il ne reste que les 53 et 19 écrites
ensuite. C'est une déduction de deux faits (l'ordre mesuré, le routage lu dans le code), pas une troisième
mesure.

### Et ce que ça fait entendre : bien moins que le mécanisme ne le laissait croire

Mesuré juste après (`tools/son/comparer-madrs.cmd vf3`, 2400 images de `vf3-match.txt`, les deux passages
produits sans fenêtre et comparés par `tools/son/mesurer-madrs.py`) :

| | miroir (l'actuel) | séparé | écart |
|---|---|---|---|
| pic moyen, pièce et sélection | 519 | 666 | **+2,2 dB** |
| pic moyen, combat | 2891 | 2898 | +0,0 dB |
| corrélation G/D, combat | **0,861** | **0,865** | néant |
| énergie par bande (4 bandes) | — | — | **±0,1 dB** |
| écho de la pièce, −20 dB atteint | 2,05 s | 2,25 s | +0,20 s |
| écho du start, −20 dB atteint | 0,95 s | 1,15 s | +0,20 s |
| écart max entre les deux fichiers | — | — | 1897, soit −24,7 dBFS |

**Les deux fichiers diffèrent, et très peu.** Et surtout : **la corrélation gauche/droite ne bouge pas**
(0,861 contre 0,865), alors que c'est LA signature de l'effondrement — sur VF2 la même mesure donnait 0,99
avec le miroir contre 0,73-0,85 sans. Ici, rien.

Donc **le mécanisme est établi et sa conséquence audible, sur cette scène, est petite** : deux dixièmes de
seconde de décroissance et 2,2 dB sur la pièce. Ce n'est pas l'ordre de grandeur de ce que `scsp.h` décrit
pour VF3 (« kept apart, they rang on for several [seconds] »), et ce n'est pas celui du défaut de Fighting
Vipers 2 (pic moyen 22831 → 6532, soit 11 dB, et la corrélation qui s'effondre).

Trois lectures possibles, **aucune tranchée** :

1. le programme DSP de VF3 utilise peu de lignes de retard, ou son envoi vers le DSP (`EFSDL`) est bas sur
   cette scène, et écraser MADRS y change donc peu ;
2. les 19 adresses écrites **après** l'effacement sont celles qui portent le son, et les 176 écrasées ne
   servaient pas ;
3. **la scène ne va pas là où ça se voit.** `vf3-match.txt` fait l'amorçage, l'attraction, une pièce, la
   sélection et un round de jeu aléatoire — ni K.O., ni écran de victoire, ni variété d'arènes. Le défaut de
   Fighting Vipers 2 vivait précisément sur l'écran de victoire (`VIC_DSP`).

La troisième est la plus probable et la moins coûteuse à vérifier, et c'est celle qu'une écoute doit viser.

### Où l'écart est vraiment, et de quelle taille

Frédéric n'a pas entendu de différence entre les deux fichiers. L'analyse du signal dit pourquoi : **l'écart
existe, il est réel, et il tient dans un seul moment.**

Le fichier **miroir tombe à zéro numérique exact à 12,54 s**, là où le **séparé garde une queue de
réverbération** qui court encore 1,9 s en décroissant de 311 à 10 (soit −40,5 à −70 dBFS). Sur toute la scène :

| | miroir | séparé |
|---|---|---|
| échantillons strictement nuls | **19,90 s** sur 38,8 | **10,68 s** sur 38,8 |

Les 9,2 s d'écart sont des queues de réverbération que le miroir coupe net. Mais réparties en **369 plages**,
dont une seule compte vraiment :

| début | durée | pic du séparé | |
|---|---|---|---|
| **12,54 s** | 0,59 s | 311 | **−40,5 dBFS** — la seule franchement audible |
| 13,12 s | 0,15 s | 115 | −49,1 dBFS |
| 13,28 et 13,41 s | 0,14 s | 72 | −53,2 dBFS |
| 15,08 s | 1,42 s | 4 | −78,3 dBFS — niveau du bit de poids faible |
| 16,58 s | 5,26 s | **1** | −90,3 dBFS — un seul LSB, inaudible par tout compte |

`tools/son/audibilite.py` (rapport bruit/masque, avec étalement du masquage, seuil absolu d'audition et bruit
de pièce) place ce moment à **+32 dB au-dessus du seuil** — parce qu'il tombe dans un silence, où rien ne le
masque. `build/extrait-vf3-AB.wav` le rend évident : 11,8 à 14,6 s, les deux versions à la suite, +18 dB.

### Le témoin, et ce qu'il a invalidé

La même mesure sur un cas dont la réponse est connue — l'écran de victoire de Fighting Vipers 2, où le miroir
fait naître un son qui monte de 24 dB jusqu'au limiteur :

| | médiane | 99e centile | part des fenêtres > 0 |
|---|---|---|---|
| écran de victoire FV2 (défaut grossier, **connu**) | −15,3 dB | +20,0 dB | **10,0 %** |
| Virtua Fighter 3 (écart ténu, inaudible à l'oreille) | −2,2 dB | +25,0 dB | **47,9 %** |

**Le témoin rend des chiffres plus petits que le cas discret.** Ce n'est pas une panne de l'outil : le NMR
mesure si un écart est **démasqué**, pas s'il est **grave**. Le défaut de FV2 est un son fort et soutenu, donc
son propre masque est énorme et le NMR reste bas ; l'écart de VF3 tombe dans un silence, où rien ne le masque.

Deux leçons, et la seconde est la vraie :

1. pour « est-ce grave », il faut le niveau, le spectre et la corrélation (`mesurer-madrs.py`), pas le NMR ;
2. **sans le témoin, j'aurais annoncé que l'écart de VF3 était plus important que le défaut de FV2.** Les deux
   premières versions de `audibilite.py` étaient fausses — la première comparait chaque bande à elle-même en
   ignorant le masquage des voisines, la seconde comptait comme audibles des résidus à −90 dBFS faute de
   plancher de bruit ambiant. Chacune donnait des chiffres plausibles. C'est le témoin qui les a tuées.

### Ce que ça renverse, et dans quelle mesure

`src/sound/scsp.h` garde le miroir pour Virtua Fighter 3 au motif qu'il y a été « MEASURED RIGHT : its coin
and start echoes die in about a second, as the reference's do ; kept apart, they rang on for several ».

Ce qui est acquis : **le mécanisme**. Le miroir écrase bien 176 adresses de ligne de retard sur VF3, et la
longueur de la réverbération en dépend — ce n'était pas un réglage neutre.

Ce qui ne l'est pas, et que la mesure du paragraphe précédent met en doute : **l'ampleur**. Les échos de la
pièce et du start, mesurés ici au même endroit, passent de 0,95 s à 1,15 s et de 2,05 s à 2,25 s. « Plusieurs
secondes » n'est pas reproduit. Ou la mesure d'origine portait sur autre chose (une autre scène, une écoute
à la fenêtre), ou elle visait un état que `vf3-match.txt` ne traverse pas.

Et c'est **le même mécanisme** qui a été jugé faux sur Fighting Vipers 2 le 2026-10-24 (commit `335c050`, le
son de l'écran de victoire qui se nourrit lui-même, pic moyen 22831 → 6532 quand on sépare), et le même que
MAME a corrigé de son côté pour `doa` (#16527). Trois jeux, un mécanisme.

**Ce qui n'est PAS tranché, et qu'il ne faut pas conclure :** ce que fait le matériel. Si un vrai SCSP mirrore
cette plage, alors la borne tournait aussi avec ses lignes de retard effondrées, et la réverbération courte
**est** le son d'époque. Rien ici ne le dit, et il n'y a pas de Model 3 sur ce bureau.

**Ce qu'il faut donc faire, et que je n'ai pas fait :** reprendre la mesure de VF3 avec
`VF3_SCSP_MADRS_SEPARATE=1`, et écouter. Changer ce réglage change le son du portage : c'est une décision, pas
une correction, et elle n'est pas à moi.


## 5 bis. Comment la question se posait avant la sonde

Rappel de la question (`docs/scsp_references.md`) : ce portage garde le miroir de MADRS pour VF3 et le sépare
pour Fighting Vipers 2 ; MAME `master` jette les écritures et renvoie MADRS en lecture. Personne n'avait
regardé ce que le programme fait.

Ce qui est trouvé : dans toute l'image, **aucune référence absolue à `+7C0`-`+7FF`** — mais **COEF (`+700`)
sept fois et MADRS (`+780`) trois fois**, toutes groupées entre `0x602A82` et `0x602C52`. Autrement dit le
chargement du DSP est là, et s'il efface `0x7C0-0x7FF` il le fait **par un registre d'adresse en boucle**,
depuis la base MADRS, pas par une adresse écrite en clair — ce qui est exactement le motif que décrit le
commentaire de `scsp.h` (« writes COEF and MADRS, then zeros 0x7c0-0x7ff »).

**La fonction est `snd_charger_preset_dsp`, `0x602B32`**, lue dans Ghidra : un interpréteur de commandes sur
une table de presets. Chaque commande porte un opcode (`+8 & 0x1F` : 1, 2, 4, 0xD, 0xE, 0x10, 0x16), un
décalage de destination pour COEF (`+0xA`) et un pour MADRS (`+0xB`), et un bit `+8 & 0x20` qui fait ajouter
un décalage de base aux valeurs MADRS. L'opcode 2 écrit 10 mots de COEF puis 8 de MADRS ; l'opcode 0x16, 38
mots de COEF ; l'opcode 4, trois COEF et deux MADRS.

Autrement dit **le code seul ne pouvait pas répondre** : les destinations viennent de la table de données. Il
a fallu la sonde du §5. C'est la leçon de méthode de la journée — lire le code dit *quoi chercher*, et
parfois seule la mesure dit *ce qui arrive*.

Le dernier passage de la fonction écrit l'octet `0x100017 + fente*0x20`, soit **EFSDL/EFPAN** de chaque fente
(le niveau de retour du DSP et son panoramique), avec la valeur de la table — ou **`0xE0` de force**
(EFSDL = 7, le maximum) si le drapeau global `0xEFB` de la RAM son est posé. C'est l'interrupteur de la
réverbération.


## 6. L'outillage, et comment refaire tout ça

| | |
|---|---|
| extraire la ROM et remettre les octets d'aplomb | `tools/son/extraire-son.py <jeu.zip> <sortie.bin>` |
| importer dans Ghidra et analyser, sans interface | `tools\son\importer-son.cmd` |
| semer les points d'entrée avant l'analyse | `tools/ghidra/FV2SonSetup.java` (en `-preScript`) |
| **chercher** dans tout le programme | `tools/son/desas_son.py <image> --cherche 100600 1006ff` |
| lire une fonction depuis une frontière sûre | `tools/son/desas_son.py <image> --at 600780 26` |
| la table des vecteurs | `tools/son/desas_son.py <image> --carte` |

Le projet Ghidra est `C:\Temp\fv2-ghidra\FV2SON.gpr`, **à part de `FV2.rep`** (le PowerPC) : ce n'est ni le
même processeur ni le même programme, et un projet ouvert dans l'interface est verrouillé pour l'analyseur
sans interface.

Partage des rôles entre les deux outils, et ce n'est pas un doublon : **Ghidra pour lire** une fonction (il
suit les tables de saut, `desas_son.py` non), **`desas_son.py` pour chercher** dans les 512 Ko depuis une
ligne de commande, de façon refaisable, sans projet. Les deux réponses dont ce portage avait besoin — les
fiches à `0x400`, l'écriture en `+0x600` — sont venues de la recherche.


## 7. Ce que la suite doit lire, par ordre d'intérêt

1. **La fonction qui contient `0x602BA2`** : le chargement COEF/MADRS, et ce qu'elle fait de `0x7C0-0x7FF`.
   C'est la question de fidélité ouverte.
2. **Qui pose l'octet `+5` d'une fiche** : le dernier tiers non confirmé de `group_of()`, donc du partage
   musique/effets. À chercher du côté du protocole MIDI `A0 <banque> <index>`.
3. **`snd_irq2` (`0x6032D2`)** : jamais regardé. L'un des trois niveaux servis.
4. **Le `FF` qui relance un son avant de le couper** (`docs/fv2-console-son.md`, mesuré en août, jamais
   expliqué) : le gestionnaire du reset MIDI le dirait.
5. **D'où viennent les 112 pas du programme DSP**, et s'ils changent par arène.
