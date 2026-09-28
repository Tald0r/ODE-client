#!/usr/bin/env bash
set -euo pipefail

# Run inside emscripten/emsdk:6.0.10 with /src read-only and /work a volume.
# Keep compilation on the Linux volume; Windows bind-mount stat calls are slow.
mkdir -p /work/source
tar -C /src --exclude=.git --exclude=build --exclude=tools/web/browser-tests/target -cf - . |
    tar -C /work/source -xf -
emcmake cmake -S /work/source -B /work/build-wasm \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_ENGINE=OFF -DBUILD_TESTS=OFF
# One target per invocation: with the Makefile generator, several targets in
# one parallel build each run their own sub-make, and those raced on the
# iconv external project's configure step in a fresh tree. WEB_BUILD_TARGETS
# narrows the set; the container image builds only the game.
for target in ${WEB_BUILD_TARGETS:-DarkEden web_sprite_tests transport_tests decore_tests}; do
    cmake --build /work/build-wasm --target "$target" -j "${WEB_BUILD_JOBS:-8}"
done
