# Reprise — le son, la journée du 2026-10-28

**Lis `scsp/docs/son.md` d'abord** : il dit où est quoi. Ce fichier-ci ne dit que ce qui a bougé ce jour-là, ce
qui est vérifié, et ce qui reste — avec, à chaque fois, l'instrument qui l'a établi.

Vingt commits, de `7876901` à `d62f732`, poussés sur `mine/fv2`. **Les six points de la liste de reprise du
matin sont fermés.**

---

## 1. `VF3_SND_PROF` ne sortait rien — quatre raisons, toutes muettes

Chacune rendait la mesure **vide ou fausse sans le dire**. Toutes fermées, et les trois premières **parlent**
maintenant. Détail en `scsp_audit.md` §4 sexies.

1. l'appel à `sndprof::report` vivait dans le bloc `if (prof)` de `tools/vf3run.cpp` : `VF3_SND_PROF=1` sans
   `VF3_PROF=1` n'imprimait rien. Il a son propre test, et écrit sur **stdout** comme la ligne `prof`.
2. **`vf3run` ne fait tourner la carte son qu'avec `--wav`.** Sans lui la sonde comptait zéro et l'affichait
   comme une mesure. L'outil le dit au lancement, et le rapport dit « rien mesuré ».
3. le rapport n'arrivait qu'aux multiples de 500 images : un rapport final couvre le reste.
4. les portées `SCSP` et `DSP` ne vivaient que dans le code de MAME ; sur `VF3_SCSP_COEUR=maison` le rapport
   donnait **97 % au 68000, zéro aux voix, zéro au DSP** — un chiffre faux, pas un trou.

## 2. Ce que le cœur du projet coûte, dans le jeu

Trois passages par cœur, alternés, fenêtres 1500-4200, 18 mesures par cœur, médiane. Les six WAV identiques
à l'octet.

| | `mame` | `maison` | écart |
|---|---|---|---|
| 68000 (témoin : même code) | 1 015 kc | 947 kc | **−68 kc** — le jeu de la mesure |
| voix | 443 | 484 | +41 kc, **sous le jeu : ne se lit pas** |
| **DSP** | **1 737** | **2 039** | **+302 kc (+17,4 %)** |
| toute la carte son | 3 284 | 3 550 | **+266 kc (+8,1 %)** |

Dans l'image sous `--rec` : **+0,09 ms, soit 0,5 % du budget de 17,4 ms**.

## 3. La trace du rejoueur n'était complète que sur 300 images — refermé

Le contrôle « la trace est-elle complète ? » échouait sur la scène entière (échantillons 772 067 et
1 149 006). **La cause : le miroir MADRS**, le seul des quatre extras hors de `scsp::core` qui change le son,
posé par `dynamic_cast` **après** la création de l'enregistreur — donc invisible à la trace, qu'un rejeu
laissait à son défaut `true` alors que Fighting Vipers 2 le veut faux.

Une seule porte maintenant (`scsp::regle_miroir_madrs` dans `scsp_select.h`), posée **avant** l'enregistreur
qui la **relit de la puce**, un en-tête de trace **version 2** qui la porte, et une trace v1 **refusée**.
Toute la mesure est en `scsp_validation.md` §3 c.

## 4. Sept tests ne traînent plus le cœur de MAME

`banc_scsp.h` incluait les deux classes concrètes pour deux aiguillages de trois lignes. **Chaque cœur
s'inscrit maintenant dans son propre `.cpp`** (`scsp::inscrit`) : le sélecteur ne nomme plus aucune classe
concrète, et **ce qui est lié décide ce qui existe**. `make test-sans-mame` construit les sept tests de bloc
avec `scsp_select.o`, `scsp_maison.o`, `dsp_maison.o` et rien d'autre, et `make test` le refait à chaque fois.

## 5. Le DSP par caractéristique : ce n'est aucune instruction, c'est le socle

`mesurer-coeur --dsp`, des programmes de 112 pas où tous les pas ne font qu'une chose, les deux cœurs côte à
côte, coût **propre** socle déduit (`scsp_audit.md` §4 septies). Le socle — 112 pas qui ne font rien — coûte
**+19 %** au cœur du projet, et le témoin de la forme du jeu a un coût propre **quasi nul**. Le cœur du
projet est **meilleur** sur la moitié de la table ; il n'est moins bon que sur l'accès mémoire.

