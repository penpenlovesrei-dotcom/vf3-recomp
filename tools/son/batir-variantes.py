# -*- coding: utf-8 -*-
# batir-variantes.py : un exe par variante du DSP, chacune VERIFIEE avant d'etre gardee.
#
#     python tools/son/batir-variantes.py
#
# Les variantes sont dans tools/son/variantes-dsp.py, qui dit ce que chacune attaque. Celui-ci les applique
# une par une a src/sound/dsp_maison.{h,cpp}, construit, lance LES DIX TESTS, et ne garde l'exe que si les
# 24 fichiers sont au vert. src/ est restaure a la fin, et mesurer-coeur reconstruit depuis l'original.
#
# POURQUOI LA VERIFICATION EST DANS CE SCRIPT ET NON A COTE. Le 2026-10-28, la variante `sortie_groupee`
# groupait les quatre tests de sortie d'un pas dans un seul masque -- et englobait au passage les ecritures
# de FRCL et d'ADRL, qui vivent entre celle de TWT et celle d'EWT. Elle passait TOUS les autres tests,
# aurait passe le rejoueur et rendu le WAV identique a l'octet, parce qu'AUCUN jeu ne pose ces deux champs.
# Seuls les cas 9 a 13 de tools/son/test_scsp_dsp.cpp l'ont arretee. Une variante non verifiee qui gagne est
# pire qu'une variante qui perd.
#
# Le jugement, lui, demande une fenetre administrateur : tools/son/mesurer-variantes.cmd.
import io, os, re, shutil, subprocess, sys

SP = os.path.dirname(os.path.abspath(__file__))
DEPOT = os.path.abspath(os.path.join(SP, "..", ".."))
CPP = os.path.join(DEPOT, 'src', 'sound', 'dsp_maison.cpp')
H = os.path.join(DEPOT, 'src', 'sound', 'dsp_maison.h')
SORTIE = os.path.join(DEPOT, 'build', 'bin', 'variantes')
ORIG_CPP = os.path.join(DEPOT, 'build', 'v-orig.cpp')
ORIG_H = os.path.join(DEPOT, 'build', 'v-orig.h')

VARIANTES = []
exec(io.open(os.path.join(SP, 'variantes-dsp.py'), encoding='utf-8').read())

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
            return f"morceau introuvable dans {os.path.basename(cible)} ({s.count(vieux)} fois)"
        io.open(cible, 'w', encoding='utf-8', newline='').write(s.replace(vieux, neuf))
    return None

def construit(cibles):
    r = subprocess.run([MAKE, 'CXX=g++'] + cibles + ['-j8'], cwd=DEPOT, env=ENV,
                       capture_output=True, text=True, errors='replace')
    return r.returncode == 0, (r.stdout + r.stderr)

print("UN EXE PAR VARIANTE, chacune verifiee par les dix tests avant d'etre gardee.\n")
gardees, refusees = [], []
for nom, quoi, patches in VARIANTES:
    err = applique(patches)
    if err:
        refusees.append((nom, err)); print(f"  {nom:<20} REFUSEE : {err}"); continue
    ok, journal = construit(['build/bin/mesurer-coeur', 'test'])
    if not ok:
        bout = [l for l in journal.split('\n') if 'rror' in l or 'ECHEC' in l][:2]
        refusees.append((nom, 'construction ou test en echec')); print(f"  {nom:<20} REFUSEE : {bout}")
        continue
    verts = journal.count('tous les cas passent')
    if verts < 24:
        refusees.append((nom, f'{verts} fichiers au vert au lieu de 24'))
        print(f"  {nom:<20} REFUSEE : {verts} fichiers au vert au lieu de 24"); continue
    exe = os.path.join(SORTIE, f'mc-{nom}.exe')
    shutil.copyfile(os.path.join(DEPOT, 'build', 'bin', 'mesurer-coeur.exe'), exe)
    gardees.append((nom, quoi))
    print(f"  {nom:<20} ok, 24 fichiers au vert  ->  build/bin/variantes/mc-{nom}.exe")

restaure()
construit(['build/bin/mesurer-coeur'])
print(f"\n  gardees : {len(gardees)} ; refusees : {len(refusees)}")
print("  (src/ est restaure, et mesurer-coeur reconstruit depuis l'original)")
io.open(os.path.join(DEPOT, 'build', 'variantes-gardees.txt'), 'w', encoding='utf-8').write(
    '\n'.join(f"{n}\t{q}" for n, q in gardees))
