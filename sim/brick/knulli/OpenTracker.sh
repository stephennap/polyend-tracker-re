#!/bin/bash
# Knulli (and other Batocera-based systems): started from the Ports list.
# Install: this script and the opentracker/ folder go to /userdata/roms/ports/.
# Everything stays in opentracker/: the card is sd-card/, with
# sd-card.eeprom (settings), sd-card.keys.txt (controls, written on first
# start) and sd-card.log next to it. Quit with SELECT + MENU.
DIR="$(cd "$(dirname "$0")" && pwd)/opentracker"
cd "$DIR" || exit 1
export HOME="$DIR"
# Libraries bundled in opentracker/lib/ win over the system's.
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Controller mappings: a gamecontrollerdb.txt in opentracker/ is read by the
# app itself; otherwise hand SDL the system's database if there is one.
if [ ! -f "$DIR/gamecontrollerdb.txt" ] && [ -z "${SDL_GAMECONTROLLERCONFIG_FILE:-}" ]; then
    for db in /userdata/system/configs/sdl2/gamecontrollerdb.txt /usr/share/sdl2/gamecontrollerdb.txt \
              /usr/share/batocera/datainit/system/configs/sdl2/gamecontrollerdb.txt; do
        if [ -f "$db" ]; then export SDL_GAMECONTROLLERCONFIG_FILE="$db"; break; fi
    done
fi
mkdir -p "$DIR/sd-card"
exec ./opentracker --sd "$DIR/sd-card" --fullscreen "$@" >"$DIR/sd-card.log" 2>&1
