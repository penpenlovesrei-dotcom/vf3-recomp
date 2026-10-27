# Le SCSP autonome — contrat d'interface et découpage

> Ce document est l'un des neuf du son. La porte d'entrée, qui dit où est quoi : [docs/son.md](son.md).

> **Mise à jour du 2026-10-25.** Ce document décrit l'**interface C++**. Le second morceau du contrat —
> ce que le pilote attend de la **puce**, champ par champ, avec la provenance de chaque nombre — existe
> maintenant à part : `src/sound/scsp_contract.h`, vérifié par `tools/son/test_scsp_contract.cpp` contre
> l'implémentation et contre la ROM. Les deux ensemble sont le préalable que `docs/scsp_migration_plan.md`
> appelait « le contrat d'interface ».

**Ceci est un plan, pas une description.** Rien de ce qui suit n'est écrit à ce jour (2026-10-24) : le cœur du
son est toujours celui de MAME, modifié (`docs/scsp_audit.md` §2). Ce document existe pour que, le jour où on
s'y mettra, le premier geste ne soit pas de deviner.

Pourquoi un cœur autonome : objectif A du cahier des charges — sortir progressivement du code tiers. Ce n'est
**pas** un objectif de vitesse (les gains mesurés du 2026-10-24 ont été obtenus sur le cœur de MAME tel quel,
sans en changer une sortie), et ce n'est **pas** un objectif de fidélité (le code de MAME est aujourd'hui la
référence de fidélité de ce portage, mesurée jeu par jeu).


## 1. L'interface d'aujourd'hui, telle qu'elle est

Avant de dessiner la suivante, il faut lire celle-là. Ce que `SoundBoard` demande au `scsp_device`, et
réciproquement, est exactement :

**La carte vers le chip** (appels) :

| | |
|---|---|
| construction | `scsp_device(u32 clock, u16 *ram, u32 ram_bytes, const host &)` — la RAM est **partagée** avec le 68000, pas copiée |
| registres | `u16 read(offs_t offset)` / `void write(offs_t offset, u16 data, u16 mem_mask)`, en mots depuis la base du chip |
| MIDI | `midi_in(int state)` (un bit) et `midi_in_byte(u8)` (un octet, la voie réellement utilisée) |
| timers | `timer_fired(int timer)` — 0-2 les timers A/B/C, 3 l'horloge du récepteur, 4 celle de l'émetteur |
| temps | `update()` — « fabrique les échantillons jusqu'à maintenant » |
| sortie | `std::vector<float> m_out` (paires stéréo, −1..1, avant le gain), `output_gain()`, `samples_made()` |

