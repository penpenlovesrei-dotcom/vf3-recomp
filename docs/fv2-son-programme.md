# Le programme son, lu dans son code

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [scsp/docs/sound.md](son.md).

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

`tools/sound/extraire-son.py` fait l'extraction et l'échange, et **refuse d'écrire** si le PC de reset ne tombe
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
| `0x200` + décalage puce | le **journal d'annulation** du chargeur d'échantillons : les anciennes valeurs, un long l'entrée (§5 bis sexies) |
| `0xF80-0xF9F` + décalage puce | **un mot par canal MIDI** — 16 canaux, 32 octets — indexé `canal*2` par `0x603172` ; effacé à l'arrêt d'une voix (§5 bis septies). Contenu non établi. **Attention : l'index n'est pas celui de la table voisine de `0xF00`**, qui est par fente |
| `0x3000` + décalage puce | les **fiches d'échantillon**, `0x0C` octets l'entrée : `+0` adresse et banque, `+4` longueur, `+8` cumul de relocation (§5 bis sexies) |
| `0x0000-0x00FF` | la **table des vecteurs vivante**, copiée de la ROM par `0x6003D0` — et la **pile** part de `0x2000` et descend dessous (`lea $2000.w, a7` en `0x6006C4`) |
| `0x2000-0x2FFF` + décalage puce | la **banque de seize formes d'onde** d'un cycle, treize distinctes, à une adresse réservée que rien n'écrase ; des fentes y pointent par leur `SA` pour une **voix FM** (§4 septies) |
| `0x4EE8-0x706B` | la **table des instruments en RAM** (`[0x608038]`, longueur `[0x608030]`), et `0x706C` est la base d'allocation des échantillons : les deux se touchent **exactement** |

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
par ailleurs 1726 et 715 écritures du mot `0x6` des fentes, aux offsets `0x00C`, `0x02C`, `0x04C`…

**Et le 2026-10-25, un témoin positif sur la branche elle-même**, parce qu'une sonde muette et une sonde qui
dit « rien » sont indiscernables dans la sortie. La branche `RINGBUF` a été pointée temporairement sur
`0x400-0x4FF`, une plage écrite à coup sûr (les registres communs) : **786 179 lignes**. La condition, le
libellé et le `fprintf` fonctionnent donc, et la branche est atteignable. Le zéro est un zéro.

Les autres zéros de ce document portent leur propre témoin **dans la même ligne** : `dspprog` annonce
`frcl 0 adrl 0 yrl 0 negb 0` à côté de `iwt 23 twt 39 mrd 23 mwt 10 ewt 5`, comptés par le même code au même
moment — des compteurs qui parlent pour quatre voisins qui se taisent.

### Et le zéro n'était pas général : il dépend de l'arène

Le même 2026-10-25, en forçant l'arène (`VF3_STAGE`, voir §4 sexies) au lieu de laisser le jeu choisir la
sienne, la même sonde sur la même scène de 4 200 images donne :

| arène | écritures dans `0x600-0x6FF` |
|---|---|
| celle que le jeu choisit seul | **0** — la mesure ci-dessus |
| **arène 2** | **993**, à partir de l'image 3783 |
| les onze autres (1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) | **0** |

Donc le zéro était vrai, et il ne voulait pas dire ce que j'en avais tiré. **Ce n'est pas un garde qui ne
s'ouvre jamais : c'est un garde qu'une arène sur douze ouvre.** Les offsets touchés le confirment —
`0x600` à `0x63E` et `0x640` à `0x67E`, c'est-à-dire exactement `0x600 + fente × 2` et `0x640 + fente × 2`,
les deux plages que `0x60153A` écrit (§5 bis quaterdecies). Le code que j'ai lu est bien celui qui s'exécute.

**Et la question de `STWINH` se tranche du même coup.** C'était l'inquiétude du portage : il route `+0x600`
vers le tampon FM de MAME, que le chemin audio réécrit à chaque échantillon **sauf si `STWINH` protège
l'entrée**. En suivant l'état de `STWINH` fente par fente dans le journal ordonné, et en le lisant au moment
de chaque écriture :

```
993 ecritures dans le tampon FM :
  STWINH pose  (l'entree est protegee) : 993  100,0 %
  STWINH clair (le son la reecrit)     :   0    0,0 %
  etat inconnu (jamais vu de SLOT6)    :   0
```

**Le pilote protège l'entrée chaque fois, sans une exception sur 993.** La modulation logicielle atteint donc
le matériel intacte, et le routage du portage est correct sur ce point. C'était l'autre question ouverte de
cette section, et elle est fermée.

La leçon, qui est celle de ce document depuis le début : un zéro mesuré avec témoin positif est un vrai zéro,
mais il ne porte que sur **ce qu'on a fait tourner**. Il m'avait fallu une heure, le même jour, pour écrire
« la fonctionnalité existe, ces scènes ne l'emploient pas » — c'était juste, et il suffisait de changer
d'arène pour trouver une scène qui l'emploie.

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


## 4 septies. Les 4 Ko de `0x2000` : seize formes d'onde, et la carte de la RAM qui se referme

Point du §5 ter, pris le 2026-10-25. `0x600524` copie 4 096 octets de la ROM (`0x603FE2`) vers `0x2000` dans
**chacune des deux RAM sonores**, par 512 tours de deux `move.l`. Le contenu était noté « non identifié ».

### C'est une banque de seize formes d'onde

Les 2 048 mots se lisent comme du PCM 16 bits signé, et se classent par fenêtres de 128 mots :

| adresse | ce que c'est |
|---|---|
| `0x2000` | **une sinusoïde** — une période exacte de 128 mots, amplitude 32 767, **écart maximal 5** contre `−32767·sin(2πk/128)` |
| `0x2200`, `0x2B00` | deux **carrés** à pleine échelle |
| `0x2C00` | une **constante**, 32 704 sur les 128 mots |
| `0x2300`, `0x2400`, `0x2500`, `0x2600`, `0x2800`, `0x2A00`, `0x2D00`, `0x2E00`, `0x2F00` | des formes **lisses** d'un cycle (saut moyen de 500 à 1 800 sur une plage de 65 000, donc continues) |
| `0x2100`, `0x2700`, `0x2900` | **irrégulières à grands sauts** (18 000 à 21 000 de saut moyen) : du bruit |

**Seize emplacements de 256 octets, treize formes distinctes** : `591bf29e` occupe `0x2300`, `0x2A00` et
`0x2F00`, et `8dcc2bdc` occupe `0x2600` et `0x2D00`. Les formes lisses se raccordent bien d'un bout à
l'autre — `|w[0] − w[127]|` vaut 0, 1 ou ~1 600, soit un pas de sinusoïde — et les carrés sautent de
65 281 au raccord, ce qu'un carré doit faire.

C'est donc **une banque d'oscillateurs d'un cycle**, ce dont un pilote a besoin pour les voix synthétiques,
puisque le SCSP ne sait jouer que du PCM pris dans sa RAM.

### Et la carte de la RAM se referme sur elle

Les longs de la zone de constantes, lus au même endroit :

| | |
|---|---|
| `[0x608038]` | `0x4EE8` — la table des instruments **en RAM** |
| `[0x608030]` | `0x2184` — sa longueur |
| `[0x608034]` | `0x706C` — la **base d'allocation** des échantillons chargés |

Et `0x4EE8 + 0x2184 = 0x706C`, **exactement**. La table des instruments finit où commence l'allocation des
échantillons, sans un octet de jeu. La RAM sonore se lit donc ainsi, de bas en haut :

```
0x0000-0x00FF  la table des vecteurs vivante, copiee de la ROM par 0x6003D0
0x0100-0x1FFF  ... et la PILE, qui part de 0x2000 et descend (lea $2000.w, a7 en 0x6006C4)
0x2000-0x2FFF  LA BANQUE DE SEIZE FORMES D'ONDE
0x3000-...     les fiches d'echantillon, copiees de [0x608000] par 0x600556
0x4EE8-0x706B  la table des instruments, copiee de [0x60802C] par 0x600592
0x706C-...     les echantillons charges, empiles vers le haut par 0x60288A
```

La banque est donc à une adresse **réservée** : l'allocateur d'échantillons part de `0x706C` et monte, il ne
la touche jamais. C'est une région permanente, et c'est cohérent avec des données que les fentes jouent.

### Qui la fait jouer : mesuré, et c'est une voix FM

Deux recherches statiques n'avaient rien donné — aucune instruction ne nomme une adresse de `0x2001` à
`0x2FFF`, aucune des 659 fiches d'échantillon n'y pointe. Restait une seule hypothèse testable : **ce sont
les fentes qui la jouent**, par leur registre `SA`. Ça ne se lit pas, ça se mesure.

La sonde `VF3_SCSP_DSPLOG` a donc été étendue aux mots `0x0` et `0x2` d'une fente — `SA` tient ses bits
19-16 dans le quartet bas du premier et ses bits 15-0 dans le second. Deux passages de 4 200 images sur
`fv2-fight.txt`, arènes 2 et 7 :

| | arène 2 | arène 7 |
|---|---|---|
| lignes `SLOTSA` | 6 641 | 6 151 |
| adresses `SA` complètes | 1 013, **21 distinctes** | 940, 25 distinctes |
| **tombant dans `0x2000-0x2FFF`** | **37** | **0** |

Les 37 pointent toutes sur `SA = 0x02800`, soit **l'emplacement 8 de la banque** — une seule des seize
formes, toutes sur la **puce 0**, et de l'image **3783** à 4191.

**Et 3783 est exactement l'image où commencent les 993 écritures du tampon FM** (§4, fin). Même arène, même
puce, même image. Les deux choses que je prenais pour deux questions n'en sont qu'une.

### La chaîne complète, et ce que chaque maillon doit à quoi

```
0x60153A  move.b $a(a1), $e(a5)     ; le +0x0A de l'instrument -> registre 0x0E d'une fente
          move.w $c(a1), (0x600 + fente x 2)
          move.w $e(a1), (0x640 + fente x 2)
```

Le registre `0x0E` est, d'après l'implémentation du portage (`scsp.cpp` : `data[0x7]`), **`MDL` bits 15-12,
`MDXSL` bits 11-6, `MDYSL` bits 5-0** — le niveau de modulation FM et les deux entrées du tampon circulaire
qui l'alimentent. Donc `0x60153A` **arme la FM sur la fente et fournit lui-même la modulation**, dans les
deux entrées que la fente lira.

Ce qui fait tenir l'ensemble :

| maillon | d'où il vient |
|---|---|
| la forme d'onde porteuse | la banque de `0x2000`, emplacement 8, par `SA` — **mesuré**, 37 fentes |
| la FM armée sur la fente | le `+0x0A` de l'instrument dans le registre `0x0E` — **lu** en `0x60153A` |
| les valeurs de modulation | `0x600 + fente × 2` et `0x640 + fente × 2` — **mesuré**, 993 écritures |
| elles survivent au chemin audio | `STWINH` posé **993 fois sur 993** — mesuré (§4) |
| et tout cela dans une arène sur douze | l'arène 2 — mesuré des deux côtés |

**C'est donc une voix FM**, dont le pilote écrit la modulation plutôt que de laisser la fente se moduler sur
l'audio en cours. Et le portage le savait déjà pour l'autre jeu : le commentaire de `scsp.cpp` autour de la
ligne 1095 note « le thème du décor de Pai a une voix FM (le second SCSP, `MDL 0xF`, `MDXSL 0x20` : son propre
dernier échantillon) à partir de 9,4 s », profondeur réglée **à l'oreille** contre Supermodel. Les deux cas
diffèrent sur un point : à Pai la fente se module sur elle-même, ici le pilote fournit les valeurs.

### Ce que ça laisse ouvert

- **Quinze des seize formes ne servent pas** dans les deux arènes mesurées. Elles servent ailleurs, ou pas.
- **Pourquoi l'emplacement 8** et pas un autre : ce qui choisit le `SA` n'est pas lu. Ce n'est pas une fiche
  d'échantillon, puisque aucune n'y pointe ; c'est donc une table ou un calcul que je n'ai pas trouvé.
- Et la profondeur de la FM dans le portage reste **réglée à l'oreille**, pas mesurée sur matériel : le
  commentaire de `scsp.cpp` le dit, et ce travail-ci ne le change pas.

## 4 sexies. Le programme du DSP change par arène — pour trois arènes sur douze

Point 5 de la liste de lecture, **resté sans réponse faute de levier** depuis le début. Le levier existe
depuis que l'autre session a écrit la sélection de décor : `Model3::fv2_hold_stage` commande l'arène d'un
versus et `fv2_hold_training_stage` l'octet de route, qui commande celle du **premier match d'une partie**.

`tools/vf3run.cpp` reçoit donc `VF3_STAGE=n`, qui **tient les deux** à chaque image — tenus et non écrits une
fois, parce que le tirage marque son propre choix comme utilisé en le prenant. Le levier est dans mon outil
et non dans `src/host/console_play.h`, qui est le fichier d'une autre session.

> **Cette section a d'abord conclu l'inverse, et c'est l'erreur la plus instructive de la journée.** Avec
> **deux** arènes — la 2 et la 7 — tout était identique écriture pour écriture, et j'ai écrit « le programme
> du DSP ne change pas par arène » en notant la limite : « deux arènes sur douze… refaire les dix autres
> coûte dix minutes et vaudrait d'être fait ». Les dix minutes ont retourné le titre : **la 2 et la 7 sont
> toutes deux dans le groupe de neuf qui ne charge rien de plus.** Le caveat était le bon, l'affirmation ne
> l'était pas. Ce qui suit est la mesure sur les douze.

### Le témoin, arène par arène

Une écriture ne prouve pas que le jeu l'a lue. Douze passages de 4 200 images sur `fv2-fight.txt`, un par
arène, et pour chacun l'empreinte de la RAM finale et celle du WAV :

**douze empreintes de RAM distinctes, douze WAV distincts.** Le levier a donc pris pour les douze, et chaque
passage est bien une partie différente — la condition pour que la comparaison veuille dire quelque chose.

### Toutes les arènes partagent le même chargement, et trois en ajoutent un

Les rafales MPRO tombent aux mêmes images pour les douze jusqu'à l'image 2438. Puis la **dernière** rafale
sépare les arènes en deux familles :

| arènes | dernière rafale MPRO | programme |
|---|---|---|
| 1, 2, 3, 4, 5, 7, 8, 10, 12 | puce **1**, images 2396-2438, 1 225 écritures | `149e507a` |
| **6**, **9** | puce **0**, images **2722-2738**, 1 193 écritures | **`efff4af1`** |
| **11** | puce **0**, images **2736-2738**, 1 169 écritures | **`f977884a`** |

**Trois arènes sur douze chargent un programme de plus, sur la puce 0, après le début du combat.** Et
`efff4af1` **n'apparaît nulle part ailleurs** : neuf arènes ne le voient jamais. C'est donc une variation du
microcode par arène, et non un simple décalage de chargement.

Le détail de l'image 2722-2724 pour l'arène 9 est à noter : c'est **exactement** l'image où une mesure
antérieure de ce document avait vu « un programme écrit à l'image 2724 ». Cette mesure portait donc sur une
arène de ce groupe-là — celle que le jeu choisit seul.

### Il fallait comparer les images, pas les comptes

Le piège était à un pas. L'arène 11 fait **25 155 écritures MPRO** contre 23 986 pour les neuf — plus, donc
« un programme différent », aurait-on conclu du compte seul. Mais en empreintant **l'image complète de
`0x800-0xBFF`** à la fin de chaque rafale, l'arène 11 ne tient que les deux mêmes programmes que les neuf :
sa rafale supplémentaire **recharge `f977884a`**, elle n'apporte rien de neuf. Les arènes 6 et 9, elles,
tiennent bien un troisième programme.

Une séquence d'écritures plus longue ne prouve pas un programme différent. Les deux méthodes ne répondent
pas à la même question, et c'est la seconde qui répond à celle-ci.

### Les coefficients, eux, varient en cinq jeux

Même méthode, sur les séquences d'écritures COEF et MADRS, numéro d'image retiré :

| arènes | COEF | MADRS |
|---|---|---|
| 1, 2, 5, 7, 10 | `5b9d79a9e0` | `1b9661bb72` |
| 3, 4, 8, 12 | `308b7ad0ad` | `cd9a948eb7` |
| 6 | `980fc20056` | `a29a1505aa` |
| 9 | `099510d537` | `89bcda78a6` |
| 11 | `32b130ec67` | `7444a5af8d` |

**Cinq jeux distincts de coefficients et d'adresses de ligne de retard**, et les deux zones donnent
**exactement la même partition** — ce qui est attendu, puisque l'interprète de `0x602B32` les écrit depuis
les mêmes descripteurs (§5 bis bis). Les neuf arènes qui partagent un microcode se partagent donc malgré
tout **deux** réglages différents.

Et la plage contestée `0x7C0-0x7FF` suit le microcode : 1 280 écritures pour les neuf, 1 344 pour {6, 9, 11}.
Les 64 de plus sont le programme plus long, pas une incursion nouvelle.

### Le tampon FM : une seule arène sur douze

| arènes | écritures dans `0x600-0x6FF` |
|---|---|
| 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 | **0** |
| **2** | **993**, à partir de l'image 3783 |

La modulation logicielle de `0x60153A` est donc employée par **une seule arène sur douze**. Voir la fin du
§4 : c'est aussi là que `STWINH` se tranche, et il protège les 993 sans exception.

### Ce que ça établit, et ce que ça ne fait pas

**La réponse à la question posée est oui** : le pilote peut charger un programme DSP propre à l'arène, et il
le fait pour trois des douze. Les paramètres, eux, varient pour toutes — en cinq jeux.

Ce que ça ne fait pas :

- **Rien sur *Virtua Fighter 3***, dont le pilote est le même et les données non.
- **Une seule scène**, `fv2-fight.txt`, et seulement son premier match. Une partie plus longue traverse
  plusieurs arènes et pourrait en montrer d'autres.
- **Pourquoi** ces trois arènes-là, et pourquoi deux d'entre elles partagent un programme : non lu. Le
  `0xEF1` qui choisit le préréglage est écrit par une commande de contrôle (§5 bis bis) ; qui la lui envoie
  côté PowerPC n'est pas cherché.
- Et l'écart de comptage avec la mesure antérieure de ce document (« 43 rafales, 9 programmes ») **reste
  inexpliqué**. Ce comptage-ci en voit 13 à 15 selon l'arène, et 2 ou 3 programmes. Je ne le donne pas pour
  résolu.

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
d'argument que `scsp/docs/scsp_references.md` demande de préférer.

### Ce que ça fait, et pourquoi c'est une déduction et pas une mesure

`scsp.cpp` (`w16`) avec le miroir : une écriture en `0x7C0 + k` va dans `MADRS[k]`. Les 32 mots valant zéro,
**MADRS est remis à zéro en entier**, après le chargement. Donc, avec le miroir, les 878 adresses de
Fighting Vipers 2 et les **176 de Virtua Fighter 3** sont écrasées, et il ne reste que les 53 et 19 écrites
ensuite. C'est une déduction de deux faits (l'ordre mesuré, le routage lu dans le code), pas une troisième
mesure.

### Et ce que ça fait entendre : bien moins que le mécanisme ne le laissait croire

Mesuré juste après (`tools/sound/compare-madrs.cmd vf3`, 2400 images de `vf3-match.txt`, les deux passages
produits sans fenêtre et comparés par `tools/sound/mesurer-madrs.py`) :

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

