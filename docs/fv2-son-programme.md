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
dans l'interruption. Le pas de `0x20` et le compte de 32 sont dans le code, pas déduits.

**Et une erreur à moi, corrigée le 2026-10-24 au soir.** J'avais écrit que « les globales du pilote commencent
juste au-dessus, à `0xC01` — soit `0x400 + 32 × 0x20 = 0xC00`, la géométrie ferme ». **C'est faux :**
`32 × 0x20 = 0x400`, donc les fiches occupent `0x400-0x7FF` et `0x400 + 0x400 = 0x800`, pas `0xC00`. Ce qui
est à `0x800` est la file des commandes MIDI (§3 bis). Mon « la géométrie ferme » était une coïncidence
arithmétique sur laquelle j'ai bâti une conclusion — exactement la faute que la mémoire du dépôt appelle une
cause sans mesure. La carte réelle, celle-là mesurée :

| | |
|---|---|
| `0x300-0x30F` | un bloc maître ; `tst.b $300` commande l'entrée du séquenceur |
| `0x310-0x37F` | **7 pistes de séquenceur** de `0x10`, champ `+0xF` = un quartet de groupe (`0x6026AE`) |
| `0x380-0x3FF` | **8 entrées** de `0x10`, initialisées d'une table ROM à `0x608024`, et leur `+5` mis à `0xFF` (`0x600422`) |
| **`0x400-0x7FF`** | **les 32 fiches de voix de `0x20`** — la table de `group_of()` |
| `0x800-0xBFF` | **la file des commandes MIDI**, 4 octets l'entrée (§3 bis) |
| `0xC00` et au-delà | les globales : `0xC01` occupé, `0xC10` décalage du chip, `0xC1B`-`0xC1D`, `0xC30`, `0xC3E`/`0xC3F`, `0xEC0`-`0xECD` l'état de l'analyseur MIDI |
| `0x1000-0x13FF`, `0x201000` | **32 descripteurs de copie d'échantillons** de `0x20`, parcourus par A6 |

**Le premier long de la fiche est l'adresse des registres de la fente**, confirmé par `0x6006DC` :

```
006006DC  movea.l   $0(a4),a1            ; a1 <- le long a +0 de la fiche
006006E0  move.b    #$1f,$b(a1)
006006E6  move.b    #$10,$0(a1)          ; ecrit DANS les registres de la fente
```

Donc `group_of()`'s `r[0] != (n ? 0x30 : 0x10)` et `r[1] != slot * 0x20` testent **le mot haut et le mot bas
de ce long** : `0x00100000 + fente*0x20` pour le chip 0, `0x00300000 + fente*0x20` pour le chip 1. Les deux
tests sont **justes**, et ils sont ce que le programme écrit.

## 3 bis. Le protocole `A0 <banque> <index>`, suivi de bout en bout

Lu le 2026-10-24 au soir, et il est entièrement du MIDI standard :

1. **`snd_irq3` (`0x6005BE`) est la réception MIDI.** Elle lit l'octet au registre `+0x405`, le **réémet** vers
   `+0x407` si `0xC3F` est posé (un MIDI thru), puis l'analyse : bit 7 posé = octet de statut, sinon donnée.
   Le compte d'octets de données attendus est posé à **2, ou 1 si le statut est dans `0xC0..0xDF`** — la règle
   MIDI exacte (Program Change et Channel Pressure prennent une donnée, les autres deux).
2. **Le message complet est mis en file à `0x800`**, 4 octets l'entrée, index d'écriture `0xEC4`, masque
   `0x3FF`. Et `cmpi.w #$A000` (`0x60063C`) — **la comparaison est sur un MOT**, statut et première donnée
   ensemble, ce qui explique qu'une recherche de `cmpi.b #$A0` ne trouve rien : `A0 00 01/02/03` sont des
   commandes de contrôle qui effacent les pistes `0x300`-`0x370`.
3. **Le répartiteur est `0x600852`**, appelé par la boucle principale (`0x601CB8`) quand `0xECC` n'est pas nul.
   Il prend `D5 = statut & 0x0F` — **le canal MIDI** — et saute par une table à `0x60088E`, huit `bra.w`
   indexés par le quartet haut :

   | statut | | | statut | |
   |---|---|---|---|---|
   | `8n` note off | `0x60120A` | | `Cn` program change | `0x601C1A` |
   | `9n` note on | `0x6008AC` | | `Dn` | `rts`, ignoré |
   | **`An` le protocole du jeu** | **`0x6025F2`** | | `En` pitch bend | `0x6012D6` |
   | `Bn` control change | `0x601402` | | `Fn` | `rts`, ignoré |

4. **`0x6025F2` fait la recherche banque/index** : la banque est **décalée de `0x10`** (d'où `0x10` pour les
   musiques), rejetée si `>= 0x60` ou `< 0x10` ; table à deux niveaux depuis le pointeur `0x60801C`, bornée
   par un compte à chaque niveau, `0xFFFF` marquant une banque vide. Puis **`tst.b d5`** (`0x60264A`) : canal
   0 → la musique et le séquenceur à `0x300` ; canal non nul → `0x602694`.

**Et le troisième test est confirmé lui aussi, le 2026-10-24 au soir.** Il a fallu le chercher par l'autre
bout : non pas « qui pose un canal » mais « qui écrit l'octet `+5` ». Une seule instruction le fait sur le
registre des fiches (`0x6010F2`) :

