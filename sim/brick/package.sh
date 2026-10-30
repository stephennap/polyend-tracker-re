#!/bin/sh
# Packs the Brick build as an app for the stock TrimUI launcher:
#   sh sim/brick/package.sh OUT_DIR
# OUT_DIR/Apps/OpenTracker/ is copied to the SD card's Apps/ folder.
# Test songs are added when build/sim/testsongs exists (`make -C sim check`
# on the build machine generates them; they are the same on every platform).
set -eu
OUT=${1:?usage: package.sh OUT_DIR}
BIN=build/brick/opentracker.stripped
[ -f "$BIN" ] || BIN=build/brick/opentracker
[ -f "$BIN" ] || { echo "package: build/brick/opentracker missing (make -C sim PLATFORM=brick)" >&2; exit 1; }
SONGS=build/sim/testsongs
APP="$OUT/Apps/OpenTracker"

rm -rf "$OUT"
mkdir -p "$APP/sd-card" "$APP/licenses"
cp "$BIN" "$APP/opentracker"
chmod +x "$APP/opentracker"
cp sim/brick/launch.sh sim/brick/config.json "$APP/"
chmod +x "$APP/launch.sh"
[ -f sim/brick/icon.png ] && cp sim/brick/icon.png "$APP/"
cp sim/brick/README.md "$APP/README.md"
if [ -d "$SONGS/Projects" ]; then cp -R "$SONGS"/. "$APP/sd-card/"; fi
cp sim/third_party/DEJAVU_LICENSE.txt sim/third_party/STB_IMAGE_LICENSE.txt "$APP/licenses/"
{
    echo "OpenTracker for TrimUI Brick"
    echo "commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "built:  $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$APP/VERSION.txt"
echo "package: $APP ($(du -sh "$APP" | cut -f1))"