`tools/sound/audibilite.py` (rapport bruit/masque, avec étalement du masquage, seuil absolu d'audition et bruit
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

Rappel de la question (`scsp/docs/scsp_references.md`) : ce portage garde le miroir de MADRS pour VF3 et le sépare
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


## 5 ter. A5 et A6 : une affirmation à moi, fausse, et défaite le lendemain

Le 2026-10-24 j'ai écrit — en commentaire Ghidra, dans un script et dans le message de `fd74c38` — que le
pilote met A5 à `0x00100000` et A6 à `0x1000` **une fois au reset et n'y touche plus**, et que le dire à
Ghidra remonterait son compte de fonctions. **Les deux moitiés étaient fausses.**

### Ce que la mesure dit

| | écritures | |
|---|---|---|
| **A5** | **33** | `0x00100000` par `lea` en sept endroits ; **`[0xC10]`, le décalage du chip (0 ou `0x200000`), en trois** ; zéro en deux ; un pointeur indirect dans une vingtaine d'autres |
| **A6** | **8** | `0x1000` deux fois, `0x00201000` une fois, et des pointeurs indirects |

Ce sont des **registres d'adresse ordinaires**, pas des bases globales. Et en `0x600B9C` — dans la section
4 quinquies de ce document, écrite la même nuit — A5 prend **l'adresse des registres d'une fente**. Le
document se contredisait déjà lui-même à deux sections d'écart.

### Ce que l'intervention a donné

Posées sur toute la plage puis l'analyse relancée, en sans-interface :

```
functions before: 41
functions after:  41   (+0)
```

Rien. Et il restait une **constante fausse dans la base du projet**, que le décompilateur aurait appliquée aux
vingt-six endroits où A5 est autre chose. Retirée (`tools/ghidra/FV2SonRegistresDefaire.java`, qui relit
`null` pour le vérifier), le commentaire de `0x600142` remplacé par ce qui a été mesuré, et
`FV2SonRegistres.java` supprimé du dépôt.

### Les trois façons dont ça a tenu, et la quatrième pendant la correction

1. **L'affirmation vient d'une seule instruction, généralisée** — le `lea` du reset — alors que mes notes de
   la même session **citaient déjà** `movea.l $c10.w, a5` en `0x602B32`. Le contre-exemple était en main et
   n'a pas été lu.
2. **Mon premier balayage d'opcodes a rendu « zéro écriture »**, ce qui ressemblait à une confirmation. Le
   masque `0xFFC0` avait été écrit `0xF1C0`, qui **effaçait les bits du registre** : la comparaison ne pouvait
   jamais être vraie. Je ne l'ai attrapé que parce que j'avais lu les instructions à la main la veille et que
   le zéro contredisait ce que je savais. Sans cette vérité sous les yeux, j'aurais publié un deuxième faux.
3. **Et pendant la correction elle-même** : mon filtre `Select-String` cherchait `Defaire>` là où le préfixe
   est `FV2SonRegistresDefaire.java>`. Le script avait tourné et réussi ; sa sortie était masquée. Une passe
   réussie et une passe muette, indiscernables — exactement la mémoire du dépôt sur les sondes, en petit.

C'est la même faute que les deux de la veille (§4, l'arithmétique des fiches) : **une fréquence ou une
portée affirmée depuis une lecture, sans mesure.** Trois fois en deux jours, et la troisième avait l'air
d'être vérifiée.

### Et pourquoi Ghidra n'en trouve que 41 : mesuré, et c'était bien les tables de saut

La question est restée « sans explication » une heure, le temps de la mesurer au lieu de la supposer. Et
d'abord en vérifiant que « 41 contre 95 » comparait bien la même chose — mon marcheur appelle *fonction* toute
cible de `bsr`, ce que Ghidra ne fait pas. Sur mes 82 points d'entrée de cette passe :

| | |
|---|---|
| 34 | sont une **entrée** de fonction Ghidra |
| **2** | tombent **dans** un corps sans en être l'entrée — des étiquettes, pas des fonctions |
| **46** | tombent **hors de tout corps** — du code que Ghidra ignore |

Donc le dénominateur était honnête à deux près, et l'écart est réel. Ce que Ghidra couvre, en adresses :

**6 806 des 15 136 octets de code, soit 45 %.** Et les trous ne sont pas dispersés : **le plus gros fait
7 278 octets**, de `0x600BC6` à `0x602833` — près de la moitié du code en un bloc. Il commence **exactement**
là où s'arrête le corps de `snd_reset`, que Ghidra a fondu sur 2 726 octets d'un seul tenant.

Et le fait qui tranche :

| cible du répartiteur | | état chez Ghidra |
|---|---|---|
| `0x60120A` | `8n` note off | **ignorée** |
| `0x6008AC` | `9n` note on | **ignorée** |
| `0x6025F2` | `An` le protocole du jeu | **ignorée** |
| `0x601402` | `Bn` control change | **ignorée** |
| `0x601C1A` | `Cn` program change | **ignorée** |
| `0x6012D6` | `En` pitch bend | **ignorée** |

**Les six, sans exception.** Ghidra suit les appels et les enchaînements, mais il ne récupère pas les deux
tables de saut relatives au PC — `jmp (A1)` en `0x60088C` après `lea (0x60088E,PC,D0.w)`, et
`jmp $602730(pc,d2.w)` en `0x60272C`. Tout ce qui n'est atteignable que par là — le note on, le note off, le
contrôle, le programme, le pitch bend, le moteur de copie, la programmation de la fente — lui est invisible.
C'est le trou de 7 278 octets.

### Et ouvrir les deux tables : mesuré, +49 fonctions et la couverture de 45 à 77 %

Fait le 2026-10-25 (`tools/ghidra/FV2SonTablesDeSaut.java`). Les deux tables ont d'abord été **relues et
vérifiées** avant qu'une ligne soit écrite, parce que leurs formes ne sont pas les mêmes :

| | forme | cibles |
|---|---|---|
| `0x60088E`, le répartiteur de statut MIDI | **huit créneaux de 4 octets**, chacun un `bra.w` (le créneau `Fn` porte le `rts` qu'ils partagent) | 7 distinctes, toutes dans le code |
| `0x602730`, les commandes de contrôle (banques `0x00`-`0x0F`) | **seize mots**, chacun un décalage **relatif à la base de la table** — `move.w $602730(pc,d2.w), d2` puis `jmp $602730(pc,d2.w)` | 12 distinctes (`0x602750` quatre fois : la banque inutilisée), toutes dans le code |

Le script désassemble chaque créneau, ajoute une référence `COMPUTED_JUMP` de chaque `jmp` vers chacune de ses
destinations, crée une fonction à chaque cible distincte, puis relance l'analyse. Et il imprime **le compte de
fonctions et les octets couverts**, avant et après — le compte seul peut monter sans que la couverture suive.

```
before: 41 functions,  6806 bytes covered (45 % of the 15136 of code)
   20 distinct targets: 18 functions created, 2 already there
after:  90 functions, 11668 bytes covered (77 % of the 15136 of code)
```

**+49 fonctions, et la couverture de 45 à 77 %.** Les 90 rejoignent les 82 à 95 que le marcheur semé à la main
atteignait : **l'écart est fermé, et la cause était bien les deux tables.** C'est, pour une fois dans cette
journée, une affirmation que j'ai annoncée avant de la mesurer et que la mesure a confirmée.

### Les 23 % restants : deux tables de plus, et un dénominateur à moi qui comptait des données

Repris le même jour. Et d'abord une faute de méthode corrigée : mon calcul de trous supposait qu'un corps de
fonction Ghidra est **contigu**. Il ne l'est pas — 90 fonctions portaient 101 plages, donc onze corps
fragmentés. Les plages réelles demandées, voici ce que les trous étaient.

**Les sauts calculés du programme, énumérés au lieu d'être devinés** — onze, là où j'en avais traité deux :

| | |
|---|---|
| `0x60088C` | `jmp (A1)` — table 1, le répartiteur de statut MIDI |
| `0x60272C` | `jmp (d8,PC,D2.w)` — table 2, les commandes de contrôle |
| **`0x601418`** | `jmp (d8,PC,D2.w)` — **table 3** à `0x60141C`, le sous-répartiteur du `Bn` |
| **`0x602EDC`** | `jmp (d8,PC,D0.w)` — **table 4** à `0x602EE0`, sous la banque `0x00` |
| sept autres | `jmp (An)` — rien de statique ne les résout, et ils ne sont pas touchés |

Et les deux gros trous étaient **exactement** les cibles des tables 3 et 4 : 1 496 octets à `0x6014C0` pour la
première, 956 à `0x602F16` pour la seconde — où l'on trouve des routines d'une ligne terminées par `rts`, la
signature d'une table.

**Comment les dimensionner, puisque leur longueur n'est écrite nulle part :** chaque table **finit où commence
sa cible la plus basse**, donc le compte vaut `(cible la plus basse − base) / 2`. La règle a été **vérifiée
d'abord sur la table 2**, dont les 16 entrées avaient été comptées à la main : elle redonne 16, et le script
s'arrête s'il cesse de le faire plutôt que de dimensionner les autres à l'aveugle. Table 3 : **81 entrées, 36
cibles**. Table 4 : **26 entrées, 23 cibles**.

```
avant :  90 fonctions, 11668 octets couverts (77 % des 15136 de code)
   79 cibles distinctes en tout : 59 fonctions creees, 20 deja la
apres : 168 fonctions, 14484 octets couverts (96 % des 15136 de code)
```

**41 → 90 → 168 fonctions. 45 % → 77 % → 96 %.**

Et ce qui reste, identifié plutôt que laissé en « non expliqué » — 654 octets en 9 trous :

| | octets | |
|---|---|---|
| `0x600000-0x60011F` | 288 | la table des vecteurs |
| `0x60141C-0x6014BD` | 162 | **la table de saut 3 elle-même** |
| `0x602EE0-0x602F13` | 52 | **la table de saut 4 elle-même** |
| `0x602730-0x60274F` | 32 | **la table de saut 2 elle-même** |
| `0x6002D4-0x6002E7` | 20 | les dix mots que `0x600292` recopie dans les registres d'interruption |
| `0x600396`, `0x60103C`, et deux mots | 100 | **non identifié** |

**554 des 654 sont des données** — et mon dénominateur de « 15 136 octets de code » les comptait comme du
code. Reste **100 octets, 0,7 %**, dont deux mots qui suivent immédiatement un `jmp (An)` : l'instruction
d'après, qu'aucun flux n'atteint puisque la destination du saut est inconnue.

Donc les 23 % étaient **deux tables de plus, plus un dénominateur à moi qui comptait des tables de données
comme du code**. Et le vrai plafond de cette méthode est les sept `jmp (An)`.

> Deux affirmations de ce paragraphe sont fausses, et les sections suivantes les corrigent avec les
> mesures : les `96 %` tiennent encore le même dénominateur (15 136 octets « de code » dont 650 sont des
> données, et la borne de fin elle-même était trop basse de deux octets), et **les sept `jmp (An)` se
> résolvent tous les sept** — chacun a un `lea (d8,PC,Dn)` quatre octets au-dessus. Il n'y avait pas de
> plafond.


### Les 4 % restants : encore mon dénominateur, et le pilote qui signe son nom

Repris le 2026-10-25. Les « 4 % non décompilés » n'existaient pas. Voici les neuf trous, un par un, et ce
qu'ils sont vraiment.

**D'abord la borne, parce qu'elle faussait l'arithmétique.** Ma constante disait que le code finit à
`0x603B20`. Le `rts` de cette adresse occupe `0x603B20-0x603B21` : le code va donc à **`0x603B22`**, et il y
a 15 138 octets, pas 15 136. Ce qui suit est une table d'octets croissants (`80 81 … 8b 90 … 9b a0 …`),
nettement des données. La vérification est arithmétique : avec `0x603B22`, couvert + trous = 14 484 + 654 =
15 138 et ça ferme ; avec `0x603B20` ça donnait 15 138 pour 15 136, et **ces deux octets d'écart étaient le
signe que la borne était fausse** — je les avais vus et pas lus.

**Ensuite `0x600100`, et c'est la plus belle des neuf.** Trente-deux octets qui ne sont ni du code ni une
table :

```
Ver3.5.9|98  1/13|model3|by Hiro
```

Le pilote porte **sa version, sa date, sa cible et son auteur**, et ces trente-deux octets finissent
*exactement* là où le vecteur de reset envoie le processeur, `0x600120`. Le nom est coupé par la fin du
champ, pas par la lecture. Donc : le code sonore de *Fighting Vipers 2* est la version 3.5.9 du pilote
« model3 », datée du 13 janvier 1998.

**Les six tables de mots, et ce que chacune est — ou n'est pas.**

| | octets | ce qui est établi |
|---|---|---|
| `0x600000` | 256 | la table des vecteurs du 68000 : 64 longs, des données par construction |
| **`0x6002D4`** | 20 | **dix mots, dix registres** : `lea 0x100418,A0` / `lea 0x6002D4,A1` / `moveq #9,D7` / `move.w (A1)+,(A0)+` / `dbra`, en `0x600288`. Ce sont les registres communs `0x418` à `0x42A` du SCSP |
| `0x600396` | 32 | seize mots lus par `move.w (A1,D0.w)` après `lea 0x600396,A1`, index doublé, écrits dans un tampon `(A0)+`. **Ce qu'ils valent n'est pas établi** |
| `0x60103C` | 64 | **trente-deux mots décroissants** (`0x1A00` → `0x0001`), lus en `0x600CF6` et `0x6038DA`, toujours par `move.w (table,Dn.w), 0x0E(A4)`, l'index étant un compteur rangé en `0x0A(A5)`. **Une courbe de 32 pas ; sa nature n'est pas établie** |
| `0x60141C` | 162 | la table de saut 3 elle-même : 81 mots |
| `0x602730` | 32 | la table de saut 2 elle-même : 16 mots |
| `0x602EE0` | 52 | la table de saut 4 elle-même : 26 mots |

**`0x6002D4` se lit entièrement**, et c'est la configuration des interruptions de la carte :

| registre | valeur | |
|---|---|---|
| `0x418` TIMA/TACTL | `0x00CE` | compte `0xCE`, prédiviseur 0 |
| `0x41A` TIMB/TBCTL | `0x03C6` | compte `0xC6`, prédiviseur 3 |
| `0x41C` TIMC/TCCTL | `0x0100` | compte `0x00`, prédiviseur 1 |
| `0x41E` **SCIEB** | `0x00C8` | bits 3, 6, 7 : **entrée MIDI, timer A, timer B**. Le timer C n'est **pas** armé |
| `0x420` SCIPD | `0x0000` | |
| `0x422` SCIRE | `0x00C8` | les trois mêmes, remis à zéro |
| `0x424` SCILV0 | `0x0088` | |
| `0x426` SCILV1 | `0x0048` | |
| `0x428` SCILV2 | `0x0000` | |
| `0x42A` MCIEB | `0x0020` | bit 5 : le timer A, armé vers l'autre côté |

Les trois `SCILV` donnent le niveau par source, un bit chacun : **MIDI (bit 3) prend `0b011` = 3, timer A
(bit 6) prend `0b010` = 2, timer B (bit 7) prend `0b001` = 1**. Trois sources, trois niveaux — ce qui
recoupe les trois niveaux que la table des vecteurs sert. C'est une **lecture**, pas une mesure sur machine :
elle vient de ces dix mots et de la disposition des registres du SCSP. Ce qu'elle prédit et qui reste à
vérifier sur le portage : trois sources d'interruption et trois seulement, à ces trois niveaux.

> **Les noms des timers dans ce tableau ont été corrigés le même jour.** La première version disait « timer B
> et timer C armés, le timer A non », et l'inverse est vrai. Ce qui l'a révélé : `snd_irq2` recharge `0x418`
> — TIMA — et acquitte le bit 6. Les bits viennent maintenant de l'implémentation du portage
> (`timerA_cb()` pose `0x40`, `timerB_cb()` `0x80`, `timerC_cb()` `0x100`) et non d'un souvenir de table.
> Les **niveaux** 3, 2, 1 étaient justes dès le départ ; seuls les noms étaient permutés. Voir 5 bis ter.

**Et les quatre derniers octets, les seuls que Ghidra manquait vraiment.** Deux mots, `24 d9` chacun, en
`0x601F3C` et `0x6023E2`. Je les avais comptés parmi les **sept `jmp (An)` que « rien de statique ne
résout »**. C'était faux : un `lea` les nomme quatre octets plus haut.

```
00601F2E  move.w  #$200, d0
00601F32  sub.w   d5, d0
00601F34  lsr.w   #$1, d0
00601F36  lea.l   $601f3c(pc, d0.w), a0
00601F3A  jmp     (a0)
00601F3C  move.l  (a1)+, (a2)+        <- les deux octets
00601F3E  move.l  (a1)+, (a2)+        ... et 126 fois encore
```

C'est un **dispositif de Duff** : 128 `move.l (A1)+,(A2)+` d'affilée, et l'entrée se calcule —
`D0 = (0x200 − D5) >> 1` — pour que la copie commence d'autant plus loin dans le bloc qu'il reste moins à
copier. 512 octets au plus, sans test de boucle. Les deux blocs appartiennent à `snd_reset`, qui passe de
2 728 à 2 730 octets de corps. **Il reste donc cinq `jmp (An)` insolubles, pas sept** : `0x600308`,
`0x602042`, `0x6024E8`, `0x603494`, `0x60376A`.

**Le compte final, et son témoin.** `tools/ghidra/FV2SonDonnees.java` déclare les sept blocs de données,
rattache les quatre octets de code, puis **recompte lui-même** ce qui reste hors de tout corps de fonction
*et* hors de toute donnée déclarée :

```
168 fonctions, 14488 octets dans un corps, 650 octets de donnees declarees, sur 15138 de 600000-603B22
code reel = 15138 - 650 = 14488 octets, dont 14488 dans un corps = 100.00 %
RESTE HORS DE TOUT : 0 octets
```

Un script qui compte son propre travail n'est pas un témoin, donc le compte a été refait par **l'autre
chemin** : `FV2SonListe.java`, que ce travail n'a pas touché, redonne les plages, et le calcul de trous
extérieur en tire **14 488 octets couverts et 7 trous pour 650 octets** — exactement les sept blocs de
données, les deux trous de deux octets ayant disparu. 14 488 + 650 = 15 138. Les deux chemins ne partagent
rien et donnent le même nombre.

**Et ce que « 100 % » veut dire, pour qu'on ne le lise pas plus grand qu'il n'est :** chaque octet de code
est dans le corps d'une fonction, et chaque octet de données est déclaré et commenté. Ce n'est **pas** dire
que le programme est compris. Cinquante-neuf des 168 fonctions portent encore le nom que Ghidra leur a
donné, personne ne les a lues, et deux des tables de données ci-dessus n'ont pas de sens établi. La
couverture mesure la surface atteinte, pas la lecture faite.

**41 → 90 → 168 fonctions. Et la couverture du code, le dénominateur une fois honnête : 100 %.** Les 4 %
n'étaient pas du code non décompilé ; c'étaient une table de vecteurs, une signature d'auteur, trois tables
de saut, trois tables de valeurs — et une borne de fin fausse de deux octets. Ce qui reste ouvert n'est pas
une surface non lue, c'est **le sens** de deux tables (`0x600396`, `0x60103C`) et cinq sauts indirects dont
la destination ne se calcule pas sans exécuter le programme.

### Et les cinq `jmp (An)` restants : zéro. La phrase était fausse sur les sept

La leçon des deux premiers s'applique immédiatement aux cinq autres, et il suffisait de regarder
**l'instruction d'avant**. Les cinq, mesurés le même jour :

| le `jmp` | ce qui est quatre octets au-dessus | ce que c'est |
|---|---|---|
| `0x600308` | `lea 0x60030A(pc,D1.w),A1`, `D1 = <v> & 0x0E` | table de branchement, **8 créneaux de 2** (`bra.b`) |
| `0x603494` | `lea 0x603496(pc,D0.w),A0`, `D0 = (D1>>2) & 0x1C` | table de branchement, **8 créneaux de 4** (`bra.w`) |
| `0x60376A` | `lea 0x60376C(pc,D0.w),A4`, `D0 = (D0 & 7) << 2` | table de branchement, **8 créneaux de 4** (`bra.w`) |
| `0x602042` | `lea 0x602044(pc,D5.w),A0` | bloc déroulé, **128 × `clr.l (A2)+`** |
| `0x6024E8` | `lea 0x6024EA(pc,D5.w),A0` | bloc déroulé, **128 × `clr.l (A2)+`** |

**Le masque de l'index donne le compte**, et c'est mieux que la règle d'auto-dimensionnement parce que c'est
écrit dans le code : `& 0x0E` ne laisse passer que huit valeurs paires, `& 0x1C` et `(&7)<<2` huit multiples
de quatre.

Deux surprises dans les créneaux, et elles auraient fait mentir une règle qui n'aurait compté que les
branchements :

- à `0x60030A`, **sept `bra.b` seulement**. Le huitième créneau, `0x600318`, est déjà du code ordinaire
  (`movea.l (0xC10).w,A2`) : le gestionnaire du dernier cas est posé là, au lieu d'être derrière un
  branchement.
- à `0x60376C`, **sept `bra.w` seulement**. Le huitième, `0x603788`, est `move.w #$2000,SR ; rts` en ligne —
  et le créneau 6 branche dessus. **La table 1 fait exactement la même chose** avec le `rts` que ses
  créneaux partagent.

Huit destinations chacune donc, sept branchements et un gestionnaire en ligne. Le script vérifie l'opcode de
chaque créneau et **nomme** ceux qui ne sont pas des `bra` au lieu de supposer ; il a redonné de lui-même les
deux adresses que la lecture avait trouvées.

**Et les deux blocs déroulés effacent, ils ne copient pas** — `clr.l (A2)+`, pas `move.l (A1)+,(A2)+`. Ils
vont par paire avec les deux copies de `0x601F3C` et `0x6023E2`, et **l'arithmétique de la paire ferme** :

| | index | instructions exécutées | effet |
|---|---|---|---|
| la copie, `0x601F36` | `D0 = (0x200 − D5) >> 1` | `128 − (0x200−D5)/4 = D5/4` | **`D5` octets copiés** |
| l'effacement, `0x60203E` | `D5 >> 1` | `128 − D5/4` | **`512 − D5` octets effacés** |

Une destination de 512 octets : `D5` octets depuis `(A1)`, des zéros pour le reste. Les quatre blocs font
**128 instructions, `0x100` octets, 512 octets traités** — mesuré en comptant les motifs identiques, pas
déduit. Le `0x200` de `move.w #$200,D0` est ce tampon, et il apparaît deux fois. **Ce que sont ces 512
octets n'est pas établi.**

`tools/ghidra/FV2SonSautsIndirects.java` pose les 26 références de saut calculé. Il **ne crée aucune
fonction** : la couverture est déjà de 100 %, ces cibles sont dans des corps existants, et couper une
fonction en deux sur une cible de branchement rendrait le listing pire. Le contrôle après coup — toujours
`FV2SonListe` — redonne 168 fonctions, 14 488 octets, les sept blocs de données : rien n'a régressé.

**Il ne reste aucun saut non résolu dans ce programme.** Et ce qui a tenu l'erreur si longtemps est simple :
j'ai lu `jmp (An)` comme l'instruction entière, alors que la réponse était quatre octets plus haut, dans
l'instruction que je n'avais pas regardée.


## 5 bis quaterdecies. Le reste du programme, lu

Les soixante dernières fonctions, lues le 2026-10-25 pour fermer la lecture. Elles se rangent en six
familles, et plusieurs referment des boucles ouvertes ailleurs dans ce document.

### Ce que le reset installe en RAM

| | |
|---|---|
| `0x6003D0` | **copie les 64 longs de `0x600000` vers `0x0`** — la table des vecteurs, de la ROM vers la RAM. C'est ainsi que le 68000 obtient ses vecteurs, puisqu'à l'adresse 0 la carte met de la RAM et la ROM est à `0x600000`. La table de `0x600000` est le modèle, celle de `0x000` est la vivante |
| `0x600592` | copie `[0x608030]` octets de `[0x60802C]` vers `[0x608038]` : **la table des instruments**, de la ROM vers la RAM. D'où le pointeur que `0x6014C0` et CC 1 suivent |
| `0x600556` | copie un bloc de `[0x608000]` vers `0x3000` : **la table des fiches d'échantillon**, si `[0x608000]` n'est pas négatif. L'origine du `0x3000` du §5 bis sexies |
| `0x600524` | `lea 0x603FE2, a0` puis 512 tours de deux `move.l` vers `0x2000`, et de nouveau vers `0x202000` : **4 096 octets de ROM dans chaque RAM sonore**, à `0x2000-0x2FFF`. Contenu non identifié |
| `0x6002A2` | les 32 fiches de voix : `+0x15` sur seize octets par pas de 8, `+0x0B` et `+0x0D` à `0xFF`, `+0` à `0x10` |
| `0x6003B6` | efface l'octet `0x17` des registres de **18** fentes (`moveq #$11`). Pourquoi 18 et non 32 n'est pas expliqué |

### Les six créneaux de la table 1

`0x600892`, `0x600896`, `0x60089A`, `0x60089E`, `0x6008A2`, `0x6008A6` : quatre octets chacun, un `bra.w`
chacun. Ce sont **les créneaux de la table 1** eux-mêmes, que Ghidra compte comme des fonctions parce que le
`jmp (A1)` de `0x60088C` y saute. Le septième créneau est le `rts` partagé de `0x6008AA` (§5 bis quinquies).

### L'extinction d'une note

| | |
|---|---|
| `0x60128E` | **note off**. Si le bit 6 de l'objet — la pédale de maintien — est posé, elle ne coupe rien : `bset #$1, $4(a4)`, la voix est marquée « à relâcher ». Sinon `move.b #$10, $0(a5)`, bits 0 et 5 du `+4`, et le `+0x0E`/`+0x10` de la fiche servent de compte à rebours |
| `0x601370`, `0x601388` | un balayage des 32 fiches sur le canal `D1`, et le corps appelé pour chacune, qui lit `[0xED4]` et le `+7` de la fiche |
| `0x601BFA` | balaie les 32 fiches et arrête **toutes celles du canal `D5`**, puis `andi.b #$ef, $c01.w`. C'est le « toutes notes coupées » qu'appelle CC `0x50` |
| `0x602F68`, `0x602F84` | une **quatrième granularité d'arrêt** (§5 bis septies en donnait trois) : les voix dont le `+4` a le **bit 2** posé, sur les deux puces — le même bit que le prédicat du voleur de voix (§5 bis decies) |

### Les onze éditeurs de la fiche d'instrument

`0x6014DE` trouve l'objet de canal du canal courant et le laisse dans `A0`, en **dépilant l'adresse de
retour** s'il n'y en a pas — une sortie d'urgence qui annule l'appelant. `0x6014C0` suit l'index `+2` de cet
objet dans la table des instruments de `[0x608038]`, vérifie le mot magique `0x8000`, et laisse la fiche
dans `A1`. Onze gestionnaires s'en servent, et le patron de partage de code est remarquable :

| | écrit |
|---|---|
| `0x601582` | `$a(a1)` bits 7-4 |
| `0x601598` | `$a(a1)` bit 3, posé ou effacé selon la valeur |
| `0x6015B8` / `0x6015D6` | `$b(a1)` bit 7 / ses bits 6-0 — un drapeau et une valeur |
| `0x6015EC` / `0x6015F6` | les mêmes, avec `D2 = 5` : donc `$10(a1)` |
| `0x601600` / `0x60160A` | les mêmes, avec `D2 = 6` : donc `$11(a1)` |
| `0x601572`, `0x60157A` | `D2 = 2`, et ils rejoignent les corps de CC `0x41` et `0x42` |

Trois paires (drapeau, valeur) sur `+0x0B`, `+0x10`, `+0x11`, chacune écrite par deux gestionnaires qui
partagent le même corps à un registre d'index près. Puis `0x601526` réapplique, en balayant les 32 fiches
des deux puces par `0x60153A`.

### `0x60153A`, et le tampon FM que ma sonde n'avait jamais vu écrire

```
0060154A  move.b $a(a1), $e(a5)        ; le +0x0A de l'instrument -> registre 0x0E de la fente
00601550  move.l a5, d4 ; lsr.w #$4, d4 ; addi.l #$600, d4
0060155C  move.w $c(a1), $0(a5)        ; -> 0x600 + fente x 2
00601562  move.w $e(a1), $40(a5)       ; -> 0x640 + fente x 2
```

Le calcul est à regarder de près : `lsr.w #$4` ne décale que **le mot bas**. Avec `A5 = 0x100000 + fente ×
0x20`, le mot bas vaut `fente × 0x20`, décalé de 4 il donne `fente × 2`, et le mot haut reste `0x0010`. Plus
`0x600`, l'adresse est donc `0x100600 + fente × 2` — **le tampon FM**.

**Et c'est la réconciliation d'une mesure du §4.** Ma sonde avait cherché les écritures en `+0x600` et en
avait trouvé **zéro** sur 4 200 images de *Fighting Vipers 2* et 2 400 de *Virtua Fighter 3*, avec un témoin
positif. Voici le code qui en ferait. Il n'est atteint que par `0x601526`, donc par ces onze contrôleurs-là.
La conclusion n'est pas que la sonde s'est trompée : c'est que **la fonctionnalité existe et que ces scènes
ne l'emploient pas**. Les deux faits tiennent ensemble.

### Les groupes de voix de `0x380`

La table de saut de `0x60376C` — celle du `jmp (A4)` de `0x60376A`, ouverte à la section « Et les cinq
`jmp (An)` restants » — distribue six gestionnaires qui
travaillent tous sur une structure `A0` de `0x10` octets : `+0` un masque de huit bits disant quels membres
sont actifs, `+8` à `+0xF` **les numéros de fente des huit membres** (`move.b $8(a0,d6.w), d5 / andi.w #$1f /
lsl.w #$5` donne `fente × 0x20`). Ce sont les **huit entrées de `0x380-0x3FF`** que le §2 avait vues sans
savoir à quoi elles servaient : un **groupe de voix**, un instrument à plusieurs fentes.

| | |
|---|---|
| `0x6039A6` | éteint **un** membre : `0x10` dans son registre `0x00`, `+4` bit 0 de sa fiche, le bit du membre effacé, `+0x0E` recopié dans `+0x10` |
| `0x6039F4` | lit un panoramique du flux, le convertit par `0x601A98` — le convertisseur `DIPAN` du §5 quater — le range en `+2` et l'applique aux huit membres |
| `0x603A38` | un niveau borné à 7, rangé dans `+3` sous masque `0x78`, appliqué aux huit |
| `0x603A82` | **le même corps**, rejoint en `0x603A46` avec `D7 = 7` au lieu de `0x78` : la valeur `(v & 0xF) << 3` va donc dans les bits 6-3 du même octet. Deux champs, un corps, un masque dans un registre |
| `0x603A90` | **charge une définition de groupe** : index borné par le mot `+0` de `[0x60803C]`, pointeur long à `+2 + index × 4`, et les octets copiés dans `+1`, `+3`, `+4` |

`[0x60803C]` est donc la racine des définitions de groupe — et c'est le même long que `0x603198` teste pour
décider si la fonctionnalité existe (§5 bis septies).

### Les commandes de contrôle restantes

| | |
|---|---|
| `0x602A60`, `0x602B20` | **choisissent la puce** : `D5` nul → `[0xC10] = 0`, sinon `0x200000`. C'est l'origine du sélecteur que tout le pilote consulte |
| `0x602E4C`, `0x602E9E` | appellent `0x602E58` et `0x602EB0` une fois par puce |
| `0x602E58` | un second lecteur du bloc de préréglages DSP (`0x602CE4`), qui exige `$10(a1) == 1` et compare `$11(a1)` au canal : une variante par canal |
| `0x602EB0` | gardée par `0xEFB` comme CC 10, elle écrit l'octet `0x17` du registre de la fente d'indice `canal` (`(D5 & 0xF) << 5` ajouté à la base des registres), alimenté par le convertisseur `0x601A98`. **Quel champ portent respectivement les octets `0x16` et `0x17` n'est pas tranché** : CC 10 écrit `0x16`, ceci écrit `0x17`, et la disposition que j'ai des registres n'explique pas les deux |
| `0x60279A`, `0x602834` | une paire qui pose `[0xC19]`/`[0xC1A]` et met l'un à l'échelle de l'autre |
| `0x602AD0` | `[0xEF1 + puce] = 0xFF` : le préréglage DSP remis à « aucun » |
| `0x602F38` / `0x602F40` | `[0xEDC]` à `0xFF` / à zéro. Et `0xEDC` est ce que `0x60344A` teste pour **exécuter l'événement suivant sans attendre le compte à rebours** : donc une avance rapide des pistes |
| `0x602F46` / `0x602F62` | transposent de `+1` / `−1` le `+4` des objets dont le bit 0 est clair — même corps, `D1` valant `1` ou `0xFF` |
| `0x602F9A` | `[0xEFE] = 0x8000` |
| `0x603274` / `0x60327C` | `[0xC3F]` à 1 / à zéro |
| `0x602F14`, `0x602F34`, `0x602F36`, `0x602F66` | des `rts` nus : des entrées de la table 4 qui ne font **rien** |
| `0x601954` | la seconde moitié de `0x60193A`, que Ghidra compte à part parce que la première y tombe en passant |

### Deux routines en double exemplaire

`0x603718` et `0x60361E` sont **le même code** : `btst #$5, $0(a2)` sur les drapeaux de la piste, puis
l'attente de `MOFULL` (bit 4 de l'octet `0x404`) avant d'écrire l'octet dans `MOBUF` à `0x407`. Deux copies
de la recopie MIDI, dans deux chemins de lecture différents (§5 bis quater).

