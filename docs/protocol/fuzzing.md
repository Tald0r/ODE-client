---
type: Guide
title: "Fuzzing the packet readers and handlers"
description: "Five libFuzzer targets under ASan and UBSan: three drive the game stream, the login stream and the peer datagram readers with hostile bytes, two run the client's receive loop into the packet handlers; every native tree replays their seed corpus and the fixed crashes under tests/fuzz/regressions, and each fix also carries a unit test."
tags: [protocol, testing, security]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Fuzzing the packet readers and handlers

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

# The handler targets

Two more targets go past `read()`: `tests/fuzz/fuzz_client_stream_handlers.cpp`, the game
connection, and `tests/fuzz/fuzz_client_login_stream_handlers.cpp`, the login connection. They
take the read-only targets' input, `[encrypt code byte][stream]`, so the same seeds serve them,
and run `ClientPlayer::processCommand` itself on a player whose socket never connected: the
production gates, `PacketDispatcher::dispatch` into the handler `Client/PacketHandlerRegistry.cpp`
binds, and the player status a handler sets carrying over to the next frame. The game target runs
every input three times in `CPS_NORMAL`, the player a slayer, a vampire and an ousters, since many
handlers branch on the race; the login target runs it from each of the seven login statuses.

The world they dispatch into is `tests/fuzz/handler_world.h` (its header is the full
description): empty data tables sized where code assumes a row exists (an id past a table reads
its default row in the game too, so a crash there is one a server can cause), `InitGameObject`
itself, and for the game target a 32 by 32 zone with the player in its middle (the size of three
of the game's zones; the server's map files hold zones from 17 by 25 to 256 by 256, and a wire
coordinate is a byte). `InitGameObject` installs the executable's host adapters; the run removes
them all (`ReleaseGameObjectHosts`) and puts back the model's, `ZoneState`, `CreatureState`,
`PlayerState` and `MItem`'s, without the entries that reach the UI, a sound or a party member
over UDP. Every presentation call is then skipped, as in the unit tests, so what an adapter does
with a handler's value (`GameMain`'s `SetWeather`, a dialog's text) is not reached.
Nor does the world hold a request the player made: a reply that checks the request mode
`TempInformation` records (a storage move, a trade) finds none and does nothing, as it would if a
server sent it unasked. `GCReconnectLogin` and `LCReconnect` are read and not dispatched: they
reconnect the socket to the address the packet names.

**State is reset per run.** libFuzzer runs every input in one process, so each run builds its
world afresh and deletes it after: every object it makes, its global set back to `NULL`, the
hosts removed, `g_Mode` and `rand()` put back. The tables no run changes are made once per
process (the code that writes them after loading runs only from a presentation host). State
outside the world, a function's static or a frame counter, is not reset; the replay reads its
inputs in one sorted order, so it is deterministic regardless, and a crash libFuzzer reports is
confirmed by replaying its input alone.

**What counts as a finding** is the read-only targets' rule: a sanitizer report, an abort, or
any exception but a `Throwable`, which `UpdateSocketInput` catches and answers with a disconnect.
**A crash can be the harness's** instead: a global the game always sets in that state and the
world does not. Such a crash is fixed in the harness, never in the handler, unless production can
reach the state; the test is whether the game, in the status the target models, can hold the
value. `g_pTradeManager` and `g_pStorage` are `NULL` in the game until a trade or a storage
window opens, and a server can send a trade packet at any time, so the world leaves them `NULL`
and a handler that dereferences one would be a finding. The UDP manager and the peer file manager
are left `NULL` too, because they bind a socket and write files; every use of either tests for
`NULL` already, so no finding can come from that choice.

# Replays in every tree

Every native test tree builds each target with `tests/fuzz/replay_main.cpp` as
`fuzz_replay_<target>`, and the ctest of that name replays two sets of inputs through it: the seed
corpus, which the `fuzz_corpus_<target>` setup step writes into the build tree with
`tools/fuzz/golden2corpus.pl` (one seed per golden of the connection's packets, plus an empty and
a zero-filled frame per id: 536 `GC` seeds for `client_stream`, 32 `LC` seeds for
`client_login_stream` today, which `client_stream_handlers` and `client_login_stream_handlers`
take too; its `--datagram` mode writes the 12 `RC` seeds of `client_datagram` from the four `RC`
`.datagram.` goldens, with the pad byte those leave out), and
`tests/fuzz/regressions/<target>/*.hex`, one file per fixed crash in the goldens' hex style
(`xxd -p crash-... | tr -d '\n'`), named after the packet and the value.

A crash there is a finding that came back, but only a finding that aborted (a libc `assert`, an
escaping `std::exception`) crashes in every tree. One that corrupted memory replays green in a tree
without a sanitizer that sees it: on the unfixed code the `LCPCList` slot input replays green in
the plain `macos` tree and stops at an invalid `Slot` load under Clang's UBSan. So each fix also
carries a test in `tests/unit/test_packet_fuzz_findings.cpp` or the handler's own test file, which
catches it in every tree (the datagram type confusion, found by reading, is pinned in
`tests/unit/test_datagram_frame.cpp`). The handler replays also hold one input each that
`GCReconnectLogin` or `LCReconnect` reads: dispatched, it would reconnect the socket.
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

The handler targets take the same seeds, each in a scratch directory of its own (the run writes
`fuzz_handlers_version.inf` there, the file `InitGameObject` opens first):

```bash
perl tools/fuzz/golden2corpus.pl tests/golden tests/wire-layout.txt /tmp/fz-h/corpus '^GC'
cd /tmp/fz-h && ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  <repo>/build/presets/macos-fuzz/bin/fuzz_client_stream_handlers -max_total_time=900 \
  -timeout=10 -rss_limit_mb=2048 -max_len=32768 -close_fd_mask=3 corpus
```

and `fuzz_client_login_stream_handlers` over the `'^LC'` seeds. A run builds a world, three for
every game input and seven for every login input, so they make about 250 inputs a second on an
Apple Silicon Mac, where the read-only targets make thousands.

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
`fuzz_replay_client_login_stream`, `fuzz_replay_client_datagram`,
`fuzz_replay_client_stream_handlers`, `fuzz_replay_client_login_stream_handlers`).

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
