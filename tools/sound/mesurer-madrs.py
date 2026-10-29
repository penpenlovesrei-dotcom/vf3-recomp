#!/usr/bin/env python3
"""Compare deux WAV du meme passage, et dit ce qui les separe.

  tools/sound/mesurer-madrs.py <wav_a> <wav_b> [--noms "a,b"] [--scene vf3|fv2]

Ecrit pour une question precise (docs/fv2-son-programme.md section 5) : le miroir de MADRS replie les
lignes de retard du DSP sur une seule adresse, et il faut savoir ce que ca fait au son de Virtua
Fighter 3, ou le reglage a ete garde sur la seule LONGUEUR de la reverberation.

Les quatre mesures, et pourquoi celles-la :

  niveau       -- pic moyen par phase, comme la mesure qui a tranche Fighting Vipers 2 le 2026-10-24
                  (pic moyen de VIC_DSP 22831 -> 6532). Comparable a celle-la.
  correlation  -- la correlation gauche/droite. C'est LA signature de l'effondrement : si toutes les
                  lignes de retard lisent le meme tampon, les deux canaux se ressemblent. Sur VF2 la
                  mesure donnait 0,99 avec le miroir contre 0,73-0,85 sans.
  decroissance -- l'enveloppe apres la piece et apres le start, par pas de 50 ms sur 3 s. C'est
                  exactement ce que scsp.h affirme avoir mesure (« die in about a second ... rang on
                  for several »), donc la mesure se refait au meme endroit.
  spectre      -- l'energie par bande, pour voir si c'est la couleur qui bouge et pas que le niveau.

Les phases viennent des marques du script de scene, pas d'un decoupage au jugé.
"""
import sys
import wave

import numpy as np

FRAME_HZ = 57.524160            # la cadence du Model 3 (main.cpp, FRAME_HZ)
RATE = 44100.0
SPF = RATE / FRAME_HZ           # echantillons par image : 766,63...

# Les phases, en images, lues dans les scripts de tools/scripts.
SCENES = {
    # vf3-match.txt : 0 boot, 300 attract, 600 piece, 750 select, 930 fight, 2190 fin
    "vf3": [("amorcage", 0, 300), ("attraction", 300, 600), ("piece et selection", 600, 930),
            ("combat", 930, 2190), ("fin", 2190, 2230)],
    # fv2-fight.txt
    "fv2": [("amorcage", 0, 960), ("attraction", 960, 1900), ("piece et selection", 1900, 2500),
            ("chargement et intro", 2500, 2950), ("combat", 2950, 4150)],
}

# Les moments ou scsp.h dit avoir mesure la decroissance des echos.
ECHOS = {"vf3": [("la piece", 600), ("le start", 660)],
         "fv2": [("la piece", 1900), ("le start", 1960)]}