```
006010EA  move.w  a0, d0
006010EC  subi.w  #$c40, d0        ; A0 - 0xC40
006010F0  lsr.w   #$5, d0          ; / 0x20
006010F2  move.b  d0, $5(a4)       ; -> l'octet +5 de la fiche
```

**L'octet `+5` n'est pas un canal MIDI : c'est l'indice d'une entrée dans une table de structures de `0x20`
octets basée à `0xC40`.** C'est pour ça qu'il peut dépasser 15, ce qu'un quartet de statut MIDI ne peut pas.

Et le `0x0F` du seuil n'est pas un nombre magique, c'est une **conséquence de la disposition**. `0x600430`
construit cette table depuis une table ROM (le pointeur `0x608014` → `0x608338`, puis `+0x24` → `0x60835C`),
et il la place **alignée à droite pour finir à `0xEC0`** :

```
0060044E  move.w  #$280, d6
00600452  sub.w   d7, d6           ; 0x280 - nombre * 0x20
00600454  adda.l  d6, a2          ; a2 = 0xC40 + ce reste
```

`0x280 / 0x20` = de la place pour **20 entrées**, et `0xEC0` est justement où commence l'état de l'analyseur
MIDI. La table ROM en déclare **5**. Donc elles occupent `0xE20`-`0xEBF`, et leurs indices sont **`0x0F` à
`0x13`** — les quinze places du bas, `0x00` à `0x0E`, restant à la musique.

| | |
|---|---|
| `group_of()` : `r[0] == 0x10/0x30` | ✔ confirmé par l'écriture (`0x6004E0`) |
| `group_of()` : `r[1] == fente * 0x20` | ✔ confirmé par la même |
| `group_of()` : `(octet +5) >= 0x0F` | ✔ **confirmé** : les cinq objets de canal des effets sont `0x0F`-`0x13` |

**Une correction au passage.** `soundboard.h` écrit « channels 0x0F to 0x12 » : la mesure en donne **cinq**,
`0x0F` à `0x13`. Le test du portage étant un `>=`, il les attrape toutes et rien n'était cassé — mais le
commentaire disait quatre là où il y en a cinq, et il est corrigé.

Les cinq entrées, telles que la ROM les donne (16 octets chacune, le reste mis à zéro) :

```
indice 0x0F : 83 0b 00 df 00 00 60 20 4e 10 df 00 08 00 00 00
indice 0x10 : a3 0c 00 eb 00 00 60 20 4e 10 eb 00 04 00 00 00
indice 0x11 : a3 0d 00 eb 00 00 60 20 4e 10 eb 00 04 00 00 00
indice 0x12 : a3 0e 00 ed 00 00 60 20 4e 10 ed 00 08 00 00 00
indice 0x13 : a3 0f 00 db 00 00 60 20 4e 10 db 00 04 00 00 00
```

Le premier octet est un statut MIDI (`0x83`, puis `0xA3` quatre fois) et le second va de `0x0B` à `0x0F` :
non lu plus loin, et noté tel quel plutôt que deviné.

**Ce qui reste ouvert sur ce point :** `driver_known` garde tout son sens. Rien de ce qui précède ne dit
qu'un AUTRE jeu range ses fiches ainsi — ce sont les tables de ce pilote-là, et le garde-fou reste la bonne
réponse pour un troisième jeu.


## 3 ter. Le `FF` n'arrête pas le son : il redémarre la carte

Le dépôt a mesuré ce piège en août et l'a contourné sans l'expliquer. `docs/fv2-console-son.md` le pose
ainsi : « le piège du reset : `FF` **relance** un son encore en cours avant de le couper », et « le délai
d'arrêt : 80 images, pour que le reset survive au bip d'ouverture ». Voici ce que `FF` fait réellement, lu
dans `snd_irq3_reception_midi` :

```
006005F2  cmpi.b  #$ff, d0
006005F6  beq.w   $6006c4
...
006006C4  lea.l   $2000.w, a7           ; la pile remise a sa valeur de reset
006006C8  move.l  #$4572726f, $c08.w    ; "Erro"
006006D0  move.l  #$72536574, $c0c.w    ; "rSet"
006006D8  bra.w   $600120               ; saut dans snd_reset
```

**`FF` est un redémarrage logiciel de la carte son.** Il remet la pile à `0x2000` (la valeur du vecteur de
reset), écrit la signature `"ErrorSet"` à `0xC08`, et branche sur le code du reset. Ce n'est pas une commande
d'arrêt : c'est le reset système MIDI pris au mot.

Et `snd_reset` relit cette signature, ce qui ferme la boucle :

```
00600196  cmpi.l  #$4572726f, $c08.w
006001A0  cmpi.l  #$72536574, $c0c.w
006001AA  move.b  $c04.w, d4            ; l'octet garde a travers l'effacement
006001AE  lea.l   $0.w, a0              ; puis la RAM son est effacee
```

Le marqueur distingue donc un **redémarrage à chaud** d'un démarrage froid, et un seul octet (`0xC04`) passe
à travers l'effacement de la RAM son — le reste est remis à neuf : registres, MVOL, RBL/RBP, les 32 fiches,
les tables de pistes, tout.