## 6. L'assembleur, lu après que le banc ait resserré la question

`Run<0>` : 108 instructions, **16 branchements**. `boucle<false>` : 145 et **31**. **Cinq hypothèses mortes
à l'inspection** : les deux corps existent, le pas décodé est plus petit (16 octets contre 28), pas de table
de saut, pas de division, le produit 64 bits est identique chez MAME, et les deux compilent `std::clamp` en
`cmov`.

## 7. Deux lots pour enlever ces branchements — et les deux perdent

Défaits (`scsp_audit.md` §4 octies). Cinq octets de plus dans `Pas` : **+7 %**. Les mêmes décisions dans les
bits libres du mot : **+16 %**. Et un faux gain de 6,9 % a failli être gardé — `mame` dérivait dans le
dénominateur. D'où `mesurer-coeur --socle`, assez rapide pour entrelacer référence et variante.

## 8. Les quatre champs du DSP : cherchés partout, puis éprouvés

**Aucun jeu servi ne les pose** : 152 écritures de programme, cinq formes distinctes, trois jeux dont
`srally2` avec un tout autre programme (`scsp_audit.md` §5). **`test_scsp_dsp.cpp` cas 9 à 13** les mesurent,
chacun par le seul chemin de lecture qui existe. Le cœur du projet les rend tous **du premier coup**, et
quatre fautes injectées sont attrapées **chacune par son cas**.

## 9. Le compteur matériel était là, et j'avais dit deux fois qu'il n'y était pas

`xperf`, du Windows Performance Toolkit, est installé sur cette machine. Je l'avais déclaré absent sans le
chercher, et §4 septies comme §4 octies s'étaient arrêtées sur cette phrase — toutes deux la gardent
maintenant, datées, avec le renvoi à la mesure qui les corrige.

| par pas de DSP | rapport `maison` / `mame` |
|---|---|
| instructions retirées | **×1,144** |
| branchements mal prédits | **×1,010 — identiques** |

**Ce ne sont pas les branchements** : ils sont parfaitement prédits, ils coûtent leur décodage et non leur
raté — ce qui explique pourquoi les deux lots du point 7 perdaient. **Ce sont les instructions.**

Seuls les **rapports** sont publiables : l'intervalle qu'annonce `xperf -pmcsources` ne tient pas à
l'arithmétique (§4 nonies le détaille).

## 10. La chasse aux huit instructions : première piste, et elle ne donne rien

MAME porte `u16 *const __restrict rp = ram;` dans son en-tête de boucle ; le projet ne l'avait pas, et c'est
une vraie promesse avec une explication complète. **Elle ne gagne rien** : −0,08 % au socle, plat sur le jeu.
Défaite. Et le rapport `maison`/`mame`, qui protégeait des deux lots, **devient un piège** dès qu'un seul
côté change : c'est l'absolu du côté modifié qu'il faut lire.

## 11. Situées jusqu'à la fonction, et pas plus bas

Attribution par fonction sans PDB : le dumper donne le `PrgrmCtr`, la trace la base de chargement, `nm` les
plages. **La boucle de pas retire +19,6 %** ; le +14,4 % global était dilué parce que le cœur du projet
**gagne 15,7 %** sur l'enveloppe par échantillon.

**Trois tentatives pour descendre plus bas, trois raisons de s'arrêter** : l'inlining brouille les tables de
lignes ; le décalage d'échantillonnage sans PEBS rend les histogrammes par instruction incomparables ; et le
`PrgrmCtr` n'est pas une adresse d'instruction — 21 des 84 adresses chaudes de MAME y tombent.

## 12. Les variantes jugées au compteur, et le classement se renverse

Six variantes construites par `batir-variantes.py`, chacune **passée aux dix tests** avant d'être gardée, et
jugées par `mesurer-variantes.cmd` sur les **deux** programmes :

| | socle | **forme du jeu** |
|---|---|---|
| `ysel_coef_premier` | −1,55 % | **−3,09 %** |
| `shift_deux_bits` | −3,03 % | **−1,17 %** |
| `sortie_groupee` | −6,02 % | +0,85 % |
| `tout` | — | +3,28 % |
| **`entree_paresseuse`** | **−15,00 %** | **+4,68 %** |