Et `0x603638` est un **second lecteur de quantité à longueur variable** : `move.b (a3)+, d1 / bpl -> fini /
andi.b #$7f / or / lsl.l #$7 / recommence`. Celui-là n'est **pas borné à deux octets** comme celui des
delta-times (`0x603664`), il accumule autant d'octets que le flux en donne. Deux lecteurs de VLQ, deux
limites.


## 5 bis duodecies. `0x60048C` : l'initialisation qui pose les trois sentinelles

82 octets, appelée une seule fois, depuis `0x60020C` dans `snd_reset`. Elle est courte et chaque ligne
referme une boucle ouverte ailleurs.

```
0060048C  lea.l $400.w, a4 ; move.l #$100000, d0 ; bsr $6004de     ; la puce 0
00600498  lea.l $200400.l, a4 ; move.l #$300000, d0 ; bsr $6004de  ; la puce 1
006004DE  moveq #$1f, d7 ; move.l d0, (a4) ; lea.l $20(a4), a4 ; addi.w #$20, d0 ; dbra
```

**Voici la boucle que `group_of()` suppose.** Le premier long de chaque fiche reçoit l'adresse des registres
de sa fente : `0x100000 + fente × 0x20` pour la première puce, `0x300000 + fente × 0x20` pour la seconde.
C'est exactement ce que le portage teste (`r[0] != (n ? 0x30 : 0x10)`), et le §2 le donnait comme confirmé
« par une boucle au reset » — la voici nommée.

Puis les sentinelles, et les trois correspondent à un test lu dans une autre fonction :

| | posé à | et c'est le sentinelle testé par |
|---|---|---|
| `[0xEF9]` | `0x77` | les bits que `0x60288A` compose dans le registre de banque (§5 bis sexies) |
| `[0xC18]`, `[0x200C18]` | `0x7F` | le `cmpi.b #$7f, d0 / beq -> sortie` de `0x60288A` : « aucun jeu d'échantillons chargé » |
| `[0xEF1]`, `[0x200EF1]` | `0xFF` | le `bmi -> abandon` de `0x602CE4` : négatif, donc « aucun préréglage DSP » |

Et une ligne qui reste obscure : `moveq #$ff, d1 / move.b $60807d.l, d0 / lsr.l d0, d1` puis
`[0xED8] = [0x200ED8] = d1`. `[0x60807D]` est le quota de voix, qui vaut 32 (§5 bis decies), et décaler un
long de 32 rangs donne **zéro**. Le masque est donc nul sur cette ROM, et il ne serait non nul qu'avec un
quota inférieur à 8. À quoi `0xED8` sert n'est **pas** lu.

## 5 bis terdecies. `0x603282` : une recherche à deux niveaux, par banque et index

80 octets, atteinte seulement par une table de saut. Elle est gardée deux fois, puis fait deux
indirections :

```
00603282  tst.b $c1c.w ; bmi -> sortie          ; un mode, negatif = inactif
00603288  cmp.b $c1b.w, d5 ; bne -> sortie      ; D5, le canal MIDI, doit valoir [0xC1B]
0060328E  movea.l $60801c.l, a4 ; movea.l a4, a2
00603298  move.b $c1c.w, d0 ; subi.w #$10, d0 ; lsl.w #$1, d0
006032A2  move.w $2(a4, d0.w), d0
006032A6  cmpi.w #$ffff, d0 ; beq -> 602644      ; absent
006032AE  adda.l d0, a4
006032B2  move.b $c1d.w, d0
006032B6  cmp.w (a4), d0 ; bgt -> 602644         ; borne : le mot +0 de la sous-table
006032BC  lsl.w #$1, d0 ; move.w $2(a4, d0.w), d1 ; adda.l d1, a2
```

Donc : **`[0xC1B]` un canal, `[0xC1C]` un premier index décalé de `0x10`, `[0xC1D]` un second**, et deux
tables de mots en cascade depuis `[0x60801C]`, chacune bornée par son propre mot `+0` — la même convention
que les quatre autres tables de ce pilote — avec `0xFFFF` pour « pas d'entrée ».

Une recherche à deux niveaux par banque et index, gardée sur un canal : **ça ressemble beaucoup au protocole
`A0 <banque> <index>`** que la liste de lecture cherchait. Ce n'est pas établi — ce qui l'est, c'est la forme
de la recherche et les trois globales qui l'indexent.


## 5 bis undecies. Les 35 changements de contrôle, et les deux structures qu'ils éditent

Le §5 quater avait lu CC 1, 7 et 10. Voici la famille entière, lue le 2026-10-25, et elle se range en **deux
groupes selon la structure visée** — ce qui est le fait d'architecture de cette lecture.

### Groupe 1 : le registre LFO, dans l'objet de canal

Le mot `+8` d'un objet de canal est l'image du registre `0x12` d'une fente, et cinq contrôleurs en éditent
chacun un champ, puis appellent `0x60193A` pour l'appliquer :

| CC | code | masque | champ du registre `0x12` |
|---|---|---|---|
| `0x01` | `0x601882` | `+9 & 0xE0` | **`PLFOS`** (bits 7-5), profondeur du LFO de hauteur |
| `0x02` | `0x601904` | `+9 & 0xF8`, valeur `>>4` | **`ALFOS`** (bits 2-0), profondeur du LFO d'amplitude |
| `0x15` | `0x60169C` | `+9 & 0xE7`, valeur `<<3`, rejetée si ≥ 4 | **`ALFOWS`** (bits 4-3), forme d'onde d'amplitude |
| `0x18` | `0x6016CE` | `+8 & 0x83`, valeur `<<2`, bornée à `0x1F` | **`LFOF`** (bits 14-10), fréquence du LFO |
| `0x14` | `0x60166C` → `0x601684` | `+8 & 0xFC`, rejeté si ≥ 3 | **`PLFOWS`** (bits 9-8), forme d'onde de hauteur |