**Ce que ça explique.** Un son encore en cours s'entend à nouveau parce que la carte **réinitialise ses puces
pendant que des voix tournent toujours** : le reset repose MVOL à `0xF`, réécrit les registres communs et
balaie les fentes, et ce qui était en train de décroître repasse par là. Ce n'est pas un bogue du portage ni
un caprice du chip : c'est un reboot.

**Et ce qu'il faut en déduire avec prudence.** Les 80 images de délai que le dépôt a trouvées à l'essai
s'expliquent très probablement par la **recopie des échantillons** : le redémarrage efface la RAM son, et le
moteur de copie (§ la boucle principale) remonte les échantillons depuis la ROM **512 octets par passe**.
Rien de plus ne peut jouer avant. **C'est une déduction, pas une mesure** — personne n'a chronométré la
recopie.

**La conséquence pratique pour ce portage** : `FF` n'est pas un arrêt bon marché. Tout ce qui l'envoie
redémarre la carte son, et le portage l'envoie (c'est son levier d'arrêt, `docs/fv2-console-son.md`). La
question « faut-il geler le jeu sous la liste des modes » posée dans ce document se lit différemment une fois
qu'on sait ça.


## 3 quater. Le pont : comment un canal MIDI devient une voix, et sur quel chip

C'était le trou nommé plus haut. Il se lit dans le **note on** (`9n` → `0x6008AC`) :

```
006008BC  lea.l   $c40.w, a0        ; la table des objets de canal
006008C0  moveq   #$13, d7          ; VINGT entrees (dbra -> 20 tours)
006008C2  tst.b   $0(a0)            ; l'objet est-il en service ?
006008C8  cmp.b   $1(a0), d5        ; son octet +1 == le canal MIDI du message ?
006008DC  btst.b  #$5, $0(a0)       ; bit 5 de +0 :
006008E4  move.l  #$200000, $c10.w  ; ... alors c'est le SECOND chip
006008EC  bsr.b   $600912           ; jouer la note sur ce chip
006008F8  lea.l   $20(a0), a0       ; pas de 0x20
```

Trois choses d'un coup :

1. **La table de `0xC40` a bien 20 entrées de `0x20`** — `moveq #$13,d7` le dit, et c'est le même 20 que
   `0x280 / 0x20` de la section 3. Deux chemins, le même compte.
2. **L'octet `+1` d'un objet est le canal MIDI qu'il écoute**, et un note on joue sur **tous** les objets dont
   `+1` correspond — donc un seul message MIDI peut démarrer des voix sur les deux chips.
3. **Le bit 5 de l'octet `+0` choisit le chip** : posé, `[0xC10]` passe à `0x200000`, et tout ce qui suit
   travaille sur le second SCSP.

Relus avec cette sémantique, les cinq objets des effets que la ROM déclare :

| indice (l'octet `+5` d'une fiche) | `+0` | bit 5 | chip | `+1` = canal MIDI |
|---|---|---|---|---|
| `0x0F` | `0x83` | 0 | **0** | `0x0B` |
| `0x10` | `0xA3` | 1 | **1** | `0x0C` |
| `0x11` | `0xA3` | 1 | **1** | `0x0D` |
| `0x12` | `0xA3` | 1 | **1** | `0x0E` |
| `0x13` | `0xA3` | 1 | **1** | `0x0F` |

**Et ça confirme la dernière affirmation de `soundboard.h`** — « the game's effects, voices and announcer ask
for [...] **on the second SCSP** » : quatre des cinq y sont, par le bit 5 de leur octet `+0`. La chaîne
complète se lit donc d'un bout à l'autre :

```
le jeu envoie  A<canal> <banque> <index>       canal 0 = musique, 0x0B-0x0F = effets et voix
  -> snd_irq3_reception_midi, file a 0x800
  -> snd_repartir_commande_midi, D5 = le canal
  -> l'objet de canal a 0xC40 dont +1 == D5     indices 0x0F-0x13 pour les effets
  -> son bit 5 choisit le chip                  quatre des cinq sur le second
  -> la fiche de voix a 0x400 + fente*0x20
       son +5 = l'indice de cet objet           (A0 - 0xC40) / 0x20
  -> SoundBoard::group_of() lit ce +5 et seuille a 0x0F
```

Les trois tests du portage sont justes, et le seuil `0x0F` est la frontière entre les quinze objets de la
musique et les cinq des effets, posée par la disposition de la table et non choisie.


## 3 quinquies. Les sons longs sont STREAMÉS, et le portage en dépend

Une question neuve, prise parce que toutes les précédentes étaient fermées — et celle-là touche le portage.

La boucle principale est le moteur de copie des échantillons (A6 parcourt 32 descripteurs de `0x20` à
`0x1000`, `0x201000` pour le second chip). Deux choses y sautent aux yeux :

```
00601CA4  move.b    (0x3,A6), (0x00100408).l   ; le descripteur dit QUELLE FENTE surveiller
...
00601CF8  tst.b     $100409.l
00601CFE  bpl.b     $601ca4                    ; bit clair -> on repasse
00601D00  movea.l   $10(a6), a2                ; destination A
...
00601D22  tst.b     $100409.l
00601D28  bmi.w     $601ca4                    ; bit POSE -> on repasse
00601D34  movea.l   $8(a6), a2                 ; destination B
```

**Deux destinations dans le descripteur (`+0x08` et `+0x10`), choisies par la polarité opposée du même bit.**
C'est un double tampon, sans ambiguïté.