**La grande gagnante du socle est la pire du jeu** : elle ajoute un test par pas pour éviter un calcul dont
37 pas sur 112 ont besoin. Le socle **situe** un coût, il ne dit pas ce qu'une modification rend. D'où
`mesurer-coeur --jeu`. L'instrument est reproductible **à 0,07 point près** d'une séance à l'autre.

**Et les cas 9 à 13 ont arrêté `sortie_groupee` au premier tour** : son masque groupé englobait les écritures
de `FRCL` et d'`ADRL`. Elle passait tous les autres tests, le rejoueur, et rendait le WAV identique — parce
qu'aucun jeu ne pose ces champs. C'est ce que ces cas achètent vraiment, et ce n'est pas ce que j'avais
annoncé : **ils protègent ces quatre champs contre nos propres optimisations.**

## 13. L'optimisation gardée : −3,47 %

`1a14781`. COEF testé en premier dans le choix de Y, et le champ `SHIFT` résolu en deux bits du mot
d'indicateurs — **sans grossir le pas décodé**, ce qui avait coulé le lot B. Aucun des deux ne change ce
qu'un pas **calcule**, seulement la façon dont il **choisit**. Barre complète passée.

## 14. Le cœur validé à l'oreille, dans le vrai jeu

`fv2-stable-1` + les dix-huit fichiers de `src/sound` de HEAD + quatre lignes de Makefile : **ça compile et
ça joue**, sur une base vieille de dix jours. Aucune différence à l'oreille, comme le rejoueur le promettait.

Et le bug sonore de l'écran WINNER y est tombé **avec une ligne** — `sound.madrs_mirror = board.set == "vf3"`,
que `stable-1` n'avait pas. Ce n'est pas la puce qui corrige : le miroir est un réglage par jeu, et le cœur
ne connaît pas le jeu. Mais il le rend corrigeable en une ligne nommée et mesurée.

## 15. Le SCSP est un dépôt autonome, et c'est git qui l'y porte

