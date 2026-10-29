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
DEPOT = os.path.abspath(os.path.join(SP, "..", ".."))
CPP = os.path.join(DEPOT, 'src', 'sound', 'dsp_own.cpp')
H = os.path.join(DEPOT, 'src', 'sound', 'dsp_own.h')
SORTIE = os.path.join(DEPOT, 'build', 'bin', 'variantes')
ORIG_CPP = os.path.join(DEPOT, 'build', 'v-orig.cpp')
ORIG_H = os.path.join(DEPOT, 'build', 'v-orig.h')

VARIANTS = []
exec(io.open(os.path.join(SP, 'dsp-variants.py'), encoding='utf-8').read())

ENV = dict(os.environ)
ENV['PATH'] = r"C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + ENV['PATH']
ENV['TMP'] = ENV['TEMP'] = ENV['TMPDIR'] = os.path.join(os.environ['LOCALAPPDATA'], 'Temp')
MAKE = r"C:\msys64\usr\bin\make.exe"

shutil.copyfile(CPP, ORIG_CPP)
shutil.copyfile(H, ORIG_H)
os.makedirs(SORTIE, exist_ok=True)

def restaure():
    shutil.copyfile(ORIG_CPP, CPP)
    shutil.copyfile(ORIG_H, H)

def applique(patches):
    restaure()
    for p in patches:
        vieux, neuf = p[0], p[1]
        cible = H if (len(p) > 2 and p[2] == 'h') else CPP
        s = io.open(cible, encoding='utf-8').read()
        if s.count(vieux) != 1:
            return f"fragment not found in {os.path.basename(cible)} ({s.count(vieux)} times)"
        io.open(cible, 'w', encoding='utf-8', newline='').write(s.replace(vieux, neuf))
    return None

def construit(cibles):
    r = subprocess.run([MAKE, 'CXX=g++'] + cibles + ['-j8'], cwd=DEPOT, env=ENV,
                       capture_output=True, text=True, errors='replace')
    return r.returncode == 0, (r.stdout + r.stderr)

print("ONE EXE PER VARIANT, each verified by the ten tests before being kept.\n")
gardees, refusees = [], []
for nom, quoi, patches in VARIANTS:
    err = applique(patches)
    if err:
        refusees.append((nom, err)); print(f"  {nom:<20} REFUSED: {err}"); continue
    ok, journal = construit(['build/bin/measure-core', 'test'])
    if not ok:
        bout = [l for l in journal.split('\n') if 'rror' in l or 'FAILED' in l][:2]
        refusees.append((nom, 'build or test failed')); print(f"  {nom:<20} REFUSED: {bout}")
        continue
    verts = journal.count('all cases pass')
    if verts < 24:
        refusees.append((nom, f'{verts} files green instead of 24'))
        print(f"  {nom:<20} REFUSED: {verts} files green instead of 24"); continue
    exe = os.path.join(SORTIE, f'mc-{nom}.exe')
    shutil.copyfile(os.path.join(DEPOT, 'build', 'bin', 'measure-core.exe'), exe)
    gardees.append((nom, quoi))
    print(f"  {nom:<20} ok, 24 files green  ->  build/bin/variantes/mc-{nom}.exe")

restaure()
construit(['build/bin/measure-core'])
print(f"\n  kept: {len(gardees)}; refused: {len(refusees)}")
print("  (src/ is restored, and measure-core rebuilt from the original)")
io.open(os.path.join(DEPOT, 'build', 'variants-kept.txt'), 'w', encoding='utf-8').write(
    '\n'.join(f"{n}\t{q}" for n, q in gardees))
