#!/usr/bin/env python3
"""Two WAVs of the same scene: does the difference between them rise above the threshold of hearing?

  tools/sound/audibility.py <wav_a> <wav_b> [--names "a,b"] [--spl 96] [--noise 30] [--worst 8]

Why this tool: "I hear no difference" is information, not a measurement -- it depends on the
headphones, the room, the attention and the passage. What can be measured is the noise-to-mask
ratio (NMR), the quantity perceptual codecs use: is the difference above what the signal itself
makes inaudible at the same instant?

WHAT THE FIRST VERSION OF THIS FILE DID BADLY, and that is the reason for this comment: it compared
a band's difference with THAT BAND'S signal alone. A difference at -50 dBFS in a band where the
signal is at -68 dBFS came out "+18 dB above its mask" -- when a neighbouring band at -10 dBFS masks
it completely, and when at -50 dBFS one is near the absolute threshold of hearing anyway. Two things
were missing: the spreading of the masking between bands, and an absolute threshold. Without them
the measurement classed as audible a difference nobody hears, which is exactly what happened.

The model, and it is simplified:

  1. windows of 46 ms, a hop of 23 ms, Hann, and the mono of both channels;
  2. 25 approximately critical (Bark) bands;
  3. the absolute level is POSTULATED: full scale = --spl dB SPL (96 by default, headphone
     listening at a reasonable level). That is a HYPOTHESIS, not a measurement, and all the rest
     depends on it;
  4. the spreading of the masking: a mask in band j masks band i with 10 dB/Bark above it
     (spreading upwards, shallow) and 27 dB/Bark below (steeper);
  5. threshold = max(spread mask - 6 dB, the absolute threshold of hearing, the room's ambient
     noise); the ambient noise (--noise, 30 dB SPL by default, spread over the bands) is the third
     term, and it was not there on the first attempt: without it the model counted as audible
     residues at -90 dBFS in a digital silence -- true in an anechoic chamber, false in a room;
  6. NMR = the difference's level - the threshold, per band and per window.

    NMR < 0 dB  : the difference is under the threshold, it should not be heard in that band
    NMR > 0 dB  : it is above it, it should be heard

WHAT THE CONTROL TAUGHT, ON 2026-10-24, AND IT IS THIS FILE'S LESSON. Run on a case whose answer is
known -- Fighting Vipers 2's victory screen, where the MADRS mirror gives rise to a sound that climbs
24 dB up to the limiter (commit 335c050) -- this program returns figures SMALLER than on a far more
subtle case:

    FV2's victory screen (gross defect, known)      median -15.3 dB   99th +20.0   10.0 % > 0
    Virtua Fighter 3 (slight difference, inaudible)  median -2.2 dB   99th +25.0   47.9 % > 0

That is not a failure: the NMR measures whether a difference is UNMASKED, not whether it is SERIOUS.
FV2's defect is a loud, sustained sound, so its own mask is enormous and the NMR stays low; VF3's
difference falls into a digital silence, where nothing masks it, and the NMR climbs. Both questions
are legitimate and they are two questions:

    a high NMR -> one can DETECT that the two files differ
    a low NMR  -> one will not detect it, even if one of the two is grossly wrong

For "is it serious", the level, the spectrum and the correlation are what to look at
(tools/sound/measure-madrs.py), not this. Never read this program as a measure of seriousness.

The limits, stated in advance: no tonal/noise distinction, no temporal masking (neither pre- nor
post-), no model of loudness integration, an absolute threshold approximated from ISO 226, and the
playback level postulated by hand. This program is for RANKING two cases against each other -- it
must always be run on a control whose answer is known, or its figures are worth nothing.
"""
import sys
import wave

import numpy as np

RATE = 44100.0
N = 2048                      # 46 ms
HOP = N // 2

# The edges of the 25 critical (Bark) bands, in Hz.
BARK = [0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320,
        2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 22050]

# The absolute threshold of hearing per band, in dB SPL (ISO 226 in a free field, approximated).
ATH = [40, 25, 15, 10, 8, 6, 5, 4, 3.5, 3.5, 3, 2, 0, -2, -4, -5, -6, -6, -4, 0, 6, 12, 16, 20, 40]

AMBIENT_NOISE = 30.0          # dB SPL, a quiet room's noise, spread over the 25 bands
SPREAD_UP = 10.0              # dB per Bark, a mask spreads towards the higher bands
SPREAD_DOWN = 27.0            # dB per Bark, towards the lower bands (steeper)
MASKING_INDEX = 6.0           # dB under the spread mask


