# Reprise — le son, état au 2026-10-28 (fin de séance)

Séance arrêtée en cours de vérification, à la demande. **Lis `docs/son.md` d'abord** : il dit où est quoi.
Ce fichier ne dit que ce qui a bougé ce jour-là, ce qui est vérifié, et ce qui ne l'est pas.

## Ce qui a été fait, et par quelle mesure

### 1. `VF3_SND_PROF` ne sortait rien — quatre raisons, toutes muettes

Chacune rendait la mesure **vide ou fausse sans le dire**. Toutes fermées, et les trois premières **parlent**
maintenant. Le détail est en `docs/scsp_audit.md` §4 sexies.

1. l'appel à `sndprof::report` vivait dans le bloc `if (prof)` de `tools/vf3run.cpp` : `VF3_SND_PROF=1` sans
   `VF3_PROF=1` n'imprimait rien. Il a son propre test, et écrit sur **stdout** comme la ligne `prof`.
2. **`vf3run` ne fait tourner la carte son qu'avec `--wav`.** Sans lui la sonde comptait zéro et l'affichait
   comme une mesure. L'outil le dit au lancement, et le rapport dit « rien mesuré ».
3. le rapport n'arrivait qu'aux multiples de 500 images : un rapport final couvre le reste.
4. les portées `SCSP` et `DSP` ne vivaient que dans le code de MAME ; sur `VF3_SCSP_COEUR=maison` le rapport
   donnait **97 % au 68000, zéro aux voix, zéro au DSP** — un chiffre faux, pas un trou. Elles sont dans
   `scsp_maison::run`, aux endroits qui répondent à `DoMasterSamples` et `m_DSP.Step()`.

### 2. Ce que le cœur du projet coûte, dans le jeu

Trois passages par cœur, alternés, fenêtres 1500-4200, 18 mesures par cœur, médiane. Les six WAV identiques
à l'octet (`9A2310F8AF000E09EB18BD114852B003`).

| | `mame` | `maison` | écart |
|---|---|---|---|
| 68000 (témoin : même code) | 1 015 kc | 947 kc | **−68 kc** — le jeu de la mesure |
| voix | 443 | 484 | +41 kc, **sous le jeu : ne se lit pas** |
| **DSP** | **1 737** | **2 039** | **+302 kc (+17,4 %)** |
| toute la carte son | 3 284 | 3 550 | **+266 kc (+8,1 %)** |

Sans dénominateur : **9,60 → 11,50 cycles par pas de DSP, +19,8 %** — le +20 % de `mesurer-coeur` retrouvé
dans le jeu. Et dans l'image sous `--rec` : **+0,09 ms, soit 0,5 % du budget de 17,4 ms** ; la part du son
passe de 30,7 % à 31,6 %.

**Pour la suite du point 2 de ta liste** : le DSP est tout le surcoût mesurable. Le +18 % des fentes ne se
lit pas dans le jeu (+18 % sur 443 kc vaudrait +80 kc, j'en mesure +41, sous le jeu de 68) — la mesure est
compatible avec tout entre zéro et le chiffre du banc. Il faut le profilage à l'instruction pour trancher.

### 3. La trace du rejoueur n'était complète que sur 300 images — trouvé, nommé, refermé

Le contrôle « la trace est-elle complète ? » échouait sur la scène entière (échantillons 772 067 et
1 149 006). **La cause : le miroir MADRS**, le seul des quatre extras hors de `scsp::core` qui change le son,
posé par `dynamic_cast` dans `SoundBoard::reset` **après** la création de l'enregistreur — donc invisible à
la trace, qu'un rejeu laissait à son défaut `true` alors que Fighting Vipers 2 le veut faux.

Refermé ainsi :

- **une seule porte** : `scsp::regle_miroir_madrs` / `scsp::miroir_madrs` dans `src/sound/scsp_select.h`,
  dont l'en-tête nommait déjà ce manque. Le `dynamic_cast` ne vit plus dans `soundboard.cpp` ;
- **l'ordre** : le miroir se pose avant que l'enregistreur existe, et l'enregistreur le **relit de la puce** ;
- **la trace le porte** : en-tête **version 2**, `# scsp-trace 2 puce=N horloge=H ram=B miroir=0|1` ;
- **une trace v1 est refusée** avec un message qui dit quoi faire, et non devinée.

Toute la mesure, les pistes écartées et la leçon sont en `docs/scsp_validation.md` §3 c.

## Ce qui est vérifié, et ce qui ne l'est pas

**Vérifié, tout l'est maintenant** (les deux derniers contrôles relancés le 2026-10-28 à la reprise) :

| | |
|---|---|
| la construction | `make test build/bin/vf3run build/bin/rejouer build/bin/mesurer-coeur`, code 0 |
| le rejoueur, scène entière | `tools/son/rejouer.cmd fvipers2 tools/scripts/fv2-fight.txt 4200 mame maison` → **TOUT CONCORDE**, les **quatre** contrôles, 3 219 865 échantillons par puce. C'est le contrôle qui compte |
| les dix tests | `make test` **après** le correctif du miroir : tous les cas passent, les deux cœurs là où le test prend un nom de cœur |
| l'empreinte de bout en bout | `vf3run --frames 4200 --rec --wav` avec chaque cœur : **`9A2310F8AF000E09EB18BD114852B003`** des deux côtés — le correctif ne change pas une valeur du son (critère 3 du plan de migration) |

**Reste hors de ce travail** : `verifier-vf3.cmd` (critère 2) n'a pas été lancé, et il ne regarde pas le son.


## Ce qui reste de ta liste, inchangé

3. les gains par groupe ne vivent que dans le cœur de MAME (`soundboard.h` dit pourquoi) ; inertes à volumes 1.
4. la mesure de concurrence sur le portable deux cœurs : `C:\Users\frede\Desktop\mesure-concurrence`, à
   lancer par toi, et lire son `LISEZ-MOI.txt` avant de commenter les chiffres.
5. `FRCL`, `ADRL`, `YRL`, `NEGB` — écrits, validés par aucune mesure, le jeu ne les exerce pas.
6. le dépôt séparé : le correctif du miroir **avance** ce point, parce que le rejoueur est l'instrument qui
   doit le justifier et que sa garantie de complétude vaut maintenant sur la scène entière. Reste à couper la
   dépendance de `tools/son/banc_scsp.h` au cœur de MAME.
