# -*- coding: utf-8 -*-
# build-variants.py: one exe per DSP variant, each one VERIFIED before being kept.
#
#     python tools/sound/build-variants.py
#
# The variants live in tools/sound/dsp-variants.py, which says what each one attacks. This script applies
# them one by one to src/sound/dsp_own.{h,cpp}, builds, runs ALL TEN TESTS, and keeps the exe only if
# all 24 files are green. src/ is restored at the end, and measure-core rebuilt from the original.
#
# WHY THE VERIFICATION IS IN THIS SCRIPT AND NOT BESIDE IT. On 2026-10-28, the `grouped_output` variant
# grouped a step's four output tests into a single mask -- and swept up along the way the writes of FRCL
# and ADRL, which live between TWT's and EWT's. It passed ALL the other tests, would have passed the
# replayer and produced a byte-identical WAV, because NO game sets those two fields. Only cases 9 to 13 of
# tools/sound/test_scsp_dsp.cpp stopped it. An unverified variant that wins is worse than one that loses.
#
# The judging itself needs an administrator window: tools/sound/measure-variants.cmd.
import io, os, re, shutil, subprocess, sys

SP = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(SP, "..", ".."))
CPP = os.path.join(REPO, 'src', 'sound', 'dsp_own.cpp')
H = os.path.join(REPO, 'src', 'sound', 'dsp_own.h')
OUT = os.path.join(REPO, 'build', 'bin', 'variants')
ORIG_CPP = os.path.join(REPO, 'build', 'v-orig.cpp')
ORIG_H = os.path.join(REPO, 'build', 'v-orig.h')

VARIANTS = []
exec(io.open(os.path.join(SP, 'dsp-variants.py'), encoding='utf-8').read())

ENV = dict(os.environ)
ENV['PATH'] = r"C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + ENV['PATH']
ENV['TMP'] = ENV['TEMP'] = ENV['TMPDIR'] = os.path.join(os.environ['LOCALAPPDATA'], 'Temp')
MAKE = r"C:\msys64\usr\bin\make.exe"

shutil.copyfile(CPP, ORIG_CPP)
shutil.copyfile(H, ORIG_H)
os.makedirs(OUT, exist_ok=True)

def restore():
    shutil.copyfile(ORIG_CPP, CPP)
    shutil.copyfile(ORIG_H, H)

def apply_patches(patches):
    restore()
    for p in patches:
        old, new = p[0], p[1]
        target = H if (len(p) > 2 and p[2] == 'h') else CPP
        s = io.open(target, encoding='utf-8').read()
        if s.count(old) != 1:
            return f"fragment not found in {os.path.basename(target)} ({s.count(old)} times)"
        io.open(target, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
    return None

def build(targets):
    r = subprocess.run([MAKE, 'CXX=g++'] + targets + ['-j8'], cwd=REPO, env=ENV,
                       capture_output=True, text=True, errors='replace')
    return r.returncode == 0, (r.stdout + r.stderr)

print("ONE EXE PER VARIANT, each verified by the ten tests before being kept.\n")
kept, refused = [], []
for name, what, patches in VARIANTS:
    err = apply_patches(patches)
    if err:
        refused.append((name, err)); print(f"  {name:<20} REFUSED: {err}"); continue
    ok, log = build(['build/bin/measure-core', 'test'])
    if not ok:
        bit = [l for l in log.split('\n') if 'rror' in l or 'FAILED' in l][:2]
        refused.append((name, 'build or test failed')); print(f"  {name:<20} REFUSED: {bit}")
        continue
    green = log.count('all cases pass')
    if green < 24:
        refused.append((name, f'{green} files green instead of 24'))
        print(f"  {name:<20} REFUSED: {green} files green instead of 24"); continue
    exe = os.path.join(OUT, f'mc-{name}.exe')
    shutil.copyfile(os.path.join(REPO, 'build', 'bin', 'measure-core.exe'), exe)
    kept.append((name, what))
    print(f"  {name:<20} ok, 24 files green  ->  build/bin/variants/mc-{name}.exe")

restore()
build(['build/bin/measure-core'])
print(f"\n  kept: {len(kept)}; refused: {len(refused)}")
print("  (src/ is restored, and measure-core rebuilt from the original)")
io.open(os.path.join(REPO, 'build', 'variants-kept.txt'), 'w', encoding='utf-8').write(
    '\n'.join(f"{n}\t{q}" for n, q in kept))