Et le bit, c'est **`CA`**. L'octet `+0x409` est la moitié basse du mot `0x408`, que le portage remplit en
`(CA << 7) | (SGC << 5) | EG` — donc son bit 7 est **le bit 0 de `CA`**. Et `CA = (cur_addr >> (SHIFT + 12))
& 0xF` avec `SHIFT = 12`, soit l'indice d'échantillon décalé de 12 : **`CA` compte par 4096 échantillons**, et
son bit 0 bascule toutes les **93 ms** à 44,1 kHz.

`0x6004F0` complète le tableau : il pose l'octet `+3` de chaque descripteur à `8, 16, 24…`, soit
`(fente + 1) << 3` — et écrit dans l'octet haut du mot `0x408`, c'est-à-dire **`MSLC`**. Le descripteur dit
donc quelle fente le chip doit rapporter, et la boucle lit où elle en est.

**Ce que ça établit :** le pilote **streame** les sons longs dans la RAM du SCSP, par moitiés de 4096
échantillons, en interrogeant la position de lecture pour savoir laquelle est libre. Il ne les charge pas en
entier.

**La conséquence pour ce portage, et elle est directe :** `CA` est **porteur**. Si le portage le rendait faux
ou figé, le moteur de copie tournerait en rond sur `0x601CA4` ou écrirait dans la moitié qu'on écoute — un
bégaiement ou un grésillement sur les sons longs, c'est-à-dire la musique. Le portage le calcule au vol depuis
`cur_addr`, et `VF3_SCSP_MSLC_LATCH=1` existe pour comparer avec la version figée une fois par échantillon
(`scsp.h`) — on sait maintenant **sur quoi** cette variable agit, et pourquoi elle n'est pas anodine.

**Et ça affine la déduction de la section 3 ter** : les 80 images de délai après un `FF` ne sont pas « recopier
tous les échantillons », c'est **réamorcer les tampons de streaming** — rien ne peut jouer avant qu'une moitié
soit remplie. Toujours une déduction, mais mieux cadrée.


## 4. Le `+0x600` : une question que j'ai mal posée, et que la mesure referme

**Cette section disait quelque chose de faux, et le voici corrigé.** J'avais écrit, le 2026-10-24
l'après-midi : « le pilote **accumule**, par fente et pour les deux chips, dans la fenêtre de registres à
`+0x600` [...] 32 fois par chip, à chaque interruption ». C'était lu dans le code et **jamais mesuré**.

L'instruction existe bien, dans `snd_tour_de_fente` :

```
006007F0  tst.b   $4(a4)              ; le bit 7 de +4 de la fiche
006007F4  bpl.b   $600848             ; s'il est clair, on passe a la fiche suivante
...
0060083A  lea.l   $100600.l, a1
00600840  adda.l  $c10.w, a1          ; 0x100600 ou 0x300600
00600844  add.w   d2, (a1, d6.w)      ; accumule a +0x600 + fente*2
```

Mais elle est **gardée par le bit 7 de l'octet `+4` de la fiche**, et ce garde ne s'est jamais ouvert.
Mesuré en élargissant la sonde `VF3_SCSP_DSPLOG` à toute la fenêtre de registres :

| | écritures dans `0x600-0x6FF` |
|---|---|
| Fighting Vipers 2, `fv2-fight.txt`, 4200 images, 35 096 écritures journalisées | **0** |
| Virtua Fighter 3, `vf3-match.txt`, 2400 images, 15 417 écritures | **0** |

Pas une seule, sur les deux jeux. La sonde voit bien les deux chips et les petits offsets — elle journalise
par ailleurs 1726 et 715 écritures du mot `0x6` des fentes, aux offsets `0x00C`, `0x02C`, `0x04C`… — donc le
zéro est un zéro, pas un angle mort.

**Conséquence : l'inquiétude que j'avais soulevée est sans objet.** Le portage route bien `0x600-0x6FF` vers
le tampon circulaire FM de MAME, et le chemin audio l'écrase bien à chaque échantillon — mais **il n'y a rien
à y écraser**, sur ces scènes. Le chemin de code existe et n'est pas emprunté.

**Et ce que le pilote fait vraiment de `STWINH`**, mesuré du même coup : il le **pose**, dans 890 des 1726
programmations de fente sur Fighting Vipers 2 et 423 des 715 sur Virtua Fighter 3, sur toutes les fentes. Donc
il s'en sert pour ce à quoi il sert — décider quelles fentes alimentent le tampon circulaire — et non pour
protéger quelque chose qu'il viendrait d'y écrire, ce que j'avais avancé comme hypothèse.

**La leçon, et c'est la deuxième fois dans la journée.** J'ai tiré une fréquence (« 32 fois par chip, à chaque
interruption ») d'une lecture de code, sans la mesurer, et je l'ai écrite comme un fait. La mesure dit zéro.
La règle du dépôt — lire le code pour savoir **quoi** chercher, mesurer pour savoir **ce qui arrive** — vaut
aussi, et surtout, pour ce qu'on croit avoir compris en lisant.

**Ce qui reste ouvert**, réduit à sa vraie taille : une autre scène pourrait ouvrir ce garde. La question
n'est plus « le portage perd-il des écritures » mais « ce chemin sert-il jamais », et elle se referme d'un
seul lancement avec la sonde élargie dès qu'on soupçonne une scène de l'emprunter.


