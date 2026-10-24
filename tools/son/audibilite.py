#!/usr/bin/env python3
"""Deux WAV du meme passage : l'ecart entre eux passe-t-il au-dessus du seuil d'audition ?

  tools/son/audibilite.py <wav_a> <wav_b> [--noms "a,b"] [--spl 96] [--bruit 30] [--pires 8]

Pourquoi cet outil : « je n'entends pas de difference » est une information, pas une mesure -- elle
depend du casque, de la piece, de l'attention et du passage. Ce qui se mesure, c'est le rapport
bruit/masque (NMR), la grandeur qu'utilisent les codecs perceptuels : l'ecart est-il au-dessus de ce
que le signal lui-meme rend inaudible au meme instant ?

CE QUE LA PREMIERE VERSION DE CE FICHIER FAISAIT MAL, et qui est la raison de ce commentaire :
elle comparait l'ecart d'une bande au signal de CETTE SEULE bande. Un ecart a -50 dBFS dans une
bande ou le signal est a -68 dBFS en sortait « +18 dB au-dessus de son masque » -- alors qu'une
bande voisine a -10 dBFS le masque completement, et qu'a -50 dBFS on est de toute facon pres du
seuil absolu d'audition. Deux choses manquaient : l'etalement du masquage entre bandes, et un seuil
absolu. Sans elles la mesure classait comme audible un ecart que personne n'entend, ce qui est
exactement ce qui est arrive.

Le modele, et il est simplifie :

  1. fenetres de 46 ms, pas de 23 ms, Hann, et le mono des deux canaux ;
  2. 25 bandes approximativement critiques (Bark) ;
  3. le niveau absolu est pose : pleine echelle = --spl dB SPL (96 par defaut, une ecoute au casque
     a niveau raisonnable). C'est une HYPOTHESE, pas une mesure, et tout le reste en depend ;
  4. etalement du masquage : un masque a la bande j masque la bande i avec 10 dB/Bark au-dessus de
     lui (etalement vers le haut, peu pentu) et 27 dB/Bark au-dessous (plus raide) ;
  5. seuil = max(masque etale - 6 dB, seuil absolu d'audition, bruit ambiant de la piece) ;
     le bruit ambiant (--bruit, 30 dB SPL par defaut, reparti sur les bandes) est le troisieme
     terme, et il n'etait pas la au premier essai : sans lui le modele comptait comme audible des
     residus a -90 dBFS dans un silence numerique -- vrai en chambre sourde, faux dans une piece ;
  6. NMR = niveau de l'ecart - seuil, par bande et par fenetre.

    NMR < 0 dB  : l'ecart est sous le seuil, il ne devrait pas s'entendre dans cette bande
    NMR > 0 dB  : il depasse, il devrait s'entendre

CE QUE LE TEMOIN A APPRIS, LE 2026-10-24, ET QUI EST LA LECON DE CE FICHIER. Fait tourner sur un
cas dont la reponse est connue -- l'ecran de victoire de Fighting Vipers 2, ou le miroir de MADRS
fait naitre un son qui monte de 24 dB jusqu'au limiteur (commit 335c050) -- ce programme rend des
chiffres PLUS PETITS que sur un cas bien plus discret :

    ecran de victoire FV2 (defaut grossier, connu)  mediane -15,3 dB   99e +20,0   10,0 % > 0
    Virtua Fighter 3 (ecart tenu, inaudible a l'oreille)  mediane -2,2 dB   99e +25,0   47,9 % > 0

Ce n'est pas une panne : le NMR mesure si un ecart est DEMASQUE, pas s'il est GRAVE. Le defaut de
FV2 est un son fort et soutenu, donc son propre masque est enorme et le NMR reste bas ; l'ecart de
VF3 tombe dans un silence numerique, ou rien ne le masque, et le NMR grimpe. Les deux questions
sont legitimes et ce sont deux questions :

    NMR eleve  -> on peut DETECTER que les deux fichiers different
    NMR faible -> on ne le detectera pas, meme si l'un des deux est grossierement faux

Pour « est-ce grave », il faut regarder le niveau, le spectre et la correlation
(tools/son/mesurer-madrs.py), pas celui-ci. Ne jamais lire ce programme comme une mesure de gravite.

Les limites, dites d'avance : pas de distinction tonal/bruite, pas de masquage temporel (ni pre- ni
post-), pas de modele d'integration de la sonie, seuil absolu approche d'ISO 226, et le niveau de
lecture est pose au doigt. Ce programme sert a CLASSER deux cas l'un contre l'autre -- il faut
toujours le faire tourner sur un temoin dont on connait la reponse, sinon ses chiffres ne valent
rien.
"""
import sys
import wave

