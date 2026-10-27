#!/bin/sh
# Packs the Windows simulator into a folder that runs without MSYS2:
#   sh sim/windows/package.sh OUT_DIR
# Run from the repository root in an MSYS2 UCRT64 shell, after
# `make -C sim check` (which builds the exes and the test songs).
set -eu
OUT=${1:?usage: package.sh OUT_DIR}
EXE=build/sim/opentracker-sim.exe
GUI=build/sim/OpenTracker.exe
SONGS=build/sim/testsongs

for f in "$EXE" "$GUI"; do
    [ -f "$f" ] || { echo "package: $f missing (run make -C sim first)" >&2; exit 1; }
done
[ -d "$SONGS/Projects" ] || { echo "package: $SONGS/Projects missing (run make -C sim check first)" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/sd-card" "$OUT/licenses"
# Without the debug info (-g) the exes are a fraction of the size.
# OpenTracker.exe: double-click, no console, finds sd-card/ next to it.
# opentracker-sim.exe: the same program for a terminal (--help, --list-midi).
strip -o "$OUT/OpenTracker.exe" "$GUI"
strip -o "$OUT/opentracker-sim.exe" "$EXE"
cp sim/windows/README.txt "$OUT/"
# The card: generated test songs (Projects/) and their source samples.
cp -R "$SONGS"/. "$OUT/sd-card/"
# Third-party code and data linked into the exe.
cp sim/third_party/DEJAVU_LICENSE.txt sim/third_party/STB_IMAGE_LICENSE.txt "$OUT/licenses/"
prefix=$(pkg-config --variable=prefix sdl2 2>/dev/null || true)
for f in "$prefix/share/licenses/SDL2/LICENSE.txt" "$prefix/share/licenses/SDL2/COPYING.txt"; do
    if [ -f "$f" ]; then cp "$f" "$OUT/licenses/SDL2_LICENSE.txt"; break; fi
done
# Version stamp: the commit the package was built from.
{
    echo "OpenTracker simulator for Windows"
    echo "commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "built:  $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT/VERSION.txt"
echo "package: $OUT ($(du -sh "$OUT" | cut -f1))"
