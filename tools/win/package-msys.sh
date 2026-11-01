#!/bin/bash
# The Windows package, built natively on Windows with MSYS2's MinGW-w64 toolchain, for a machine that has
# MSYS2 rather than the Mac cross-compiler tools/win/build.sh wants. Same shape as that script's package:
# a folder with the program, the DLLs it needs, the assets, a README and LICENSES, zipped.
#
#   tools/win/package-msys.sh            -> build/win-msys/Model3Recomp/ and Model3Recomp-windows.zip
#   FV2=1 NAME=FightingVipers2 EXE=FightingVipers2.exe tools/win/package-msys.sh
#   FV2=1 CONSOLE=1 ...                  -> the same, with the console menu
#
# FV2=1 makes the Fighting Vipers 2 package instead of the one that serves both games: its own README, and
# texture_replacement/CHARACTERS/Kuhn -- his blue and black, which are not in the repository and which the
# player cannot make for himself. The game starts on the arcade's own road.
#
# CONSOLE=1 adds the console menu on top: assets/menu-console (its art and the ending's, 193 MB, also not in
# the repository) and a settings.ini that has it on from the first start. Without it the folder is about
# 11 MB and the console menu, though still in the program, is off and has no art to draw.
#
# A folder asked for and missing stops the script, rather than shipping a package that half works.
#
# No ROMs and nothing made from them: the player chooses their own vf3.zip or fvipers2.zip at the first
# launch, and it is copied into the package's own roms folder.
#
# MSYS2's SDL3, FreeType and zlib are used as they are, so they come along as DLLs: everything the program
# links that lives in /mingw64/bin is copied beside it, and then whatever those need, until nothing new turns
# up. That is what makes the folder stand on its own -- it runs on a machine with no MSYS2 at all. (The
# cross-built release of tools/win/build.sh links libgcc, libstdc++ and winpthread in and ships only SDL3.dll;
# here they simply come along, which costs a few megabytes and nothing else.)
set -e
cd "$(dirname "$0")/../.."

MINGW=${MINGW:-/mingw64}
NAME=${NAME:-Model3Recomp}
EXE=${EXE:-Model3Recomp.exe}
D=$PWD/build/win-msys
P=$D/$NAME

# make must be handed a writable temp folder, or the compiler tries C:\WINDOWS and stops. One inside the build
# folder always works and needs nothing from the environment.
SCRATCH=${SCRATCH:-$PWD/build/win-msys/tmp}
mkdir -p "$SCRATCH"

echo "---- building ----"
mkdir -p "$D"
make CXX=g++ TMP="$SCRATCH" TEMP="$SCRATCH" TMPDIR="$SCRATCH" \
	OBJ=build/win-msys/obj BIN=build/win-msys/bin \
	LDLIBS="-lz -lws2_32 -liphlpapi -lwinhttp -mwindows -Wl,--stack,16777216" \
	build/win-msys/bin/vf3 -j"$(nproc 2>/dev/null || echo 8)"

echo "---- assembling ----"
rm -rf "$P"
mkdir -p "$P/assets/icon" "$P/roms"
cp build/win-msys/bin/vf3.exe "$P/$EXE"
strip "$P/$EXE"

# every DLL the program loads that comes from MSYS2, and then the DLLs those need, until nothing new turns up
copy_deps() {
	local changed=1
	while [ $changed -eq 1 ]; do
		changed=0
		for f in "$P/$EXE" "$P"/*.dll; do
			[ -e "$f" ] || continue
			ldd "$f" 2>/dev/null | awk '{print $3}' | grep -i "^${MINGW}/bin/" | while read -r dll; do
				[ -f "$dll" ] || continue
				[ -f "$P/$(basename "$dll")" ] && continue
				cp "$dll" "$P/" && echo "  + $(basename "$dll")"
			done
		done
		# anything new this pass?
		for f in "$P"/*.dll; do
			[ -e "$f" ] || continue
			ldd "$f" 2>/dev/null | awk '{print $3}' | grep -i "^${MINGW}/bin/" | while read -r dll; do
				[ -f "$P/$(basename "$dll")" ] || exit 1
			done || changed=1
		done
	done
}
copy_deps

cp -R assets/ui assets/fonts "$P/assets/"
cp assets/icon/icon.png "$P/assets/icon/"
if [ "${FV2:-0}" = 1 ]; then
	[ -d texture_remplacement/CHARACTERS/Kuhn ] || { echo "FV2=1 needs texture_remplacement/CHARACTERS/Kuhn in the working tree (it is not in the repository)"; exit 1; }
	mkdir -p "$P/texture_replacement/CHARACTERS"
	cp -R texture_remplacement/CHARACTERS/Kuhn "$P/texture_replacement/CHARACTERS/"
	cp tools/win/README-fv2.txt "$P/README.txt"
	# The hidden fighters ON from the very first start, before the player has any settings of his own.
	# They are what this package is for; the port's own default is off, and a player who does not know
	# the menu row is there finds the select screen exactly as the cabinet left it.
	printf 'settings_version = 3\nfull_roster = 1\n' > "$P/settings.ini"
	if [ "${CONSOLE:-0}" = 1 ]; then
		[ -d assets/menu-console ] || { echo "CONSOLE=1 needs assets/menu-console in the working tree (it is not in the repository)"; exit 1; }
		cp -R assets/menu-console "$P/assets/"
		cat tools/win/README-fv2-console.txt >> "$P/README.txt"
		printf 'console_mode = 1\n' >> "$P/settings.ini"
	fi
else
	cp tools/win/README-msys.txt "$P/README.txt"
fi
tools/release/licenses.sh "$P/LICENSES.txt" --windows 2>/dev/null || \
	echo "(LICENSES.txt needs tools/release/licenses.sh and its dependency tarballs; see tools/release/README.txt)" > "$P/LICENSES.txt"
: > "$P/portable.txt"   # the saves and settings stay in this folder

echo "---- zipping ----"
rm -f "$D/$NAME-windows.zip"
# MSYS2 ships no zip by default; Windows' own PowerShell does the job
if command -v zip >/dev/null 2>&1; then
	(cd "$D" && zip -qr "$NAME-windows.zip" "$NAME")
else
	powershell -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$P")' -DestinationPath '$(cygpath -w "$D/$NAME-windows.zip")' -Force"
fi
echo
ls -la "$P"
echo
ls -la "$D/$NAME-windows.zip"