import numpy as np

RATE = 44100.0
N = 2048                      # 46 ms
HOP = N // 2

# Les bords des 25 bandes critiques (Bark), en Hz.
BARK = [0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320,
        2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 22050]

# Seuil absolu d'audition par bande, en dB SPL (ISO 226 en champ libre, approche).
ATH = [40, 25, 15, 10, 8, 6, 5, 4, 3.5, 3.5, 3, 2, 0, -2, -4, -5, -6, -6, -4, 0, 6, 12, 16, 20, 40]

BRUIT_AMBIANT = 30.0          # dB SPL, le bruit d'une piece calme, reparti sur les 25 bandes
ETAL_HAUT = 10.0              # dB par Bark, un masque s'etale vers les bandes plus hautes
ETAL_BAS = 27.0               # dB par Bark, vers les bandes plus basses (plus raide)
INDICE_MASQUAGE = 6.0         # dB sous le masque etale


def lire(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2:
            raise SystemExit("%s : attendu 16 bits stereo" % path)
        d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return d.reshape(-1, 2)


def matrice_etalement(nb):
    """att[i, j] : combien de dB un masque en j perd quand il masque la bande i."""
    att = np.zeros((nb, nb))
    for i in range(nb):
        for j in range(nb):
            att[i, j] = ETAL_HAUT * (i - j) if i > j else ETAL_BAS * (j - i)
    return att


def bandes(puissance, hz):
    out = np.zeros((puissance.shape[0], len(BARK) - 1))
    for b in range(len(BARK) - 1):
        m = (hz >= BARK[b]) & (hz < BARK[b + 1])
        if m.any():
            out[:, b] = puissance[:, m].sum(axis=1)
    return out


def analyser(A, B, noms, spl, pires, bruit=BRUIT_AMBIANT):
    n = min(len(A), len(B))
    A, B = A[:n], B[:n]
    a = A.mean(axis=1)
    d = (A - B).mean(axis=1)

    nf = (len(a) - N) // HOP + 1
    if nf < 2:
        raise SystemExit("passage trop court")
    idx = np.arange(nf) * HOP
    fen = np.hanning(N)
    hz = np.fft.rfftfreq(N, 1.0 / RATE)
    norm = (fen.sum() / 2.0) ** 2

    PA = np.empty((nf, len(hz)))
    PD = np.empty((nf, len(hz)))
    for k in range(nf):
        i = idx[k]
        PA[k] = np.abs(np.fft.rfft(a[i:i + N] * fen)) ** 2 / norm
        PD[k] = np.abs(np.fft.rfft(d[i:i + N] * fen)) ** 2 / norm

    nb = len(BARK) - 1
    ba, bd = bandes(PA, hz), bandes(PD, hz)

    # en dB SPL : pleine echelle (32768 d'amplitude) vaut `spl`
    ref = 32768.0 ** 2 / 2.0
    la = 10.0 * np.log10(np.maximum(ba, 1e-20) / ref) + spl
    ld = 10.0 * np.log10(np.maximum(bd, 1e-20) / ref) + spl

    att = matrice_etalement(nb)
    # masque etale : pour chaque bande i, la somme en energie des masques voisins attenues
    masque = np.empty_like(la)
    for i in range(nb):
        contrib = la - att[i][None, :]
        masque[:, i] = 10.0 * np.log10(np.sum(10.0 ** (contrib / 10.0), axis=1))
    # le bruit ambiant, reparti egalement sur les bandes (une approximation : un vrai bruit de
    # piece est penche vers le bas du spectre, ce qui le rendrait encore plus masquant en bas)
    par_bande = bruit - 10.0 * np.log10(nb)
    seuil = np.maximum(np.maximum(masque - INDICE_MASQUAGE, np.array(ATH)[None, :]), par_bande)

    nmr = ld - seuil
    pire_bande = nmr.max(axis=1)

    print("  %d fenetres sur %.1f s ; pleine echelle posee a %.0f dB SPL, bruit ambiant %.0f dB SPL"
          % (nf, n / RATE, spl, bruit))
    print("  rapport bruit/masque (NMR), pire bande de chaque fenetre :")
    for q, lbl in ((50, "mediane"), (90, "90e centile"), (99, "99e centile"), (100, "maximum")):
        print("    %-12s %+7.1f dB" % (lbl, np.percentile(pire_bande, q)))
    print("  part des fenetres dont une bande depasse le seuil (NMR > 0) : %.2f %%"
          % (100.0 * np.mean(pire_bande > 0)))
    print("  part au-dessus de +6 dB : %.2f %%   au-dessus de +12 dB : %.2f %%"
          % (100.0 * np.mean(pire_bande > 6), 100.0 * np.mean(pire_bande > 12)))

    print("  les %d fenetres les plus exposees :" % pires)
    for k in sorted(np.argsort(-pire_bande)[:pires]):
        b = int(np.argmax(nmr[k]))
        print("    t = %6.2f s  bande %5d-%5d Hz  signal %6.1f  ecart %6.1f  seuil %6.1f  NMR %+6.1f dB"
              % (idx[k] / RATE, BARK[b], BARK[b + 1], max(la[k, b], -99.0), ld[k, b], seuil[k, b], nmr[k, b]))

    print("  par bande, le pire NMR du passage :")
    for b in range(nb):
        k = int(np.argmax(nmr[:, b]))
        if nmr[k, b] < -30:
            continue
        print("    %5d-%5d Hz   NMR %+6.1f dB   a t = %6.2f s   (signal %.1f, seuil %.1f)"
              % (BARK[b], BARK[b + 1], nmr[k, b], idx[k] / RATE, la[k, b], seuil[k, b]))
    return float(np.percentile(pire_bande, 99)), float(np.mean(pire_bande > 0))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    noms = ("A", "B")
    spl = 96.0
    bruit = BRUIT_AMBIANT
    pires = 8
    for i, x in enumerate(argv):
        if x == "--noms" and i + 1 < len(argv):
            noms = tuple(argv[i + 1].split(",")[:2])
        if x == "--spl" and i + 1 < len(argv):
            spl = float(argv[i + 1])
        if x == "--bruit" and i + 1 < len(argv):
            bruit = float(argv[i + 1])
        if x == "--pires" and i + 1 < len(argv):
            pires = int(argv[i + 1])
    print("%s  contre  %s" % (noms[0], noms[1]))
    A, B = lire(argv[1]), lire(argv[2])
    m = min(len(A), len(B))
    if np.array_equal(A[:m], B[:m]):
        print("  Les deux fichiers sont IDENTIQUES. Rien a entendre, par construction.")
        return 0
    analyser(A, B, noms, spl, pires, bruit)
    print()
    print("  NMR < 0 : sous le seuil, ne devrait pas s'entendre. NMR > 0 : devrait s'entendre.")
    print("  Le niveau absolu est POSE (--spl), pas mesure, et tout en depend. Ce classement ne")
    print("  vaut que compare a un temoin dont on connait la reponse.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