## 4 bis. Les trois autres choses, et ce que la table `0xF00` est vraiment

### La table `0xF00` : une modulation logicielle, écrite dans le tampon FM

C'est **l'explication** de la section 4, et pas seulement sa correction. Le tick par fente lit une table de
**4 octets par fente** à `0xF00` (indexée par `fente*4`, obtenue du `+2` de la fiche décalé de 3) :

```
00600806  subq.b  #$1, (a1, d6.w)       ; +0 : un compte a rebours
0060080A  bne.b   $600832               ; pas echu -> on va droit a +1
0060080C  move.b  $2(a1, d6.w), d2      ; +2 : la recharge
00600810  bne.b   $60081a
00600812  bclr.b  #$7, $4(a4)           ; +2 nul -> FIN : le bit 7 du +4 de la fiche tombe
0060081A  move.b  d2, (a1, d6.w)        ; recharge +0
0060081E  move.b  $3(a1, d6.w), d2      ; +3 : une seconde valeur
00600822  beq.b   $60082e
00600824  move.b  d2, $1(a1, d6.w)      ; +3 non nul -> +1 = +3, un coup unique
00600828  clr.b   $2(a1, d6.w)
0060082E  neg.b   $1(a1, d6.w)          ; +3 nul -> ON INVERSE LE SIGNE de +1
00600832  move.b  $1(a1, d6.w), d2
00600838  lsr.b   #$1, d6               ; fente*2
0060083A  lea.l   $100600.l, a1
00600844  add.w   d2, (a1, d6.w)        ; accumule dans le tampon FM
```

| octet | |
|---|---|
| `+0` | un compte à rebours, décrémenté à chaque tick d'IRQ1 |
| `+1` | une valeur **signée**, accumulée dans `0x600 + fente*2` |
| `+2` | la recharge de `+0` ; **nulle, elle met fin à l'effet** |
| `+3` | une seconde valeur, qui remplace `+1` une fois puis s'effface |

Donc : une valeur dont **le signe s'inverse tous les N ticks** est accumulée dans le tampon circulaire FM, à
l'emplacement de la fente. Et une fente dont `MDXSL`/`MDYSL` pointe là le lit **comme source de modulation**.

**C'est un vibrato — ou un balayage de hauteur — piloté par logiciel, qui se sert du tampon FM du chip comme
d'un canal de modulation.** Voilà ce que le `+0x600` de la section 4 est, et pourquoi le portage, qui met
MAME's ring buffer là et l'écrase à chaque échantillon, le perdrait **si la fonction servait**. Elle ne sert
pas dans les deux scènes mesurées : le bit 7 du `+4` d'une fiche, qui la commande, ne se pose jamais.

### Les deux autres choses d'IRQ2 : un fondu descendant et un fondu montant

`snd_irq2` appelle trois aides. La troisième, `0x603460`, est l'interpréteur de flux de séquence (§3 bis : les
quantités à longueur variable et le *End of Track*). Les deux premières sont des **fondus** :

| | armé par | pas | |
|---|---|---|---|
| `0x6033C8` | `[0xEEB]` | `[0xEED] -= 4` | **fondu descendant** ; arrivé sous zéro il envoie `A0 00 02`, qui arrête la piste maîtresse |
| `0x603408` | `[0xEF2]` | `[0xEED] += 4` | **fondu montant**, jusqu'à `[0xEEE]` |

Toutes deux construisent `d1 = 0xA0010000 | (niveau << 8)` — soit la commande **`A0 01 <niveau>`** — et
branchent sur `0x6036FC`, qui **la poste dans la même file à `0x800`** que le récepteur MIDI remplit, en
avançant les deux index :

```
006036FC  lea.l   $800.w, a1
00603702  move.w  $ec4.w, d0          ; l'index d'ecriture
0060370C  move.l  d1, (a1)            ; la commande, 4 octets
0060370E  addq.w  #$4, $ec4.w
00603712  addq.b  #$1, $ecc.w         ; le compte de commandes en attente
```

**Les fondus se parlent donc à eux-mêmes par le chemin public des commandes**, au lieu de toucher le volume
directement. Et comme la banque `0x01` est inférieure à `0x10`, `0x6025F2` l'envoie sur sa table de saut de
commandes de contrôle (`0x602722`, `jmp $602730(pc,d2.w)`) : les banques `0x00`-`0x0F` sont des **ordres**, et
les banques `0x10` et au-delà sont des **sons**. La section 3 bis disait « banque décalée de `0x10` » ; voilà
pourquoi.

### Le chemin musique du canal 0

```
0060264E  tst.b   $c3e.w
00602654  move.b  #$f1, $100407.l     ; si arme, un 0xF1 est emis en MIDI (une marque de synchro)
00602662  lea.l   $300.w, a4          ; LA PISTE MAITRESSE
00602666  bsr.w   $602712             ; ses 0x10 octets remis a zero
0060266C  move.b  (a2)+, d0
0060266E  andi.b  #$60, d0
00602672  or.b    d0, $0(a4)          ; deux bits de drapeau pris dans l'en-tete du son
00602676  clr.b   $eeb.w              ; UN FONDU DESCENDANT EN COURS EST ANNULE
0060267A  move.l  (a2)+, $4(a4)
0060267E  move.l  a2, $8(a4)          ; +8 = LE POINTEUR DE FLUX de la sequence
00602682  move.w  #$1, $2(a4)         ; +2 = 1, le premier delai
```

