#!/bin/sh
# Packs the Brick build for the handheld's launcher:
#   sh sim/brick/package.sh OUT_DIR
#   OUT_DIR/knulli/ports/   -> the SD card's /userdata/roms/ports/ (Knulli, Batocera)
#   OUT_DIR/stock/Apps/     -> the SD card's Apps/ (stock TrimUI OS)
# Test songs are added when build/sim/testsongs exists (`make -C sim check`
# on the build machine generates them; they are the same on every platform).
set -eu
OUT=${1:?usage: package.sh OUT_DIR}
BIN=build/brick/opentracker.stripped
[ -f "$BIN" ] || BIN=build/brick/opentracker
[ -f "$BIN" ] || { echo "package: build/brick/opentracker missing (make -C sim PLATFORM=brick)" >&2; exit 1; }
SONGS=build/sim/testsongs

# The app folder, the same for every launcher: binary, card, licenses, notes.
fill() {
    mkdir -p "$1/sd-card" "$1/licenses"
    cp "$BIN" "$1/opentracker"
    chmod +x "$1/opentracker"
    cp sim/brick/README.md "$1/README.md"
    if [ -d "$SONGS/Projects" ]; then cp -R "$SONGS"/. "$1/sd-card/"; fi
    cp sim/third_party/DEJAVU_LICENSE.txt sim/third_party/STB_IMAGE_LICENSE.txt "$1/licenses/"
    {
        echo "OpenTracker for TrimUI Brick"
        echo "commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
        echo "built:  $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    } > "$1/VERSION.txt"
}

rm -rf "$OUT"

# Knulli: ports/OpenTracker.sh shows in the Ports list; data in ports/opentracker/.
fill "$OUT/knulli/ports/opentracker"
cp sim/brick/knulli/OpenTracker.sh "$OUT/knulli/ports/"
chmod +x "$OUT/knulli/ports/OpenTracker.sh"

# Stock TrimUI OS: Apps/OpenTracker/ with config.json for the launcher.
APP="$OUT/stock/Apps/OpenTracker"
fill "$APP"
cp sim/brick/launch.sh sim/brick/config.json "$APP/"
chmod +x "$APP/launch.sh"
if [ -f sim/brick/icon.png ]; then cp sim/brick/icon.png "$APP/"; fi

echo "package: $OUT ($(du -sh "$OUT" | cut -f1))"