**Le chip vers la carte** (`scsp_device::host`, aujourd'hui cinq `std::function`) :

| | |
|---|---|
| `irq(line, state)` | les lignes IPL du 68000 (seul le premier chip y est relié) |
| `midi_out(state)` | TxD vers la carte principale |
| `timer_adjust(timer, delay, period)` | armer/désarmer, en attosecondes, `period != 0` = périodique |
| `sample_index()` | l'échantillon courant (`now * 44100`) |
| `time_is_zero()` | vrai juste après la mise sous tension |

Ce que cette interface **traîne de MAME**, et qu'un cœur autonome n'a aucune raison de garder :

1. **Les attosecondes.** `timer_adjust` parle en `unsigned __int128` d'attosecondes, que `soundboard.cpp`
   reconvertit aussitôt en cycles du 68000, et arrondit à l'échantillon pour les timers A-C. Deux conversions
   pour rien : l'unité naturelle de cette carte est **l'échantillon** (et le cycle 68000, qui en vaut 1/256).
2. **Le flux tiré.** `update()` est le `sound_stream::update` de MAME : le chip fabrique « jusqu'à maintenant »
   quand on le touche. C'est ce qui fait que les échantillons naissent **à l'intérieur** des tranches du 68000
   et que la mesure a été difficile à découper (`docs/scsp_audit.md` §1). Le modèle n'est pas faux — il est
   même ce qui donne le bon grain quand le 68000 écrit un registre au milieu d'un échantillon — mais il doit
   être **choisi**, pas hérité.
3. **Les cinq `std::function`.** Deux d'entre elles (`sample_index`, `time_is_zero`) sont appelées à chaque
   accès registre, donc des milliers de fois par image, pour renvoyer un entier. Un appel indirect par
   question.
4. **`device_serial_interface`.** Le récepteur et l'émetteur série de MAME (`emu/diserial.cpp`) sont recopiés
   dans `scsp.h` pour une seule liaison MIDI à 31250 bauds, 8N1, dont ce portage n'utilise que le chemin
   « un octet entier » (`midi_in_byte`). Une trentaine de membres et sept méthodes pour ça.


## 2. Le contrat proposé

Une seule règle de découpage : **le cœur ne connaît ni le PC, ni MAME, ni le 68000.** Il connaît des
échantillons, des registres et de la RAM.

```cpp
// Pas de std::function, pas d'attosecondes, pas de framework. L'unite de temps est l'echantillon.
class scsp_core
{
public:
    struct sink {                                   // ce que la carte branche dessous
        virtual void irq(int level) = 0;            // le niveau demande (0 = rien), pas une ligne a la fois
        virtual void midi_out(u8 byte) = 0;         // un octet, pas un bit
        virtual void timer(int which, u32 samples, u32 period) = 0;   // 0 = desarmer
    };

    scsp_core(u16 *ram, u32 ram_bytes, sink &);
    void reset();

    // Les registres. L'appelant garantit que le temps est a jour (voir run) avant d'y toucher.
    u16  read_reg(u32 word_offset) const;
    void write_reg(u32 word_offset, u16 data, u16 mem_mask);

    // Le temps, pousse : fabrique exactement n echantillons dans out (paires stereo, s32 20 bits
    // avant le DAC, comme le chip les a). L'appelant decoupe ses tranches ou il veut -- a
    // l'echantillon s'il veut le grain de MAME, plus large s'il s'en passe.
    void run(u32 n, s32 *out);

    void timer_fired(int which);
    void midi_in(u8 byte);
};
```

Ce que ce contrat change, et pourquoi :

| | aujourd'hui | proposé | pourquoi |
|---|---|---|---|
| temps | tiré (`update()` interroge l'hôte) | **poussé** (`run(n)`) | le cœur n'a plus besoin de savoir l'heure ; la granularité redevient une décision de la carte, écrite une fois dans `soundboard.cpp` |
| unité | attosecondes → cycles → échantillons | **échantillons** | l'unité du matériel ; plus d'arrondi à recoller |
| rappels | 5 `std::function` | **une interface, 3 méthodes** | un appel virtuel par événement rare au lieu d'un appel indirect par accès registre |
| sortie | `std::vector<float>` qui pousse et qu'on vide | **un tampon que l'appelant fournit** | zéro allocation dans le chemin temps réel, et le flottant arrive à la fin, une fois |
| série | `device_serial_interface` recopié | **un octet dedans, un octet dehors** ; les bauds et les bits restent dans un petit `midi_uart` à part | la liaison du Model 3 ne transporte que des octets ; les bits ne servent qu'à la cadence des interruptions, qui est la seule chose à garder |

**Le 68000 et la carte Model 3 restent dehors.** `soundboard.cpp` garde la carte (carte mémoire, banque,
timers, cadence), `third_party/mame/m68000` garde le 68000. Le cœur SCSP ne dépend d'aucun des deux, ce qui est
la condition pour le tester seul.

**Et l'API audio du PC n'entre nulle part.** Elle n'y entre pas aujourd'hui non plus (`SoundBoard::run` rend des
`int16_t`, l'hôte fait le reste) : rien à défaire.


## 3. Les blocs, et dans quel ordre les écrire

Un bloc par chose qui se teste seule. L'ordre est celui de la dépendance, pas celui de l'importance.

1. **Registres et sémantique d'accès** — la carte des 0x000-0xFFF : les 32 fentes (0x20 octets chacune), le
   commun (0x400-0x45F), les registres DSP (COEF 0x700, MADRS 0x780, MPRO 0x800, TEMP, MEMS, MIXS, EFREG,
   EXTS). Y compris les bits réservés, ce qui se relit, et **la plage 0x7C0-0x7FF**, dont ce portage et MAME ne
   font pas la même chose (`docs/scsp_references.md`). Se teste sans aucun son : écrire, relire.
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_regs.cpp`, neuf cas, et il a déjà rapporté
   deux choses qu'aucune lecture n'avait vues — l'exception « bit 5 » de MAME tombe sur MCIRE (0x42E) et non
   sur MCIPD (0x42C), que le commentaire nomme ; et la plage 0x7C0-0x7FF, mesurée dans les **deux** réglages
   du miroir MADRS plutôt que dans celui qui est par défaut.
2. **Fentes, oscillateur, PCM, adressage, bouclage** — 8 et 16 bits signés, `SA`/`LSA`/`LEA`, les quatre modes
   `LPCTL`, l'interpolation linéaire, les limites de la RAM. Se teste en comptant des échantillons.
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_fentes.cpp`, onze cas, et il a démenti la
   lecture sur les quatre bouclages à la fois. La boucle de bouclage tourne sur `cur_addr` **et** `nxt_addr`,
   mais `nxt_addr` est réaffecté à chaque échantillon : sa réécriture d'adresse est perdue, ses effets de bord
   (`Backwards`, `StopSlot`) ne le sont pas. Tout se déclenche donc **un échantillon tôt**, et aucun échantillon
   au-delà de `LEA` n'est jamais lu — contrairement à ce que la lecture seule prédisait (`scsp_contract.h` §9).
3. **Enveloppes ADSR** — les quatre états, `AR`/`D1R`/`D2R`/`RR`, `DL`, `EGHOLD`, `KRS`, `LPSLNK`, et les
   tables de temps. **Attention** : les tables `ARTimes`/`DRTimes` de MAME sont des constantes mesurées, pas
   une formule ; les reprendre est un choix de fidélité à assumer explicitement.
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_adsr.cpp`, dix cas. Il fixe les durées d'attaque
   **en échantillons** (1297, 332, 83, 24, 1 pour `AR` = 0x10 à 0x1F), si bien qu'une formule à la place des
   tables échoue là et nulle part ailleurs — l'avertissement ci-dessus devient une mesure. Il montre aussi que
   l'attaque multiplie le **volume brut** quand tout le reste passe par la courbe en décibels, et que `DECAY1`
   décrémente toujours une fois même quand `DL=0` le fait céder aussitôt (`scsp_contract.h` §10).
4. **Pitch, LFO, modulation FM** — `OCT`/`FNS`, les quatre formes de LFO, `PLFOS`/`ALFOS`, et la modulation par
   anneau (`MDL`/`MDXSL`/`MDYSL`). **Les réglages de ce portage se gardent tels quels** jusqu'à mesure
   contraire : profondeur FM à 24 et bit bas de `MDL` ignoré (`docs/scsp_audit.md` §2).
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_lfo.cpp`, onze cas, et il fixe les deux réglages
   nommés ci-dessus plutôt que de les laisser à la bonne volonté d'un relecteur. Il a aussi trouvé que **huit
   des trente-deux cadences de LFO ne font rien** — `phase_step` tronque à zéro en dessous de 0,673 Hz — et que
   `LFORE` n'est lu nulle part dans ce cœur (`scsp_contract.h` §11).
5. **Le DSP d'effets** — 128 pas, `TEMP`/`MEMS`/`MIXS`/`EFREG`, `PACK`/`UNPACK`, la ligne de retard dans la RAM
   son, l'accès mémoire sur pas impairs. C'est **le bloc le plus coûteux** (la moitié de la carte son) et celui
   qui a le plus à gagner d'être écrit pour ce qu'il est : un programme fixe de 112 pas exécuté 44 100 fois par
   seconde. La forme décodée de ce dépôt (`Inst`, `Inst::f`) est déjà au-delà de MAME et de Supermodel, qui
   décodent dans la boucle ; la suite naturelle est une forme compilée par programme, pas un interpréteur plus
   serré.
   **Le seul bloc que `scsp_maison` n'a pas écrit**, et la raison n'est pas le temps : son jeu d'instructions
   n'existe nulle part écrit. Le contrat n'en donne que la disposition (§6), et `docs/scsp_references.md` est
   clair — il n'y a ni fiche technique ni spécification indépendante, seulement les implémentations de MAME et
   de Supermodel, issues du même travail d'ElSemi. Écrire ce bloc demande donc d'abord un **test qui mesure**
   le comportement instruction par instruction, comme pour les six autres. Tant qu'il n'existe pas, il n'y a
   rien contre quoi écrire, et le cœur emprunte `scspdsp.{h,cpp}`.
6. **Timers, interruptions, synchronisation** — A/B/C, `SCIEB`/`SCIPD`/`SCILV`, `MCIEB`/`MCIPD`, le DMA, et la
   cadence des interruptions, dont Mednafen dit qu'elle est ce qui fait dériver le tempo de 2 %
   (`docs/scsp_references.md`).
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_irq.cpp`, onze cas, mesurés par le puits
   plutôt que par le son. Il fixe le `+1` de `timer_samples` — la troisième divergence voulue du portage, celle
   qui a remis le tic de VF3 à 882 Hz — et il a trouvé que la **ligne 0 ne retombe pas quand rien n'est en
   attente** : `CheckPendingIRQ` sort avant, et le `-0` n'arrive que sur une source en attente que rien n'arme
   (`scsp_contract.h` §12).
7. **Mélange et rendu** — `DISDL`/`DIPAN`, `EFSDL`/`EFPAN`, `MVOL`, `DAC18B`, les tables de panoramique. Les
   deux volumes par groupe et le niveleur d'effets de ce portage sont **au-dessus** du chip, pas dedans : ils
   restent dans la carte.
   **Le test existe** depuis le 2026-10-26 : `tools/son/test_scsp_mix.cpp`, neuf cas. Il lit les trois tables de
   décibels en entier, et il a établi que **`MVOL` n'est jamais appliqué dans la puce** — elle l'expose seulement,
   et la carte multiplie — ce qui est un **trou dans l'interface** : `scsp::core` ne sait pas rendre `MVOL`,
   donc la carte descend par `->device()` (`scsp_contract.h` §13).


## 4. Ce que la Model 3 a de particulier, et qu'une doc Saturn ne dira pas

À garder sous les yeux, parce que la tentation sera de lire une documentation Saturn et de l'appliquer :

- **Deux SCSP**, pas un. Chacun sa RAM de 1 Mo, chacun son DSP de 128 pas (les deux tournent : mesuré, 112 pas
  chacun pendant l'attraction de Fighting Vipers 2). **Seules les interruptions du premier atteignent le
  68000** ; celles du second ne vont nulle part.
- **Le 68000 est à 11,2896 MHz** (45,1584 / 4), soit exactement **256 cycles par échantillon** à 44,1 kHz.
  C'est ce qui rend l'échantillon une unité de temps honnête sur cette carte.
- **Les SCSP ne jouent que depuis leur RAM** ; c'est le 68000 qui y recopie les échantillons depuis les 16 Mo
  de ROM, par banque (bit 4 du registre 0x400001).
- **Le MIDI arrive de la carte principale** dans l'entrée du premier SCSP, et c'est tout le canal de commande :
  le PowerPC n'envoie que des octets.
- **Le Model 3 sort quatre canaux**, repliés en stéréo ici comme chez Mednafen.
- Les réglages de ce portage qui tiennent à **ces jeux-là** (le miroir de MADRS par jeu, le groupe d'une voix
  lu dans les fiches à 0x400 de la RAM son) ne sont pas du chip : ils sont de la carte et du pilote de son, et
  ils doivent le rester.


## 5. Ce qui doit exister avant la première ligne

1. Le **harnais différentiel** : il existe (`tools/son/comparer-son.cmd`), mais il compare deux *constructions*
   du portage, pas deux *cœurs*. Il lui faut un sélecteur (variable d'environnement ou option de build) qui
   fasse tourner l'ancien et le neuf dans le même binaire, sur la même séquence d'accès, et compare les PCM,
   les états observables, les interruptions et les timers.
2. Le **repli** : l'ancien cœur reste là, derrière ce sélecteur, et ne part qu'après.
3. Les **tests par bloc** de la liste du §3, écrits avant le bloc qu'ils testent.

Tant que ces trois-là n'existent pas, écrire le cœur autonome est un pari, pas un travail.
