#!/bin/sh
# Started by the TrimUI launcher (config.json). Everything stays in this
# folder: the card is sd-card/, with sd-card.eeprom (settings),
# sd-card.keys.txt (controls, written on first start) and sd-card.log next to it.
cd "$(dirname "$0")" || exit 1
APP=$(pwd)
export HOME="$APP"
# Libraries bundled next to the binary (lib/) win over the system's.
export LD_LIBRARY_PATH="$APP/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$APP/sd-card"
exec ./opentracker --sd "$APP/sd-card" --fullscreen "$@" >"$APP/sd-card.log" 2>&1
