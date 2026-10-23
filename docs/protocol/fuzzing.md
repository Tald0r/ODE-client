---
type: Guide
title: "Fuzzing the packet readers"
description: "Three libFuzzer targets drive the game stream, the login stream and the peer datagram readers with hostile bytes under ASan and UBSan; every native tree replays their seed corpus and the fixed crashes under tests/fuzz/regressions, and each fix also carries a unit test."
tags: [protocol, testing, security]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Fuzzing the packet readers

# The targets

Two libFuzzer targets read the bytes a server sends the client, one per connection of its one
`ClientPlayer`: `tests/fuzz/fuzz_client_stream.cpp`, the game connection, with the player in
`CPS_NORMAL` (every registered id accepted), and `tests/fuzz/fuzz_client_login_stream.cpp`, the
login connection, which reads every input once in each of the seven statuses the player holds
there (`CPS_AFTER_SENDING_CL_LOGIN` to `CPS_AFTER_SENDING_CL_SELECT_PC`), so a frame reaches a
parser only when that status's validator set accepts its id, as in production. Both read through
`tests/fuzz/client_stream_reader.h`: `[encrypt code byte][stream]`, read frame by frame through
the same gates as `ClientPlayer::processCommand` (its header lists them with their line numbers;
keep the two in step), once from the start of the input ring and once with the ring's wrap point
in the middle of the stream. Only a `Throwable` is a rejected input, since that is all
`UpdateSocketInput` catches; a `std::exception` from a reader is a crash, as in the client.

A third, `tests/fuzz/fuzz_client_datagram.cpp`, reads one UDP datagram arriving at the client's
peer-to-peer socket, which is bound on every interface at start-up and authenticates nothing: the
raw datagram (header, body, pad; no code byte), through `Datagram::read` and the
`CPS_CLIENT_COMMUNICATION_NORMAL` validator check, as `ClientCommunicationManager::Update` does,
stopping before dispatch.

# Replays in every tree

Every native test tree builds each target with `tests/fuzz/replay_main.cpp` as
`fuzz_replay_<target>`, and the ctest of that name replays two sets of inputs through it: the seed
corpus, which the `fuzz_corpus_<target>` setup step writes into the build tree with
`tools/fuzz/golden2corpus.pl` (one seed per golden of the connection's packets, plus an empty and
a zero-filled frame per id: 536 `GC` seeds for `client_stream`, 32 `LC` seeds for
`client_login_stream` today; its `--datagram` mode writes the 12 `RC` seeds of `client_datagram`
from the four `RC` `.datagram.` goldens, with the pad byte those leave out), and
`tests/fuzz/regressions/<target>/*.hex`, one file per fixed crash in the goldens' hex style
(`xxd -p crash-... | tr -d '\n'`), named after the packet and the value.

A crash there is a finding that came back, but only a finding that aborted (a libc `assert`, an
escaping `std::exception`) crashes in every tree. One that corrupted memory replays green in a tree
without a sanitizer that sees it: on the unfixed code the `LCPCList` slot input replays green in
the plain `macos` tree and stops at an invalid `Slot` load under Clang's UBSan. So each fix also
carries a test in `tests/unit/test_packet_fuzz_findings.cpp`, which catches it in every tree (the
datagram type confusion, found by reading, is pinned in `tests/unit/test_datagram_frame.cpp`).
**Clang's UBSan checks enum loads and GCC's does not**: a wire byte cast to an enum before its
range check passed the Linux job and aborted the first macOS sanitizer run, so a Clang sanitizer
build (the `macos-asan` preset, or the Linux container with `clang++` and the `libclang-rt`
package) is the one that sees that class.

# Running the fuzzer

The fuzzer itself needs Clang with a libFuzzer runtime and ASan (`BUILD_FUZZERS`, off by default,
refused on other compilers):

