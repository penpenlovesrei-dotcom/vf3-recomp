#!/usr/bin/env python3
"""Compares two WAVs of the same scene, and says what separates them.

  tools/sound/measure-madrs.py <wav_a> <wav_b> [--names "a,b"] [--scene vf3|fv2]

Written for one precise question (docs/fv2-sound-program.md section 5): the MADRS mirror folds the
DSP's delay lines onto a single address, and what that does to Virtua Fighter 3's sound has to be
known, since the setting was kept there on the reverberation's LENGTH alone.

The four measurements, and why those:

  level        -- the mean peak per phase, like the measurement that settled Fighting Vipers 2 on
                  2026-10-24 (VIC_DSP's mean peak 22831 -> 6532). Comparable with that one.
  correlation  -- the left/right correlation. That is THE signature of the collapse: if every delay
                  line reads the same buffer, the two channels come to resemble each other. On VF2
                  the measurement gave 0.99 with the mirror against 0.73-0.85 without.
  decay        -- the envelope after the coin and after the start, in steps of 50 ms over 3 s. That
                  is exactly what scsp.h claims to have measured ("die in about a second ... rang on
                  for several"), so the measurement is redone at the same place.
  spectrum     -- the energy per band, to see whether it is the colour that moves and not just the
                  level.

The phases come from the scene script's marks, not from a split made by eye.
"""
import sys
import wave

import numpy as np

FRAME_HZ = 57.524160            # the Model 3's rate (main.cpp, FRAME_HZ)
RATE = 44100.0
SPF = RATE / FRAME_HZ           # samples per frame: 766.63...

# The phases, in frames, read in the scripts in tools/scripts.
SCENES = {
    # vf3-match.txt: 0 boot, 300 attract, 600 coin, 750 select, 930 fight, 2190 end
    "vf3": [("boot", 0, 300), ("attract", 300, 600), ("coin and select", 600, 930),
            ("fight", 930, 2190), ("end", 2190, 2230)],
    # fv2-fight.txt
    "fv2": [("boot", 0, 960), ("attract", 960, 1900), ("coin and select", 1900, 2500),
            ("loading and intro", 2500, 2950), ("fight", 2950, 4150)],
}

# The moments where scsp.h says it measured the echoes' decay.
ECHOES = {"vf3": [("the coin", 600), ("the start", 660)],
          "fv2": [("the coin", 1900), ("the start", 1960)]}


