#!/bin/sh
# Builds OpenTracker for the TrimUI Brick in Docker and packs it:
#   sh sim/brick/build.sh [OUT_DIR]      (default dist/OpenTracker-Brick)
# Run from the repository root on Linux, macOS or WSL with Docker.
set -eu
OUT=${1:-dist/OpenTracker-Brick}
docker build -t opentracker-brick sim/brick
docker run --rm -v "$PWD:/src" -w /src -u "$(id -u):$(id -g)" opentracker-brick \
    sh -c 'make -C sim PLATFORM=brick -j"$(nproc)" && aarch64-linux-gnu-strip -o build/brick/opentracker.stripped build/brick/opentracker'
sh sim/brick/package.sh "$OUT"