```bash
cmake --preset macos-fuzz        # or linux-fuzz
cmake --build --preset macos-fuzz --target fuzz_client_stream fuzz_client_login_stream fuzz_client_datagram
perl tools/fuzz/golden2corpus.pl tests/golden tests/wire-layout.txt /tmp/fz/corpus '^GC'
cd /tmp/fz && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_stream -max_total_time=600 \
  -timeout=10 -rss_limit_mb=2048 -max_len=32768 -close_fd_mask=3 corpus
```

The login target is the same run with its own seeds and binary, in its own scratch directory:

```bash
perl tools/fuzz/golden2corpus.pl tests/golden tests/wire-layout.txt /tmp/fz-login/corpus '^LC'
cd /tmp/fz-login && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_login_stream -max_total_time=600 \
  -timeout=10 -rss_limit_mb=2048 -max_len=32768 -close_fd_mask=3 corpus
```

`-max_len=32768` is the longest input the harness reads: the code byte and a stream of at most
32767 bytes, one less than the 32768-byte input ring; a longer input returns without being read.

The datagram target has its own seed mode and a longer cap, 65536 bytes, the most `recvfrom()`
hands `DatagramSocket` (`DATAGRAM_SOCKET_BUFFER_LEN`):

```bash
perl tools/fuzz/golden2corpus.pl --datagram tests/golden /tmp/fz-dgram/corpus '^RC'
cd /tmp/fz-dgram && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_datagram -max_total_time=900 \
  -timeout=10 -rss_limit_mb=2048 -max_len=65536 -close_fd_mask=3 corpus
```

The recipes set `detect_leaks=0`, so no run looks for leaks. libFuzzer's "disabled leak detection
after every mutation" notice, which every login-target run past its first crash and a game-target
run printed (logs checked 2026-09-30), says only that over a thousand inputs ended with more
allocations than frees. It names no allocation and has not been traced to any, so it is neither a
finding nor a clean bill: a leak in a reader is found by reading (the review's
`LCServerList`/`LCWorldList` row).

# Triage and platform notes

Run it from a scratch directory: a failed `Assert` appends to `assertion_failed.log` in the
working directory. Hostile input to the game target fails about a thousand in fifteen minutes; the
login target failed none in 25 minutes. The datagram target failed none in 16 minutes run with
`DE_FUZZ_ABORT_ON_ASSERT=1` (the `RC` readers hold no `Assert`; `Datagram::read` holds two).
`DE_FUZZ_ABORT_ON_ASSERT=1` turns a failed `Assert` (an `AssertionError`, normally a rejected
input) into a crash, to see which ones hostile input reaches. `-close_fd_mask=3` hides the
crash's own message, so triage by replaying the file through the replay binary of an ASan tree:
`build/presets/macos-asan/bin/fuzz_replay_client_stream crash-...` (or
`fuzz_replay_client_login_stream`, `fuzz_replay_client_datagram`).

Apple Clang ships no libFuzzer, so `macos-fuzz` compiles with Apple Clang and links Homebrew
`llvm@21`'s `libclang_rt.fuzzer_osx.a` (`DARKEDEN_LIBFUZZER_ARCHIVE`); llvm@21's own ASan hangs at
startup on macOS 27. The preset names the Apple Silicon Homebrew path of `llvm@21`; on an Intel
Mac (Homebrew under `/usr/local`) or with another LLVM version, configure stops with "does not
exist" until you pass `-DDARKEDEN_LIBFUZZER_ARCHIVE=<path to libclang_rt.fuzzer_osx.a>`. With that
hybrid, `-fork` reports an ASan container-overflow inside libFuzzer's own merge code, not in ours,
so fuzz single-process on a Mac. `linux-fuzz` uses the distribution Clang and
`-fsanitize=fuzzer`; the `darkeden-linux` image includes the required `libclang-rt-18-dev`
package (rebuild the image if an older copy reports missing sanitizer or libFuzzer archives).
Installing it was verified on 2026-09-29: the target built, fuzzed for two minutes without a
finding, and `unit_tests` and the fuzz ctests passed under that preset's Clang ASan and UBSan.

The findings so far are under *Found by fuzzing* in the
[code-health review](../reviews/code-health-review-2026-08-29.md).
