#!/usr/bin/env bash
#----------------------------------------------------------------------
# verify-linux.sh - configure, build and test one Linux or macOS preset
#----------------------------------------------------------------------
#
#   tools/ci/verify-linux.sh linux          GCC, Debug, every target
#   tools/ci/verify-linux.sh linux-clang    Clang
#   tools/ci/verify-linux.sh linux-asan     GCC with ASan and UBSan
#   tools/ci/verify-linux.sh macos          Apple Clang on macOS
#   tools/ci/verify-linux.sh macos-asan     Apple Clang with ASan and UBSan
#
# The same steps the Linux workflow runs, so a local run and the CI run
# cannot drift, and the same steps tools/ci/verify-windows.ps1 runs on
# Windows, so the two platforms verify the same things: the preset's
# configure, a full build (every library, unit_tests, and the DarkEden
# executable, which links on Linux since the port's area E), a check
# that every required test is registered - ctest exits 0 over an empty
# test list, so "100% passed" alone proves nothing - then the test
# preset, with UBSan halting on its first report under the sanitizer
# preset, so a report is a failure and not a line in a log.
#
# Dependencies (Ubuntu 24.04 names; tools/linux/Dockerfile is the
# reference image): build-essential clang cmake ninja-build perl
# libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
# libjpeg-dev libfreetype-dev fonts-dejavu-core fonts-noto-cjk.
# On macOS (.github/workflows/macos.yml is the reference; the runner
# has cmake already, a desk Mac takes it from Homebrew too): Xcode's
# command-line tools and, from Homebrew, ninja sdl2 sdl2_image sdl2_ttf
# sdl2_mixer jpeg-turbo; the fonts are the system's own. The ratchet
# script uses no Bash 4 construct, and its grep patterns (\s, \b, \w,
# --include) are GNU extensions that the runner's grep accepts - the
# workflow prints which grep that was.
#
# Exit status: non-zero on the first failing step, with that step's
# log tail on stderr. Logs go under build/verification/<preset>/.
#----------------------------------------------------------------------
set -u

preset="${1:-}"
case "$preset" in
	linux|linux-clang|linux-asan|macos|macos-asan) ;;
	*) echo "usage: $0 linux|linux-clang|linux-asan|macos|macos-asan" >&2; exit 2 ;;
esac

cd "$(dirname "$0")/../.." || exit 2

logdir="build/verification/$preset"
mkdir -p "$logdir"

run () {
	local name="$1"; shift
	local log="$logdir/$name.log"
	echo "== $name: $*"
	if ! "$@" > "$log" 2>&1; then
		echo "-- $name FAILED; last 150 lines of $log:" >&2
		tail -n 150 "$log" >&2
		exit 1
	fi
}

run configure cmake --preset "$preset"
# Warning budgets need the whole diagnostic population, also on local reruns.
run build cmake --build --preset "$preset" --clean-first

# The required tests, the same list verify-windows.ps1 asserts. A tree
# that registered only unit_tests would otherwise go green.
required="unit_tests user_option_tests ui_tests decore_tests decore_vendored ratchets arch_includes source_encoding warning_policy warning_budget_parser format_arity packet_indices wire_inventory_fresh fuzz_replay_client_stream fuzz_replay_client_login_stream fuzz_replay_client_datagram"
inventory="$logdir/inventory.txt"
if ! ctest --preset "$preset" --show-only > "$inventory" 2>&1; then
	echo "-- cannot list the tests of $preset:" >&2
	tail -n 50 "$inventory" >&2
	exit 1
fi
for name in $required; do
	if ! grep -qE "Test +#[0-9]+: $name\$" "$inventory"; then
		echo "-- required test missing from $preset: $name (see $inventory)" >&2
		exit 1
	fi
done

# Every sanitizer report is fatal, so the suite cannot pass while
# printing one. Harmless under the other presets, which have no runtime
# to read them. Leak detection stays off - the game's static tables are
# never freed by design, and the tests construct them.
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

run test ctest --preset "$preset"

run warnings perl tools/ci/check-warnings.pl --log "$logdir/build.log" \
	--profile "$preset-$(uname -m)" --baseline tools/ci/warning-baselines.json \
	--report "$logdir/warnings.json"

# The suite's own totals, which ctest prints only on failure: the line
# CLAUDE.md's baseline is measured from.
"build/presets/$preset/bin/unit_tests" 2>/dev/null | tail -n 1

echo "== $preset: configure, build and test passed"