> **Correction du même jour.** J'avais d'abord écrit que `CC 0x12`, `0x13` et `0x14` allaient tous les trois
> à `0x601684` et posaient `PLFOWS`. C'était faux, et la faute est instructive : mon extraction automatique
> lisait vingt instructions depuis chaque gestionnaire et **franchissait le `bra`** qui termine les deux
> premiers, attribuant à leur entrée le corps de la troisième. Seul `CC 0x14` écrit `PLFOWS` ; `0x12` et
> `0x13` sautent vers `0x601632`, un corps générique.

Et `CC 0x37` (`0x60186E`) **retombe dans le code de CC 1** après un prologue : une variante d'entrée, pas un
gestionnaire à part.

Trois contrôleurs passent par `0x601632`, un corps de huit instructions qui écrit simplement l'octet `D0` à
un décalage fixe de l'objet, le décalage étant passé dans `D3` :

| CC | code | décalage | |
|---|---|---|---|
| `0x11` | `0x60162A` | `+4`, valeur `−0x40` | une seconde transposition, signée par le biais |
| `0x12` | `0x601652` | `+5`, valeur `−0x40` | **la transposition** — exactement le `+5` que `0x601390` ajoute au compte de demi-tons (§5 bis nonies) |
| `0x13` | `0x60165C` | `+7`, valeur bornée à `0x0C` puis `<<4` | non identifié |

Le `+5` referme une boucle : le §5 bis nonies l'avait vu **lu** comme transposition par le calcul de
hauteur, et voici le contrôleur qui l'**écrit**.

Deux autres éditent les octets de mixage du même objet :

| CC | code | | |
|---|---|---|---|
| `0x0A` | `0x601A08` | `+6 & 0xE0`, 5 bits bas | **`DIPAN`** (§5 quater) |
| `0x19` | `0x601702` | `+6 & 0x1F`, 3 bits hauts | **`DISDL`**, le niveau d'envoi direct |
| `0x2A` | `0x601AB0` | `+0x0C & 0x78`, valeur bornée à 7 | **`IMXL`** (bits 2-0), le niveau d'envoi vers le DSP |
| `0x2B` | `0x601ADE` | `+0x0C & 0x07`, valeur `<<3` puis `& 0x78` | **`ISEL`** (bits 6-3), **quelle entrée du DSP** la voix alimente |

`+6` porte donc `DISDL` et `DIPAN` dans un octet, comme le registre `0x16`.

Et `+0x0C` est l'image de l'octet `0x15` d'une fente, soit l'octet bas du registre `0x14` — `0x600C36` le
recopie tel quel à l'allumage d'une note : `move.b $c(a0), $15(a5)`.

> **J'avais d'abord écrit que les deux masques « se chevauchent sur le bit 7 » et que le partage n'était pas
> établi.** C'était une faute de lecture : j'avais regardé les masques d'écriture sans regarder **à quoi
> chaque gestionnaire borne `D0` avant d'écrire**. CC `0x2A` fait `cmpi.b #$7, d0 / ble / moveq #$7, d0`,
> donc `D0 ≤ 7` et le `or` ne peut pas atteindre le bit 7 ; CC `0x2B` fait `lsl.b #$3, d0 / andi.b #$78, d0`,
> donc il ne contribue que les bits 6-3. **Il n'y a aucun chevauchement**, et le bit 7 est simplement effacé
> par le `andi #$78` de CC `0x2A` et jamais reposé.

Le découpage est donc **3 bits + 4 bits + un bit inutilisé**, et six faits s'alignent dessus : les deux
largeurs, les deux positions, le bit de poids fort inutilisé, et la recopie de l'octet entier dans le
registre `0x15`. La disposition vient de l'implémentation du portage plutôt que d'un souvenir de table
(`src/sound/scsp.cpp` : `ISEL` vaut `(data[0xA] >> 3) & 0xF`, `IMXL` vaut `data[0xA] & 7`).

**Ce que ça veut dire, et ce n'est pas un détail :** `CC 0x2B` choisit **quelle entrée du DSP d'effets** une
voix alimente, et `CC 0x2A` **avec quel niveau**. C'est le routage par voix vers la réverbération, commandé
au contrôleur MIDI — l'autre bout de tout ce que les §4 sexies et 5 bis bis racontent du DSP.

### Groupe 2 : l'enveloppe ADSR, dans un gabarit pointé par `+0x1E`

`0x601762` est la clé de ce groupe, et elle tient en cinq instructions :

```
00601764  move.w $1e(a0), d1 ; beq -> 601104    ; le +0x1E de l'objet : un POINTEUR de 16 bits
0060176C  movea.l d1, a1 ; rts
```

Seize bits, donc une adresse **dans la RAM sonore**, pas dans l'espace des registres : c'est un **gabarit en
RAM**. Il est posé en `0x6009A2` et `0x600B2A`, et le calcul qui précède y dit sa taille :
`sub.b d0, d3 / mulu.w #$c, d3 / adda.l d3, a2` — une fiche de **`0x0C` octets**, choisie par un index borné.
Ce n'est donc **pas** un miroir des `0x20` octets de registres d'une fente, mais une forme compressée, et
seuls quelques champs y tiennent.

Les contrôleurs `0x30` à `0x34` l'éditent, et les cinq champs tombent exactement sur la disposition des
registres `0x08` et `0x0A` d'une fente, **avec un décalage de −2** (`a1+6` ↔ registre `0x08`) :

| CC | code | écrit | champ SCSP |
|---|---|---|---|
| `0x30` | `0x601732` | `$7(a1) & 0xE0`, 5 bits | **`AR`** — registre `0x08`, bits 4-0 |
| `0x32` | `0x6017A2` | `$6(a1) & 0xF83F`, valeur `<<6` | **`D1R`** — registre `0x08`, bits 10-6 |
| `0x33` | `0x6017D4` | `$6(a1) & 0x07`, valeur `<<3` | **`D2R`** — registre `0x08`, bits 15-11 |
| `0x31` | `0x601770` | `$8(a1) & 0x7C1F`, valeur `<<5` | **`DL`** — registre `0x0A`, bits 9-5 |
| `0x34` | `0x601808` | `$9(a1) & 0xE0`, 5 bits | **`RR`** — registre `0x0A`, bits 4-0 |

**Cinq champs, cinq correspondances exactes, un seul décalage.** C'est ce qui donne confiance : une
hypothèse de décalage qui tomberait juste par hasard sur cinq masques de cinq bits à quatre positions
différentes serait une coïncidence coûteuse. Et aucun de ces cinq n'appelle `0x60193A` — ils écrivent le
gabarit, qui sera appliqué au prochain allumage de note.

`CC 0x36` (`0x60183A`) écrit `$2(a1)` en octet, masqué à `0x3F`, soit — au même décalage — l'octet haut du
registre `0x04`, `LSA`. **Non confirmé** : un seul champ ne vaut pas les cinq précédents.

Le `+0x1E` est posé en `0x6009A2` et `0x600B2A`, tous deux dans la sélection d'un instrument. Le gabarit est
donc celui de l'instrument courant.

### Les trois qui font autre chose

**`CC 0x40`, la pédale de maintien** (`0x601B0A`) : `bset #$6, $0(a0)` à l'appui, `bclr #$6` au relâchement,
puis `bsr 0x601B50`. Donc **le bit 6 des drapeaux d'un objet de canal est la pédale de maintien** — ce qui
explique enfin le `andi.b #$bf` que `0x602FA0` et `0x603080` appliquent aux vingt objets à l'arrêt général
(§5 bis septies) : ils **relâchent la pédale partout**. La question posée là est répondue ici.

**`CC 0x50`** (`0x601BA6`) : `andi.b #$bf, $0(a0)` et `andi.w #$7f18, $8(a0)` — la pédale relâchée et le même
masque `0x7F18` que `0x603172`, qui efface `PLFOS`, `ALFOS` et `LFORE`. C'est une **remise à zéro des
contrôleurs**.

**`CC 0x41`, `0x42`, `0x4D`** (`0x6014FE`, `0x601510`, `0x601614`) passent tous par `0x6014DE` puis
`0x6014C0` — deux routines de lecture d'arguments — avant d'agir. `0x601614` écrit `$5(a1)` ; quelle
structure `A1` désigne là n'est **pas** établi, et ce n'est **pas** le `+5` d'une fiche de voix, dont
l'écriture est en `0x6010F2` (§5 quater).

Sur les 81 entrées de la table 3, **46 pointent sur le gestionnaire « rien » de `0x6014BE`**, et `CC 0x35`
en fait partie — un trou au milieu du bloc `0x30-0x34`, `0x36`, `0x37`.


## 5 bis decies. `0x601144` et `0x60119E` : l'allocation d'une voix, et le vol du plus ancien

Deux fonctions à lire ensemble, 90 et 108 octets, parce que la seconde est la branche de secours de la
première.

### L'allocateur : un tourniquet, et deux portes avant lui

```
00601144  tst.b $ed6.w ; bmi -> 60115c              ; drapeaux de l'echantillon : bit 7 saute le quota
0060114A  move.b $ee3(a3), d0 ; add.b $ee2(a3), d0  ; deux compteurs
00601152  cmp.b $60807d.l, d0
00601158  bge.w $60119e                             ; AU QUOTA -> on vole
0060115C  move.b $ee0(a3), d0 ; add.b $ee1(a3), d0  ; les deux compteurs de categorie
00601164  cmpi.b #$20, d0 ; beq -> 6011a0           ; 32 occupees -> on vole, MAIS PAS AU MEME ENDROIT
0060116A  movea.l $ee4(a3), a4                      ; le curseur du tourniquet
00601170  cmpi.w #$800, d0 ; bne -> 601180 ; lea.l $400.w, a4   ; bouclage en fin de table
00601180  tst.b $4(a4) ; beq -> 601192               ; une fiche libre
00601186  lea.l $20(a4), a4 ; dbra d7, $601170
0060118E  bra.w $601106                             ; aucune libre alors que les compteurs disent autre chose
00601192  move.l a4, d0 ; addi.w #$20, d0 ; move.l d0, $ee4(a3)  ; le curseur avance
```

Donc : **recherche en tourniquet** d'une fiche dont le `+4` est nul, le curseur gardé en `0xEE4`, avec
bouclage de `0x800` à `0x400`. Et `[0x60807D]` est un **quota en ROM** sur le nombre de voix simultanées.
**Il vaut `0x20` dans cette ROM, soit 32** — autant que de fentes : le quota est donc réglé grand ouvert, et
sa porte ne s'ouvre qu'au moment où tout est occupé de toute façon. C'est un réglage que le pilote prévoit,
pas une contrainte que ce jeu subit.

### Le voleur : le plus ancien, par deux viviers

`0x60119E` ne regarde que les fiches **occupées** (`tst.b $4(a4) ; beq -> suivante`) : ce n'est pas une
allocation, c'est un vol. Elle garde **deux** candidats en parallèle, chacun le minimum de `+0xB` dans son
vivier :

| candidat | vivier |
|---|---|
| `A1` | les fiches passant le test : `D0 ≠ 0`, ou `0xED6` bit 7, ou **`+4` bit 2** |
| `A3` | les fiches dont le **`+4` bit 5** est posé |

et la sortie préfère `A3`, sinon `A1`, sinon rend `A4 = 0` sans rien arrêter — le `rts` de `0x601200`. Dans
les deux premiers cas elle **arrête la voix choisie** (`bsr 0x6006DC`) avant de rendre la fiche.

`+0xB` est le numéro de série posé à l'allumage depuis `0xEE0` ou `0xEE1` (§5 bis septies), donc chercher
son minimum, c'est chercher **la voix la plus ancienne**.

### Le point d'entrée encode l'urgence

Le détail qui m'a arrêté : la porte du quota branche sur `0x60119E`, mais celle des 32 occupées branche sur
`0x6011A0` — **une instruction plus loin**, sautant le `clr.l d0`. Or `D0` est exactement ce que le voleur
teste pour décider s'il applique son prédicat :

| entrée | `D0` | effet |
|---|---|---|
| `0x60119E`, depuis le quota | mis à zéro | le prédicat s'applique : seules les fiches marquées `+4` bit 2 sont volables |
| `0x6011A0`, depuis les 32 occupées | vaut `0x20` | **tout est volable** |

Donc « au quota » vole prudemment, « plus une seule fente » vole n'importe quoi. Deux politiques dans une
fonction, choisies par l'adresse d'entrée.

### Et une conséquence du marquage des catégories

Le `+0xB` d'une voix de **catégorie A** a son bit 7 **forcé** par un `ori.b #$80` (§5 bis septies) ; celui
d'une catégorie B ne l'a pas. Donc toute valeur B est `< 0x80` et toute valeur A est `≥ 0x80`, et une
recherche de minimum **prendra toujours une voix B avant une voix A** à l'intérieur d'un même vivier. Le
marquage n'est pas qu'une étiquette : il installe une **priorité**, la catégorie A étant protégée.

C'est une déduction à partir de deux faits mesurés — le `ori` et la recherche de minimum — et elle vaut tant
que les deux compteurs ne débordent pas l'un dans l'autre. Laquelle des deux catégories est la musique
reste, elle, non établie.


## 5 bis nonies. `0x601390` : le numéro de note devient `OCT`/`FNS`, et la table le prouve

114 octets, un seul appelant — `0x600CA4`, dans l'allumage d'une note. Elle convertit une hauteur en
registre de fente, et son unité se lit dans le code.

```
00601390  d4 = d5 >> 8 ; d5 &= 0xFF            ; d5 : demi-tons en haut, offset fin en bas
0060139C  d3 = $c(a4) ; add.b $14(a4), d3      ; plus DEUX transpositions de la fiche
006013A4  add.b d5, d3 ; ext.w ; ext.l ; lsl.l #$1, d3
006013AC  d0 = d2 & 0x0F ; add.b d4, d0 ; ext ; lsl.l #$5, d0   ; demi-tons x 32
006013BC  add.l d3, d0
006013BE  bpl -> 6013CA
006013C0  addi.w #$180, d0 ; addi.w #$f800, d4 ; bra     ; negatif : une octave en dessous
006013CA  cmpi.l #$180, d0 ; bcs -> 6013da
006013D2  subi.w #$180, d0 ; move.w #$800, d4            ; au-dela : une octave au-dessus
006013DE  lea.l $603d62.l, a1 ; move.w (a1, d0.w), d3    ; LA TABLE DE HAUTEUR
006013E8  d0 = d2 & 0xF0 ; lsl.w #$7, d0                 ; le quartet haut -> le champ OCT
006013F0  add.w d4, d0 ; add.w d3, d0
006013F4  andi.w #$7bff, d0                              ; bits 15 et 10, inutilises
006013F8  movea.l $0(a4), a5 ; move.w d0, $10(a5)
```

**L'unité est le 1/16 de demi-ton.** `0x180` = 384, et `384 = 12 × 32` : douze demi-tons de 32 unités
d'index — mais l'index est un **décalage en octets** dans une table de mots, donc 32 octets = 16 mots par
demi-ton. Et `d0` est toujours pair (`d3` est décalé de 1, le terme des demi-tons de 5), ce qui est
nécessaire : un `move.w` à une adresse impaire ferait une erreur d'adresse sur 68000.

**La table de `0x603D62` le confirme d'elle-même.** Ses mots :

```
0000 0003 0007 000b 000e 0012 0016 001a 001e 0021 ...
```

croissants jusqu'à `03f8`, puis **l'entrée 192 revient à `0000`**. Donc 192 mots pour une octave, 16 par
demi-ton — le même nombre que le modulo `0x180` du code, obtenu par un chemin indépendant. Et `0x3F8` tient
juste sous les 10 bits de `FNS`.

L'assemblage final est exactement la disposition du registre : le quartet haut de `d2` décalé de 7 atterrit
sur les bits 11-14 (`OCT`), la correction d'octave vaut `±0x800` soit un pas de ce champ, `FNS` occupe les
bits 9-0, et le masque `0x7BFF` efface les bits 15 et 10, les deux inutilisés.

### Deux formules pour la hauteur, comme pour `TL`

`0x603928` écrit lui aussi `$10(a5)`, par un tout autre calcul — vélocité × sensibilité signée plus hauteur
de base, puis `(x<<1 + 0x4000) & 0x7800` pour l'octave (§5 bis octies). **Deux écrivains du registre de
hauteur, deux formules**, appelés depuis deux endroits différents : `0x600CA4` pour celui-ci, `0x6037F2` et
`0x60388E` pour l'autre. Le même doublement existe sur `TL`. Pourquoi deux chemins, et lequel sert quand,
n'est **pas établi**.

### Et un champ de plus nommé

`+0x14` de la fiche de voix est posé à l'allumage depuis le `+5` de l'objet de canal
(`0x600C60 : move.b $5(a0), $14(a4)`), et il est **ajouté au compte de demi-tons** ici. Donc le `+5` d'un
objet de canal est une **transposition**. Le `+0x0C` de la fiche en est une seconde, additive elle aussi.


## 5 bis octies. `0x603928` : la vélocité, et les deux choses qu'elle commande

126 octets, appelée de `0x6037F2` et `0x60388E`, tous deux dans le chemin d'allumage d'une note. `A2` est la
fiche d'instrument, `A4` la fiche de voix, `A5` les registres de la fente. L'entrée `D0` est la **vélocité**,
gardée dans `D3` pour servir deux fois.

### Premier usage : le niveau, par une courbe à points de rupture

```
0060392A  movea.l a2, a3 ; lea.l $6(a3), a3 ; moveq #$3, d1
00603932  cmp.b  (a3), d0 ; bge -> 603946        ; au-dela du point : segment suivant
00603936  sub.b  -$4(a3), d0                     ; moins l'abscisse du point courant
0060393A  mulu.w $2(a3), d0 ; lsr.w #$8, d0      ; fois la pente, / 256
00603940  add.b  -$3(a3), d0                     ; plus son ordonnee
00603946  lea.l  $4(a3), a3 ; dbra d1, $603932
0060394E  move.b -$3(a3), d0                     ; passe le dernier point : l'ordonnee finale
00603952  move.b d0, $a(a4)                      ; LE +0x0A DE LA FICHE DE VOIX
```

**Cinq points de quatre octets à `+2` de la fiche d'instrument** — abscisse, ordonnée, et un mot — donc
`+2` à `+0x15`, quatre segments interpolés linéairement. Le segment de `k` à `k+1` prend l'abscisse et
l'ordonnée du point `k` et **le mot du point `k+1`** comme pente ; au-delà du dernier point, la valeur est
plate.

Et le résultat va dans **`+0x0A` de la fiche de voix** — précisément l'octet que l'application de CC 7
multiplie par le volume du canal (`move.b $a(a4), d6 / mulu.w d0, d6`, §5 quater). La chaîne de volume est
donc complète : vélocité → courbe de l'instrument → `+0x0A` → × volume du canal → `TL`.

Mais les deux chemins vers `TL` **ne sont pas la même formule**, et c'est à noter pour la fidélité :

```
00603956  lsr.b #$1, d0                          ; 0..127
00603958  movea.l $608028.l, a3 ; adda.w $2(a3), a3
00603962  move.b (a3, d0.w), d0                  ; UNE TABLE EN ROM : niveau -> valeur
00603968  move.b $5(a0), d1 ; mulu.w d0, d1
0060396E  lsr.l #$8, d1 ; addq.w #$1, d1
00603972  not.b d1 ; lsr.b #$1, d1
00603976  move.b d1, $d(a5)                      ; TL
```

À l'allumage, le niveau traverse **une table en ROM** (`[0x608028]` plus son propre mot `+2`) avant la
multiplication, et il y a un `addq.w #$1` que CC 7 n'a pas. CC 7, lui, multiplie directement. Deux formules
pour le même registre, selon qu'on allume une note ou qu'on bouge le volume. **Le portage actuel n'a rien à
en faire** — il exécute ce code sur un 68000 émulé ; c'est un cœur autonome qui devrait les deux (§5 ter).