def read(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 2 or w.getsampwidth() != 2:
            raise SystemExit("%s: expected 16-bit stereo" % path)
        d = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    return d.reshape(-1, 2)


def db(x):
    return -np.inf if x <= 0 else 20.0 * np.log10(x / 32768.0)


def mean_peak(x):
    """The peak per frame, averaged over the phase -- FV2's measurement, in raw units."""
    n = int(len(x) // SPF)
    if n < 1:
        return 0.0
    peaks = [np.abs(x[int(i * SPF):int((i + 1) * SPF)]).max() for i in range(n)]
    return float(np.mean(peaks))


def correlation(x, window=0.25):
    """Left/right correlation, by windows; returns the median and the share of windows >= 0.95."""
    n = int(window * RATE)
    out = []
    for i in range(0, len(x) - n, n):
        left, right = x[i:i + n, 0], x[i:i + n, 1]
        if np.abs(left).max() < 64 and np.abs(right).max() < 64:
            continue                       # silence: a correlation means nothing there
        sl, sr = left.std(), right.std()
        if sl < 1e-9 or sr < 1e-9:
            continue
        out.append(float(np.mean((left - left.mean()) * (right - right.mean())) / (sl * sr)))
    if not out:
        return None, None, 0
    a = np.array(out)
    return float(np.median(a)), float(np.mean(a >= 0.95)), len(a)


def envelope(x, start_frame, length=3.0, step=0.05):
    """The envelope (peak) in steps of `step` seconds, from a frame on."""
    i0 = int(start_frame * SPF)
    n = int(step * RATE)
    out = []
    for k in range(int(length / step)):
        a = i0 + k * n
        if a + n > len(x):
            break
        out.append(float(np.abs(x[a:a + n]).max()))
    return out


def bands(x):
    """The energy in four bands, in dB relative to the total."""
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
    names = ("A", "B")
    scene = "vf3"
    for i, a in enumerate(argv):
        if a == "--names" and i + 1 < len(argv):
            names = tuple(argv[i + 1].split(",")[:2])
        if a == "--scene" and i + 1 < len(argv):
            scene = argv[i + 1]
    A, B = read(pa), read(pb)
    n = min(len(A), len(B))
    A, B = A[:n], B[:n]

    print("%s  against  %s" % (names[0], names[1]))
    print("%d samples, %.1f s, scene %s\n" % (n, n / RATE, scene))

    diff = A - B
    print("Do the two files differ?")
    print("  the largest difference on one sample : %.0f  (%.1f dBFS)"
          % (np.abs(diff).max(), db(np.abs(diff).max())))
    print("  the RMS difference                   : %.1f  (%.1f dBFS)" % (diff.std(), db(diff.std())))
    if np.abs(diff).max() == 0:
        print("  *** IDENTICAL: the setting changed nothing on this scene ***")
        return 0
    print()

    print("Mean peak per frame, by phase (raw units, like FV2's measurement)")
    print("  %-22s %12s %12s %10s" % ("phase", names[0], names[1], "difference"))
    for name, f0, f1 in SCENES[scene]:
        a, b = A[int(f0 * SPF):int(f1 * SPF)], B[int(f0 * SPF):int(f1 * SPF)]
        if len(a) < int(SPF):
            continue
        pa_, pb_ = mean_peak(a), mean_peak(b)
        r = ("%+.1f dB" % (db(pb_) - db(pa_))) if pa_ > 0 and pb_ > 0 else "-"
        print("  %-22s %12.0f %12.0f %10s" % (name, pa_, pb_, r))
    print()

    print("Left/right correlation -- the signature of the delay lines collapsing")
    print("  %-22s %12s %12s   %s" % ("phase", names[0], names[1], "share of windows >= 0.95"))
    for name, f0, f1 in SCENES[scene]:
        a, b = A[int(f0 * SPF):int(f1 * SPF)], B[int(f0 * SPF):int(f1 * SPF)]
        if len(a) < int(RATE):
            continue
        ma, qa, na = correlation(a)
        mb, qb, nb = correlation(b)
        if ma is None or mb is None:
            print("  %-22s %12s %12s   (silence)" % (name, "-", "-"))
            continue
        print("  %-22s %12.3f %12.3f   %.0f %% against %.0f %%  (%d/%d windows)"
              % (name, ma, mb, 100 * qa, 100 * qb, na, nb))
    print()

    print("Decay after a transient -- the envelope in steps of 50 ms, in dBFS")
    for name, frame in ECHOES[scene]:
        ea, eb = envelope(A, frame), envelope(B, frame)
        if not ea or not eb:
            continue
        print("  %s (frame %d):" % (name, frame))
        print("     t(s)  " + "".join("%7.2f" % (k * 0.05) for k in range(0, min(len(ea), 60), 6)))
        print("     %-6s" % names[0] + "".join("%7.1f" % db(ea[k]) for k in range(0, min(len(ea), 60), 6)))
        print("     %-6s" % names[1] + "".join("%7.1f" % db(eb[k]) for k in range(0, min(len(eb), 60), 6)))
        # the time to lose 20 dB from the peak at the start
        for lbl, e in ((names[0], ea), (names[1], eb)):
            peak = max(e[:10]) if len(e) >= 10 else max(e)
            t20 = next((k * 0.05 for k in range(len(e)) if e[k] > 0 and db(e[k]) <= db(peak) - 20), None)
            print("     %-6s peak %.1f dBFS, -20 dB reached %s"
                  % (lbl, db(peak), ("at %.2f s" % t20) if t20 is not None else "never in 3 s"))
    print()

    print("Energy per band, over the whole scene (dB relative to the total)")
    ba, bb = bands(A), bands(B)
    if ba and bb:
        for i, name in enumerate(("0-250 Hz", "250 Hz-1 kHz", "1-4 kHz", "4-22 kHz")):
            print("  %-14s %8.1f %8.1f   %+.1f dB" % (name, ba[i], bb[i], bb[i] - ba[i]))
    print()
    print("What these figures do not say: which of the two is the cabinet's sound.")
    print("There is no Model 3 here, and none of these measurements can answer that.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
