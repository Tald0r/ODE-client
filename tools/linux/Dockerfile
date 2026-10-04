# The reference Linux build environment for the client: what
# tools/ci/verify-linux.sh and .github/workflows/linux.yml assume, in one
# image, so a Windows machine or a Mac with Docker can run the Linux build
# and tests without a Linux install (docs/linux-macos-port-assessment-2026-09-07.md).
# On an arm64 host the image is aarch64, which has no warning baseline;
# pass --platform linux/amd64 (and a separate build volume) to build and run
# it as the x86_64 profile; README.md records the run that passed.
#
#   docker build -t darkeden-linux tools/linux
#   docker run --rm -v "$PWD:/src" darkeden-linux tools/ci/verify-linux.sh linux
#
# The presets write under build/presets/<preset> inside the mounted tree.
# Mounting a Windows checkout is slow for the configure step (the source
# tree is walked over the mount); a named volume for build/ is faster:
#
#   docker run --rm -v "$PWD:/src" -v darkeden-build:/src/build darkeden-linux \
#       tools/ci/verify-linux.sh linux-asan
#
# The fonts are what TextBackendSDL.cpp looks for on Linux; without them
# the text raster tests cannot draw and the game shows no text.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential clang libclang-rt-18-dev cmake ninja-build git perl ca-certificates \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev \
    libjpeg-dev libfreetype-dev libssl-dev \
    fonts-dejavu-core fonts-noto-cjk \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