### Second usage : la hauteur

```
0060397A  move.l d3, d0                          ; la velocite, a nouveau
0060397C  move.w $1c(a2), d1 ; muls.w d1, d0     ; fois la sensibilite -- MULTIPLICATION SIGNEE
00603982  lsr.l #$8, d0
00603984  add.w $16(a2), d0                      ; plus la hauteur de base de l'instrument
00603988  move.w d0, $ede.w                      ; rangee en 0xEDE
0060398C  move.w d0, d1 ; lsl.w #$1, d1
00603990  addi.w #$4000, d1 ; andi.w #$7800, d1  ; le champ OCT
00603998  andi.w #$3ff, d0 ; or.w d1, d0         ; le champ FNS
0060399E  move.w d0, $10(a5)
```

`+0x10` d'une fente est **`OCT`/`FNS`**, la hauteur : `OCT` sur les bits 14-11, signé, et `FNS` sur les bits
9-0. La valeur interne est donc un nombre dont les bits hauts donnent l'octave et les dix bas la fraction, et
le `+0x4000` avant le masque est le **biais** qui convertit l'octave signée vers l'encodage du registre.

**La vélocité module donc aussi la hauteur**, par `+0x1C` de l'instrument — une sensibilité signée, d'où le
`muls`. Et `0xEDE` garde la hauteur calculée : c'est elle que `0x600C50` recopie dans le `+0x12` de la fiche
de voix (`move.w $ede.w, $12(a4)`), vue au §5 quater sans savoir d'où elle venait.

### Ce que ça nomme dans la fiche d'instrument

| | |
|---|---|
| `+2` à `+0x15` | **cinq points de la courbe de vélocité**, 4 octets chacun : abscisse, ordonnée, pente (mot) |
| `+0x16` | la **hauteur de base** (mot) |
| `+0x1C` | la **sensibilité de la vélocité sur la hauteur** (mot signé) |