Donc une piste de `0x10` octets porte : `+0` des drapeaux (bit 7 = active), `+2` le délai courant, `+4` un
long pris dans l'en-tête du son, **`+8` le pointeur dans le flux de séquence** — celui que `0x603460` lit avec
A3 — et `+0xF` le quartet de groupe que l'allocateur compare (§3).

Et le `clr.b $eeb.w` de `0x602676` **recoupe** la lecture des fondus : `0xEEB` est bien l'armement du fondu
descendant, puisque démarrer une musique l'annule. Deux chemins indépendants, la même conclusion.


## 4 ter. Le moteur d'instruments, et le `+6` qui aurait pu tromper le portage

`0x600912`, appelée par le note on une fois l'objet de canal trouvé, est le cœur du moteur. Elle sort
d'abord sur le chemin de **note off** si la vélocité `[0xEF8]` est nulle (`0x601256`) — la convention MIDI.

### Deux tables ROM de plus, et deux octets de plus de l'objet de canal

```
00600932  movea.l $608028.l, a1       ; les COURBES DE VELOCITE
00600938  move.b  $b(a0), d1          ; l'octet +0xB de l'objet de canal choisit la courbe
0060093E  move.w  $2(a1, d1.w), d1
00600944  move.b  (a1, d0.w), $ecd.w  ; la velocite passee dans la courbe -> [0xECD]
0060094A  move.b  $2(a0), d0          ; l'octet +2 de l'objet choisit l'INSTRUMENT
0060094E  movea.l $608038.l, a2       ; la table des instruments
00600956  move.w  $2(a2, d0.w), d0
0060095A  bmi.w   $601102             ; negatif = pas d'instrument, on abandonne
```

| l'objet de canal à `0xC40` | |
|---|---|
| `+0` | drapeaux ; **bit 5 = le chip** (§3 quater) |
| `+1` | le canal MIDI écouté |
| `+2` | **l'instrument**, index dans la table `0x608038` |
| `+0xB` | **la courbe de vélocité**, index dans la table `0x608028` |

La vélocité mappée atterrit dans `[0xECD]`, que `0x600C6A` recopie dans le `+0xA` de la fiche de voix.

L'instrument est une liste de **régions de touches** : deux formes selon son octet `+1`, l'une avec une
comparaison de hauteur sur 15 bits (`note*128 + fin`, entrées de 12 octets), l'autre sur un simple intervalle
d'octets. Un multi-échantillon, donc, avec découpage par touches.

### Le `+6` et le `+9` de la fiche, et l'exclusivité des notes

```
006009C0  lea.l   $400.w, a4          ; les fiches de voix
006009C8  moveq   #$1f, d7            ; les 32
006009CA  cmp.b   $6(a4), d5          ; le +6 de la fiche == LE CANAL MIDI
006009D0  cmp.b   $9(a4), d0          ; le +9 == un GROUPE D'EXCLUSIVITE
```

L'octet `+0xB` de **l'instrument** porte ce groupe, et son signe choisit le comportement :

| `+0xB` de l'instrument | si une fiche a le même canal et le même groupe |
|---|---|
| nul | la recherche n'a pas lieu — **polyphonique** |
| négatif (bit 7) | `bra 0x601102` : **la nouvelle note est abandonnée** (on ne réattaque pas) |
| positif | `bsr snd_fente_terminee` : **l'ancienne voix est coupée et remplacée** |
| `0x7F` | un cas à part, `0x600A12` |

### Et voici ce qui aurait pu tromper le portage

**La fiche de voix a DEUX champs de canal, côte à côte :**

| | |
|---|---|
| `+5` | **l'indice de l'objet de canal** — `0x00`-`0x0E` musique, `0x0F`-`0x13` effets (§3) |
| `+6` | **le canal MIDI** — `0x00`-`0x0F` |

`SoundBoard::group_of()` lit le `+5`, et c'est le bon. **S'il avait lu le `+6` voisin**, son test `>= 0x0F`
n'aurait attrapé que le canal MIDI `0x0F` — et les quatre autres objets d'effets, qui écoutent les canaux
`0x0B` à `0x0E`, auraient été classés **avec la musique**. Quatre sur cinq mal rangés, par un octet de
décalage, et le symptôme aurait été « le volume des effets n'agit presque pas » — difficile à attribuer.

Rien à corriger : le portage avait juste. Mais c'est noté, parce qu'un octet voisin qui ressemble à ce qu'on
cherche est exactement le genre de piège où une déduction se vérifie mal.


## 4 quater. L'allocateur de voix, les plafonds de polyphonie, et ce qui sépare les deux jeux

### Un vol de voix à priorité, borné par trois constantes en ROM

Le chemin qui reste du note on, quand l'exclusivité n'a rien trouvé (`0x600A42`) :

