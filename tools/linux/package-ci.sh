#!/usr/bin/env bash
# Package the non-sanitized GCC executable after the linux CI preset passes.
set -euo pipefail
cd "$(dirname "$0")/../.."

test "$(uname -s)" = Linux
test "$(uname -m)" = x86_64
binary=build/presets/linux/bin/DarkEden
test -x "$binary"
package=build/packages/darkeden-client-linux-x64
rm -rf "$package" "$package.tar.gz"
mkdir -p "$package/licenses/unrar"
# The Debug build's DWARF is over 90% of the file; dropping it from the
# copy leaves the tested code and its symbols as they are.
strip --strip-debug -o "$package/DarkEden" "$binary"
chmod 755 "$package/DarkEden"
cp build/presets/linux/_deps/ixwebsocket-src/LICENSE.txt "$package/licenses/ixwebsocket.txt"
cp third_party/xbrz/License.txt "$package/licenses/xbrz.txt"
cp third_party/unrar/license.txt third_party/unrar/acknow.txt \
    third_party/unrar/README.md "$package/licenses/unrar/"

# Every shared library must resolve on the build machine. Dependencies stay
# external, installed from the distribution, as in the macOS package.
ldd "$binary" > "$package/DEPENDENCIES.txt"
if grep -q 'not found' "$package/DEPENDENCIES.txt"; then
    grep 'not found' "$package/DEPENDENCIES.txt" >&2
    exit 1
fi

cat > "$package/README.txt" <<'EOF'
DarkEden client - experimental Linux x64 CI build

Built and tested on GitHub Actions (Ubuntu 24.04, GCC) using the linux Debug
preset, without sanitizers; debug info is stripped from this copy.
Target: Ubuntu 24.04 x86_64. Other distributions have not been validated.

On Ubuntu 24.04, install runtime libraries and fonts:
sudo apt-get update
sudo apt-get install libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-ttf-2.0-0 libsdl2-mixer-2.0-0 libjpeg-turbo8 libfreetype6 libstdc++6 libssl3 ca-certificates fonts-dejavu-core fonts-noto-cjk

DarkEden links OpenSSL 3 dynamically (libssl.so.3 and libcrypto.so.3) for the
WebSocket transport; IXWebSocket itself is statically linked. Setting
DARKEDEN_WEBSOCKET_URL (for example wss://game.example.com/game) selects the
optional WebSocket transport; without it the client connects over TCP.

Extract this archive. Download and extract runtime assets v2 inside the extracted
directory, so Data/ is beside DarkEden:
https://github.com/bound2/opendarkeden-client/releases/tag/assets-v2

In a graphical desktop session, change to that directory and run:
./DarkEden

Run from the game directory; game data is located relative to the working directory.
Existing UserSet/ settings can be retained. Game assets and account settings are
not included. System fonts are required for text rendering.

Linux status: all targets compile and the complete automated suite is checked.
The project has previously reached the main menu using SDL's headless driver.
Display rendering, login, character selection, gameplay, chat, audio and IME have
not been verified on a Linux desktop. This package is an experimental port.

BUILD-INFO.txt records the exact source revision and build environment.
DEPENDENCIES.txt records the linked runtime libraries; they are not bundled.
EOF

{
    echo "Source: https://github.com/bound2/opendarkeden-client"
    echo "Commit: $(git rev-parse HEAD)"
    echo "Preset: linux (Debug, no sanitizers)"
    echo "CI: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
    echo "Built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    grep PRETTY_NAME /etc/os-release
    g++ --version | head -n 1
    file "$binary"
    dpkg-query -W libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev libssl-dev
} > "$package/BUILD-INFO.txt"
cp build/verification/linux/test.log "$package/CTEST.txt"
tar -czf "$package.tar.gz" -C build/packages darkeden-client-linux-x64
tar -tzf "$package.tar.gz"
sha256sum "$package.tar.gz"