`+1` était déjà connu (l'octet de modulation, §5 quater) et `+0` porte un drapeau que CC 1 teste.


## 5 bis septies. `0x602FA0` : « réinitialise le son », et deux jeux de globales parallèles

176 octets, **entrée 1 de la table 4** — les sous-commandes de la banque de contrôle `0x00`. Aucune
instruction ne nomme son adresse : elle ne s'atteint que par cette table, ce qui est exactement la raison
pour laquelle le marcheur ne la voyait pas avant qu'on lui apprenne les tables de saut.

Ce qu'elle fait, dans l'ordre, à `sr = 0x2200` :

```
00602FA4  cmpi.b #$80, (a3) ; beq -> 602fba       ; variante : si 0x80, on EPARGNE les pistes
00602FAA  bsr.w  $603198
00602FAE  lea.l $300.w, a4 ; 32 longs effaces     ; sinon, les huit pistes sont rasees
00602FBA  lea.l $400.w, a4 ; [0xC10] = 0 ; bsr $603050     ; les 32 voix de la puce 0
00602FC8  lea.l $200400.l, a4 ; [0xC10] = 0x200000 ; bsr $603050   ; celles de la puce 1
00602FDA  lea.l $c40.w, a4 ; 20 x andi.b #$bf, $0(a4)      ; bit 6 efface sur chaque objet de canal
```

et `0x603050`, par puce : pour chacune des 32 fiches dont le `+4` est non nul, `bsr 0x6006DC` — l'arrêt de
voix — puis `clr.l $ee0(a4)` et **huit longs effacés à `0xF80`**.

### Les globales existent en deux exemplaires, une par RAM

La queue recharge le DSP sur les deux puces, et c'est là que ça se voit :

```
00602FF4  move.b $ef1.w, d0                 ; le prereglage de la puce 0
00602FF8  move.b $ef0.w, $ef4.w
00603002  lea.l $100700.l, a3 ; adda.l $c10.w, a3 ; bsr $602a9e    ; le chargeur de la zone DSP
00603010  move.b $ef4.w, d0 ; bsr $602b32                           ; l'interprete de parametres
0060301E  move.b $200ef1.l, d0              ; LE PREREGLAGE DE LA PUCE 1, dans SA RAM
00603024  move.b $200ef0.l, $ef4.w
0060302C  move.l #$200000, $c10.w
00603034  lea.l $100700.l, a3 ; adda.l $c10.w, a3 ; bsr $602a9e
00603046  move.b $ef4.w, d0 ; bsr $602b32
```

`0xEF1` pour la puce 0, **`0x200EF1` pour la puce 1** : le pilote tient **deux jeux de globales au même
décalage, un dans chaque RAM de SCSP**, et `[0xC10]` — 0 ou `0x200000` — choisit lequel. C'est la raison de
tous les `$ef0(a5)`, `$eea(a5)` et `lea $3000.w / adda.l a5` rencontrés ailleurs : ce ne sont pas des bases
arbitraires, c'est **la même structure dans l'une ou l'autre RAM**. Et `0xEF4` sert de scratch *non*
qualifié, donc toujours dans la RAM de la puce 0, pour passer la valeur à l'appelé.

Cela recoupe directement la correction de `A5` du §5 ter : A5 n'est pas une base globale, il porte
`[0xC10]`, et c'est pour cette raison.

### Les trois arrêts, et deux catégories de voix

`0x602FA0` n'est pas seule : les **entrées 1, 2 et 3** de la table 4 sont trois arrêts de granularité
différente, toutes gardées par le même `cmpi.b #$80, (a3)`.

| entrée | pistes effacées | prédicat d'arrêt des voix | en plus |
|---|---|---|---|
| **1** `0x602FA0` | les 8 (`0x300`, 32 longs) | `0x603050` : `+4 ≠ 0` | **DSP rechargé** sur les deux puces |
| **2** `0x603080` | la **maîtresse seule** (`0x300`, 4 longs) | `0x603102` : `+4 ≠ 0` **et** `+0xB` bit 7 **clair** | `0xF80` effacé sur les deux puces, volume maître réappliqué (`0x6027B2`) |
| **3** `0x60311E` | les **sept autres** (`0x310`, 28 longs) | `0x60315A` : `+4` **bit 3 posé** | — |

Et les prédicats ne sont pas arbitraires : **l'allumage d'une note range la voix dans l'une de deux
catégories**, et c'est écrit noir sur blanc par deux chemins parallèles qui ne diffèrent que par là.

```
00600BFE  move.b $ee1(a3), $b(a4)     ; categorie A : le compteur 0xEE1
00600C04  ori.b  #$80, $b(a4)         ;   et le bit 7 du +0xB FORCE
00600C0A  addq.b #$1, $ee1(a3)
00600C0E  bset.b #$3, $4(a4)          ;   et le bit 3 du +4

00600C3C  move.b $ee0(a3), $b(a4)     ; categorie B : le compteur 0xEE0
00600C42  addq.b #$1, $ee0(a3)        ;   bit 7 non pose, bit 3 non pose
```

| | `+0xB` bit 7 | `+4` bit 3 | compteur | arrêtée par |
|---|---|---|---|---|
| catégorie **A** | **posé** | **posé** | `0xEE1` | entrées 1 et **3** |
| catégorie **B** | clair | clair | `0xEE0` | entrées 1 et **2** |

Le bit 7 du `+0xB` est donc un **marquage délibéré** (`ori.b #$80`), pas un débordement du compteur — ce
qu'il fallait vérifier avant de s'appuyer dessus.

**Et les deux catégories s'apparient avec le partage des pistes** : l'entrée 2 efface la piste maîtresse et
arrête la catégorie B ; l'entrée 3 efface les sept autres et arrête la catégorie A. Donc piste 0 ↔ B, et
pistes 1-7 ↔ A. C'est **le partage musique/effets vu d'un troisième angle**, après `group_of()` et les
canaux `0x0F-0x13`. Laquelle est la musique n'est **pas établi par ceci seul** ; que `snd_irq2` traite la
piste 0 à part, avec ses trois crochets de minutage, oriente vers « piste 0 = la musique » — mais c'est une
indication, pas une mesure.

`0x603172`, appelé par les entrées 2 et 3 avant chaque arrêt, nettoie deux choses :

```
00603174  move.b $6(a4), d5 ; bmi -> rts    ; le +6 : le canal MIDI, 0xFF = inutilise
0060317E  lsl.w #$1, d5 ; clr.w (a2, d5.w)  ; a2 = 0xF80 : UN MOT PAR CANAL
00603186  move.b $5(a4), d5 ; lsl.w #$5, d5 ; le +5 : l'indice d'objet, x 0x20
00603190  andi.w #$7f18, $8(a0, d5.w)       ; et on masque le mot +8 de l'objet
```

Deux choses s'en déduisent. **`0xF80` est indexé par canal MIDI × 2, en mots** — 16 canaux, 32 octets — et
non « un octet par fente » comme je l'avais d'abord supposé en voyant qu'il suit la table de `0xF00`. Les
deux tables sont voisines et **n'ont pas le même index** : `0xF00` est par fente (§4 bis). Et le masque
`0x7F18` sur l'image du registre LFO garde `LFOF` et les deux formes d'onde, mais efface `LFORE`, `ALFOS`
et surtout **`PLFOS`** : arrêter une voix **remet la profondeur de modulation à zéro** pour son objet de
canal, ce qui recoupe le §5 quater par l'autre bout.

Enfin `0x603198`, appelé par les entrées 1 et 3 : si le long de configuration `[0x60803C]` n'est pas négatif,
il efface le `+0` des huit entrées de `0x380` (pas de `0x10`) et `0xEFC`.

### Et ça relie les deux moitiés du DSP déjà lues

`0x602A9E` puis `0x602B32`, dans cet ordre, deux fois. Le second est l'interprète de paramètres du
§5 bis bis ; le premier est le chargeur de la zone DSP, voisin du `0x602AD6` que le §5 avait lu comme
l'effacement d'un trait de `0x700` à `0xBFF`. Donc la séquence complète d'un changement d'effet est :
**charger la zone, puis appliquer les descripteurs** — et les 59 adresses énumérées au §5 bis bis sont la
seconde étape, pas la totalité.


## 5 bis sexies. `0x60288A` : le chargeur d'échantillons, et son journal d'annulation

470 octets, **la plus grosse fonction du programme après `snd_reset`**, et jamais citée par ce document
avant le 2026-10-25 — c'est la mesure d'avancement qui l'a désignée. Appelée de `0x602DBA` et `0x602DD6`,
deux commandes de contrôle, plus un `bra` de `0x602880`. Elle tourne à `sr = 0x2200`.

**Ce qu'elle fait : « charge le jeu d'échantillons `D0` ».** Et comme elle *remplace* le jeu précédent, elle
commence par le défaire.

### Les deux tables

| | |
|---|---|
| `0x3000` + `[0xC10]` | un **en-tête de deux mots** — `+0` l'indice de base, `+2` l'indice maximum — puis les **fiches d'échantillon** à partir de `0x3004`, `0x0C` octets : `+0` l'adresse (24 bits) avec les bits de banque et un marqueur bit 7 dans l'octet haut, `+4` la longueur, `+8` un cumul |
| `0x200` + `[0xC10]` | le **journal d'annulation**, un long par fiche remplacée |
| `[0xEEA + décalage]` | le **nombre de fiches à défaire**, et il plafonne le `dbra` |
| `[0xC18 + décalage]` | le jeu actuellement chargé — si `D0` l'égale déjà, la fonction sort tout de suite |

### Défaire, puis faire

Le premier balayage (`0x6028B6`) cherche les fiches marquées — `tst.b (a2) / bmi`, pas de `0x0C` — et pour
chacune remet le long du journal à la place du courant, en corrigeant le cumul dans les deux sens :

```
006028C2  move.l (a1, d2.w), d1          ; l'ancienne valeur, du journal
006028C6  move.l $0(a2), d3              ; la valeur en place
006028CA  move.l d1, $0(a2)              ; on restaure
006028CE  bclr.b #$1c, d1 ; add.l d1, $8(a2)
006028D6  bclr.b #$1f, d3 ; sub.l d3, $8(a2)
```

Puis le chargement. `[0x608020]` est la racine d'une table en ROM, bornée par son propre mot `+0`
(`cmp.w (a1), d0 / bgt -> sortie`, la même convention que les trois autres tables de ce pilote), et la fiche
du jeu donne un compte masqué à `0x3F` — **64 échantillons au plus par jeu**. La destination part de
`[0x608034] + [0xC10]` et avance de la longueur arrondie au pair : **un allocateur à pointeur qui monte**
dans la RAM sonore.

Pour chaque échantillon du jeu :

```
0060292E  move.l (a3, d1.w), d2 ; move.l d2, (a2)+   ; la source ROM, et on la JOURNALISE
00602934  andi.l #$ffffff, d2                        ; 24 bits : l'adresse dans la fenetre
0060293A  move.b $ef9.w, d6 ; andi.b #$f, d6
00602942  or.b $608068.l, d6
00602948  or.b (a3, d1.w), d6                        ; les bits de banque de la fiche
0060294C  move.b d6, $400001.l                       ; LE REGISTRE DE BANQUE
00602952  move.l d0, (a3, d1.w) ; bset.b #$7, ...    ; la fiche pointe desormais la RAM, et on la marque
0060295C  addq.b #$1, $eea(a5)                       ; une de plus a defaire
00602960  sub.l d2, $8(...) ; add.l d0, $8(...)      ; le cumul de relocation
```

**L'octet de banque est composé de trois sources** : `[0xEF9] & 0x0F`, une constante en ROM à `[0x608068]`,
et les bits de poids fort de la fiche elle-même. Il est reposé **à chaque échantillon**, pas une fois par
jeu — parce que deux échantillons d'un même jeu peuvent être dans deux moitiés différentes des 16 Mo.

### La copie, et son arithmétique

```
00602980  move.l d6, d5 ; lsr.l #$8, d5               ; d6 = la longueur ; d5 = longueur >> 8
00602984  [16 x : movem.l (a0)+,d1-d4 / movem.l d1-d4,(a4) / lea $10(a4),a4]
00602A44  dbra d5, $602984                            ; un tour = 256 octets
00602A48  andi.l #$ff, d6 ; lsr.l #$1, d6
00602A50  move.w (a0)+, (a4)+ ; dbra d6               ; la queue, mot par mot
```

**Seize pas de `movem.l` à quatre registres, donc 16 octets par pas et 256 octets par tour de boucle**, sans
test entre les pas — mesuré en comptant les motifs identiques : 16 exactement, 192 octets de bloc. Ce n'est
**pas** un dispositif de Duff comme les quatre de `snd_reset` : ici le reste est traité à part, mot par mot,
`(longueur & 0xFF) / 2` fois. Les 192 octets du bloc sont 41 % de la fonction.

### Ce que ça établit, et ce que ça ne dit pas

Les fiches de `0x3000` sont **réécrites sur place** : avant le chargement `+0` est l'adresse dans la ROM
d'échantillons, après c'est l'adresse dans la RAM sonore. Le journal de `0x200` existe précisément pour que
le jeu suivant puisse rendre les fiches à leur état ROM. C'est une substitution réversible, et `[0xEEA]`
compte ce qu'il y a à rendre.

À ne pas confondre avec les **32 descripteurs de `0x1000`** que `A6` parcourt par pas de `0x20` (§2) : ce
sont deux tables distinctes, de pas et de taille différents. Le lien entre les deux n'est pas établi.

**L'en-tête de deux mots a été trouvé par les consommateurs, pas ici.** `0x60288A` fait `tst.l (a2)+` et
`tst.l (a3)+` avant ses boucles — quatre octets sautés, sans dire pourquoi. Ce sont `0x600B4C` et `0x6037FA`
qui le nomment, en faisant la même chose dans l'autre sens :

```
00600B54  cmp.w $2(a1), d1 ; ble -> 600b6a     ; d1 au-dela du maximum : on refuse
00600B6A  sub.w (a1), d1                       ; moins l'indice de base
00600B6C  tst.l (a1)+                          ; on saute l'en-tete
00600B6E  mulu.w #$c, d1 ; adda.l d1, a1       ; la fiche
00600B74  move.l a1, $ed0.w                    ; 0xED0 : LA FICHE COURANTE
00600B78  move.b $0(a1), $ed6.w ; bmi -> rts   ; son octet de drapeaux, bit 7 = deja en RAM
```

Donc un **numéro d'échantillon global** devient un indice local par soustraction de la base, et le bit 7 que
`0x60288A` pose est ce que le consommateur teste pour savoir si l'échantillon est déjà en RAM. `0xED0` est
la fiche sélectionnée et `0xED6` ses drapeaux — et `0xED0` est relu à l'allumage d'une note (`0x6038E6` :
`movea.l $ed0.w, a2 / tst.b $4(a2) / bmi`). La chaîne va donc du chargeur à la note.

Et le `+8`, le cumul : il reçoit `−ancienne +nouvelle` à chaque substitution, donc il suit **le déplacement**
des adresses. À quoi il sert ensuite n'est **pas** lu : aucun des cinq emplois de `0x3000` ne le consomme.


## 5 bis quinquies. `0x600852` : le consommateur, et la table 1 enfin nommée

Lu le 2026-10-25. C'est l'autre bout de l'anneau de `0x800`, et il referme la chaîne entière.

```
00600852  move.b $ecc.w, d0
00600856  cmp.b  $efa.w, d0 ; bcs -> 600860
0060085C  move.b d0, $efa.w          ; 0xEFA : UN FILIGRANE, le maximum jamais atteint
00600860  subq.b #$1, $ecc.w         ; un message de moins en attente
00600864  lea.l $800.w, a3
00600868  move.w $ec6.w, d0 ; andi.l #$3ff, d0   ; 0xEC6 : le curseur de LECTURE
00600874  addq.w #$4, $ec6.w
0060087A  move.b (a3)+, d0           ; l'octet de statut
0060087C  move.l d0, d5 ; andi.b #$f, d5         ; D5 = LE CANAL MIDI, le quartet bas
00600882  lsr.b #$2, d0 ; andi.w #$1c, d0
00600888  lea.l $60088e(pc, d0.w), a1 ; jmp (a1)
```

**Trois choses d'un coup.**

**La table 1 de `0x60088E` est le répartiteur du consommateur**, pas celui du MIDI entrant. Je l'avais
ouverte à Ghidra en l'appelant « le répartiteur de statut MIDI » sans savoir qui l'atteignait. Elle
distribue les messages **tirés de l'anneau**. Ses huit créneaux :

| créneau | statut | cible |
|---|---|---|
| 0 | `0x8n` note off | `0x60120A` |
| 1 | `0x9n` note on | `0x6008AC` |
| 2 | `0xAn` commandes du pilote | `0x6025F2` |
| 3 | `0xBn` changement de contrôle | `0x601402` |
| 4 | `0xCn` changement de programme | `0x601C1A` |
| 5 | `0xDn` | `0x6008AA`, c'est-à-dire **le `rts` : ignoré** |
| 6 | `0xEn` molette de hauteur | `0x6012D6` |
| 7 | `0xFn` | le `rts` lui-même, en ligne |

Sept `bra.w` et un `rts` partagé — exactement la forme que `FV2SonTablesDeSaut.java` avait décrite pour
cette table, et que les deux tables de `0x60030A` et `0x60376C` reprennent.

**`D5` naît ici.** Le quartet bas du statut, c'est-à-dire le canal MIDI, est posé dans `D5` avant le saut —
et c'est le `D5` que CC 1, CC 7 et CC 10 comparent au `+1` de chaque objet de canal. La provenance de cette
convention est donc établie, et non plus supposée.

**Et la taille de la table 3 est confirmée une quatrième fois.** `0x601402`, la branche `0xBn` :

```
00601404  move.b (a3)+, d0
00601406  cmpi.b #$50, d0 ; bgt -> 6014be    ; au-dela de 0x50 : le gestionnaire << rien >>
00601414  move.w $60141c(pc, d2.w), d2 ; jmp $60141c(pc, d2.w)
```

La borne `0x50` est **écrite dans le code**, et `0x50 + 1 = 81` — le compte que la règle
d'auto-dimensionnement avait donné et que la taille du trou (162 octets) avait redonné. Trois mesures
indépendantes, plus celle-ci.

### Qui appelle, et à quel rythme

Deux endroits seulement, `0x601CB8` et `0x602160`, avec la même forme :

```
00601CAC  move.b #$1, $c30.w
00601CB2  tst.b  $ecc.w ; beq -> 601cc0     ; rien en attente : on passe
00601CB8  bsr.w  $600852                    ; UN SEUL message
00601CBC  clr.b  $c30.w
00601CC0  move.l a6, d0 ; addi.w #$20, d0 ; cmpi.w #$1400, d0
00602174  lea.l  $1000.w, a6                ; et on reboucle a 0x1000
```

**Un message par passe**, entrelacé avec un parcours de `a6` sur `0x1000` à `0x13FF` par pas de `0x20` —
soit **32 structures de `0x20` octets à `0x1000`**, les bornes étant écrites dans le `cmpi.w #$1400` et le
`lea $1000.w`. C'est la boucle principale, en tourniquet.

**Donc la chaîne complète, des données au matériel :**

```
pistes de 0x300 -> le tic du timer A decode -> anneau de 0x800 (256 longs, 0xEC4 ecrit, 0xECC compte)
   -> 0x600852 (0xEC6 lit) -> table 1 par statut -> 0x601402 -> table 3 par controleur
   -> CC 1 / 7 / 10 -> objets de canal de 0xC40 -> fiches de voix de 0x400 -> registres de fente
```

Et la contre-pression s'explique : le consommateur ne tire **qu'un** message par tour de boucle principale,
alors que le tic du timer A peut en empiler plusieurs par interruption. D'où le `cmpi.b #$fc, $ecc.w` qui
arrête la production à 252 sur 256.

### Deux détails

`0xEFA` est un **filigrane** : le maximum que `0xECC` ait atteint. Il est écrit et lu **uniquement dans
`0x600852`** — aucun autre emploi dans tout le code atteint. C'est une trace laissée par l'auteur, morte
pour le programme.

`0xC30` est posé autour du tirage et **effacé par `snd_irq2`** (`0x6033B0`). Les deux se parlent donc par cet
octet ; ce qu'il protège exactement n'est pas établi.

### Ce que ça laisse ouvert

Les six gestionnaires de la table 1 que je n'ai pas lus : note off, note on, les commandes `0xAn`, le
changement de programme, la molette de hauteur.

Les 32 structures de `0x1000` que `A6` parcourt ne sont **pas** ouvertes : le §2 les a déjà nommées — les
**descripteurs de copie d'échantillons**, `0x20` octets chacun, `0x1000-0x13FF` et `0x201000`. Les bornes que
je viens de relever dans la boucle (`cmpi.w #$1400`, `lea $1000.w`) les redonnent depuis un autre endroit du
code, ce qui est une confirmation de plus.

### Et pourquoi la table 1 ignore `0xFn`

Parce que les messages système ne passent **jamais** par l'anneau : ils sont interceptés à la réception. Le
§3 ter l'avait lu dans `snd_irq3_reception_midi` — `cmpi.b #$ff, d0 / beq -> 0x6006C4`, et `0x6006C4` remet
la pile à `0x2000` et écrit `"Erro"` en `0xC08`, c'est-à-dire **redémarre la carte**. Le `0xFn` que le
consommateur jette n'est donc pas un oubli : rien ne l'y met. Les deux lectures, faites à deux jours
d'écart et par deux chemins, se recoupent.


## 5 bis quater. Les huit pistes de `0x300` : un lecteur de fichier MIDI standard

Lu le 2026-10-25, à la suite de `snd_irq2`. Les huit structures que le tic parcourt sont **huit pistes de
séquenceur**, et le pilote est un lecteur de format MIDI standard — delta-times à longueur variable, statut
courant, et une liste d'enchaînement par piste.

### La taille, confirmée trois fois

`0x10` octets par piste, `0x300` à `0x37F`, et les trois mesures ne partagent rien :

| | |
|---|---|
| `snd_irq2` | sept `lea $10(a2), a2` entre les appels, donc un pas de `0x10` |
| `0x602712` | `moveq #$3, d7 / clr.l (a4)+ / dbra` — **quatre longs**, l'effacement d'une piste |
| `0x6036EE` | `lea $300.w, a0 / moveq #$1f, d0 / clr.l (a0)+` — **32 longs = `0x80` octets**, soit les huit |

### La disposition

| | | |
|---|---|---|
| `+0` | drapeaux | bit **7** active (`bset` en `0x602706`, `bclr` en `0x603202` pour la piste 0) ; bits **6 et 5** fournis par la commande de démarrage (`andi.b #$60, d0 / or.b d0, $0(a4)`) ; bit **3** « la piste a une liste d'enchaînement » ; bit **0** « un autre événement suit tout de suite » |
| `+1` | **statut courant** | le statut MIDI mémorisé, pour le statut implicite |
| `+2` | mot | le **compte à rebours** jusqu'au prochain événement |
| `+4` | long | le **pointeur de lecture** dans les données |
| `+8` | long | le **curseur dans la liste d'enchaînement** |
| `+0xC` | — | `+0xF` est **un quartet de groupe**, posé par `0x6026AE` qui balaie `0x310` sur sept pistes — le §2 l'avait déjà relevé. `+0xC` à `+0xE` restent non identifiés |

### Le statut implicite et le delta-time, tels quels

```
0060347A  move.b (a3)+, d1 ; bmi -> 603484     ; bit 7 pose : un nouveau statut
0060347E  move.b $1(a2), d1 ; subq.l #$1, a3   ; sinon : STATUT IMPLICITE, on rend l'octet
00603484  move.b d1, $1(a2)                    ; et on le memorise
00603488  lsr.b #$2, d0 ; andi.w #$1c, d0
00603490  lea.l $603496(pc, d0.w), a0 ; jmp (a0)
```

C'est **le statut implicite de MIDI** (un octet de donnée réutilise le statut précédent), et le `jmp` est
celui de `0x603494` — la table de huit créneaux que j'avais ouverte la veille sans savoir ce qu'elle
distribuait. `(statut >> 2) & 0x1C` prend les bits 6-4 du statut et les multiplie par quatre, donc les huit
familles `0x8n` à `0xFn`. Les créneaux 1 et 3 partagent `0x60352C` : ce sont `0x9n` et `0xBn`, les deux
messages à **trois octets**, et une seule routine les assemble. `0xAn` a son propre gestionnaire
(`0x6034B6`), qui examine ses globales à lui — **ce n'est donc pas de l'aftertouch mais le canal de commandes
du pilote**, ce que la liste de lecture appelait « le protocole `A0 banque index` ».

Et la queue commune lit le delta-time :

```
00603664  btst.b #$0, $0(a2) ; bne -> 603464   ; drapeau pose : evenement suivant immediatement
00603670  move.b (a3)+, d1 ; beq -> 603464     ; delta nul : idem
00603676  bclr.b #$7, d1 ; beq -> 603680       ; bit 7 pose : un second octet suit
0060367C  lsl.w #$7, d1 ; or.b (a3)+, d1       ; sept bits de plus
00603680  move.w d1, $2(a2) ; move.l a3, $4(a2)
```

**La quantité de longueur variable du format MIDI standard**, bornée à deux octets — donc 14 bits, 16 383
tics au plus. Et les deux chemins de retour à `0x603464` sont ce qui permet les accords : plusieurs
événements dans le même tic.

### La liste d'enchaînement : un petit programme par piste

Quand une séquence finit, si le bit 3 est posé, `+8` est un curseur dans **une liste de longs**, et chaque
long est une instruction :

| valeur | ce qu'elle fait |
|---|---|
| `0xFFFFFFFF` | **fin** : `clr.b $0(a2)`, la piste s'arrête |
| `0xFFFFFFF1` | **indirection** : `movea.l (a0), a0` — on saute ailleurs dans la liste, et c'est ainsi qu'un morceau **boucle** |
| `0xFFFFFFF2` | **tout arrêter** : les 32 longs de `0x300` effacés, les huit pistes avec |
| `0x80xxxxxx` | une **commande en ligne** : `d1 = long << 8 \| 0x80`, poussée dans la file |
| autre | la **séquence suivante** à jouer |

Et sans le bit 3, la fin d'une séquence fait simplement `clr.b $0(a2)`.

### Et la boucle se referme sur `snd_irq2`

`0x6036FC`, appelé par les gestionnaires d'événement, **n'agit pas** : il pousse.

```
006036FC  lea.l $800.w, a1
00603702  move.w $ec4.w, d0 ; andi.w #$3ff, d0  ; un anneau de 0x400 octets
0060370A  adda.l d0, a1 ; move.l d1, (a1)       ; le message de quatre octets
0060370E  addq.w #$4, $ec4.w                    ; le curseur d'ecriture
00603712  addq.b #$1, $ecc.w                    ; LE NOMBRE EN ATTENTE
```

C'est **la file de commandes de `0x800`** — un anneau de `0x400` octets, soit **256 longs**, avec `0xEC4`
en curseur d'écriture et **`0xECC` en compte d'attente**. Le consommateur est ailleurs : `0x600852` lit
`0xECC`, `0x600860` le décrémente.

**Et ça explique le `0xFC` de `snd_irq2`.** Le gestionnaire du timer A commence par
`cmpi.b #$fc, $ecc.w / bcc -> rte` : il **cesse de produire quand la file est presque pleine**, 252 sur 256.
Les trois nombres s'emboîtent — anneau de `0x400` octets, masque `0x3FF`, seuil `0xFC` — et aucun n'a été
supposé.

L'architecture du pilote tient donc en une phrase : **le tic du timer A décode les séquences et empile des
messages dans l'anneau de `0x800` ; un consommateur hors interruption le vide.**

### Ce que ça laisse ouvert

Les quatre octets `+0xC` d'une piste, qu'aucun code atteint ne touche. Le rôle du bit 6 des drapeaux, fourni
par la commande de démarrage à côté du bit 5 dont je sais qu'il recopie vers la sortie MIDI. Le consommateur
de la file, lui, a été lu juste après : voir 5 bis quinquies.


## 5 bis ter. `snd_irq2` (`0x6032D2`) : le tic de la musique, et une file de tics différés

Point 3 de la liste de lecture. Lu le 2026-10-25.

### D'abord une correction : j'avais inversé les timers

La section précédente, écrite le même jour, disait de `SCIEB = 0x00C8` : « entrée MIDI, **timer B et timer C**
armés ; le timer A ne l'est **pas** ». **C'est faux, et c'est l'inverse.** Ce qui l'a révélé : `snd_irq2`
recharge le registre `0x418` — TIMA, le timer A — et acquitte `SCIRE = 0x40`, le bit 6. Un gestionnaire qui
rechargerait le timer A sans que celui-ci soit armé n'aurait pas de sens.

Les bits, pris dans l'implémentation du portage plutôt que dans mon souvenir d'une table
(`src/sound/scsp.cpp`) : `timerA_cb()` pose `0x40` dans SCIPD, `timerB_cb()` pose `0x80`, `timerC_cb()`
pose `0x100`, et le chemin d'entrée MIDI pose `8`. Donc :

| bit | source | armé par `0x00C8` ? | niveau d'après SCILV `0x88`/`0x48`/`0x00` |
|---|---|---|---|
| 3 (`0x08`) | entrée MIDI | **oui** | `0b011` = **3** |
| 6 (`0x40`) | **timer A** | **oui** | `0b010` = **2** |
| 7 (`0x80`) | **timer B** | **oui** | `0b001` = **1** |
| 8 (`0x100`) | timer C | **non** | — |

Les niveaux que j'avais calculés — 3, 2, 1 — étaient bons ; seuls les **noms** des sources étaient
permutés. Donc **`snd_irq2` est le gestionnaire du timer A**, et c'est le tic de la musique.

Et la valeur le confirme de l'extérieur : le gestionnaire recharge TIMA depuis `[0x60806A]`, qui vaut
**`0x00CE`** — exactement le premier des dix mots de `0x6002D4`, et exactement ce que le commentaire du
portage (`scsp.cpp`, ligne 372) avait déjà noté : « timer A at 0xCE: 50 samples, 882 a second ». Trois
sources indépendantes sur la même valeur.

### Un tic qu'on peut étirer ou comprimer, une fois

```
006032D8  move.w $60806a.l, d0      ; 0x00CE, la valeur nominale
006032DE  tst.b  $c3c.w             ; zero : rien
006032E6  subi.w #$30, d0           ; si positif -> 0x009E
006032F4  addi.w #$30, d0           ; si negatif -> 0x00FE
006032F8  move.w d0, $100418.l      ; TIMA
006032FE  move.w #$40, $100422.l    ; SCIRE : acquitte le timer A
```

`0xC3C` n'est écrit qu'en deux endroits, `0x602F24` (`#$1`) et `0x602F2C` (`#$ff`), tous deux dans la zone
des commandes de contrôle — ce sont donc **deux commandes**, et le gestionnaire efface l'octet, donc l'effet
ne dure **qu'un intervalle**. Les chiffres, puisque le compte monte jusqu'à `0xFF` :

| `0xC3C` | TIMA | tours de prédiviseur | effet sur l'intervalle |
|---|---|---|---|
| 0 | `0xCE` | 50 | nominal |
| 1 | `0x9E` | **98** | presque deux fois plus long |
| `0xFF` | `0xFE` | **2** | vingt-cinq fois plus court |

Une asymétrie de cette taille n'est pas un réglage de tempo. Ça ressemble à une synchronisation avec la
carte principale — « saute un temps » d'un côté, « rappelle-moi tout de suite » de l'autre. **Je ne le nomme
pas** : ce qui est mesuré, ce sont les deux valeurs et le fait que l'effet est à un coup.

### Ce qui compte vraiment : les tics ne sont jamais perdus

```
00603306  tst.b $c01.w ; bne -> 6033BE     ; deja dedans ?
0060330E  cmpi.b #$fc, $ecc.w ; bcc -> rte ; sature : on ne fait que recharger le timer
00603318  move.b #$80, $c01.w              ; on prend le verrou
...
00603368  tst.b $c1e.w ; beq -> 603374
0060336E  subq.b #$1, $c1e.w ; bra 603322  ; un tic differe : on refait le tour
...
006033BE  addq.b #$1, $c1e.w ; rte         ; LE CHEMIN REENTRANT
```

`0xC01` est un **verrou de réentrance**, et `0xC1E` un **compteur de tics différés** — ses trois seuls
emplois dans tout le programme sont ces trois lignes. Quand l'interruption retombe pendant que le
gestionnaire tourne encore, il **n'exécute rien : il incrémente `0xC1E` et sort**. Puis la boucle principale
se refait un tour pour chaque tic en attente avant de rendre la main.

**Le pilote ne perd donc jamais un tic ; il les met en file et rattrape dans une seule invocation.** C'est
important pour le portage : si l'ordonnancement de l'hôte fait déborder le gestionnaire, le vrai pilote
rattrape au lieu de laisser tomber des notes. Le seul cas où un tour est réellement sauté est `0xECC >= 0xFC`
— qui est, mesuré depuis `0x6036FC`, **le nombre de commandes en attente dans l'anneau de `0x800`**. Celui-ci
tient 256 longs (`0x400` octets, masque `0x3FF`), donc `0xFC` = 252 est un seuil de **contre-pression**, pas
un nombre magique. Voir 5 bis quater.

Le corps du tour, entre `sr = 0x2700` (tout masqué) et `sr = 0x2000` : `a2 = 0x300`, puis sept
`lea $10(a2), a2` suivis d'un `bsr 0x60344A`. Donc **huit structures de `0x10` octets à `0x300-0x37F`**, la
première traitée à part (`0x6033C8`, `0x603408`, `0x603460` selon trois drapeaux) et les sept autres par la
même routine. Ce sont les **huit pistes du séquenceur**, lues juste après : voir 5 bis quater, qui donne aussi
ce que `0xECC` compte et pourquoi le seuil vaut `0xFC`.

### Et une voie de retour que le portage jette

```
00603322  tst.b $c3e.w ; beq -> 60333c
00603328  subq.b #$1, $c3d.w ; bne -> 60333c
0060332E  move.b #$a, $c3d.w
00603334  move.b #$f0, $100407.l            ; MOBUF
```

`0x407` est l'octet bas du registre `0x406`, soit **MOBUF, le tampon de sortie MIDI**. Quand `0xC3E` est
posé — encore une commande, `0x602F16` et `0x602F1E` — le pilote **envoie `0xF0` à la carte principale tous
les dix tics**, soit environ 88 fois par seconde.

Et dans le portage, `src/sound/soundboard.cpp` ligne 133 : `h.midi_out = [](int) {};`. **La sortie est
jetée.** Si la carte principale attend cet octet, ça se verrait ; si elle ne le lit pas, ça ne coûte rien.
**Lequel des deux n'est pas mesuré** — il faudrait voir si le code PowerPC lit le port de retour de la carte
son.


## 5 bis bis. `0x602B32` : l'interprète de paramètres du DSP, et les 59 adresses qu'il écrit

Point 1 de la liste de lecture — la fonction qui contient `0x602BA2`, le chargement COEF/MADRS. Lue le
2026-10-25, sans Ghidra. Ce n'est pas un chargeur de bloc : c'est un **interprète piloté par descripteurs**.

### Comment on arrive au bloc

`0x602CE4` fournit le bloc, et l'indexation est en trois sauts :

```
00602CE4  movea.l $608018.l, a1        ; la racine de la table des effets
00602CEC  bmi -> abandon               ; pointeur negatif : on sort en depilant
00602CF0  move.b  $ef1(a5), d1         ; 0xEF1 : LE PREREGLAGE COURANT
00602CF4  bmi -> abandon
00602CF6  lsl.w   #$1, d1
00602CF8  move.w  $2(a1, d1.w), d1 ; adda.l d1, a1    ; la fiche du prereglage
00602CFE  move.w  (a1), d1         ; adda.l d1, a1    ; le bloc de parametres
```

`[0x608018]` vaut `0x0060E954`, et son mot `+0` vaut **2**. Ce n'est pas un compte mais **l'indice
maximum** — la routine sœur `0x602D08` le confirme en faisant `cmp.w (a0), d0 / bgt -> abandon` sur la même
disposition. Donc **trois préréglages, 0 à 2**. Et c'est vérifié de l'autre côté : en suivant la chaîne pour
les indices 0 à 15, **seuls 0, 1 et 2** donnent un bloc dont tous les types sont connus ; à partir de 3 on
tombe sur `entrees=255`, `pas=255`, des types inexistants. Les deux mesures — le mot de la table et la
validité des blocs — se rejoignent sur trois.

### La disposition du bloc, et les sept types

```
+0  nombre d'entrees        +4  sous-index maximum        +5  octets par jeu de valeurs
+8  les descripteurs, QUATRE octets chacun : +0 type et drapeaux, +2 decalage COEF, +3 decalage MADRS
```

Le flux de valeurs commence à `(nombre + 2) × 4` du début — soit après l'en-tête de 8 octets et les
descripteurs — et le sous-index `d0` y choisit un **jeu** (`a1 += d0 × pas`). Le sous-index change donc les
valeurs, **pas les adresses**.

Les comptes de mots par type, relevés un par un entre `0x602B64` et `0x602C8C` :

| type (`& 0x1F`) | mots COEF | mots MADRS |
|---|---|---|
| `0x01` | 1 | — |
| `0x02` | 10 | 7 |
| `0x04` | 3 | 2 |
| `0x0D` | 2 | — |
| `0x0E` | 5 | — |
| `0x10` | 5 | — |
| `0x16` | **38** | 1 |

Et le **bit 5 du type** n'est pas un type : il ajoute `$2(a3)` à chaque valeur MADRS écrite
(`btst.b #$5, $8(a2)` / `move.w $2(a3), d4` / `add.w d4, d0`). C'est une **relocation des lignes de retard** :
les adresses de délai d'un préréglage sont décalées par une base. Le type `0x24` observé dans les données est
le type `4` avec ce drapeau. Quand un type n'écrit aucun mot MADRS, l'octet `+3` du descripteur vaut `0xFF` —
un marqueur, pas un décalage.

### Les 59 adresses, énumérées depuis les données

Chaque écriture est **bornée et adressée par un décalage du descripteur** : aucun balayage linéaire dans
cette fonction. Donc l'ensemble des adresses touchées se calcule depuis la ROM, et c'est fait — les trois
blocs, 32 descripteurs en tout :

```
59 adresses de registre ecrites, de 714 a 794
dans 0x7C0-0x7FF : 0
```

**Aucune.** La plus haute est `0x794`, et MADRS va jusqu'à `0x7BF`. L'interprète qui règle l'effet ne nomme
jamais `0x7C0-0x7FF`.

**C'est une deuxième ligne de preuve sur la question de fidélité, indépendante de la première.** La section 5
l'avait tranchée par le balayage : un seul passage linéaire de `0x700` à `0xBFF` depuis un bloc en ROM, donc
un miroir matériel détruirait chaque préréglage pendant son propre chargement. Celle-ci dit autre chose : la
seule autre voie par laquelle le pilote touche COEF et MADRS **n'adresse jamais la plage contestée**, et le
fait de façon énumérable plutôt que déduite. Les deux ensemble : `0x7C0-0x7FF` n'est écrit par ce pilote que
par le coup de balai qui met toute la zone à zéro.

Ce que ça ne dit toujours pas : ce que le **matériel** fait de ces 64 octets. Ça reste hors de portée de la
lecture du programme.

### Et pour la question des arènes

`0xEF1` choisit parmi **trois** jeux de paramètres COEF/MADRS. À ne pas confondre avec les **9 programmes
MPRO distincts** mesurés sur `fv2-fight.txt` : MPRO est le microcode, ces trois-là sont les coefficients et
les adresses de retard. Le point 5 de la liste de lecture porte sur MPRO et reste ouvert ; ce qui est acquis
ici, c'est qu'un seul octet de RAM sélectionne le jeu de paramètres, et qu'il n'y en a que trois.


## 5 quater. Les changements de contrôle MIDI : modulation, volume, panoramique

Lu le 2026-10-25, **sans Ghidra** — au marcheur et à l'octet. Les trois gestionnaires que la table de saut 3
(`0x60141C`) distribue par numéro de contrôleur : CC 1 (`0x601882`), CC 7 (`0x601974`), CC 10 (`0x601A08`).

### Les deux étages, et les deux clés

Les trois ont la même forme, et elle dit l'architecture du pilote :

1. **balayer les 20 objets de canal** à `0xC40`, `0x20` octets chacun ; garder ceux dont `+0` est non nul
   (actif) et dont **`+1` vaut le canal MIDI** du message ;
2. **écrire le réglage dans l'objet** ;
3. **balayer les 32 fiches de voix** de `0x400` (puis `0x200400` pour la puce 1), et pour celles qui
   appartiennent à cet objet, **pousser la valeur dans les registres de la fente**, le `+0` de la fiche étant
   un pointeur sur eux.

Et à l'étape 3, **les trois n'utilisent pas la même clé**, ce qui est le fait neuf de cette lecture :

| gestionnaire | la fiche est retenue si | ce que ça vaut |
|---|---|---|
| CC 1, `0x60193A` | `cmp.b $5(a4), d1` | **`+5` == l'indice de l'objet de canal** (`(A0 − 0xC40) >> 5`, donc 0..19) |
| CC 10, `0x601A5A` | `cmp.b $5(a4), d3` | le même `+5`, même calcul d'indice |
| CC 7, `0x6019D6` | `cmp.b $6(a4), d5` | **`+6` == le canal MIDI** (`D5`, celui que `+1` de l'objet porte) |

Une fiche porte donc **deux** identifiants : `+5` l'indice de son objet de canal, `+6` son canal MIDI. Vingt
objets pour seize canaux MIDI : les deux ne peuvent pas être la même numérotation. **C'est le tiers manquant
de `group_of()`** — et ça reste à finir, parce que *qui pose* `+5` et `+6` n'est toujours pas lu. Ce qui est
acquis : `+5` est un indice d'objet, pas un canal.

Autre asymétrie mesurée : **CC 1 applique dans la boucle**, pour chaque objet concordant ; **CC 7 et CC 10
appliquent une seule fois après la boucle**, sur le **dernier** objet concordant (`moveq #$ff,d3` en
sentinelle, `move.l a0,d3` à chaque concordance, `tst.l d3 / bpl`). Si deux objets actifs partageaient un
canal MIDI, le volume et le panoramique du premier ne seraient pas rafraîchis. Que ça puisse arriver n'est
pas établi.

### CC 1, molette de modulation : le LFO matériel, pas la table logicielle

```
00601882  lsl.b #1,d0 ; andi.b #$e0,d0        ; les 3 bits de poids fort de la valeur MIDI doublée
...
006018E2  move.b $9(a0),d3 ; andi.b #$e0,d3   ; la valeur en place
006018EC  beq -> rts                          ; inchangée, on ne touche à rien
006018EE  andi.b #$1f,$9(a0) ; or.b d0,$9(a0) ; sinon on remplace les 3 bits
006018F8  bsr $60193A                         ; et on applique
```

et l'application écrit **`$12(a5)`**, masqué par `andi.w #$7fff`. `+0x12` d'une fente SCSP est le registre
**LFO** : bit 15 `LFORE`, bits 14-10 `LFOF`, 9-8 `PLFOWS`, **7-5 `PLFOS`**, 4-3 `ALFOWS`, 2-0 `ALFOS`. Les
trois bits que CC 1 pose sont donc **`PLFOS`, la profondeur du LFO de hauteur**, et le masque `0x7FFF`
laisse `LFORE` à zéro : on règle le LFO **sans le réinitialiser**. Le mot `+8` de l'objet de canal est une
image prête de ce registre, dont `+9` est l'octet bas.

**Ce que ça renverse :** la molette de modulation ne passe **pas** par la table de modulation logicielle de
`0xF00`. Elle va au LFO matériel. Le bit 7 du `+4` d'une fiche, que j'ai mesuré **jamais posé** et dont je
soupçonnais CC 1, n'est donc pas armé par la molette. Les deux mesures tiennent ; ce qui arme la modulation
logicielle reste inconnu.

La profondeur de l'instrument s'y **ajoute**, en saturant :

```
006018C4  move.b $1(a1),d3 ; move.b d3,d1     ; l'octet de modulation de l'instrument
006018CA  andi.b #$e0,d3
006018CE  cmp.b  d3,d0
006018D0  bcs.b  $6018d4                      <-- la cible est l'instruction SUIVANTE
006018D4  andi.b #$7,d1 ; lsl.b #$5,d1        ; ses 3 bits de poids FAIBLE, remontés
006018DA  add.b  d1,d0 ; bcc ; move.b #$e0,d0 ; somme, saturée à 0xE0
```

Le `bcs` de `0x6018D0` a pour cible `0x6018D4`, **l'instruction qui le suit** (`65 02`, déplacement 2) : les
deux issues mènent au même endroit, la comparaison d'avant ne sert donc à rien. Deux lectures possibles — un
reste d'une version antérieure, ou un branchement neutralisé en changeant son déplacement, ce qui est une
façon connue de corriger du binaire. **Je ne sais pas laquelle**, et ça ne change pas le comportement : la
profondeur de l'instrument est toujours sommée.

L'instrument est trouvé par `movea.l $608038.l, a1` puis `move.w $2(a1,d3.w), d3` (un décalage ; négatif =
pas d'instrument) puis `adda.l d3, a1`.

### CC 7, volume principal : deux étages et une globale

```
00601974  lsl.b #1,d0 ; beq ; addq.b #1,d0    ; 7 bits -> 8 (0x7F->0xFF, 0->0)
00601994  move.b d0,$3(a0)                    ; +3 = le volume tel qu'envoyé
0060199E  btst.b #0,$0(a0) ; bne              ; bit 0 des drapeaux : exempté de la globale
006019A6  move.b $ee9.w,d2                    ; sinon, l'échelle globale 0xEE9
006019AA  mulu.w d2,d0 ; lsr.w #8,d0 ; addq   ; volume x échelle / 256, arrondi hors de zéro
006019B2  move.b d0,$a(a0)                    ; +0x0A = le volume effectif
```

puis l'application, pour chaque fiche dont `+6` vaut le canal **et dont `+4` est non nul** :

```
006019E8  move.b $a(a4),d6                    ; +0x0A de la FICHE : le niveau propre à la voix
006019EC  mulu.w d0,d6                        ; x le volume effectif du canal
006019EE  beq -> moveq #$ff,d6                ; produit nul -> 0xFF
006019F0  lsr.w #8,d6 ; not.b d6 ; lsr.b #1,d6
006019FA  move.b d6,$d(a5)                    ; TL, l'atténuation de la fente
```

`+0x0D` d'une fente est l'octet bas du mot `0x0C`, soit **`TL`**. Donc
**`TL = ¬((volume_canal × niveau_voix) / 256) / 2`**, ce qui le borne à **0..127** — la moitié de la plage —
avec `0xFF` réservé au silence exact. C'est une formule entière vérifiable telle quelle contre le portage.

### CC 10, panoramique : `DISDL` et `DIPAN` dans un octet

Le convertisseur `0x601A98` transforme les 7 bits MIDI en 5 bits SCSP, et c'est exactement l'encodage
`DIPAN` :

```
00601A98  lsr.b #2,d0                   ; 0..127 -> 0..31
00601A9A  cmpi.b #$10,d0 ; bcc -> 601AAA
00601AA0  neg.b d0 ; subq.b #1,d0 ; andi.b #$1f,d0   ; moitié gauche : 0x1F..0x10 (bit 4 posé)
00601AAA  andi.b #$f,d0                              ; moitié droite : 0x00..0x0F
```

MIDI 64 (centre) donne 0 des deux côtés, MIDI 0 donne `0x1F`, MIDI 127 donne `0x0F`. Le résultat va dans les
**5 bits de poids faible du `+6` de l'objet**, et l'application compose l'octet final :

```
00601A64  move.b $6(a0),d0 ; move.b d0,d1
00601A6A  andi.b #$e0,d0 ; lsr.b #5,d0  ; les 3 bits de poids fort : un niveau 0..7
00601A70  add.b $c04.w,d0               ; décalé par la globale 0xC04
00601A74  bpl / clr.b d0                ; négatif -> 0
00601A7A  cmpi.b #7 / ble / moveq #7    ; borné à 7
00601A82  lsl.b #5,d0 ; andi.b #$1f,d1 ; or.b d1,d0
00601A8A  move.b d0,$16(a5)
```

`+0x16` d'une fente est l'octet **haut** du mot `0x16`, soit `DISDL` (bits 7-5) et `DIPAN` (bits 4-0). Le
`+6` de l'objet de canal **est** cet octet, à l'offset `0xC04` près. Et le gestionnaire entier est
court-circuité si `0xEFB` est non nul (`tst.b $efb.w / bne -> rts`).

### Les quatre globales, et où elles sont posées

Trouvées par `--cherche` **après avoir semé les tables de saut dans le marcheur** — voir plus bas, car la
même recherche rendait auparavant un compte faux sans le dire.

| | ce qu'elle fait | posée par |
|---|---|---|
| `0xEE9` | l'échelle de volume maître, `0xFF` au départ (`0x600480`) | `0x6027B2`, `0x602D2A`, et copiée de `0xEEE` en `0x6033F8` |
| `0xEEE` | une copie en attente du maître, prise à **la moitié** (`lsr.b #1`) en `0x60278A` | `0x60278C`, `0x60286A` |
| `0xC04` | le décalage de `DISDL`, posé par une commande à `d0 − 0x40` | `0x60276C` ; sauvegardé/restauré en `0x6001AA`/`0x6001E6` |
| `0xEFB` | non nul ⇒ **CC 10 ne fait rien** ; lu en cinq endroits, tous des `tst.b` | **deux entrées voisines de la table 4** : la 6 (`0x6031B6`) l'efface, la 7 (`0x6031BC`) la met à 1 |

Deux sous-commandes de la banque de contrôle `0x00` qui ne font qu'activer ou désactiver le panoramique, et
qui appellent ensuite `0x602C94` sur les deux puces : **ça ressemble à un commutateur mono/stéréo**. Ce n'est
pas établi — ce qui l'est, c'est l'effet mesuré sur CC 10.

`0xEE9` et `0xC04` sont les deux globales que le portage doit regarder pour `VF3_VOLUMES` : l'une multiplie
le volume de chaque canal non exempté, l'autre décale le niveau d'envoi de chaque voix.

### Qui pose `+5` et `+6` : trouvé, et par l'écriture

Point 2 de la liste de lecture, ouvert depuis le début et **fermé** le 2026-10-25. Les deux écritures
existent, et ce ne sont plus des déductions tirées des lecteurs.

**`+6`, le canal MIDI**, posé à l'allumage d'une note, dans la routine qui remplit une fiche (`A4`) depuis un
objet de canal (`A0`) :

```
00600C50  move.w $ede.w, $12(a4)
00600C56  move.b d2, $7(a4)
00600C5A  move.b $1(a0), $6(a4)     ; <- le +1 de l'objet, c'est-a-dire son canal MIDI
00600C60  move.b $5(a0), $14(a4)    ; et le +5 de l'objet va en +0x14 de la fiche
```

**`+5`, l'indice de l'objet**, dans la queue de la même séquence :

```
006010EA  move.w a0, d0
006010EC  subi.w #$c40, d0
006010F0  lsr.w  #$5, d0            ; (A0 - 0xC40) / 0x20
006010F2  move.b d0, $5(a4)         ; <- exactement le calcul que les gestionnaires utilisent comme cle
006010F6  bset.b #$6, $4(a4)
006010FC  ori.b  #$8, $0(a5)        ; bit 3 de l'octet haut du registre 0x00 : KYONEX
```

**Clé et valeur sont calculées par le même code**, à l'octet près. Ce que `group_of()` fait dans le portage
est donc juste, et le seuil `0x0F` porte bien sur des indices d'objet.

Trois choses se lisent au passage dans cette queue : `andi.b #$7f, $12(a5)` efface le bit 7 de l'octet haut
du mot `0x12`, soit **`LFORE`** — le LFO est lâché au démarrage de la note, ce qui recoupe le masque
`0x7FFF` de CC 1 ; `bset` pose les bits 0 et 6 du `+4` de la fiche, l'octet de drapeaux ; et `ori.b #$8` sur
l'octet haut du registre `0x00` est **`KYONEX`**, donc la note partait là.

### Et pourquoi le marcheur rendait un compte faux

`--cherche ee9` donnait **4 emplois**. Il en donne **9**. La différence n'est pas un bug de la recherche :
le parcours récursif s'arrête sur un `jmp (An)`, donc **tout ce qui ne s'atteint que par une table de saut
était invisible** — y compris les trois gestionnaires de cette section. La recherche rendait un compte
complet en apparence sur la moitié du programme.

`tools/sound/desas_son.py` sème maintenant les onze sauts calculés (les quatre tables de mots, les quatre
tables de créneaux, les quatre entrées de bloc déroulé), ce qui porte le parcours de 95 à **184 fonctions et
4 453 instructions**. Le témoin : `0x6019A6`, la lecture de `0xEE9` par CC 7, que j'avais lue à la main et
que l'ancien parcours ne trouvait pas — elle apparaît. Et `--cherche` **imprime sa portée avant ses
résultats**, parce qu'un compte rendu par un parcours incomplet ressemble à un compte complet.


## 5 ter. Les 168 fonctions sont lues — et voici ce que ça ne veut pas dire

Mesuré le 2026-10-25, en croisant la liste des corps que Ghidra tient avec les adresses que ce document
cite : **168 fonctions sur 168, 14 488 octets sur 14 488.** Chaque fonction du programme sonore a été
ouverte et décrite quelque part ci-dessus.

**La mesure et sa méthode**, pour qu'on puisse la refaire et la contester :

```
tools/ghidra/FV2SonListe.java   -> FONCTION <adresse> <octets> et PLAGE <min> <max>
puis : toute adresse 0x6xxxxx citee dans ce document est rattachee a la fonction
       dont une PLAGE la contient ; on somme les octets des fonctions ainsi touchees
```

Elle compte **« citée »**, et c'est une borne haute, pour trois raisons qu'il faut dire :

1. **`snd_reset` pèse 2 730 octets — 19 % du programme — et n'est lue qu'en morceaux.** L'effacement de la
   zone DSP, les quatre blocs déroulés, les boucles d'initialisation, la queue de l'allumage de note : oui.
   La fonction entière, ligne à ligne : non. En la retirant des deux côtés du rapport, la part lue tombe à
   **88 %**, et c'est le chiffre le plus honnête des deux.
2. **Citer une fonction n'est pas l'expliquer.** Plusieurs des soixante dernières sont décrites par ce
   qu'elles écrivent et où, sans que le sens du champ soit établi.
3. La mesure ne dit rien du **programme du DSP** lui-même, ni des données : séquences, instruments,
   échantillons, microcode MPRO.

### Ce qui reste explicitement non établi

| | |
|---|---|
| `0x2000-0x2FFF` | **identifié le 2026-10-25** : une **banque de seize formes d'onde** d'un cycle, 128 mots chacune, treize distinctes — une sinusoïde à 5 près, deux carrés, une constante, neuf formes lisses, trois bruits. Chargée de la ROM dans les deux RAM, à une adresse que l'allocateur d'échantillons ne touche jamais. **Son consommateur est mesuré** : 37 fentes y pointent par leur `SA`, une seule forme sur seize, dans une seule arène — une voix FM dont le pilote écrit lui-même la modulation. Voir 4 septies |
| ~~octets `0x16` et `0x17` d'une fente~~ | **établi** : `0x16` porte `DISDL`/`DIPAN` (sortie directe), `0x17` porte `EFSDL`/`EFPAN` (envoi vers le DSP) — les quatre dans le même mot. Ma disposition était fausse. Voir 5 quinquies |
| ~~`+0x0C` d'un objet de canal~~ | **établi le 2026-10-25** : `IMXL` bits 2-0 (CC `0x2A`) et `ISEL` bits 6-3 (CC `0x2B`), bit 7 inutilisé — l'image de l'octet `0x15` d'une fente. Le « chevauchement sur le bit 7 » que j'avais noté venait d'avoir lu les masques d'écriture sans lire le bornage de `D0`. Voir 5 bis undecies |
| `+0x0B` d'une fiche d'instrument | **`+0x10` et `+0x11` sont établis** : ils alimentent la table `0xF00` de la modulation logicielle (5 quinquies). `+0x0B` reste écrit par deux contrôleurs et **relu par rien** |
| ~~`0x600396`, `0x60103C`~~ | **établies** : `0x600396` est le dictionnaire de seize mots d'un décompresseur, `0x60103C` une courbe de durée de relâchement que le note off lit dans le `+0x0E` d'une fiche. Voir 5 quinquies |
| `0xEFE` | **trois des quatre sont établies** (5 quinquies) : `0xF80` est la molette de hauteur par canal, `0xC3F` un MIDI thru, `0xED8` un masque de blocs réservés nul sur cette ROM. `0xEFE` est **écrit une fois et lu nulle part** |
| ~~les 512 octets des dispositifs de Duff~~ | **établi** : un morceau d'échantillon en **streaming**, source dans la ROM banquée, destination dans le `+8` d'un des 32 descripteurs de `0x1000`, avancée à chaque passe. Voir 5 quinquies |
| `0x6003B6` | le **sens est établi** — il coupe `EFSDL`/`EFPAN`, appelé par la routine qui efface la zone DSP — mais **le compte de 18 reste inexpliqué**. Sans effet audible : le DSP vient d'être mis à zéro (5 quinquies) |
| ~~laquelle des deux catégories de voix est la musique~~ | **mesuré** : la catégorie **B est la musique**, la **A les effets** — 1 756 contre 40, indices d'objet `0x00`-`0x09` contre `0x0F`-`0x13`, aucun contre-exemple. Et la musique est donc **volée avant** les effets. Voir 5 quinquies |
| le comportement du matériel sur `0x7C0-0x7FF` | deux lignes de preuve sur ce que le **pilote** fait ; rien sur la puce. **Hors de portée de toute lecture** : il faudrait un Model 3 et une mesure |

### Ce que la lecture a rendu au portage

Quatre résultats qui touchent le code, et non seulement la documentation :

1. **`group_of()` est juste, et ne repose plus sur une déduction** : la boucle qui pose le premier long d'une
   fiche est `0x60048C`, et l'écriture du `+5` est `0x6010F2`.
2. **Le `+6` d'une fiche est le canal MIDI**, que le portage ne connaissait pas — noté dans
   `src/sound/soundboard.h`.
3. **`TL` et la hauteur ont chacun deux formules**, selon qu'on allume une note ou qu'on bouge un contrôleur.
   > **Et ce point ne touche PAS le portage d'aujourd'hui.** Je l'avais écrit « un portage qui n'en
   > implémente qu'une s'entendra » : c'est faux. `src/sound/soundboard.cpp` instancie un `m68000_device`
   > sur la ROM du pilote et l'exécute — **les deux formules tournent donc telles que Sega les a écrites**,
   > rien n'est réimplémenté et rien ne peut diverger. Ce que ce résultat contraint, c'est **un cœur
   > autonome futur** : le jour où le SCSP ou le pilote seraient réécrits, il faudra les deux chemins.
   > Corrigé le 2026-10-25, après avoir vérifié ce que le portage exécute au lieu de le supposer.
4. **Deux voies de sortie MIDI sont jetées** par `h.midi_out = [](int) {}` : l'octet `0xF0` périodique de
   `snd_irq2` et la recopie par octet de `0x603718`/`0x60361E`. Si la carte principale les lit, ça compte.

### Et deux mesures réconciliées

Les écritures en `+0x600` — le tampon FM — que ma sonde avait comptées à **zéro** sur 4 200 images de
*Fighting Vipers 2* et 2 400 de *Virtua Fighter 3*, témoin positif à l'appui. `0x60155C` est le code qui en
ferait. Il n'est atteint que par onze contrôleurs précis. **Les deux faits tiennent ensemble** : la
fonctionnalité existe, ces scènes ne l'emploient pas. C'était la seule mesure de ce document qui semblait
contredire une lecture.


## 5 quinquies. Les huit points en suspens, pris un par un

Le §5 ter listait dix choses non établies ; deux ont été fermées dans la journée. Voici les huit autres,
traitées le 2026-10-25. **Six se ferment, deux restent partielles, une est hors de portée de la lecture.**

### Les octets `0x16` et `0x17` d'une fente : ma disposition était fausse

Je notais que « CC 10 écrit l'un, `0x602EB0` l'autre, tous deux avec un panoramique, et la disposition que
j'ai des registres n'explique pas les deux ». L'erreur était dans ma disposition. Les macros du portage
(`src/sound/scsp.cpp`) mettent **les quatre champs dans le même mot** `data[0xB]`, soit l'offset `0x16` :

| | |
|---|---|
| `DISDL` bits 15-13, `DIPAN` bits 12-8 | l'**octet `0x16`** — la sortie directe |
| `EFSDL` bits 7-5, `EFPAN` bits 4-0 | l'**octet `0x17`** — l'**envoi vers le DSP** |

Donc CC 10 règle le panoramique **direct** (`move.b d0, $16(a5)` avec `(niveau<<5) | (pan & 0x1F)`) et
`0x602EB0` le panoramique de l'**effet** (`andi.b #$e0, $17(a5)` qui garde `EFSDL`, puis le même
convertisseur `0x601A98` dans `EFPAN`). Deux panoramiques, deux destinations, aucune contradiction. J'avais
placé `EFSDL`/`EFPAN` au registre `0x18` de mémoire ; ils sont dans l'octet bas de `0x16`.

### `0x6003B6` et ses dix-huit fentes

Le sens est désormais clair : il efface l'octet `0x17`, donc **`EFSDL` et `EFPAN`** — il coupe l'envoi vers
le DSP. Et son appelant le dit : `0x602AD6`, la routine qui efface la zone DSP d'un trait (§5). Couper les
envois en même temps que l'effet est cohérent.

**Le compte de 18 reste inexpliqué** (`moveq #$11, d7`), et aucun des trois plafonds de polyphonie ne vaut
18 — ils valent tous `0x20`. Ce qu'on peut dire : l'omission est **sans effet audible**, puisque la même
routine vient de mettre tout le programme du DSP à zéro, et qu'un envoi vers un DSP nul ne va nulle part.
C'est pourquoi ça n'aurait jamais été remarqué. Je ne l'appelle pas pour autant intentionnel.

### Les 512 octets des dispositifs de Duff : un morceau d'échantillon en streaming

```
00601F1C  move.b d1, $400001.l        ; le registre de banque
00601F22  movea.l $4(a6), a1          ; +4 du descripteur : la SOURCE dans la ROM d'echantillons
00601F26  move.l #$80000001, $c(a6)   ; +0x0C : un marqueur d'etat
00601F2E  move.w #$200, d0 ; sub.w d5, d0 ; lsr.w #$1, d0 ; lea ... ; jmp (a0)
...                                    ; D5 octets copies, puis 512 - D5 effaces
00602144  move.l a2, $8(a6)           ; +8 : la DESTINATION, avancee
00602148  bra.w $601cc0               ; retour a la boucle principale
```

`A6` est le curseur sur les **32 descripteurs de copie d'échantillons de `0x1000`** (§2), parcourus en
tourniquet par la boucle principale. Donc chaque passe copie **au plus 512 octets d'un échantillon** de la
ROM banquée vers la RAM sonore, la destination étant gardée dans le `+8` du descripteur et avancée. C'est du
**streaming**, et le remplissage de zéros traite le dernier morceau partiel.

### Les deux tables de données

**`0x600396` est le dictionnaire d'un décompresseur.** `0x6002EC` lit des octets d'un flux : bit 7 clair, il
émet cet octet **et le suivant** dans un tampon `(a0)+` ; bit 7 posé, c'est un code, et le `jmp` de
`0x600308` le distribue sur huit branches — dont celle qui émet un des **seize mots** de `0x600396`. Un
dictionnaire des valeurs fréquentes, donc, dans un format compressé par octets.

**`0x60103C` est une courbe de durée de relâchement.** Les 32 mots décroissants vont dans le `+0x0E` d'une
fiche de voix, et le note off les lit : `0x6012B4` prend `+0x0E`, et `+0x10` garde le **minimum** des valeurs
vues (`cmp.w $10(a4), d0` puis `move.w d0, $10(a4)`). Un compte à rebours de relâchement, indexé par un
compteur qui avance d'un pas à la fois.

### Les quatre globales

| | |
|---|---|
| **`0xF80`** | la **molette de hauteur par canal MIDI**, un mot chacun : `0x60134A` l'écrit (`move.w d2, (a4, d1.w)` avec `d1 = canal × 2`), et **l'allumage d'une note la lit** (`0x600C9A` : `move.w (a1, d5.w), d5` puis `bsr 0x601390`, le convertisseur `OCT`/`FNS`). `0x601BD2` l'efface sur CC `0x50`, `0x603172` à l'arrêt d'une voix |
| **`0xC3F`** | un **MIDI thru** : `0x6005C8`, dans `snd_irq3`, fait `tst.b $c3f.w` puis `move.b d0, $100407.l` — chaque octet reçu est renvoyé tel quel en `MOBUF`. Posé et effacé par deux commandes (`0x603274`, `0x60327C`). C'est la **troisième** voie de sortie MIDI du pilote |
| **`0xED8`** | un **masque de 32 bits**, un par bloc de `0x2000` de RAM sonore : `0x600D86` le parcourt bit à bit et retire `0x2000` de l'adresse pour chaque bit posé — il **saute les blocs réservés**. Il vaut `0xFF >> [0x60807D]`, et comme le plafond vaut 32, **le masque est nul sur cette ROM** : rien n'est réservé |
| **`0xEFE`** | **écrit une fois** (`0x602F9A : move.w #$8000, $efe.w`) et **lu nulle part** dans tout le code atteint. Reste non établi |

### Les trois paires de la fiche d'instrument

`+0x10` et `+0x11` **alimentent la table `0xF00`**, celle de la modulation logicielle (§4 bis) :

```
00600D38  move.b $10(a1), d5 ; move.b $11(a1), d6      ; l'instrument
00600D40  move.w a5, d1 ; lsr.w #$3, d1                ; a5 = registres de la fente -> fente x 4
00600D44  lea.l $f00.w, a1 ; adda.l $c10.w, a1
00600D4C  move.b d0, (a1, d1.w)                        ; +0
00600D50  move.b d5, $1(a1, d1.w)                      ; +1 <- l'instrument +0x10
00600D54  move.b d6, $3(a1, d1.w)                      ; +3 <- l'instrument +0x11
00600D62  move.b d0, $2(a1, d1.w)                      ; +2, mis a zero si +0x11 vaut 0x80
```

Les deux sont donc les **paramètres de modulation logicielle de l'instrument**, et les paires
(drapeau bit 7, valeur bits 6-0) que les huit petits gestionnaires éditent (§5 bis quaterdecies) sont leurs
réglages par contrôleur. **`+0x0B` reste non lu** : les contrôleurs l'écrivent, rien dans le code atteint ne
le relit par `A1`.

### Laquelle des deux catégories de voix est la musique : mesuré

La question demandait une mesure, pas une lecture : le canal MIDI d'une voix est dans sa fiche, et sa
catégorie dans deux bits de marquage. Mettre les deux sur une ligne répond par corrélation.
`tools/vf3run.cpp` reçoit donc `VF3_FICHES=file`, qui imprime à chaque image les fiches dont le `+4` n'est
pas nul. Scène `fv2-fight.txt`, arène 2, 3 000 images, **1 807 allocations distinctes** :

| catégorie | indices d'objet (`+5`) | canaux MIDI (`+6`) | puce | compte |
|---|---|---|---|---|
| **A** (`+0xB` bit 7, `+4` bit 3) | **`0x0F`-`0x13`** | `0x0B`-`0x0F` | la **puce 1** (36 sur 40) | 40 |
| **B** (sans marque) | **`0x00`-`0x09`** et `0xFF` | `0x00`-`0x09` | la **puce 0** (1 765 sur 1 767) | 1 767 |

**La catégorie A est celle des effets, la catégorie B celle de la musique.** Les indices d'objet de A sont
exactement les cinq que `group_of()` appelle effets — `0x0F` à `0x13` — et 36 des 40 sont sur le **second
SCSP**, ce que `soundboard.h` dit déjà des effets, des voix et de l'annonceur. Les 11 cas de B classés
« effets » par le test naïf sont les `+5 = 0xFF`, le marqueur d'une fiche dont l'indice n'est pas encore
posé : **aucun contre-exemple réel**.

**Et ça oriente enfin la conséquence du §5 bis decies.** Le `+0xB` de la catégorie A a son bit 7 forcé, donc
toute valeur de A est `≥ 0x80` et toute valeur de B `< 0x80` : la recherche du minimum dans le voleur de voix
**prend toujours une voix de B avant une de A**. Autrement dit **la musique est volée avant les effets**. Pour
un jeu de combat c'est le bon choix — les coups, les voix et l'annonceur ne doivent pas tomber — et c'est
maintenant mesuré et non deviné.

Ça recoupe aussi le trio d'arrêts : l'entrée 2 de la table 4 efface la piste 0 et arrête B, donc **arrête la
musique** ; l'entrée 3 efface les pistes 1-7 et arrête A, donc **arrête les effets**. Et `snd_irq2` traite la
piste 0 à part, avec ses trois crochets de minutage : c'est le séquenceur de la musique.

### Ce qui reste hors de portée

**Le comportement du matériel sur `0x7C0-0x7FF`.** Ce document a deux lignes de preuve sur ce que le
**pilote** fait — un seul balayage linéaire qui détruirait tout miroir, et un interprète de paramètres dont
les 59 adresses n'y entrent jamais — et **rien sur la puce**. Aucune lecture de ce programme ne peut y
répondre : il faudrait un Model 3 et une mesure. C'est noté comme tel et non comme ouvert.


## 6. L'outillage, et comment refaire tout ça

| | |
|---|---|
| extraire la ROM et remettre les octets d'aplomb | `tools/sound/extraire-son.py <jeu.zip> <sortie.bin>` |
| importer dans Ghidra et analyser, sans interface | `tools\son\import-sound.cmd` |
| semer les points d'entrée avant l'analyse | `tools/ghidra/FV2SonSetup.java` (en `-preScript`) |
| **chercher** dans tout le programme | `tools/sound/desas_son.py <image> --cherche 100600 1006ff` |
| lire une fonction depuis une frontière sûre | `tools/sound/desas_son.py <image> --at 600780 26` |
| la table des vecteurs | `tools/sound/desas_son.py <image> --carte` |
| ouvrir les quatre tables de saut à Ghidra | `tools/ghidra/FV2SonTablesDeSaut.java` |
| résoudre les cinq derniers `jmp (An)` | `tools/ghidra/FV2SonSautsIndirects.java` |
| déclarer les données, rattacher les blocs déroulés, recompter | `tools/ghidra/FV2SonDonnees.java` |
| sortir les fonctions et leurs plages, sans rien changer | `tools/ghidra/FV2SonListe.java` |

Lancer un de ces scripts sans interface, sur le projet déjà importé :

```
"...\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat" C:\Temp\fv2-ghidra FV2SON ^
  -process fv2-son-68000.bin -noanalysis -scriptPath <dépôt>\tools\ghidra -postScript <Script>.java
```

`-readOnly` en plus pour ceux qui ne doivent rien écrire (`FV2SonListe`). Et **filtrer la sortie avec
prudence** : trois fois dans cette campagne un `Select-String` trop étroit a avalé la moitié du dump pendant
que le total arrivait quand même, ce qui ressemble à un résultat. Les scripts impriment leurs comptes pour
qu'on puisse les confronter au nombre de lignes reçues.

Le projet Ghidra est `C:\Temp\fv2-ghidra\FV2SON.gpr`, **à part de `FV2.rep`** (le PowerPC) : ce n'est ni le
même processeur ni le même programme, et un projet ouvert dans l'interface est verrouillé pour l'analyseur
sans interface.

Partage des rôles entre les deux outils, et ce n'est pas un doublon : **Ghidra pour lire** une fonction (il
suit les tables de saut, `desas_son.py` non), **`desas_son.py` pour chercher** dans les 512 Ko depuis une
ligne de commande, de façon refaisable, sans projet. Les deux réponses dont ce portage avait besoin — les
fiches à `0x400`, l'écriture en `+0x600` — sont venues de la recherche.