```
00600A42  btst.b #$0, $0(a0)         ; bit 0 de l'objet de canal : l'allocation est-elle permise
00600A52  move.b $ee3(a3), d0        ; un compteur de voix actives, PAR CHIP (a3 = le decalage)
00600A56  cmp.b  $60807e.l, d0       ; contre un plafond en ROM
00600A5E  add.b  $ee2(a3), d0        ; plus un second compteur
00600A62  cmp.b  $60807d.l, d0       ; contre un second plafond
00600A7E  cmp.b  $ee1(a3), d0        ; un troisieme, contre 0x60807C
00600A86  lea.l  $400.w, a4          ; sinon : LA RECHERCHE DE LA VICTIME, sur les 32 fiches
00600AA8  tst.b  $b(a4) ; bpl        ; le +0xB de la fiche, bit 7 = en service
00600ABA  cmp.b  $d(a4), d0          ; le +0xD = une PRIORITE ; on retient la plus faible
00600ACA  movea.l a4, a1             ; la candidate
```

Trois **compteurs de voix actives par chip** à `0xEE1`, `0xEE2`, `0xEE3`, contre trois **plafonds en ROM** à
`0x60807C`, `0x60807D`, `0x60807E`. Et `snd_fente_terminee` les décrémente (`subq.b #$1, $ee0(a1)`,
`$ee1`, `$ee2`) : la boucle est fermée. Quand tout est plein, la fiche de **plus faible priorité** (`+0xD`)
est volée.

### Et les plafonds ne sont pas les mêmes d'un jeu à l'autre

| | Fighting Vipers 2 | Virtua Fighter 3 |
|---|---|---|
| `0x60807C` / `0x60807D` / `0x60807E` | **`0x20` = 32** | **`0x10` = 16** |
| l'octet OU-é dans le registre de banque (`0x608068`) | `0xE0` | `0x20` |

**Fighting Vipers 2 s'autorise les 32 fentes d'un chip, Virtua Fighter 3 se limite à 16.** Le mécanisme est le
même, le réglage non. Rien dans le portage n'en dépend — c'est le pilote qui s'impose la limite à lui-même —
mais c'est une différence de comportement réelle entre les deux jeux qu'il sert.

