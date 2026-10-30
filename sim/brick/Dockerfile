# Cross toolchain for the TrimUI Brick build (aarch64 Linux):
#   docker build -t opentracker-brick sim/brick
#   docker run --rm -v "$PWD:/src" -w /src opentracker-brick make -C sim PLATFORM=brick -j
# Debian 11 (glibc 2.31): the binary runs on handheld OSes with this glibc or newer.
FROM debian:bullseye-slim

RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      make g++-aarch64-linux-gnu binutils-aarch64-linux-gnu pkg-config file \
      libsdl2-dev:arm64 libasound2-dev:arm64 \
 && rm -rf /var/lib/apt/lists/*

# pkg-config answers for the arm64 libraries, never the build machine's.
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    CROSS=aarch64-linux-gnu-