## 7. Ce que la suite doit lire, par ordre d'intérêt

> **Cette liste est close pour la lecture du code** : les 168 fonctions sont ouvertes (§5 ter), et les **cinq
> premières entrées ci-dessous sont faites**, la dernière d'entre elles par une mesure le 2026-10-25. Ce qui
> reste n'est plus « quelle fonction lire » mais **quel sens établir** — le tableau du §5 ter en donne la
> liste. **Les douze arènes ont été mesurées** (§4 sexies), et c'est ce passage-là qui a retourné la
> cinquième entrée : il n'y a plus de question de cette liste qui attende de tourner.

1. ~~**La fonction qui contient `0x602BA2`**~~ — **faite le 2026-10-25**. C'est un interprète piloté par
   descripteurs, et ses 59 adresses sont énumérées : `0x714` à `0x794`, **aucune** dans `0x7C0-0x7FF`.
   Voir 5 bis bis. Ce que le matériel fait de ces 64 octets reste hors de portée de la lecture.
2. ~~**Qui pose l'octet `+5` d'une fiche**~~ — **fait le 2026-10-25**, et par l'écriture : `0x6010F2` pour
   `+5` (l'indice de l'objet, `(A0 − 0xC40) >> 5`), `0x600C5A` pour `+6` (le canal MIDI). Voir 5 quater.