def read(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2:
            raise SystemExit("%s: expected 16-bit stereo" % path)
        d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return d.reshape(-1, 2)


def spreading_matrix(nb):
    """att[i, j]: how many dB a mask in j loses when it masks band i."""
    att = np.zeros((nb, nb))
    for i in range(nb):
        for j in range(nb):
            att[i, j] = SPREAD_UP * (i - j) if i > j else SPREAD_DOWN * (j - i)
    return att


def bands(power, hz):
    out = np.zeros((power.shape[0], len(BARK) - 1))
    for b in range(len(BARK) - 1):
        m = (hz >= BARK[b]) & (hz < BARK[b + 1])
        if m.any():
            out[:, b] = power[:, m].sum(axis=1)
    return out


def analyse(A, B, names, spl, worst, noise=AMBIENT_NOISE):
    n = min(len(A), len(B))
    A, B = A[:n], B[:n]
    a = A.mean(axis=1)
    d = (A - B).mean(axis=1)

    nf = (len(a) - N) // HOP + 1
    if nf < 2:
        raise SystemExit("the passage is too short")
    idx = np.arange(nf) * HOP
    win = np.hanning(N)
    hz = np.fft.rfftfreq(N, 1.0 / RATE)
    norm = (win.sum() / 2.0) ** 2

    PA = np.empty((nf, len(hz)))
    PD = np.empty((nf, len(hz)))
    for k in range(nf):
        i = idx[k]
        PA[k] = np.abs(np.fft.rfft(a[i:i + N] * win)) ** 2 / norm
        PD[k] = np.abs(np.fft.rfft(d[i:i + N] * win)) ** 2 / norm

    nb = len(BARK) - 1
    ba, bd = bands(PA, hz), bands(PD, hz)

    # in dB SPL: full scale (an amplitude of 32768) is `spl`
    ref = 32768.0 ** 2 / 2.0
    la = 10.0 * np.log10(np.maximum(ba, 1e-20) / ref) + spl
    ld = 10.0 * np.log10(np.maximum(bd, 1e-20) / ref) + spl

    att = spreading_matrix(nb)
    # the spread mask: for each band i, the energy sum of the attenuated neighbouring masks
    mask = np.empty_like(la)
    for i in range(nb):
        contrib = la - att[i][None, :]
        mask[:, i] = 10.0 * np.log10(np.sum(10.0 ** (contrib / 10.0), axis=1))
    # the ambient noise, spread equally over the bands (an approximation: a real room's noise
    # leans towards the bottom of the spectrum, which would make it more masking down there)
    per_band = noise - 10.0 * np.log10(nb)
    threshold = np.maximum(np.maximum(mask - MASKING_INDEX, np.array(ATH)[None, :]), per_band)

    nmr = ld - threshold
    worst_band = nmr.max(axis=1)

    print("  %d windows over %.1f s; full scale postulated at %.0f dB SPL, ambient noise %.0f dB SPL"
          % (nf, n / RATE, spl, noise))
    print("  noise-to-mask ratio (NMR), the worst band of each window:")
    for q, lbl in ((50, "median"), (90, "90th centile"), (99, "99th centile"), (100, "maximum")):
        print("    %-14s %+7.1f dB" % (lbl, np.percentile(worst_band, q)))
    print("  share of windows where a band is above the threshold (NMR > 0): %.2f %%"
          % (100.0 * np.mean(worst_band > 0)))
    print("  share above +6 dB: %.2f %%   above +12 dB: %.2f %%"
          % (100.0 * np.mean(worst_band > 6), 100.0 * np.mean(worst_band > 12)))

    print("  the %d most exposed windows:" % worst)
    for k in sorted(np.argsort(-worst_band)[:worst]):
        b = int(np.argmax(nmr[k]))
        print("    t = %6.2f s  band %5d-%5d Hz  signal %6.1f  difference %6.1f  threshold %6.1f  NMR %+6.1f dB"
              % (idx[k] / RATE, BARK[b], BARK[b + 1], max(la[k, b], -99.0), ld[k, b], threshold[k, b], nmr[k, b]))

    print("  per band, the passage's worst NMR:")
    for b in range(nb):
        k = int(np.argmax(nmr[:, b]))
        if nmr[k, b] < -30:
            continue
        print("    %5d-%5d Hz   NMR %+6.1f dB   at t = %6.2f s   (signal %.1f, threshold %.1f)"
              % (BARK[b], BARK[b + 1], nmr[k, b], idx[k] / RATE, la[k, b], threshold[k, b]))
    return float(np.percentile(worst_band, 99)), float(np.mean(worst_band > 0))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    names = ("A", "B")
    spl = 96.0
    noise = AMBIENT_NOISE
    worst = 8
    for i, x in enumerate(argv):
        if x == "--names" and i + 1 < len(argv):
            names = tuple(argv[i + 1].split(",")[:2])
        if x == "--spl" and i + 1 < len(argv):
            spl = float(argv[i + 1])
        if x == "--noise" and i + 1 < len(argv):
            noise = float(argv[i + 1])
        if x == "--worst" and i + 1 < len(argv):
            worst = int(argv[i + 1])
    print("%s  against  %s" % (names[0], names[1]))
    A, B = read(argv[1]), read(argv[2])
    m = min(len(A), len(B))
    if np.array_equal(A[:m], B[:m]):
        print("  The two files are IDENTICAL. Nothing to hear, by construction.")
        return 0
    analyse(A, B, names, spl, worst, noise)
    print()
    print("  NMR < 0: under the threshold, should not be heard. NMR > 0: should be heard.")
    print("  The absolute level is POSTULATED (--spl), not measured, and everything depends on it.")
    print("  This ranking is worth something only against a control whose answer is known.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
