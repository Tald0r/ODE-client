#!/usr/bin/env bash
# Package the non-sanitized executable after the macos CI preset passes.
set -euo pipefail
cd "$(dirname "$0")/../.."

test "$(uname -s)" = Darwin
case "$(uname -m)" in
    arm64) arch=arm64; platform='Apple Silicon (arm64)'; brew_prefix=/opt/homebrew ;;
    x86_64) arch=x64; platform='Intel (x86_64)'; brew_prefix=/usr/local ;;
    *) echo "Unsupported macOS architecture" >&2; exit 1 ;;
esac
binary=build/presets/macos/bin/DarkEden
test -x "$binary"
test "$(lipo -archs "$binary")" = "$(uname -m)"
package="build/packages/darkeden-client-macos-$arch"
mkdir -p "$package"
cp "$binary" "$package/DarkEden"
chmod 755 "$package/DarkEden"
mkdir -p "$package/licenses/unrar"
cp build/presets/macos/_deps/ixwebsocket-src/LICENSE.txt "$package/licenses/ixwebsocket.txt"
cp third_party/xbrz/License.txt "$package/licenses/xbrz.txt"
cp third_party/unrar/license.txt third_party/unrar/acknow.txt \
    third_party/unrar/README.md "$package/licenses/unrar/"

# Resolve every directly linked non-system library on the build machine.
# Dependencies stay external, as in the Linux release package.
otool -L "$binary" > "$package/DEPENDENCIES.txt"
while IFS= read -r library; do
    case "$library" in
        /usr/lib/*|/System/Library/*) ;;
        /*) test -f "$library" ;;
        *) echo "Unresolved runtime library: $library" >&2; exit 1 ;;
    esac
done < <(tail -n +2 "$package/DEPENDENCIES.txt" | awk '{print $1}')

cat > "$package/README.txt" <<EOF
DarkEden client - experimental macOS $platform CI build

Built and tested on macOS 15 using the macos Debug preset, without sanitizers.
Use macOS 15 or later on $platform.
This is a bare command-line executable, not a notarized .app bundle.
Desktop rendering, audio, IME and live-server gameplay have not been verified.

Install runtime libraries using native Homebrew at $brew_prefix:
  brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer jpeg-turbo

Extract runtime assets v2 into this directory, so Data/ is beside DarkEden:
  https://github.com/bound2/opendarkeden-client/releases/tag/assets-v2
Existing UserSet/ settings can be retained. Assets and account settings are
not included. Open Terminal, cd to this writable game directory, then run:
  ./DarkEden

BUILD-INFO.txt records the exact source revision and build environment.
DEPENDENCIES.txt records the linked runtime libraries; they are not bundled.
EOF

{
    echo "Source: https://github.com/bound2/opendarkeden-client"
    echo "Commit: $(git rev-parse HEAD)"
    echo "Preset: macos (Debug, no sanitizers)"
    echo "CI: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
    echo "Built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    sw_vers
    clang --version
    file "$binary"
    otool -l "$binary" | sed -n '/LC_BUILD_VERSION/,+6p'
    brew list --versions sdl2 sdl2_image sdl2_ttf sdl2_mixer jpeg-turbo
} > "$package/BUILD-INFO.txt"
cp build/verification/macos/test.log "$package/CTEST.txt"
tar -czf "$package.tar.gz" -C build/packages "darkeden-client-macos-$arch"
tar -tzf "$package.tar.gz"
shasum -a 256 "$package.tar.gz"