3. ~~**`snd_irq2` (`0x6032D2`)**~~ — **fait le 2026-10-25**. C'est le timer A, le tic de la musique à
   `0xCE`. Il met les tics en file plutôt que de les perdre (`0xC01` verrou, `0xC1E` compteur de différés),
   et il envoie `0xF0` en MOBUF tous les dix tics — que le portage jette. Voir 5 bis ter.
4. ~~**Le `FF` qui relance un son avant de le couper**~~ — **expliqué au §3 ter** : il redémarre la carte,
   intercepté dans `snd_irq3_reception_midi` avant tout. Cette entrée de liste disait encore « jamais
   expliqué » alors que le §3 ter le lisait : corrigé le 2026-10-25. Et le consommateur le confirme de
   l'autre bord — son créneau `0xFn` est un `rts`, parce que rien ne met jamais un message système dans
   l'anneau.
5. ~~**Si le programme DSP change par arène**~~ — **mesuré sur les douze arènes le 2026-10-25, et la
   réponse est OUI pour trois d'entre elles.** Les arènes **6** et **9** chargent sur la puce 0, vers
   l'image 2730, un programme (`efff4af1`) que les neuf autres ne voient jamais ; l'arène **11** en
   recharge un existant. Et les coefficients varient en **cinq jeux** sur les douze. Voir §4 sexies — qui
   garde aussi l'erreur : avec deux arènes seulement, la 2 et la 7, tout paraissait identique, et les deux
   sont dans le groupe de neuf qui ne charge rien de plus.
6. **Ce que les deux dispositifs de Duff copient** (`0x601F3C`, `0x6023E2`) : 512 octets au plus, `A1` vers
   `A2`, et les deux sont dans `snd_reset`. Deux copies déroulées à la main au même endroit du démarrage,
   c'est l'installation de quelque chose de volumineux — une table ou un programme DSP. La taille `D5` et
   les deux pointeurs le diraient.
7. **Les deux tables dont le sens manque** : `0x600396` (seize mots émis dans un flux par le distributeur
   de `0x6002EC`) et `0x60103C` (trente-deux mots décroissants, indexés par un compteur, écrits en
   `0x0E(A4)`). La seconde ressemble à une durée par pas d'enveloppe ; **ce n'est pas établi**, et ce qui le
   trancherait est l'identité du champ `+0x0E` de la fiche pointée par `A4`.
8. **Ce que sont les 512 octets** que la paire copie-puis-efface installe deux fois dans `snd_reset`. C'est
   la même question que le point 6, et elle est maintenant chiffrée : `D5` octets depuis `(A1)`, `512 − D5`
   zéros, un tampon de `0x200`. Les deux pointeurs la trancheraient.
