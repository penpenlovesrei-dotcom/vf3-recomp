#!/bin/sh
# Runs the instrumented Supermodel hidden, in its own data folder, playing an input script.
#   tools/ref/runcov.sh <run-name> <script.txt> [rom.zip]
# Output (coverage, summary, screenshots) goes to $VF3REF_RUNS/<run-name>/out. Nothing here is committed:
# the outputs are ROM-derived.
#
# Environment:
#   SUPERMODEL    the patched binary (default ~/sites/vf3-ref/Supermodel/bin/supermodel)
#   VF3_ROM       the ROM set (default ~/sites/vf3-ref/roms/vf3.zip), or pass it as the third argument
#   VF3REF_RUNS   where run folders go (default ~/sites/vf3-ref/runs)
#   SM_GAMES_XML  a Games.xml to use instead of Supermodel's (the covtest self-check uses this)
#
# Hidden: on Linux the game draws into a private Xvfb display; on a Mac the window is created hidden with no
# Dock icon (SM_HIDDEN). No window is ever shown. Sound is emulated but played to SDL's dummy device.
set -eu

name=${1:?run name}
script=${2:?input script}
rom=${3:-${VF3_ROM:-$HOME/sites/vf3-ref/roms/vf3.zip}}
sm=${SUPERMODEL:-$HOME/sites/vf3-ref/Supermodel/bin/supermodel}
runs=${VF3REF_RUNS:-$HOME/sites/vf3-ref/runs}
smdir=$(cd "$(dirname "$sm")/.." && pwd)

[ -x "$sm" ] || { echo "no Supermodel binary at $sm" >&2; exit 1; }
[ -f "$rom" ] || { echo "no ROM set at $rom" >&2; exit 1; }
[ -f "$script" ] || { echo "no script at $script" >&2; exit 1; }
script=$(cd "$(dirname "$script")" && pwd)/$(basename "$script")
rom=$(cd "$(dirname "$rom")" && pwd)/$(basename "$rom")

# A fresh data folder per run: Supermodel uses ./Config, ./NVRAM etc. when ./Config exists
dir=$runs/$name
if [ -e "$dir" ]; then
  echo "$dir already exists; pick another run name" >&2
  exit 1
fi
mkdir -p "$dir/Config" "$dir/Assets" "$dir/NVRAM" "$dir/Saves" "$dir/out"
cp "${SM_GAMES_XML:-$smdir/Config/Games.xml}" "$dir/Config/Games.xml"
cp "$smdir/Config/Music.xml" "$dir/Config/"
cp "$smdir/Assets/"*.bmp "$dir/Assets/"
cp "$script" "$dir/script.txt"

opts="${SM_EXTRA:-} -no-throttle -no-vsync -no-threads -res=496,384 -window"
cd "$dir"
export SM_COV_DIR="$dir/out" SM_SCRIPT="$dir/script.txt" SDL_AUDIODRIVER=dummy
start=$(date +%s)
case "$(uname)" in
  Darwin | MINGW* | MSYS* | CYGWIN*)
    # a hidden window: a Mac creates it with no Dock icon, Windows simply never shows it (there is no Xvfb)
    SM_HIDDEN=1 "$sm" "$rom" $opts > "$dir/stdout.txt" 2>&1 || true
    ;;
  *)
    xvfb-run -a -s "-screen 0 1024x768x24" "$sm" "$rom" $opts > "$dir/stdout.txt" 2>&1 || true
    ;;
esac
echo "$name: $(( $(date +%s) - start )) s"
if [ -f "$dir/out/summary.txt" ]; then
  sed -n '1,4p' "$dir/out/summary.txt"
else
  echo "no summary; see $dir/stdout.txt and $dir/Supermodel.log" >&2
  exit 1
fi