Et l'octet de banque : le pilote OU-e `0xE0` (FV2) ou `0x20` (VF3) dans ce qu'il écrit à `0x400001`. Le
portage ne lit que **le bit 4** de ce registre (`soundboard.h` : « bit 4: the samples' upper 8 MB ») ; les
bits 5 à 7 sont donc rangés dans `b.ctrl` et ignorés. Ce sont probablement d'autres lignes de commande de la
carte. **Rien ne dit qu'elles manquent au portage** — rien n'en a besoin — mais c'est noté.

### La nuance que ça apporte à `driver_known`

Les deux ROM son ne sont identiques qu'à **28,9 %** (151 272 octets sur 524 288 ; 33 pages de 4 Ko sur 128).
Et les pages identiques ne sont **pas** celles du code : le code vit dans les pages 0 à 3, et elles diffèrent
toutes les quatre.

Pourtant tout ce que ce document établit vaut pour les deux : même SSP et même PC de reset, même table de
vecteurs, mêmes tables à `0x300`/`0x380`/`0x400`/`0x800`/`0xC40`, mêmes vingt objets de canal, même seuil, et
**trois programmes DSP identiques à l'octet**.

Donc : **le même pilote, construit et réglé différemment pour chaque jeu** — pas le même binaire. C'est
exactement ce qui justifie le garde-fou `driver_known` de `soundboard.h` : non pas qu'un autre jeu aurait un
pilote sans rapport, mais que *celui-ci* se recompile par jeu, et qu'un troisième pourrait déplacer ce que ce
document a cloué.


## 4 quinquies. La programmation de la fente, et trois boucles qui se referment

`0x600B96` est le point où tous les chemins du note on convergent, et la première instruction dit tout :

```
00600B96  movea.l $c10.w, a3
00600B9C  movea.l $0(a4), a5       ; <<<< A5 = LES REGISTRES DE LA FENTE, pris dans le +0 de la fiche
```

**C'est le pointeur même que `group_of()` valide.** Le portage vérifie que le premier long d'une fiche vaut
`0x00100000 + fente*0x20` ; le pilote, lui, le **déréférence** pour écrire dans la fente. Le test du portage
n'est donc pas une heuristique sur une valeur qui ressemble : c'est exactement l'usage que le pilote en fait.

### Les deux niveaux d'une voix, et où ils vont

```
00600BD4  move.b  $5(a2), d0       ; le +5 de la REGION d'instrument
00600BDA  andi.b  #$7f, d3
00600BDE  bne.b   $600be4
00600BE0  move.b  $c(a0), d0       ; nul -> on retombe sur le +0xC de l'OBJET DE CANAL
00600BE8  move.b  d0, $15(a5)      ; -> l'octet 0x15 de la fente
```

L'octet `0x15` d'une fente est la moitié basse du mot `0xA`, soit **`ISEL` et `IMXL`** — *quelle entrée du DSP
et à quel niveau*. Donc **l'envoi vers la réverbération est une propriété de l'instrument**, avec un défaut
par canal. C'est le robinet dont toute la question MADRS de la section 5 décide du destin.

Et `0x601108`, appelé juste avant avec le `+4` de la région (ou le `+6` de l'objet), calcule l'autre niveau :

```
0060110A  andi.b  #$e0, d0 ; lsr.b #$5, d0   ; les 3 bits de poids fort -> 0..7
00601110  add.b   $c04.w, d0                 ; + UN TRIM GLOBAL
00601116  clr.b   d0                         ; borne en bas
00601120  moveq   #$7, d0                    ; et en haut
00601122  lsl.b   #$5, d0
00601124  andi.b  #$1f, d3 ; or.b d3, d0     ; recombine : niveau(3) : panoramique(5)
```

`niveau(3 bits) : panoramique(5 bits)` est **exactement** la disposition de l'octet haut du mot `0xB` d'une
fente — `DISDL` aux bits 13-15 et `DIPAN` aux bits 8-12. C'est donc la **sortie directe** qui se calcule là.

### Trois boucles qui se referment, et c'est ce qui donne confiance

1. **`[0xC04]`, le seul octet que le `FF` préserve** (§3 ter : `snd_reset` le sauve dans D4 à travers
   l'effacement de la RAM), est **un trim de niveau global borné à 0-7, ajouté au `DISDL` de chaque voix**.
   La seule chose qu'il valait la peine de porter à travers un redémarrage est le volume d'ensemble. Deux
   lectures séparées, à deux heures d'écart, qui se rejoignent.
2. **`[0xEFB]`** est consulté ici (`0x60112A`) **et** par le chargeur de preset DSP (§3 bis : `0xE0` de force,
   soit `EFSDL = 7`). C'est **l'interrupteur global de réverbération**, lu des deux côtés — l'envoi d'une voix
   et le retour du DSP. Deux chemins indépendants, le même drapeau.
3. **Les compteurs de voix** : `0x600BFE` met `[0xEE1]` dans le `+0xB` de la fiche, pose son bit 7, **et
   incrémente `[0xEE1]`** ; `0x600C1A` incrémente `[0xEE3]`. Ce sont les compteurs que l'allocateur (§4 quater)
   compare aux plafonds ROM et que `snd_fente_terminee` décrémente. La boucle est bouclée dans les trois sens.

Et les champs de la fiche se recoupent aussi : le `+0xD` (priorité, que l'allocateur lit pour choisir sa
victime) vient du `+0xA` de la région, et le `+9` (groupe d'exclusivité, que la recherche de §4 ter compare)
du `+0xB` de la région. Ce que l'allocateur lit, c'est ce que la programmation écrit.


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

### Ce que c'est vraiment : un seul balayage, et le miroir se détruit lui-même

Corrigé le 2026-10-24 au soir, après avoir lu qui écrit MPRO. Ma première description — « le pilote charge
MADRS, **puis** efface `0x7C0-0x7FF` » — décrivait bien l'ordre mais pas le mécanisme, et le mécanisme est
plus net.

`0x602AD6`, appelé par `snd_reset` pour les deux chips, efface la zone DSP d'un trait :

```
00602ADA  lea.l   $100700.l, a3        ; le debut de la zone DSP
00602AE0  adda.l  $c10.w, a3           ; + le decalage du chip
00602AE4  move.w  #$4f, d7             ; 80 tours
00602AE8  clr.l   (a3)+   x4           ; 16 octets par tour
```

80 × 16 = `0x500` octets, soit **`0x700` à `0xBFF` d'un seul coup** : COEF, MADRS, `0x7C0-0x7FF` et MPRO.
Mesuré, image 39 : `COEF zéro ×64, MADRS zéro ×32, 7C0 zéro ×32, MPRO zéro ×512` — 640 mots, exactement.

Et **le chargement d'un preset suit le même chemin**. Image 45, chip 0 : **981 écritures, de `0x700` à
`0xBFE`, monotones, pas dominant `+2`** (avec 237 mots écrits en deux octets, `mask ff00` puis `00ff`). Ce
n'est pas « charger MADRS puis effacer » : c'est **un seul balayage linéaire de toute la zone DSP depuis un
bloc en ROM**, et les 32 mots de `0x7C0-0x7FF` de ce bloc valent zéro.

**D'où l'argument, et c'est le plus fort de la journée :** si le matériel mirrorait `0x7C0-0x7FF` sur MADRS,
**aucun preset ne survivrait à son propre chargement** — le balayage écrirait les 32 adresses de ligne de
retard, puis les écraserait 32 mots plus loin, à chaque fois, pour tous les presets et tous les jeux. Le DSP
tournerait toujours avec des adresses nulles. Or la borne avait une réverbération.

C'est une **inférence**, pas une mesure : elle suppose que la réverbération d'époque fonctionnait comme prévue.
Mais elle vient du code du jeu lui-même, et non d'un accord entre émulateurs — ce qui est exactement la sorte
d'argument que `docs/scsp_references.md` demande de préférer.

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
5. **Si le programme DSP change par arène** — et ce point est **resté sans réponse faute de levier**.
   Mesuré : sur `fv2-fight.txt`, **43 rafales MPRO et 9 programmes distincts**, dont un écrit à l'image 2724,
   entre le chargement du décor (2500) et l'intro (2650). Et **trois de ces programmes sont les mêmes octets
   que sur Virtua Fighter 3** (empreintes `35be7110`, `8457aa56`, `c7f97684`), ce qui confirme le pilote
   commun. Mais la scène ne traverse qu'**une** arène, et `vf3run --console-mode versus` appelle
   `choose_versus()` sans argument de décor : **on ne peut pas comparer deux arènes depuis l'outil sans
   fenêtre aujourd'hui**. Le levier existe — la sélection de décor que l'autre session est en train d'écrire
   dans `src/host/console_play.h` — et il faudra deux passages avec deux décors pour trancher.
   Une arène ne prouve rien sur la variation : c'est noté comme non résolu, pas comme probable.