**[github.com/penpenlovesrei-dotcom/scsp-model3](https://github.com/penpenlovesrei-dotcom/scsp-model3)**,
un vrai `git subtree` sous le préfixe `scsp/`. Un clone neuf du dépôt publié passe ses **sept tests**, sans
une ligne de MAME. Les chemins sont les mêmes des deux côtés, donc un `diff` se lit sans traduction, et la
circulation tient en une ligne dans chaque sens :

    git subtree push --prefix=scsp scsp main
    git subtree pull --prefix=scsp scsp main

`tools/son/exporter-scsp.py` a disparu : il recopiait les fichiers dans un dossier neuf, donc sans passé.
`scsp/LICENSE` est né avec le dépôt — le MIT sans sa liste de tiers, puisque la liste ne s'applique plus :
rien là-bas ne vient de MAME, et la frontière de licence passe exactement à la limite du préfixe.

**L'histoire ne traverse pas, et il faut le dire.** `git subtree split` suit un chemin, et avant le
déplacement le chemin `scsp/` n'existait pas : le dépôt autonome commence au jour du préfixe. Les quarante
commits qui ont fait le cœur restent ici, où `git log --follow scsp/src/sound/scsp_maison.cpp` les remonte
tous. Je les ai reconstruits pour voir (filtre sur les 30 anciens chemins, 40 commits, les mêmes blobs au
sommet) ; les greffer coûtait **une fusion à chaque synchronisation**, et le choix a été de garder
`subtree push` en une ligne. Le README du dépôt porte cette phrase, pour qui arrive par là.

Ce que ce dépôt ne peut pas faire est dans son README : refaire ses propres nombres — **la preuve vit dans
le jeu**, et c'était l'argument de `son.md` §7.

## 16. Sur deux cœurs : le dernier chiffre

AMD A9-9425, deux cœurs, deux processeurs logiques. Cinq passages de 4 000 images, médianes sur 70 à 76
relevés (`scsp_audit.md` §4 quater bis) :

| | une image | le fil du son | le jeu attend |
|---|---|---|---|
| `off` | **12,21 ms** | 0 | 0,001 |
| `mame-normal` | **12,34** | 6,10 | 0,003 |
| `maison-normal` | **12,38** | 6,46 | 0,003 |
| `mame-sync` | **16,03** | 4,67 | 0,000 |
| `maison-sync` | **15,95** | 4,64 | 0,000 |

- **Le son sur son fil coûte +0,12 ms par image** — environ 1 %. Le mot « gratuit » peut enfin être pesé.
- **Les 8 % du cœur du projet ne se voient pas**, même ici : +0,04 ms à l'image, **non résolu** dans une
  dispersion qui va de 1,86 à 52,65 ms. Au fil, le surcoût est bien là : +5,8 %, qui recoupe le +8,1 %.
- **Le recouvrement achète 3,7 ms par image** — trente pour cent de la durée d'une image. C'est la
  justification mesurée du fil du son, et elle n'existait pas.

---

## Ce qui est vérifié

| | |
|---|---|
| les tests | **24 fichiers au vert** : les dix sur les deux cœurs, plus les sept **sans une ligne de MAME** |
| le rejoueur, scène entière | **TOUT CONCORDE**, les quatre contrôles, 3 219 865 échantillons par puce |
| l'empreinte de bout en bout | **`9A2310F8AF000E09EB18BD114852B003`** avec chaque cœur |
| la non-régression de VF3 | `verifier-vf3.cmd 3000` → **IDENTIQUE** sur les quatre contrôles |
| à l'oreille | le vrai jeu, fenêtre, aucune différence |

**Les huit critères d'acceptation du plan de migration sont satisfaits.**

## Ce qui n'est pas vérifié, et il n'en reste qu'un

**Que MAME ait raison sur les quatre champs du DSP.** Personne ne les a mesurés sur du matériel, et
`scsp_references.md` refuse le consensus entre émulateurs comme preuve. Les cas 9 à 13 établissent que le
DSP du projet atteint le comportement **visé**, et rien de plus.

## Ce qui reste

**Rien sur le son.** Le dernier point ouvert était le vrai `git subtree`, fait le 2026-10-29 : les 30
fichiers déplacés sous `scsp/` par `git mv`, le Makefile et les inclusions suivis, les 33 renvois des
documents réécrits, et les trois contrôles repassés après le déplacement. Le dépôt autonome est en ligne et
un clone neuf s'y construit.

**One thing is open, and it is not technical: the language.** The rule for this repository is **English**,
settled on 2026-10-29. Everything written during the sound work went the other way -- 30 of the 32 files
under `scsp/` are in French, and so are 36 of the 41 commit messages. Only `scsp_contract.h` and
`scsp_core.h` are in English, and they are the two a stranger opens first.

What to translate, and how far, is **not decided**: the whole repository, or only the published subtree?
Measuring it was stopped before it finished, so the figure above is the only one there is. Everything new
is written in English from here, this paragraph included; the French around it waits for that decision.

Les six points de la liste du matin sont fermés, et ce que j'ai pu nommer en chemin — le pic
d'attente isolé à 7,4 ms, les cycles de `maison`, l'assembleur sous la fonction — a été **mesuré et écarté**,
pas laissé ouvert.

## Les outils nouveaux ou changés ce jour-là

| | |
|---|---|
| `mesurer-coeur --dsp` | le DSP par caractéristique, les deux cœurs, socle déduit |
| `mesurer-coeur --socle [cœur]` | le socle seul ; avec un nom de cœur, un seul par processus |
| `mesurer-coeur --jeu <cœur>` | **le programme de la forme du jeu** — le socle est artificiel |
| `make test-sans-mame` | les sept tests de bloc liés sans une ligne de MAME |
| `batir-variantes.py`, `variantes-dsp.py` | une variante par exe, chacune passée aux dix tests |
| `mesurer-variantes.cmd [socle\|jeu]` | le jugement au compteur matériel — fenêtre **administrateur** |
| `git subtree push --prefix=scsp` | le dépôt SCSP autonome, sans rien recopier |
| `mesurer-concurrence.cmd` | cinq passages, **les deux cœurs** — pour le portable |
| `VF3_SND_PROF=1` | sans `VF3_PROF`, exige `--wav`, et vaut pour les deux cœurs |
| trace `# scsp-trace 2` | l'en-tête porte `miroir=0|1` ; une trace v1 est refusée |