def lire(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2:
            raise SystemExit("%s : attendu 16 bits stereo" % path)
        d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return d.reshape(-1, 2)


def db(x):
    return -np.inf if x <= 0 else 20.0 * np.log10(x / 32768.0)


def pic_moyen(x):
    """Le pic par image, moyenne sur la phase -- la mesure de FV2, en unites brutes."""
    n = int(len(x) // SPF)
    if n < 1:
        return 0.0
    pics = [np.abs(x[int(i * SPF):int((i + 1) * SPF)]).max() for i in range(n)]
    return float(np.mean(pics))


def correlation(x, fen=0.25):
    """Correlation gauche/droite, par fenetres ; rend la mediane et la part de fenetres >= 0,95."""
    n = int(fen * RATE)
    out = []
    for i in range(0, len(x) - n, n):
        g, d = x[i:i + n, 0], x[i:i + n, 1]
        if np.abs(g).max() < 64 and np.abs(d).max() < 64:
            continue                       # silence : une correlation n'y veut rien dire
        sg, sd = g.std(), d.std()
        if sg < 1e-9 or sd < 1e-9:
            continue
        out.append(float(np.mean((g - g.mean()) * (d - d.mean())) / (sg * sd)))
    if not out:
        return None, None, 0
    a = np.array(out)
    return float(np.median(a)), float(np.mean(a >= 0.95)), len(a)


def enveloppe(x, debut_image, duree=3.0, pas=0.05):
    """L'enveloppe (pic) par pas de `pas` secondes, a partir d'une image."""
    i0 = int(debut_image * SPF)
    n = int(pas * RATE)
    out = []
    for k in range(int(duree / pas)):
        a = i0 + k * n
        if a + n > len(x):
            break
        out.append(float(np.abs(x[a:a + n]).max()))
    return out


def bandes(x):
    """L'energie dans quatre bandes, en dB relatif au total."""
    m = x.mean(axis=1)
    if len(m) < 4096:
        return None
    f = np.fft.rfft(m[:len(m) // 4096 * 4096].reshape(-1, 4096) * np.hanning(4096), axis=1)
    p = (np.abs(f) ** 2).mean(axis=0)
    hz = np.fft.rfftfreq(4096, 1.0 / RATE)
    out = []
    for lo, hi in ((0, 250), (250, 1000), (1000, 4000), (4000, 22050)):
        s = p[(hz >= lo) & (hz < hi)].sum()
        out.append(s)
    t = sum(out) or 1.0
    return [10.0 * np.log10(v / t) if v > 0 else -np.inf for v in out]


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    pa, pb = argv[1], argv[2]
    noms = ("A", "B")
    scene = "vf3"
    for i, a in enumerate(argv):
        if a == "--noms" and i + 1 < len(argv):
            noms = tuple(argv[i + 1].split(",")[:2])
        if a == "--scene" and i + 1 < len(argv):
            scene = argv[i + 1]
    A, B = lire(pa), lire(pb)
    n = min(len(A), len(B))
    A, B = A[:n], B[:n]

    print("%s  contre  %s" % (noms[0], noms[1]))
    print("%d echantillons, %.1f s, scene %s\n" % (n, n / RATE, scene))

    diff = A - B
    print("Les deux fichiers differe-t-il ?")
    print("  ecart max sur un echantillon : %.0f  (%.1f dBFS)" % (np.abs(diff).max(), db(np.abs(diff).max())))
    print("  ecart RMS                    : %.1f  (%.1f dBFS)" % (diff.std(), db(diff.std())))
    if np.abs(diff).max() == 0:
        print("  *** IDENTIQUES : le reglage n'a rien change sur cette scene ***")
        return 0
    print()

    print("Pic moyen par image, par phase (unites brutes, comme la mesure de FV2)")
    print("  %-22s %12s %12s %10s" % ("phase", noms[0], noms[1], "ecart"))
    for nom, f0, f1 in SCENES[scene]:
        a, b = A[int(f0 * SPF):int(f1 * SPF)], B[int(f0 * SPF):int(f1 * SPF)]
        if len(a) < int(SPF):
            continue
        pa_, pb_ = pic_moyen(a), pic_moyen(b)
        r = ("%+.1f dB" % (db(pb_) - db(pa_))) if pa_ > 0 and pb_ > 0 else "-"
        print("  %-22s %12.0f %12.0f %10s" % (nom, pa_, pb_, r))
    print()

    print("Correlation gauche/droite -- la signature de l'effondrement des lignes de retard")
    print("  %-22s %12s %12s   %s" % ("phase", noms[0], noms[1], "part des fenetres >= 0,95"))
    for nom, f0, f1 in SCENES[scene]:
        a, b = A[int(f0 * SPF):int(f1 * SPF)], B[int(f0 * SPF):int(f1 * SPF)]
        if len(a) < int(RATE):
            continue
        ma, qa, na = correlation(a)
        mb, qb, nb = correlation(b)
        if ma is None or mb is None:
            print("  %-22s %12s %12s   (silence)" % (nom, "-", "-"))
            continue
        print("  %-22s %12.3f %12.3f   %.0f %% contre %.0f %%  (%d/%d fenetres)"
              % (nom, ma, mb, 100 * qa, 100 * qb, na, nb))
    print()

    print("Decroissance apres un transitoire -- enveloppe par pas de 50 ms, en dBFS")
    for nom, img in ECHOS[scene]:
        ea, eb = enveloppe(A, img), enveloppe(B, img)
        if not ea or not eb:
            continue
        print("  %s (image %d) :" % (nom, img))
        print("     t(s)  " + "".join("%7.2f" % (k * 0.05) for k in range(0, min(len(ea), 60), 6)))
        print("     %-6s" % noms[0] + "".join("%7.1f" % db(ea[k]) for k in range(0, min(len(ea), 60), 6)))
        print("     %-6s" % noms[1] + "".join("%7.1f" % db(eb[k]) for k in range(0, min(len(eb), 60), 6)))
        # le temps pour perdre 20 dB depuis le pic du debut
        for lbl, e in ((noms[0], ea), (noms[1], eb)):
            pic = max(e[:10]) if len(e) >= 10 else max(e)
            t20 = next((k * 0.05 for k in range(len(e)) if e[k] > 0 and db(e[k]) <= db(pic) - 20), None)
            print("     %-6s pic %.1f dBFS, -20 dB atteint %s"
                  % (lbl, db(pic), ("a %.2f s" % t20) if t20 is not None else "jamais en 3 s"))
    print()

    print("Energie par bande, sur tout le passage (dB relatif au total)")
    ba, bb = bandes(A), bandes(B)
    if ba and bb:
        for i, nom in enumerate(("0-250 Hz", "250 Hz-1 kHz", "1-4 kHz", "4-22 kHz")):
            print("  %-14s %8.1f %8.1f   %+.1f dB" % (nom, ba[i], bb[i], bb[i] - ba[i]))
    print()
    print("Ce que ces chiffres ne disent pas : lequel des deux est le son de la borne.")
    print("Il n'y a pas de Model 3 ici, et aucune de ces mesures ne peut y repondre.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
