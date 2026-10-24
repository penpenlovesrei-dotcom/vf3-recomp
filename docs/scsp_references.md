# Sources du SCSP — ce qui a été lu, et ce qu'on en sait

Relevé le 2026-10-24. Chaque ligne dit **où elle a été lue**. Ce qui n'a pas été vérifié est marqué comme tel ;
rien ici n'est déduit d'un souvenir.


## L'avertissement qui vient en premier

**Les implémentations ne sont pas indépendantes les unes des autres, et leur accord ne prouve rien.**

Le SCSP de MAME porte en tête `copyright-holders: ElSemi, R. Belmont` : ElSemi l'a écrit, R. Belmont l'a
converti pour MAME/M1, kingshriek l'a corrigé. Le SCSP de Supermodel vient **du même ElSemi**, qui l'a donné au
projet ; l'en-tête de `Src/Sound/SCSP.cpp` le dit et crédite par ailleurs une conversion MAME par Paul Prosser.
Les deux émulateurs ne sont donc pas deux témoins : c'est **un seul témoin lu deux fois**. Quand MAME et
Supermodel sonnent pareil, cela n'établit pas que le YMF292 sonne comme ça.

Conséquence pratique : toute question de fidélité tranchée par « MAME et Supermodel font pareil » est une
question **non tranchée**. Dans ce dépôt, les réglages qui s'écartent de MAME ont été décidés par la mesure sur
la borne (le niveau d'un état, la corrélation gauche/droite de la réverbération, l'écart spectral d'une voix),
pas par le vote des émulateurs — et c'est la bonne méthode à garder.


## Licences, et ce qu'on peut faire de chaque source

| source | licence | ce qu'on peut en faire ici |
|---|---|---|
| MAME (`mamedev/mame`), `sound/scsp.cpp`, `sound/scspdsp.cpp`, `cpu/m68000/` | **BSD-3-Clause** | **réutilisable** en gardant l'avis, les conditions et la clause de non-garantie. C'est ce que fait le dépôt. |
| Supermodel (`trzy/Supermodel`) | **GPL** | **observation seulement**. Ni copie, ni traduction mécanique : ce dépôt est sous MIT. On peut regarder ce qu'il *fait*, pas reprendre comment il l'écrit. |
| Mednafen (SASPlay) | **GPL** | idem : observation seulement. |
| Yabause, Kronos | **GPL** | idem. |

`tools/ref/README.md` pose déjà cette ligne pour le correctif de couverture Supermodel : « Supermodel is GPL,
and so is this patch. The port's own code must not copy from it. » Elle vaut pour le son aussi.


## Ce qui a été vérifié aujourd'hui, à la source

### MAME — le DSP décode son programme à chaque échantillon

Lu dans `src/devices/sound/scspdsp.cpp` de la branche `master` : la boucle ouvre sur

```
for (int step = 0; step < /*128*/LastStep; ++step)
    u16 *const IPtr = MPRO + (step * 4);
    u32 const TRA = (IPtr[0] >> 8) & 0x7f;
    ...
```

et **extrait les champs par décalages et masques à chaque pas de chaque échantillon**. La seule chose
pré-calculée est `LastStep`, que `Start()` obtient en cherchant la dernière instruction non nulle.

Supermodel fait la même chose : `Src/Sound/SCSPDSP.cpp` calcule `IPtr = DSP->MPRO + step * 4` et extrait
TRA, TWT, XSEL, MWT… dans la boucle, à chaque échantillon.

**Ce dépôt est donc en avance sur les deux** sur ce point précis : `SCSPDSP::Decode` écrit le programme décodé
dans `prog[128]` une fois, au moment où le 68000 l'écrit, et depuis aujourd'hui les contrôles d'un bit sont
regroupés dans un mot (`Inst::f`). Voir `docs/scsp_audit.md` §7. Ce n'est pas une divergence de comportement :
le décodage est une bijection, et le WAV est identique à l'octet.

Sont bien présents dans MAME comme ici : `PACK`/`UNPACK`, et l'accès mémoire limité aux pas impairs
(`if (MRD && (step & 1)) //memory only allowed on odd?`, avec la remarque sur les NOP que DoA insère sur les
pas pairs).

### MAME — le miroir de MADRS, et ce qu'il en a fait

`mamedev/mame` **PR #16527**, fusionnée le **2026-10-19** :
> `sound/scsp.cpp: Don't mirror MADRS at 0x7c0-0x7ff, fixes corrupted and then silent sound in doa`

État de `master` lu aujourd'hui, et il faut être précis parce que les deux chemins ne font pas la même chose :

| | MAME `master` | ce dépôt, `madrs_mirror = true` | ce dépôt, `madrs_mirror = false` |
|---|---|---|---|
| écriture dans 0x7C0-0x7FF | **jetée** (branche vide, commentaire « 0x7c0-0x7ff does not mirror MADRS: doa clears this range ») | va dans MADRS | gardée dans `m_madrs_hi` |
| lecture de 0x7C0-0x7FF | **renvoie MADRS** | renvoie MADRS | renvoie `m_madrs_hi` |

Donc :

1. **Du côté écriture**, MAME `master` et le mode « séparé » de ce dépôt font la même chose pour ce qui compte :
   les zéros que le programme son écrit par-dessus n'effacent plus les adresses des lignes de retard. C'est le
   réglage que la mesure a imposé pour **Fighting Vipers 2** (`docs/fv2-console-son.md` : le son de l'écran de
   victoire qui se nourrit lui-même, pic moyen de l'état 22831 → 6532 quand on les sépare). Le dépôt et MAME se
   sont retrouvés sur la même réponse, par deux chemins séparés et pour deux jeux différents (`doa` là-bas,
   `fvipers2` ici).
2. **Du côté lecture**, les deux diffèrent : MAME renvoie MADRS, le mode « séparé » renvoie les mots écrits. Si
   un programme son relit cette plage et agit dessus, les deux ne donneront pas la même chose. **Non mesuré**
   ici : on ne sait pas si un des deux jeux la relit.
3. **Et pour Virtua Fighter 3, ce dépôt garde le miroir** (`soundboard.h` : `madrs_mirror = true`), parce que
   sans lui la réverbération tenait plusieurs secondes là où la référence la coupe en une
   (`src/sound/scsp.h`). Avec le comportement de MAME `master`, les zéros de VF3 seraient jetés et la
   réverbération serait la longue. **C'est une divergence ouverte, et elle est consignée comme telle** : ou le
   réglage de VF3 compense autre chose, ou la référence à laquelle il a été comparé n'était pas le matériel.
   Rien dans ce document ne la tranche.

### Mednafen — SASPlay

Lu sur `https://mednafen.github.io/documentation/sasplay.html` : c'est un module de **lecture de la musique**
des bornes Sega Model 2A/2B/2C et 3, pas une référence de chip. Ce qui s'y trouve d'utile :

- le Model 3 sort **4 canaux**, que SASPlay replie en stéréo ;
- une base de volumes par jeu, avec des ajustements par morceau (Virtua Fighter 3 y est nommé) ;
- et surtout, dit noir sur blanc : la musique des Model 3 peut jouer **jusqu'à 2 % trop vite**, parce que
  Mednafen n'émule pas « dynamic SCSP wait states and interrupt signaling delays », et que les gestionnaires
  d'interruption y sont sensibles.

Ce dernier point vaut d'être retenu : la **cadence** de la musique dépend des états d'attente du SCSP et du
délai de signalement des interruptions. Ce dépôt a déjà dû traiter la même famille de problème autrement — ses
timers A-C comptent les échantillons et tombent sur un échantillon rond, comme ceux de Supermodel
(`src/sound/scsp.h`), et `SoundBoard::run` ne donne au 68000 qu'un échantillon de cycles à la fois pour qu'un
timer réarmé dans une interruption soit vu dans la tranche où il a été posé (le commentaire de
`soundboard.cpp` dit ce qui arrive sinon : « the music slowed to a crawl »). Si un jour la cadence de la
musique est mise en doute, c'est là qu'il faudra regarder, et la phrase de Mednafen dit où.

### Ce qui n'a PAS été fait

- **Yabause** (`libretro/yabause`) et **Kronos** (`FCare/Kronos`) : non lus. Ce sont les deux implémentations
  Saturn que le cahier des charges cite, et le commentaire de `src/sound/scsp.h` affirme déjà quelque chose à
  leur sujet (« Mednafen's SCSP (its MADRS at 0x780-0x7bf only) and Yabause's keep them apart ») — **cette
  affirmation n'a pas été revérifiée à la source aujourd'hui**.
- **La documentation technique du YMF292-F / SCSP** : non obtenue. Rien n'a été téléchargé, et aucune
  affirmation de ce dépôt ne repose dessus pour l'instant.
- **Le code de Supermodel au-delà de `SCSPDSP.cpp`** : seul l'en-tête et la forme de la boucle ont été lus.
  L'affirmation de `src/sound/scsp.h` selon laquelle « Supermodel kept ElSemi's older DSP for this very game
  until it made its DSP's memory access follow the manual » **n'a pas été confirmée** : le fichier lu
  aujourd'hui ne mentionne ni MAME, ni DSP d'origine, ni manuel. Il existe par ailleurs un indice indirect
  (une recherche renvoie un commit Supermodel qui « ported MAME's implementation and added a legacy DSP option
  for games with DSP glitches »), mais c'est une source secondaire et elle n'est pas retenue comme preuve.

### Et ce qui restera impossible à vérifier ici

Aucune mesure sur **matériel réel** n'est possible depuis ce poste : pas de borne Model 3, pas de capture
analogique de référence. Toute question de fidélité tranchée ici l'est contre **une autre émulation** ou contre
**la cohérence interne du jeu** (un niveau qui ne devrait pas monter, une réverbération qui ne devrait pas se
replier en mono). C'est utile et ce n'est pas la même chose. Aucun énoncé de ce dépôt ne doit dire « conforme
au matériel » sur cette base.


## Sources

- MAME, [`src/devices/sound/scspdsp.cpp`](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/sound/scspdsp.cpp) et [`src/devices/sound/scsp.cpp`](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/sound/scsp.cpp) (branche `master`, lues le 2026-10-24)
- MAME, [PR #16527](https://github.com/mamedev/mame/pull/16527) — « Don't mirror MADRS at 0x7c0-0x7ff »
- Supermodel, [`Src/Sound/SCSPDSP.cpp`](https://raw.githubusercontent.com/trzy/Supermodel/master/Src/Sound/SCSPDSP.cpp) et [`Src/Sound/SCSP.cpp`](https://repo.retrodeck.net/Xargon/Supermodel/src/branch/master/Src/Sound/SCSP.cpp)
- Mednafen, [documentation SASPlay](https://mednafen.github.io/documentation/sasplay.html)
- [`github.com/mirror/model3emu`](https://github.com/mirror/model3emu) (miroir de Supermodel)
