#!/bin/bash
# The Windows package, built natively on Windows with MSYS2's MinGW-w64 toolchain, for a machine that has
# MSYS2 rather than the Mac cross-compiler tools/win/build.sh wants. Same shape as that script's package:
# a folder with the program, the DLLs it needs, the assets, a README and LICENSES, zipped.
#
#   tools/win/package-msys.sh            -> build/win-msys/Model3Recomp/ and Model3Recomp-windows.zip
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
	LDLIBS="-lz -mwindows -Wl,--stack,16777216" \
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
cp tools/win/README-msys.txt "$P/README.txt"
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
