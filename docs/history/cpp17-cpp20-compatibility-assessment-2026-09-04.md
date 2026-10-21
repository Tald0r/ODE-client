---
type: Record
title: "C++17/C++20 compatibility assessment"
description: "The language-standard upgrade of the client, libraries, tests and tools, sized by priority, with the verification policy of 2026-09-17."
tags: [history, build]
---

# C++17/C++20 compatibility assessment

**Date:** 2026-09-04

**Scope:** Language-standard upgrade of the existing client, libraries, tests, and
tools. This is not an operating-system port or a general modernization rewrite.

**Current live platform:** Windows x64, Visual Studio 2022, vcpkg, SDL2.

**Verification policy (2026-09-17):** live-server smoke testing is optional,
not a merge or migration-completion requirement, per the user's instruction.
The automated build, compiler, test and wire-compatibility criteria below remain
required. The original estimates and historical findings do not add a runtime gate.

## Executive conclusion

The project is close to **building in a newer MSVC language mode**, but it is not
currently ISO C++17- or C++20-conforming.

For the supported Windows/MSVC path, this is moderate work rather than a rewrite:

| Delivery level | Estimated effort for one engineer | Result |
|---|---:|---|
| C++17, current MSVC extensions allowed | **3-5 working days** | All targets build and tests pass in `/std:c++17`, but removed syntax remains and another conforming compiler will reject it. |
| C++20, current MSVC extensions allowed | **5-8 working days** | All targets build and tests pass in `/std:c++20`; the known `char*`/literal failures are fixed, but removed exception syntax and `register` remain as MSVC extensions. |
| ISO-clean C++20 with MSVC and Clang validation | **15-25 working days** | Removed syntax is eliminated, strict compilation is enabled, and both compiler paths are kept green. |

The range includes implementation, review, full builds, automated tests, and a
basic in-game smoke test. It does not include fixing unrelated warnings or doing a
Linux/macOS port.

**Recommendation:** target C++20 directly. The measured increment from C++17 to
C++20 is small compared with the shared conformance cleanup, and Visual Studio
2022 already supports the target. Do not first land a large C++17 cleanup and then
repeat the stabilization cycle for C++20.

If the requirement only means "the current Windows build accepts `/std:c++20`,"
budget about one engineer-week. If it means "valid C++20 that is checked by more
than MSVC," budget three to five engineer-weeks.

## What "compatible" means in this assessment

There are three materially different outcomes:

1. **MSVC build-compatible:** every configured target compiles and links with the
   selected `/std:c++17` or `/std:c++20` mode using Visual Studio's default
   permissiveness.
2. **ISO source-compatible:** project code does not depend on MSVC accepting
   constructs removed from the selected C++ standard. A current Clang build is the
   practical second check.
3. **Modernized:** old ownership, strings, containers, and error handling are
   redesigned to use newer standard-library facilities.

Only the first two are a port. The third is a much larger refactor and is not
included in the estimates.

Likewise, C++20 source compatibility does not make the non-Windows path supported.
The repository describes Windows/MSVC as the live path, has platform-specific ATL
and Win32 integration, and does not currently have a Linux/macOS CI build. An OS
port needs a separate assessment.

## Measured project size

The repository is large, which makes validation and large mechanical diffs more
expensive even though the number of root causes is small.

| Surface | Files | Lines |
|---|---:|---:|
| `.cpp` | 1,232 | 408,556 |
| `.c` | 45 | 14,947 |
| `.h` | 1,056 | 126,478 |
| Unique C/C++ implementation files in the generated target graph | 1,220 | 390,088 |

The largest configured projects are `packetwire` (525 translation units),
`DarkEden` (489), `VS_UI` (53), `unit_tests` (39), `gamemodel` (34), and
`SpriteLib` (30). This count includes every normal executable and tool, not only
the game executable.

The external libraries are not a major standard-version risk. SDL2, SDL2_image,
SDL2_ttf, SDL2_mixer, iconv, and libjpeg-turbo are consumed through stable C APIs
or supported CMake packages. The 14 configured `.c` files are compiled as C and
are unaffected by the selected C++ standard.

## Current build contract

The root build is explicitly pinned to C++11 at `CMakeLists.txt:13-14`, while both
the root and test projects claim a CMake 3.10 minimum. The existing MSVC projects
generated from that configuration contain no `LanguageStandard` setting because
MSVC has no selectable C++11 mode; the effective Windows mode is the compiler's
default C++14-like mode.

For a real upgrade, the build contract should:

- raise `CMAKE_CXX_STANDARD` directly to 20 (or 17 if that is the chosen target);
- keep `CMAKE_CXX_STANDARD_REQUIRED ON`;
- set `CMAKE_CXX_EXTENSIONS OFF`;
- raise the declared CMake minimum to the already documented requirement of 3.20;
- compile at least one MSVC configuration with `/permissive-`;
- add a second compiler job so MSVC extensions cannot silently become required.

Passing `/std:c++20` in an ad hoc flags variable is useful for an audit but should
not be the committed implementation.

**Build-contract status (2026-09-07):** `CMAKE_CXX_STANDARD 20` with
`CMAKE_CXX_STANDARD_REQUIRED ON` was already the contract (PR #83), and the
root `CMakeLists.txt` now adds `CMAKE_CXX_EXTENSIONS OFF` and, for MSVC,
`/permissive-` and `/Zc:__cplusplus` on every C++ target through
`add_compile_options`, next to `/MP`. With findings 3 and 4 closed the whole
tree - 1,241 translation units, the tests included - builds under
`/permissive-` with **0 errors**, and every test passes in the plain and the
ASan tree, so there was no strict-mode workload left to schedule: the
string-literal fixes of finding 5 and the exception and `register` sweeps
were the whole of it. One trap, recorded because the first probe fell into
it: passing `/permissive-` in `-DCMAKE_CXX_FLAGS=` on the configure line
replaces CMake's MSVC defaults, `/EHsc` among them, and without `/EHsc` a
C++ exception cannot be caught by type - six tests failed with "uncaught
exception of unknown type" and the receive-loop test saw packets leak on the
throw path, none of which had anything to do with conformance. The committed
change adds the option and keeps the defaults. The second-compiler job
followed the same day: the `windows-clang` preset builds the whole tree with
clang-cl 19 through NMake from a VS developer prompt (the ClangCL toolset is
not installed, but the compiler is), and the experiments table below has the
result. A Linux or macOS build is a port, not a language-mode question.

## Build experiments

The audit used CMake 4.4.3, MSVC 19.44.35228, Windows SDK 10.0.22621, the existing
vcpkg installation, and a fresh out-of-tree Debug build with `BUILD_TESTS=ON`.
The final successful diagnostic build retained MSVC's normal `/EHsc` exception
mode.

| Experiment | Result |
|---|---|
| Plain MSVC `/std:c++17` | Failed on ambiguity between C++17 `std::byte` and the Windows SDK's global `byte`. |
| C++17 with `std::byte` temporarily disabled | Reached the executable and failed on the one removed `std::auto_ptr`. |
| C++17 with `_HAS_STD_BYTE=0` and `_HAS_AUTO_PTR_ETC=1` probes | The complete target graph built; all six CTest entries passed. |
| C++17 plus `/permissive-` | Exposed 81 distinct legacy string-literal/`char*` error sites in `VS_UI` before the dependent game target could build. |
| C++20 with the two library probes | Exposed 126 unique string-literal/`char*` error sites across 24 files: 81 in `VS_UI`, plus 48 in executable sources, with three shared header sites counted once. |
| C++20 plus `_HAS_STD_BYTE=0`, `_HAS_AUTO_PTR_ETC=1`, and `/Zc:strictStrings-` probes | The complete target graph built; all six CTest entries passed. The unit binary reported **375 tests, 5,066 checks, 0 failures**. |
| Clang 19.1.5 C++17 compile probe | Rejected non-empty dynamic exception specifications immediately; one representative packet translation unit hit Clang's 20-error limit in `SocketAPI.h`. |
| Clang 19.1.5 SpriteLib probe | Rejected `register` declarations as invalid ISO C++17. |
| clang-cl 19.1.5, whole tree, 2026-09-07 (NMake, the `windows-clang` preset) | With findings 3 and 4 closed: every library, every tool, `unit_tests` and `DarkEden` compile and link; the clang-built unit binary reports **596 tests, 294,382 checks, 0 failures** and every ctest passes. Two ISO defects MSVC had accepted as extensions were found and fixed: a `POINT` brace-initialised from `0xFFFFFFFF` (narrowing to `LONG`), and an `enum` whose first value `0xffff0000` gave it `int` as its underlying type, so that its members were negative `case` labels against an unsigned `id_t`; it now has `unsigned int` as its underlying type. |

The `_HAS_*` macros and `/Zc:strictStrings-` were used only to expose the next
layer of errors. They are **not proposed fixes**: they disable new library features
or retain non-conforming source behavior.

Only Debug was compiled in this audit. Release, ASan, and in-game behavior remain
acceptance work for the implementation.

## Findings

### 1. C++17 `std::byte` conflicts with global namespace imports

`Client/Client_PCH.h:30` contains `using namespace std;`. Some translation units
then include Windows headers such as `wtypes.h`, whose unqualified `byte` collides
with `std::byte`. `Properties.cpp` and `StringStream.cpp` reproduce this failure.

A repository scan found 65 `using namespace std` directives in 63 files. Fifty
matches are in headers; two of those are commented out, leaving approximately 48
active header-level imports. The precompiled header is the high-impact instance
because it affects most client translation units.

The preferred fix is to remove namespace directives from headers and qualify the
standard-library names they expose. A Windows-only `_HAS_STD_BYTE=0` definition is
a small compatibility shim, but it hides rather than fixes the namespace problem
and prevents use of a standard C++17 feature.

### 2. One removed standard-library type is used

`Client/PacketHandler/GCMonsterKillQuestInfoHandler.cpp:30` has the repository's
only `std::auto_ptr` occurrence. It owns the raw pointer returned by
`popQuestInfo()`, so replacing it with `std::unique_ptr` should be local and low
risk.

The scan found no uses of the other common C++17/C++20 removals: `random_shuffle`,
`bind1st`/`bind2nd`, `mem_fun`, `std::iterator`, `result_of`, old allocator member
functions, `uncaught_exception`, or `get_temporary_buffer`. It also found no `u8`
string literals that would trigger the C++20 `char8_t` type change.

### 3. Dynamic exception specifications are the main conformance workload

The packet subsystem uses old specifications on declarations and definitions; for
example, `Client/Packet/SocketAPI.h:78-203`. A syntax-focused scan found:

- **3,304 non-empty type-list specifications** such as
  `throw(ProtocolException, Error)`;
- **1,317 files** containing those non-empty specifications;
- **8,655 empty specifications** written as `throw()`.

The non-empty form was removed in C++17. MSVC continues to accept and ignore it as
an extension (the existing C4290 warning family), which is why a permissive MSVC
C++20 build can succeed. Clang rejects it.

The safe mechanical mapping for the current Windows behavior is:

```cpp
throw()                         -> noexcept
throw(ProtocolException, Error) -> noexcept(false)
```

MSVC already ignores the list of permitted exception types, so `noexcept(false)`
matches the effective supported-platform behavior more closely than attempting to
recreate the old runtime type filter. Declarations, definitions, base classes, and
overrides must be changed consistently because exception specifications are part
of the function type in modern C++.

This edit is scriptable, but it touches most of the packet tree and therefore has
substantial review, merge-conflict, and clean-build cost. A syntax-aware rewrite
or tightly constrained pattern is required so parenthesized `throw` expressions
and comments are not altered.

There is a related pre-existing correctness concern: MSVC reports functions
declared `throw()` whose bodies can throw. Converting `throw()` to `noexcept`
preserves today's termination behavior; deciding that those functions should
propagate instead is a separate behavioral audit and should not be mixed into the
language port.

**Conformance status (2026-09-06):** the first slice of this finding was scoped
to `basic/` and **removed nothing, because `basic/` has never carried a dynamic
exception specification**: 0 before, 0 after. That is not a quirk of the smallest
library. Measured with the pattern R9 and R10 now use - `throw` followed by
parentheses holding type names or nothing, files joined so a wrapped type list
still counts, `//` tails stripped - **every library in this tree is already at
0**: `basic`, `Client/SpriteLib`, `Client/TextSystem`, `Client/DXLib`,
`Client/framelib`, `VS_UI` and every `gamemodel` member. The entire workload is
the packet tree and what hangs off it. The repository holds **11,790**
specifications in `.h` and `.cpp` files outside comments (a first measurement
that did not strip `/* */` blocks read 11,841; the 51 it over-counted were dead
comment text), 8,513 empty and 3,277 type lists, split as: **11,463** in the
library set (2,211 in `packetwire`'s own `.cpp`, 9,252 in the headers under
`Client/Packet`, over the set's 1,473 files), **284**
in `Client/PacketHandler`, **34** in the remaining executable sources - the two
request-side packet factory managers, `PacketFunction.cpp`, `RequestFileManager`
and `Updater/UpdateManager.h` - and **9** in `tests/`, in test doubles that
implement wire interfaces and have to match the specifications on their bases.
The tree's 45 `.c` files carry none, so unlike `register` in finding 4 there is
nothing here that can be left alone for being C. This restates finding 3's own
scan (3,304 non-empty, 8,655 empty, 1,317 files), taken with a different pattern
over a scope the audit did not record; the two agree to within about 1% and the
shape of the conclusion is unchanged.

The slice therefore shipped the instrument rather than an edit.
`tests/ratchet/ratchets.sh` gains two baselines: **R9 = 0** over `basic/`, which
holds the line there, and **R10 = 11,463** over the library set, which later
slices ratchet down. R10's set deliberately includes every `.h` under
`Client/Packet` even though `tests/arch/packetwire_files.txt` lists only `.cpp`,
because an exception specification is part of the function type and a
declaration and its definition must change together - a `.cpp`-only metric would
have counted exactly half of every edit as progress. Both were injection-tested
in both directions: adding `throw ( Error )` or `throw ()` to a `basic/` header
fails them, `noexcept` and the `throw Error(...)` / `throw ("...")` statements do
not, and removing one specification from `SocketAPI.h` fails R10 as unrecorded
progress. `Client/PacketHandler` and the rest of the executable are outside both,
and adding a ratchet for them belongs with the slice that clears them.

A ratchet is the only instrument available here, because the build says nothing.
The Debug log of this tree at `/std:c++20` contains **C4290 zero times** - the
warning the project notes name for the packet tree - so not one of the 11,499
sites shows up as a diagnostic. What the build does emit is **539 distinct
C4297**, "function assumed not to throw an exception but does", across 257
`Client/Packet` files. That is the pre-existing correctness concern this finding
raises, now with a number: those are `throw()` functions whose bodies throw,
already undefined under MSVC's own reading of the specification, and they are the
set where `throw()` must become a dropped specification rather than `noexcept`.

**The next slice is `packetwire`, because there is no smaller library left.** It
is the whole 11,463 and cannot land in one commit; the natural subdivision is the
one `Client/Packet` already has - the root files first (the exception header the
`__BEGIN_TRY`/`__END_CATCH` pair comes from, the streams, the sockets and the
framing, which is where the declarations everything else overrides live), then
the packet directories one at a time. Three things found while measuring should
travel with those slices.

Dropping `throw(X, Y)` is mechanical: MSVC ignores the list already, so removing
it changes nothing MSVC does. `throw()` -> `noexcept` is not, and it is a
judgement per function. MSVC already treats `throw()` as nothrow and omits the
unwinding, so a `throw()` function whose body can throw is undefined today -
which means the specification should be **dropped** wherever the body cannot be
shown nothrow, and promoted to `noexcept` only where it can. The tree already
carries one worked example of that reasoning, at `Client/Packet/WireHost.h:155`:
`RequestClientPlayer::readInputStream` and `RequestServerPlayer::send` are
declared `throw(ProtocolException, Error)` on purpose, because the peer teardown
depends on that exception unwinding to `RequestServerPlayerManager::Update`, and
a `throw()` there would make the path undefined under MSVC and `std::terminate`
under C++17 or on clang and gcc.

`SocketAPI.cpp` writes five of its specifications across two lines and repeats
each one in the banner comment above the function, so a line-oriented codemod
will edit the code and leave the comment claiming the old contract. One of those
comments is already wrong today: `bind_ex`'s names an `MBindException` that both
the declaration and the definition spell `BindException`.

Finally, the nine specifications in `tests/` are not debt of the same kind. They
sit on test doubles that override wire interfaces
(`test_output_stream_flush.cpp`, `test_player_base.cpp`) and mirror what those
bases declare, so they belong in the same commit as the base they mirror. The
`throw()` half of that is the part that cannot be deferred: once a base is
`noexcept`, an override that is not stops compiling.

**Packet-root status (2026-09-07):** the 162 files directly under
`Client/Packet` are at 0 - 143 of them carried a specification, 1,799 in
all - and so is `tests/`; **R10 = 9,664**. The work went as
four slices - the wire core (the exception header, the asserts, the string
stream, the sockets, the datagram classes, the streams and the file API, 524
sites), the packet framework and the player classes (224), the
player-character info classes (558) and the remaining info classes (493) -
plus the nine in `tests/`, which moved with the bases they mirror. The rule
each slice applied, and the numbers it produced:

- **A type list is deleted, not respelled** - 643 of them. MSVC ignored the
  list already, and a function with no specification is what ISO C++ means
  by potentially throwing, so deleting it changes nothing MSVC does. The
  `noexcept(false)` this finding names above is the same thing said longer,
  and is not written anywhere except in the one place it is not the same
  thing: **a destructor**. A destructor with no specification is `noexcept`
  by default, so the ten that carried `throw(ProtocolException, Error)` -
  `Player` and its three subclasses, `Socket`, `SocketImpl`, `ServerSocket`,
  `DatagramSocket` and the two streams - are spelled `noexcept(false)`, which
  is what MSVC had read the list as. They can throw: the player destructors
  hold an `Assert` on the session state, and the socket ones call `close()`.
  The slices' first version deleted those lists too, and the compiler said
  so: no C4297 for any of the ten on master, C4297 for all ten once the list
  was gone.
- **An empty `throw()` is a judgement per function**: 599 became `noexcept`
  and 566 were deleted, 48 of the latter on destructors, which are `noexcept`
  by default anyway. Promoted: the scalar, pointer, enum and array-indexed
  getters and setters, the `getSize`/`getMaxSize` bodies that sum constants
  (`std::string::size()` and `std::list::size()` are `noexcept` by the
  standard), `clearList()` over `std::list::clear()`, and the bitset setters
  in the slayer outlooks. Deleted: anything returning or assigning a
  `std::string` or a container by value (every `getName`, `setName`,
  `toString`, the `*Info3` copy constructors), anything wrapped in
  `__BEGIN_TRY`/`__END_CATCH` (the pair rethrows), `new`, `front()` and
  `pop_front()`, the bitset getters (`to_ulong` and `test` throw), and the
  exception-class constructors - `Throwable` holds a `std::list<std::string>`
  and MSVC's `std::list` default constructor allocates its sentinel node and
  is not `noexcept`, so `Throwable()` and the 41 default constructors that
  chain to it are deleted rather than promoted, though libstdc++ and libc++
  would have allowed it.
- **A virtual is promoted only when every override in the repository is
  `throw()` or `noexcept` and passes the same test**, and the slices checked
  by grep before promoting: `PCInfo::getPCType` and `PCInfo::getSize` (nine
  overrides, all in the slice), and the three `getSize` overrides of
  `PCSkillInfo` (whose base is deleted, since those overrides walk a list).
  Deleted rather than promoted, on purpose: `PacketFactory::getPacketID` and
  `getPacketMaxSize` (448 factory subclasses in the packet directories, still
  `throw()` and rewritten by later slices; `GCPetStashListFactory` and
  `GCGoodsListFactory` already call an unspecified `getPacketMaxSize`),
  `DatagramPacket`'s pure virtuals, `ModifyInfo::getPacketSize` (not
  virtual, but about thirty `Gpackets` classes declare a member of the same
  name that hides it) and `WarInfo::getSize`, whose overrides add
  `ValueList::getPacketSize`. A base with no specification compiles under any
  override, which is the property the packet-directory slices need.
- **The two things this finding said should travel with the slice did**:
  `SocketAPI.cpp`'s five two-line specifications are gone, along with the
  copies in the banner comments above them and the `MBindException` name one
  of those had wrong; and the `WireHost.h` comment that said `readInputStream`
  and `send` "are throw(ProtocolException, Error)" now says they propagate
  those exceptions.

Two facts about the files that the next slices should know. The packet-root
files are **CRLF in the working tree** on this machine, and `Datagram.h` and
`SocketInputStream.h` are stored with mixed endings; `grep -c $'\r'` reports
0 for many of them and is not a line-ending check here - count bytes with
perl, compare the changed-line count of `git diff --numstat` with the
specification count per file, and expect the Edit tool to normalise a mixed
file. And the compiler is a usable oracle for one direction: a `throw()`
promoted to `noexcept` whose body can throw draws C4297, so a promotion that
adds a C4297 to the build is wrong, while the C4297s that remain on the
destructors wrapped in `__BEGIN_TRY` are the pre-existing concern this finding
raises and not this slice's to settle.

Verified: unit_tests in `build/tests` and `build/tests-asan`, 596 tests,
294,382 checks, 0 failed in both; DarkEden in `build/vs2022` with 0 errors,
which is the check that matters for the headers every handler includes; the
wire inventory and every golden unchanged.

**Small packet directories (2026-09-07):** `Lpackets` (319 sites, 36 files),
`Upackets` (34, 4) and `Rpackets` (256, 22) are at 0 as well, and **R10 =
9,055**. The packet classes are regular enough that this slice was done by
script rather than by hand, with the same rule narrowed to what a script can
prove: a type list is deleted (163; the script aborts on a destructor carrying
one, and none does); a `throw()` becomes `noexcept` only on an inline
one-line member that returns a constant or a scalar or reference member with
no call in the expression, or assigns one scalar by-value parameter to a
member (162 - the `getPacketID`, `getPacketSize` and `getPacketMaxSize`
constants and the scalar getters and setters); every other `throw()` is
deleted (284 - the `getPacketName` and `toString` that return a
`std::string`, the `createPacket` that call `new`, the size functions with
out-of-line or calling bodies, the string setters, the list operations and
the eight destructors). Scalar means the fundamentals plus every typedef and
enum declared under `Client/Packet`, `Client/Packet/Types`, `basic/` and the
directory itself, so a `std::string` getter or a const-reference setter never
qualifies. The script's tokenizer copies comments and string literals
through, and a second tokenizer compared every file with its committed
version - every comment and literal identical, every code token identical
once the specifications and the `noexcept` tokens are erased. A body the rule
does not recognise is deleted, never promoted, so the script can only err by
leaving a nothrow function unspecified, which ISO C++ allows; the review of
its output is a review of the 162 promoted lines, which reduce to about sixty
distinct shapes. `Cpackets` (3,172) and `Gpackets` (5,883) are what remains,
and the same script applies to them.

**Cpackets (2026-09-07):** at 0, 3,172 sites in 326 files, **R10 = 5,883**.
The script's decisions were 873 type lists deleted, 1,049 `throw()` promoted
and 1,250 deleted, verified the same way (residual 0, every file compared
token by token with its committed version, the promoted lines reduced to
their shapes and read). The one thing the script cannot see is a packet
deriving from another packet: `CGUseMessageItemFromInventory` derives from
`CGUseItemFromInventory` and overrides its `getPacketSize` and its factory's
`getPacketMaxSize` with multi-line bodies the script deletes, while the base's
one-liners it promotes - and MSVC refused the pair, C2694, an override with a
less restrictive specification than its base. The base's two functions are
unspecified, by hand, and the build is the check that finds the next such
pair; `Gpackets` should expect the same for its `OK1`/`OK2`-style families.

**Gpackets (2026-09-07):** at 0, 5,883 sites in 516 files, and **R10 = 0**:
every library a test binary can link is free of dynamic exception
specifications, and the ratchet now holds the whole set at zero the way R9
holds `basic/`. The script's decisions were 1,293 type lists deleted, 1,845
`throw()` promoted and 2,745 deleted, verified as before; the promoted return
and parameter types were listed and read (the `PacketID_t`/`PacketSize_t`
constants, the `*_t` typedefs, the enums, the fundamentals, and const
references to `std::string` and the `PC*Info3` classes). Five packets derive
from another packet here; a scan of that shape ahead of the build found the
two that override a promoted one-liner (`GCMakeItemOK` and `GCMakeItemFail`
over `GCChangeInventoryItemNum::getPacketSize`), the base was left
unspecified by hand, and the build agreed. What this finding still owes is
outside R10: `Client/PacketHandler` (284) and the remaining executable
sources (34), which the slice that clears them should put under a ratchet of
their own, since a `throw()` there whose body throws is the same undefined
behaviour it was in the libraries.

**Executable side (2026-09-07): finding 3 is closed on the source side.**
The 319 specifications outside the library set are gone: 284 in
`Client/PacketHandler`, one on each handler's `execute` definition, all type
lists; 32 in the two request-side packet factory managers under
`Client/OtherClass`, whose constructors, destructors and `toString` had
`throw()` (deleted: `__BEGIN_TRY` bodies and a `std::string`) and whose
`getPacket`/`getPacketMaxSize` and validation entry points had lists; and
three lists in `RequestFileManager` and `Updater/UpdateManager.h`. Nothing
was promoted. The same script and the same token-by-token verification as
the packet directories; the only test path is the executable build, since
none of these files links into a test binary. Ratchet **R12 = 0** holds every
`.h`, `.cpp` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`,
`third_party` and `tests` at zero, so R9 and R10 are now subsumed and kept
only as the record of how the libraries got there. What this finding still
does not settle is the pre-existing concern it raised at the top: the
`throw()` functions whose bodies throw were undefined under MSVC's reading and
now simply propagate, which is the behaviour ISO C++ gives an unspecified
function and the one the callers were written against; the C4297 count in
the build is what remains of it, on destructors wrapped in
`__BEGIN_TRY`/`__END_CATCH`, which are `noexcept` by default whatever is
written on them.

### 4. `register` remains in C++ source

There are roughly 650 declaration-like uses of the removed `register` storage
specifier. They are concentrated in SpriteLib drawing loops, `MTopView.cpp`, and
several UI loops. Twenty-six textual occurrences are in `.c` files and do not need
to change because those files remain C.

MSVC accepts the C++ occurrences as extensions. Clang 19 rejects them in C++17.
Removing `register` is mechanical and has no intended runtime effect, but it must
be included for an ISO-clean result.

**Conformance status (2026-09-07):** closed. The exact count, measured with
comments and string literals removed, was **627 declarations in 23 C++
files** - 584 of them in `Client/SpriteLib` (the blitters in `CSprite.cpp`,
`CIndexSprite.cpp`, the alpha, shadow, palette and surface variants,
`CFilter.cpp`, and one in `CTypePackVector.h`), 17 in `Client/MTopView.cpp`,
and the rest in `Client/CSpritePal.cpp`, `UtilityFunction.cpp`,
`MEventManager.cpp`, `MSector.cpp` and `VS_UI/src/vs_ui_gamecommon2.cpp`.
All 627 are deleted, seven of them written `int register i` rather than
`register int i`. The edit was scripted through a tokenizer that copies
comments and string literals through untouched, so the commented-out copies
of blitter loops that still say `register` inside `/* */` blocks are left as
they were, and a second tokenizer then compared every changed file with its
master version: every comment and literal identical, every code token
different only by the keyword and one run of blanks. That verifier exists
because the first pass of the sweep was run through a tokenizer whose string
class had been corrupted in transit and edited six NPC script lines in
`MNPCScriptTableEnglish.cpp` that say "register as a couple", "register a
team", "a clan" or "a guild"; the sweep was redone from master. The 21
declarations in the three `.c` files (`deflate.c`, `inftrees.c`, `trees.c`)
stay, as this finding said they should - this finding's "twenty-six in the
`.c` files" was a line count that included the prose in `crc32.c`'s comments,
which has no declaration at all. Ratchet **R11 = 0** holds the line over
every `.cpp`, `.h` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`,
`third_party` and `tests`, counted by `tests/tools/count_register.pl`, which
also accepts the `WORD register i` spelling with any type name in front, not
only the seven `int register i` the sweep met; it does not reuse the R9/R10
comment-stripping pipeline, because that pipeline strips `/* */` before `//`
and so reads the blitters' `//*pDest = ...` line comments as block-comment
openers - measured over the 23 files on master it saw 538 of the 627, and
the figure moves with the order the files are joined in, because a swallowed
comment runs across file boundaries - and because it does not strip string
literals, so those six NPC lines would count as declarations to it. An
under-count is tolerable for a ratchet that must stay at zero only if nothing
can hide there. The same weakness applies to R10, where it can only
under-count, and is noted for the slice that next moves that baseline.

### 5. C++20 makes legacy string-literal conversions hard errors

With the two removed-library issues bypassed, MSVC C++20 reported at least **126
unique error sites across 24 files**. These are not 126 independent designs:

- `basic/BasicException.h:30` declares read-only error text and `__FILE__` as
  mutable `char*`; making the declaration and definition const-correct resolves
  many repeated sites.
- Tables such as `VS_UI/src/VS_UI_GameSlayer.cpp:1100`,
  `Client/CrashReport.cpp:33`, and `Client/md5.cpp:30` store string literals in
  mutable pointer types. Most should be arrays of `const char*`.
- Several APIs accept `char*` even when their implementations appear to read only,
  including help-key lookup, chat text, login-ID backup, opening-video paths, and
  UI cursor setup.

Every affected API must be checked before changing its parameter to `const char*`.
Using `const_cast` or retaining `/Zc:strictStrings-` would conceal a real contract
problem and is not an acceptable final fix.

Strict C++17/Clang validation will expose the same const-correctness debt even
though default MSVC C++17 still permits many of the conversions.

### 6. C++20 adds little after the shared cleanup

Once `std::byte`, `auto_ptr`, and strict string literals were temporarily handled,
the entire MSVC C++20 graph compiled and all tests passed. The static scan also
found no live identifiers conflicting with `concept`, `requires`, `module`, or the
coroutine keywords, and no additional removed standard-library APIs.

This does not prove there are no errors hidden behind the known failures. Only a
complete Clang build after the exception-specification and `register` sweeps can do
that. A 25-35% contingency is included in the ISO-clean estimate for such
second-order failures.

### 7. Automated coverage is useful but not sufficient

The seven CTest entries cover the unit binaries plus architecture, formatting, packet
index, and generated-inventory checks. The unit binary exercises the static
libraries, including `packetwire` and `gamemodel`, and its wire goldens are valuable
because the port should not change packet bytes.

`tests/CMakeLists.txt` explicitly records the main limit: game logic compiled
directly into `DarkEden`, including packet handlers, cannot be linked into the unit
test binary. Automated checks do not prove every UI string lifetime, exception
path, login, zone loading, chat or shutdown interaction. Live-server smoke testing
can investigate those paths, but is optional and does not block this migration.

The Windows, Linux and macOS workflows under `.github/workflows/` enforce the
C++20 build and automated checks; the original assessment predated those workflows.

## Estimated work breakdown

The following is the recommended ISO-clean C++20 scope. Some tasks overlap, so the
total is a range rather than a direct sum of maximums.

| Workstream | Effort | Notes |
|---|---:|---|
| Build contract and compiler matrix | 1-2 days | CMake minimum/standard, extensions off, strict MSVC job, Clang job, Debug and Release. |
| `std::byte`, namespace hygiene, and `auto_ptr` | 1-3 days | The one ownership change is trivial; removing header namespace pollution determines the range. |
| Const-correct string interfaces | 2-4 days | At least 126 observed sites in 24 files; several collapse to shared API fixes, but mutation contracts need review. |
| Exception-specification migration | 4-7 days | About 11,959 total specification sites, 3,304 of them illegal type lists, spread over 1,317 files. Includes a codemod, declaration/definition repair, and review. |
| Remove C++ `register` and fix remaining strict-compiler findings | 2-4 days | Roughly 650 candidates plus issues revealed only after the dominant blockers are gone. |
| Verification and stabilization | 3-5 days | Clean Debug/Release builds, ordinary and ASan tests, wire checks, startup/login/zone/chat/UI/shutdown smoke tests. |

Expected total: **15-25 engineer-days**. A reviewer familiar with the packet
generator-style files can keep the work near the low end. Discovering behavioral
exception issues or cross-platform requirements would push it beyond the range.

The MSVC-extension estimates are shorter because they deliberately skip the
exception-specification and `register` sweeps. That is a valid staged milestone,
but it should be named "MSVC C++20 build" rather than "portable C++20."

## Recommended implementation sequence

1. Add a C++20 audit configuration and record the current failure counts. Keep the
   existing C++11 build green during the first fixes.
2. Replace the single `auto_ptr`, correct the `std::byte` namespace collision, and
   fix the const-string API families. Do not commit the diagnostic `_HAS_*` or
   `/Zc:strictStrings-` switches as solutions.
3. Land the exception-specification conversion in reviewable subsystem batches.
   Add a zero-growth/zero-target ratchet so old `throw(Type)` syntax cannot return.
4. Remove C++ `register` uses, leaving C sources alone. Add the same kind of
   ratchet for C++ files.
5. Turn on C++20, `CMAKE_CXX_EXTENSIONS OFF`, and strict MSVC compilation as the
   normal build. Build every target from a clean directory.
6. Add Clang compilation. The local machine has a Clang 19 executable, but the
   Visual Studio ClangCL toolset integration is not installed, so this audit could
   run representative compile probes rather than a complete Clang target graph.
7. Run all tests in ordinary and ASan trees, add Release coverage, and verify the
   compiler and wire checks before declaring the port complete. Live-server smoke
   testing is optional.

## Acceptance criteria

The port is complete when all of the following are true:

- CMake requests C++20 directly and refuses an older standard;
- no diagnostic feature-disabling macros or permissive string-literal switch are
  required;
- no non-empty dynamic exception specification remains;
- no `register` specifier remains in C++ source;
- all configured libraries, tools, tests, and `DarkEden` build cleanly in Debug and
  Release with the supported MSVC toolset;
- a second compiler builds the intended portable subset or complete Windows graph,
  depending on the agreed support promise;
- all CTest checks and the unit suite pass in ordinary and ASan builds;
- packet wire inventories and golden bytes are unchanged.

Live-server startup, login, zone entry, UI, chat and shutdown checks are optional;
their absence does not block merging or declaring this migration complete.

Modernizing unrelated raw pointers, containers, rendering code, platform APIs, or
the remaining warning backlog is explicitly outside these criteria.

## Post-migration C++20 modernization backlog

These items are not required to declare the language-mode migration complete.
They are follow-up opportunities that use the new baseline to reduce defects and
maintenance cost. PR #83 establishes the Windows/MSVC C++20 build milestone; the
items below should be delivered separately in small, reviewable subsystem PRs.

The occurrence counts are approximate static-scan results recorded on 2026-09-04.
They indicate where to investigate, not how many mechanical replacements are safe.

| Priority | Opportunity | Likely payoff | Suggested first slice | Size |
|---:|---|---|---|---:|
| 1 | `std::source_location` for diagnostics | Records file, line, and function without manually forwarding `__FILE__` and `__LINE__`. | Adapt `basic/BasicException.h` and `basic/DebugLog.h` while retaining compatibility wrappers for existing call sites. | Small |
| 2 | C++20 container and string helpers | `contains`, `starts_with`, `ends_with`, and `std::erase_if` express intent and remove repeated iterator boilerplate. | Convert a few table/manager membership checks and add focused tests; do not mix this with behavioral changes. | Small |
| 3 | `std::span` at packet and buffer boundaries | Couples a buffer with its extent and prevents pointer/length disagreement. The scan found roughly 217 pointer-plus-size interfaces. | Add span overloads to `SocketInputStream` and `SocketOutputStream`, keep the old overloads as adapters, and migrate one packet family. | Medium |
| 4 | Typed packet serialization with concepts | Constrains wire reads/writes to supported fixed-width integral and enum types, producing clearer compile-time errors. | Introduce an internal `WireScalar` concept plus size assertions without changing existing packet bytes or public packet IDs. | Medium |
| 5 | `std::chrono::steady_clock` and typed durations | Avoids unit confusion and the rollover-prone `previous + delay <= now` pattern. About 259 `GetTickCount`/`timeGetTime` calls remain. | Add a central monotonic-clock adapter, then migrate `basic/Timer2` and one UI timer with wraparound tests. | Medium |
| 6 | `std::filesystem` for path and directory work | Removes manual search-handle lifetime and path-buffer handling. About 34 Win32/CRT enumeration calls remain. | Migrate profile discovery or log cleanup first, preserving filename ordering, case handling, and wildcard behavior. | Medium |
| 7 | Bounded modern formatting | Reduces format/argument mismatches and destination-buffer mistakes. The scan found about 628 printf-family and 443 `strcpy`/`strcat` calls. | Use `std::format` or `format_to_n` only for developer-owned logging strings at first. | Large/staged |
| 8 | `std::jthread` and `std::stop_token` for owned workers | Makes join and cancellation responsibilities explicit and exception-safe. Raw Win32 thread/synchronization primitives appear in about 36 locations. | Prototype on one clearly owned worker; preserve required Windows event and message-loop integration. | Large/staged |
| 9 | Ownership RAII | Replacing proven owning raw pointers with `unique_ptr` prevents leaks and partial-initialization cleanup bugs. This is not C++20-specific, but the migration makes it a natural follow-up. | Inventory ownership in one manager and convert only pointers with unambiguous single ownership. | Large/staged |

**Source-location status (2026-09-05):** priority 1 is implemented for both
diagnostics facilities. `basic/BasicException.h` gains an `ExceptionSite` whose
default constructor captures the caller through a defaulted
`std::source_location::current()`, plus a `g_BasicException(code, sz_error,
site)` entry point; `_Error`, `_ErrorStr` and `CheckMemAlloc` no longer spell
`__FILE__` and `__LINE__` out, and the `(code, sz_error, file, line)` function
stays as a compatibility wrapper for call sites that name a location
explicitly. `basic/DebugLog.h` gains the equivalent `LogSite` and a
`log_write_at(site, level, fmt, ...)` entry point, with the site in front of
the format because a `std::source_location` cannot follow a C variadic `...`;
the `LOG_*` and `DEBUG_ADD*` macros are untouched and both entry points share
one core. The log line written to console and file is unchanged - the function
name is captured but not printed. `tests/unit/test_source_location_diagnostics.cpp`
pins every capture against the test file's own `__FILE__` and `__LINE__`, which
is what proves the nested defaulted `current()` reports the call site rather
than the header. No call site elsewhere in the tree is converted; that is a
later slice.

**Source-location second slice (2026-09-05):** the explicit `__FILE__` and
`__LINE__` forwarders in the packet wire layer are converted, so the library
that carries them is also the one a test binary can link.
`Client/Packet/Exception.h` gains `DiagnosticSite` - the same shape as
`ExceptionSite` and `LogSite`, a separate type only because the wire layer's
exception header may not drag in the `EXCEPTION_CODE` enum and the
`_Error` macros those carry - and a `Throwable::addStack(site)` overload that
defaults it; `__END_CATCH`, which
wraps essentially every method in the tree, now calls `addStack()` and pushes
the identical `"file:line"` frame. `Client/Packet/PacketAssert.h` gains
`__assert__(func, expr, site)` and its `Assert(expr)` macro forwards only the
two things a location cannot carry, the platform's function spelling and the
stringized expression; the four-argument `__assert__` stays as the
compatibility entry point and is what the new one delegates to, so the line
written to `assertion_failed.log` and the message inside the thrown
`AssertionError` are byte for byte what they were, empty-function-name
separator quirk included. Pinning that text exposed a pre-existing defect,
now under *Found by reading* in the code-health review:
`StringStream::operator<<(char)` appends a NUL after every streamed
character, so every bug report the client hands `SendBugReport` was cut off
before its stack trace; the tests spelled the NUL out rather than hiding it,
and the `fix:` slice that removed it followed (`fix/stringstream-nul`, which
also took the NUL back out of these tests' expectations; `SendBugReport`'s
own cut then bounded what the server sees at 100 bytes, and `fix/bug-report-cut`
raised it to the 116 a `CGSay` message leaves once the `*bug_report ` prefix
has 12 of its 128).
`Client/Packet/Assert1.h`, an unreferenced duplicate
of `PacketAssert.h` carrying the same include guard, is kept in step rather
than left to rot. `Client/Packet/ClientPlayer.cpp`'s packet-skip notice
repeated its file and line inside a message the log header already stamped
with them; both halves now read one captured `LogSite`, and the site is
captured on the line the macro occupied, so the text is unchanged. No macro
name changed, so nothing under `Client/PacketHandler` needed editing.
`tests/unit/test_packet_source_location.cpp` pins each converted shape against
the test file's own `__FILE__` and `__LINE__`, and pins the packet-skip line
by writing the old spelling and the new one into the log file sink and
comparing both. Left alone: `Client/Packet/SocketOutputStream.cpp`'s
`printf`, which is not a forwarder - it prints its own location, so a capture
there would only respell the same two tokens. Everything executable-side
(`Client/PacketFunction.cpp`, `Client/MEffectGeneratorTable.cpp`, the
`GCSkillToTileOK` handlers, `Client/LeakMemoryDumper.*`,
`Client/DebugInfo.h`, `VS_UI/src/Imm/IFCErrors.h`) is a later slice.

**Source-location third slice (2026-09-17):** the executable-side
forwarders the second slice left. Most of that list turned out not to
be forwarders, or not to be code: `MEffectGeneratorTable.cpp`'s and
`DebugInfo.h`'s mentions are commented out; `LeakMemoryDumper.h`'s
`DEBUG_NEW` places `__FILE__` and `__LINE__` into a tracked
`operator new` under `ENABLE_LEAK_TRACKER`, where a `source_location`
cannot be defaulted in a placement-new, and stays; `IFCErrors.h`'s `IFC_SET_ERROR` has no call site (its one reference is another macro nothing expands). What remained were eighteen bug reports that formatted their site by hand - fifteen in `PacketFunction.cpp` (three as `[%s,%d]` from `__FILE__` and `__LINE__`, twelve as the function's name and `__LINE__`), three in the `GCSkillToTileOK` handlers as `[%s,%d]` - and they call a new `SendBugReportAt(site, fmt, ...)` in the wire layer,
which writes the same `[file,line] ` prefix from a `DiagnosticSite`
captured at the call (two of `PacketFunction.cpp`'s printed the file alone and gain the line; the twelve that printed the function's name print the file's instead, so one function reports one shape). `SendBugReport` and it share one bounded
core - the prefix goes into the same 256-byte buffer ahead of the
caller's text, so the cut lands where it did, and the "nothing usable, send nothing" rule reads the caller's text, not the prefix - and the text the server receives is unchanged where the shape was already `[file,line]`. `tests/unit/test_bug_report_message.cpp` pins the prefix for an explicit site and for the defaulted one against the test file's own `__FILE__` and `__LINE__`, the cut with the prefix in the count, and the short-text rule. What stays: `MCreature.cpp`'s 28 `DEBUG_ADD_FORMAT` lines and `CSpriteSurface_Adapter.cpp`'s three debug macros (one of them compiled) put `__LINE__` into the text of a log line whose header the logger already stamps with the file and line, so a capture would only respell a number the line carries twice; `VS_UI_GameCommon.cpp`'s 11 hand it to an inventory-log method whose body returns at once (nine) and to the UI message that reaches `SendBugReport` as a number, which no site can cross (two). With those left as text, every forwarder into a facility that records a location is converted, which is what priority 1 set out to do.

**Container-helper status (2026-09-05):** the first priority-2 slice is
implemented. Twelve membership and line-trimming sites in library code -
`PacketIDSet`, `GCTimeLimitItemInfo`, `GCNPCAskVariable`, `Properties`,
`SystemAvailabilities` and the item, sorted-item, time-item and skill-domain
managers - are written as `contains`, `starts_with` and `ends_with`. Every
converted site is reached through a public entry point by
`tests/unit/test_cpp20_container_helpers.cpp`, which was run against the
pre-conversion sources as well and gave the same result. No container type
changed, no packet byte changed, and the candidates that were not equivalent
(an erase of one element where `std::erase` would remove all matches, and a
linear scan over a map by `operator==`) are listed in the commit and left
alone.

**Container-helper status, second slice (2026-09-06):** eleven more library
sites, in three files. `MSkillDomain::IsExistSkillStep` asks the step map with
`contains`; `MSkillDomain::AddSkillStep` asks its step list with
`std::ranges::find` instead of an index loop that set a flag;
`UseEnglishTextFrom` tests the language file's comment character and its
`LANGUAGE` keyword with `starts_with` over a `std::string_view` of the line, in
place of an index and an eight-byte `strncmp`;
`SystemAvailabilitiesManager::LoadFromStream` tests its four line kinds - `;`,
`*`, `Z`, `S` - the same way, with `empty()` where it called `strlen` on the
same buffer; `SystemAvailabilitiesManager::ZoneFiltering` and `CheckScript`
became `std::ranges::any_of` and `std::ranges::none_of` over the predicates
their hand written scans already used. Every converted site is reached through
a public entry point by `tests/unit/test_cpp20_container_helpers_2.cpp`, whose
six tests were built and run twice - once with the three sources reverted to
their committed state and once with the conversion in place - and reported
identical counts both times, all six passing in both; the stack this lands on
ends at 498 tests, 10,694 checks, 0 failed in both trees. One site has only
one reachable arm: `AddSkillStep`'s duplicate test is `true` on every call the
public API can make, because `AddSkill` calls it only for a skill not yet in
the domain, so the test that covers it pins the public contract rather than
the dead arm.

One candidate was converted, found untestable and reverted, and the reason is
worth keeping: `C_DIRECTORY::GetMixedDirectory` in `basic/Directory.cpp` tests
for a trailing path separator with `path[strlen(path) - 1]`, which `ends_with`
says directly - but a test that calls it does not link. `C_DIRECTORY`'s
constructor references `platform_get_executable_dir`, which `basic/PlatformSDL.cpp`
defines only under `#ifndef PLATFORM_WINDOWS`, so pulling `Directory.obj` into
`unit_tests` fails with `LNK2019`. The whole tree links today only because
nothing on the Windows build references `C_DIRECTORY` at all. That is a latent
defect rather than a compatibility one and was not fixed here; the first bounded-formatting slice (2026-09-17) deleted the class and its two files instead, since nothing referenced it.

Candidates read and rejected: every `find` whose iterator is
dereferenced afterwards, which is not a membership test (`MItemManager`'s
`GetItem` and `RemoveItem`, `MSkillSet`'s five accessors, `MSkillDomain`'s
status and learn paths, `MTradeManager::Undo`, `Properties::getProperty`,
`GCTimeLimitItemInfo::getTimeLimit`, `GCNPCAskVariable::getValue`,
`TextBackendSDL`'s font and glyph caches); `PacketIDSet::deletePacketID`, whose
lookup is both a membership test and the iterator it erases - and whose
condition is inverted, a defect left for its own commit; `MTimeItemManager::RemoveTimeItem`,
where `erase(key)` would be the tidy spelling but is not a C++20 helper;
`MSkillDomain::AddSkill`'s linear scan over a map, already rejected by the
first slice; `strstr` membership in `ClientCommunicationManager`, because
`std::string::contains` is C++23; and `platform_config_get_string`'s `strncmp`
prefix in `basic/PlatformSDL.cpp`, which sits behind the same `#ifndef
PLATFORM_WINDOWS` and is not compiled here.

**Span status (2026-09-04):** the first priority-3 slice is implemented in PR
#84. `SocketInputStream` and `SocketOutputStream` now expose
bounded `std::span<char>` and `std::span<std::byte>` overloads while retaining the
pointer/length APIs as adapters. `GCExchangeList` is the representative migrated
packet: its seven length-prefixed strings and two raw 64-bit fields use spans, and
a pre-migration golden pins the complete body byte-for-byte. Future slices should
migrate small packet families behind equivalent round-trip/golden coverage rather
than expanding this into a mechanical whole-tree rewrite.

**Typed-scalar status (2026-09-04):** the first priority-4 slice is implemented
in PR #85. The stream layer accepts only exact-width signed
and unsigned integers, or enums backed by those types, through constrained
`readWire`/`writeWire` entry points. Existing plain and encrypted scalar overloads
remain compatible and delegate to that implementation; ambiguous `bool` and plain
`char` behavior is deliberately unchanged. The Exchange buy/list 64-bit identifiers
and the ordinary `CGAttack` combat path are the representative migrations,
protected by client/server-shared goldens across every encryption code. Later
slices can migrate remaining raw scalar casts family by family without widening
the accepted type set.

**Second span/typed-scalar slice (2026-09-06):** the client skill-activation
family - `CGSkillToSelf`, `CGSkillToObject`, `CGSkillToTile` and
`CGSkillToNamed` - is migrated. It was chosen because it is the largest
remaining cluster of `(char*)&field, szField` casts in `Client/Packet` that
shares one shape (a `SkillType_t`/`CEffectID_t` header, then a target), and
because the golden evidence was almost complete before the work started: the
first three already carry client/server-shared goldens for all six encryption
codes, recorded when task 2.4 pinned the nineteen encrypter users. Only
`CGSkillToNamed` was unpinned - it never reaches the encrypter, so it fell
outside that sweep - and its golden was recorded in a commit of its own, ahead
of the migration, so the bytes it pins are provably the old code's. All four
goldens still pass unchanged, and `tests/wire-layout.txt` is untouched.
`CGSkillToInventory` is the family's fifth member and needed no change: it was
already calling the typed `read`/`write` overloads, which now route through
`readWire`/`writeWire` themselves. `tests/unit/test_packet_skill_family_wire.cpp`
adds what a byte golden cannot see - per-field round-trips through the packets'
own getters under every encryption code, and truncated bodies refused with
`InsufficientDataException`, the same exception the pointer/length read raised,
because both spellings now reach one bounded core.

What stays raw in this family is deliberate. The encrypter branch
(`SHUFFLE_STATEMENT_*` over `readEncrypt`/`writeEncrypt`) is untouched, so the
migration covers the plain branch only and the round-trips run over both.
`CGSkillToObject` stages its `ObjectID_t` through a `std::uint32_t`, as
`CGAttack` does, because `DWORD` is `unsigned long` on this toolchain and so is
not one of the exact-width types the constraint accepts; a `static_assert` now
ties the two widths together so a change to `ObjectID_t` is a compile error
rather than a silent change in how many bytes go on the wire. `CGSkillToNamed`
reads its target name through the `std::string` overload, which is already
bounded by construction, and only its write moved to a span. Reading it
exposed one defect, fixed in its own `fix:` commit on top of the refactor: the
cap was tested after narrowing the length to a `BYTE`, so a 276-character name
narrowed to 20, passed the check, and then wrote all 276 characters behind a
length byte claiming 20. `read()` cannot produce such a name, but
`setTargetName()` accepts any `std::string`; the cap is now applied to the
`std::string` size, before the narrowing, the bounded view ties the emitted
bytes to the length byte just written, and the test file pins it. What the fix
does not do, and every throwing `write()` in the tree shares, is roll back the
framing header `SocketOutputStream` has already put in the ring. That residue
is closed in a `fix:` commit of its own on top of this slice
(`fix/output-stream-header-rollback`, found by this slice's adversarial
review): `SocketOutputStream::write(const Packet*)` now saves the ring's data
length and the sequence byte before the header goes in and restores both when
the body write throws, so a refused packet leaves the stream exactly as it
found it instead of leaving a header the peer would fill from the next packet;
`tests/unit/test_output_stream_rollback.cpp` pins it over both the plain and
the encrypt stream, and the frame on the non-throwing path is byte for byte
what it was.

**Third span/typed-scalar slice (2026-09-10): the packet directories are
free of raw scalar casts.** A count of `(char*)&field, size` reads and
writes over `Client/Packet` found the shape the first two slices were
chosen for almost gone already. `Cpackets` and `Gpackets` hold 1,315 stream
reads and 1,315 stream writes (`iStream.read*(` and `oStream.write*(` with
comments stripped), and every scalar among them but 20 already went through
the typed integer `read(m_Field)`/`write(m_Field)` overloads, which route
through `readWire`/`writeWire` since the first slice. The 20 (26 textually;
six sit in two blocks upstream commented out) were in four packets -
`CGAddZoneToMouse`, `CGBloodDrain`, `GCAttack` and `GCGetDamage`, the
melee combat exchange and the drag-to-cursor pickup - and this slice
migrates them the way `CGSkillToObject` was:
`Coord_t`, `Dir_t` and `WORD` fields go straight to `readWire`/`writeWire`,
and the `ObjectID_t` (a `DWORD`, `unsigned long` on MSVC and so not an
exact-width type there, `uint32_t` off Windows) is staged through a
`std::uint32_t` under a `static_assert` tying the two widths.
`CGAttack`, the first slice's exemplar, staged the same way but without
the assertion; it gains one here, so the six staging sites are uniform.
Only the plain branch of `CGAddZoneToMouse` changes; its
`SHUFFLE_STATEMENT_3` encrypter branch is untouched, and the other three
never reach the encrypter. `Lpackets`, `Upackets` and `Rpackets` had none.
The 132 other two-argument pointer-plus-length calls in the two directories
are a different shape - `char` buffers, `std::string` and `c_str()` reads
such as `m_Name, szName` - already bounded by the span and string overloads
underneath, and not a scalar cast.

The goldens came first, in a commit of their own, so the bytes they pin are
the old code's. `CGAddZoneToMouse` was already under the shared encrypter
set at all six codes. `GCAttack` and `GCGetDamage` are pinned by the server's
`packet_combat_test.cpp` at code 0, so their goldens are byte-identical
copies of its files, with its fixture values, and add nothing to the
cross-repo golden diff (which is not clean: of the 136 files both repos
pin, 134 agree and `CLLogin.code0.hex` and `GCGuildChat.code0.hex` differ,
a pre-existing state this slice does not touch); `CGBloodDrain` has no
server pin and its golden is client-authored. All three are
encrypter-free, and `EncrypterFree()` holds both their write and their
parse code-insensitive.
`tests/unit/test_packet_combat_family_wire.cpp` adds what a golden cannot
see: per-field round-trips under every code, the full 32-bit `ObjectID`
through the staging in both directions (so the cast neither sign-extends nor
narrows), and every truncation of every body, from one byte short down to
empty, refused with the same `InsufficientDataException` the pointer/length
read raised. The field *order* is pinned by the goldens alone - a
symmetric swap of two same-width fields in both `read()` and `write()`
round-trips cleanly and only the golden sees it, which the review
demonstrated by mutation.

What stays raw under `Client/Packet` after this slice is 31 live casts
of the pointer-and-size shape, none of them a packet field (this
paragraph first said 27: it had grepped `(char*)&` and missed the four
the output streams spell `(const char*)&`, the measure-the-spelling
mistake CLAUDE.md warns about). Seven are the framing itself - the packet id,
size and sequence byte that `SocketOutputStream::write(const Packet*)` and
`Datagram::read`/`write(Packet*)` put in front of every body;
`GCMoveOK.framed.code0.hex` pins the three in the stream and **nothing
pins the four in `Datagram`**, which no test frames a packet through.
Sixteen are `Datagram.h`'s own typed scalar overloads, and eight are the
`bool` and `char` overloads of the four socket streams, which do not
route through `readWire`. In the packet directories themselves, `CLLogin` and
`CGConnect` keep four `(char*)m_MacAddress, 6` writes and reads of a
`char` array, the shape a span overload fits and the second slice's commit named as
remaining. Moving the framing belongs to a slice that reads it on its own
terms rather than as one more packet, and that slice should pin the
datagram header first.

**Fourth span/typed-scalar slice (2026-09-10): the framing header, and
with it the whole of `Datagram`.** The slice went as the paragraph above
asked: pin the datagram first, then move. The UDP transport is live -
`CGPortCheck` reaches the login server through it and the `RC*` packets
travel between peers - and nothing had pinned either side of
`Datagram::read`/`write(DatagramPacket*)`. Two goldens now do
(`CGPortCheck.datagram`, `RCPositionInfo.datagram`: the id, the size and
the body), `tests/unit/test_datagram_frame.cpp` builds the frame by hand
and pins the read side through a real `PacketFactoryManager` with its
three refusals (an id at or past `PACKET_MAX`, a size over the factory's
maximum, a length that disagrees with `szPacketHeader + size` either
way), and the two bounded primitives every datagram packet parses through
are pinned at the end of the buffer.

Pinning found two defects, each fixed in a `fix:` commit of its own
between the pin and the move, and both recorded in the code-health
review. `Datagram::write` sizes its buffer at `szPacketHeader + body` and
writes one byte less - the slot the stream's sequence byte occupies, which
both peers count in the length and neither reads - from a buffer a bare
`new char[]` returned, so **every UDP packet the client sent carried one
byte of heap memory**; the server's `Datagram` zero-fills for exactly this
reason and documents the pad as travelling as zero, and the client now
matches it (the test read the pad back as `0xCD` on the unfixed code).
The review's open Medium on the bounds checks - `m_InputOffset + len >
m_Length` wraps, and the write bound was an `Assert` that Release compiles
away - is closed the same way; its read test did not fail on the unfixed
code, it crashed the test process at the first wrapping length.

The move itself: `SocketOutputStream::write(const Packet*)` writes the
id, the size and the sequence byte through `writeWire` (all three are
exact-width types on every platform, so no staging), and `Datagram` gains
what the streams have had since the first slice - `read(std::span<char>)`
and `write(std::span<const char>)` as the one bounded core each way, the
pointer/length pair as adapters, `std::byte` spans beside them, and
`readWire`/`writeWire` under the same `WireScalar` concept, with the
fourteen fixed-width overloads routed through those and `char` read as a
one-byte span with no cast at all. `Datagram.h` was stored with mixed line
endings, the eighteen lines a previous edit had added being LF, and is
CRLF throughout now. Every golden and the wire inventory unchanged; 643
tests, 294,975 checks, 0 failed in both trees after the review's repairs;
`DarkEden` builds with 0
errors, which is the check that every `RC*` packet and `CGPortCheck` still
resolve their `Datagram` calls.

What stays raw under `Client/Packet` after this slice is twelve live
casts of the pointer-and-size shape: the `bool` and `char` overloads of
the four socket streams (eight - `bool` is deliberately unchanged since
the first slice, and `char` could take the one-byte span `Datagram` now
uses; the output pair spells them `(const char*)&`, which is why a grep
for `(char*)&` reports eight in total and not twelve), and the four
`(char*)m_MacAddress, 6` reads and writes in `CLLogin` and `CGConnect`,
which a `std::span<BYTE, 6>` of the array fits. `SocketAPI.cpp` holds six
more at the OS socket calls, which are not wire scalars, and
`CGBloodDrain`'s six sit in comments.

The adversarial review of this slice (two fresh-context readers, one on
the code and one on the claims) found no wire-byte defect and four
things worth recording. The wrap-proof bound as first written, `len >
m_Length - offset`, depended on an offset-never-past-length invariant
that nothing enforces and would *admit* a read if it were broken, where
the old form refused; it is now the review's own recommended form, `len >
m_Length || offset > m_Length - len`, safe whatever the offsets hold, and
it runs before the adapter builds its span, so a hostile length never
becomes an invalid range. The write bound alone still let a body one byte
over its declaration eat the pad slot and go out looking honest, the
peer dropping the last field; `Datagram::write(const DatagramPacket*)`
now holds the body to the size its header declared and refuses either
direction of drift, where the server measures the body and back-fills
the size - the client refuses, the server corrects, and both keep the
byte off the wire. `GLIncomingConnectionError::getPacketSize` declared
one of the two strings its `write` emits, stale against the server's
copy, and now declares both (the client neither sends nor receives it).
And the claims audit found the record overstating in three places: the
write test's wrapping lengths slipped past the old `Assert` too, so it
is a second reproduction rather than a test that "passed only because
the Assert fired"; the Release half of "live in every build" is shown by
no test, the suite being a Debug build; and the residue count above.

**Fifth span/typed-scalar slice (2026-09-10): `Client/Packet` holds no
pointer-and-size cast of a wire scalar or array.** The twelve the fourth
slice counted are gone. `CLLogin` and `CGConnect` read and write their
six MAC bytes through `std::as_writable_bytes(std::span(m_MacAddress))`
and `std::as_bytes(...)`, a `std::span<BYTE, 6>` deduced from the array's
declaration, so the count on the wire is the array's own and not a
literal restated beside it. `CGConnect` was pinned first, byte-identical
to the server's golden with its fixture; `CLLogin` already was, in both
layouts. Of the streams' eight `bool` and `char` overloads, `char` goes
through a one-byte `std::span<char>` as `Datagram`'s does, a `bool` goes
out as the `BYTE` 0 or 1 it holds through `writeWire`, and the encrypt
pair keep their transform and delegate to the plain overloads.
`tests/unit/test_wire_bool_char.cpp` pinned all eight before the move,
byte for byte under ten codes on both sides of the encrypter's bool flip
at 128.

The two `bool` *reads* were left for a `fix:` commit of their own,
because respelling them was not enough: they copied the wire byte into
the bool's storage, the code-health review's open Medium on invalid bool
representations. They now take the byte as a `BYTE` and store `b != 0`,
and the test written first (red on the unfixed code, 25 of its 32
checks) showed what an invalid bool does under MSVC: one holding 0x02,
0x7F, 0x80 or 0xFF takes the true branch of `if (a)` and is unequal to
`true` at the same time. No consumer in the tree compares a wire bool
with `== true`, so in play the finding was the sanitizer trap it names
plus an invalid value propagating into UI structs, not an inverted
branch. Every packet that reads a bool is covered by the one change and
no packet file moved; the encrypting bool read has no production caller
at all. What no test guards is the write side's normalisation
(`buf ? 1 : 0`), because nothing in the tree can hand `write(bool)` an
invalid bool - the one bool the client sends, `CLRegisterPlayer`'s
public flag, is set from a literal - and a `static_assert` in
`SystemTypes.h` now ties `szbool` to the one byte the overloads move.

What remains under `Client/Packet` is not wire: `SocketAPI.cpp`'s six
casts at the OS socket calls, and seven in comments - `CGBloodDrain`'s
six and a debug hex dump in `SocketInputStream.cpp` inside a commented
block. Priority 3's boundary work in the wire library is complete; the
executable side (`Client/PacketHandler` and the game code) reads packets
through accessors and was never in this count. The adversarial review
(two fresh-context readers, code and claims) found no wire-byte defect;
what it found is in the record above, and in the repair commit: the
stream header's four new comment lines were LF in a CRLF file, the same
slip the fourth slice had repaired in `Datagram.h`; two test lines
loaded the invalid bool the test exists to prevent and would have
aborted a red run under Clang's `-fsanitize=bool`; and the empty-stream
test asserted the untouched value for the plain reads only. 651 tests,
297,358 checks, 0 failed in both trees after the repairs; `DarkEden` builds with 0
errors.

**Clock status (2026-09-05):** the first priority-5 slice is implemented.
`basic/MonotonicClock.{h,cpp}` is the central adapter: `Now()` is
`std::chrono::steady_clock` truncated to milliseconds, `Duration` is
`std::chrono::milliseconds`, and `SetTestSource()`/`ScopedTestSource` inject a
deterministic clock so timing tests are neither sleeps nor flakes.
`LegacyTicks()` is deliberately **not** derived from `Now()`: on the real clock
it is `platform_get_ticks()`, so a class that is only half converted keeps
comparing against the same epoch it always did, and later slices can migrate
call sites one at a time. `basic/Timer2` is migrated internally with its public
`C_TIMER2` API - `DWORD` milliseconds, `timer_id_t`, `INVALID_TID` - unchanged,
and `C_VS_UI_TITLE::Timer()` (the title-screen credit scroll) is the
representative UI site. `tests/unit/test_monotonic_clock.cpp` and
`tests/unit/test_timer2.cpp` pin the firing rule and drive the injected clock
across the 32-bit tick's wrap and past 2^32 ms of elapsed time.

Reading the code for this recorded one thing that is worth knowing before the
next slice: on `PLATFORM_WINDOWS`, `Platform.h` redefines only `timeGetTime()`
as `platform_get_ticks()`. `GetTickCount()` is still kernel32's, so the client
already reads two counters that share neither epoch (since boot versus since
SDL init) nor resolution (about 15.6 ms versus 1 ms), and nothing in the source
marks which sites use which. Moving a site off `GetTickCount()` therefore also
takes it off the 15.6 ms quantisation, which is a small change in when it fires
and has to be stated each time it is made.

The second priority-5 slice (2026-09-05) is `Client/MTimeItemManager`, the
register of items that carry a time limit. Its stored deadline was a `DWORD`
holding `timeGetTime()/1000 + lifetime` and is now an absolute point on
`MonotonicClock`'s clock, kept at the whole-second resolution the class has
always counted in. Nothing stayed on the legacy counter: every tick read in
the file was `timeGetTime()`, the deadline is compared only against another
read from the same file and never reaches a packet, another class or
persistent storage, so `MonotonicClock::Now()` is used throughout and
`LegacyTicks()` is not needed here. The public API is unchanged - `AddTimeItem`
still takes a `DWORD` of seconds, because that is a lifetime the server sends
and not a tick, and the four accessors still return `int` fields of a
countdown. Only the `std::map` base's mapped type changed, and nothing outside
the class ever read it.

The quantisation statement for this slice is that there is nothing to state:
these sites read `timeGetTime()`, which `Platform.h` already redefines as
`platform_get_ticks()`, so they were on a 1 ms counter and were never
quantised to `GetTickCount()`'s ~15.6 ms step. The resolution is unchanged;
only the epoch and the width are. The floor to a whole second is preserved
exactly, so an item added part-way through a second still expires up to a
second early and the countdown still steps on the clock's second boundary
rather than on the item's. What the rewrite removes is three defects, which is
why the commit is a `fix:`. The first two are demonstrated in the test beside
the new assertion rather than reproduced against a running server: after 49.7
days of tick the register read every held deadline as up to 49.7 days in the
future instead of long past, and a lifetime near the top of a `DWORD` wrapped
the stored sum and expired the item on arrival - a value the server controls.
The third was the adversarial review's: each old accessor read the clock twice,
and when the second read fell one second after the first the unsigned
subtraction went round, so the description panel could paint 49,710 days on
the very frame an item expired. One read now decides both the sign and the
value, and the remaining count stays a 64-bit `std::chrono::seconds` all the
way to the accessors rather than narrowing to a `DWORD`.
`tests/unit/test_time_item_manager.cpp` drives the wraps through the injected
clock, works the old `DWORD` arithmetic out on the same numbers beside each
assertion, and pins the countdown, the boundary one millisecond before expiry,
the preserved second-floor rounding and the un-narrowed remainder.

The third priority-5 slice (2026-09-10) is the pet item's life, the first
clock in an item class (the first slice took a `basic` timer and a title
screen, the second a manager). A pet's durability
is the number of minutes it has left, counted from the moment the
durability was set; `MPetItem` stamped that moment as a `DWORD` from
`timeGetTime()` and handed the tick out, and the affect check in
`MCreature` and the description panel in `VS_UI_Description.cpp` each
worked the countdown out for themselves. The stamp is a
`MonotonicClock::TimePoint` now, `MinutesSinceUpdate()` and
`GetRemainingDurability()` are the item's own, both readers ask them, and
the tick accessors are gone. The quantisation statement is the second
slice's again: `timeGetTime()` was already the 1 ms counter, so only the
epoch and the width change, and the width change is what removes the
defect - after 49.7 days the `DWORD` subtraction came round and a pet
set with a long durability read as hours old, alive and lending its
status; the elapsed count is 64-bit now and is compared with the 32-bit
durability in the wider type. It is 64-bit by the item's own `Minutes`
typedef, not by `std::chrono::minutes`, whose representation is an `int`
on MSVC - the slice's first version used the latter and claimed a width
it did not have on the live platform, and the review's reader caught it
by reading the two STLs. The pet window's "food remaining", which two
executable sites filled from the raw durability and so disagreed with
the tooltip by the elapsed minutes, is filled from the countdown too.
What made this slice possible is a restructuring commit ahead of the fix,
in the shape the Linux port used for the potion (`82f809b6`): `MPetItem`'s
constructor was in the executable's `MItemUse.cpp`, so no test could
construct a pet, and constructing one in the library makes its vtable
the library's to emit, which names a `UseInventory` whose body sends a
packet - so the member is defined in `MItem.cpp` and delegates to a new
`MItemHost` slot the executable installs (with designated initialisers,
now that the host has eleven entries; `s_PriceHost` beside it is still
positional). `tests/unit/test_pet_item_countdown.cpp` drives the injected
clock through the countdown, the restart, a never-set pet and the legacy
wrap - the old arithmetic worked out beside the wrap assertions - and an
elapsed count past 2^32 minutes, which an `int` representation would
wrap. 655 tests, 297,382 checks, 0 failed in both trees. Left as found:
`_Item_Description_Calculator` sizes the pet's duration line under
`GetGrade() != -1` while the drawing pass prints it under `== -1`, so the
width measured is for a line never drawn; upstream's, and not this
slice's to settle.

What is left of priority 5 after that slice: 214 live tick reads in 34
files (calls of `GetTickCount`/`timeGetTime` over `Client/` and `VS_UI/`
with comments and string literals removed; this paragraph first said
209 in 32, counted by a regex strip of block comments that reads a
`//*` line comment as an opener and swallows code up to the next `*/` -
in `VS_UI_Item.cpp` that is lines 228 to 285, `srand(GetTickCount())`
among them, and `Vs_ui.cpp` has the same idiom - so the fourth slice's
`tests/tools/count_tick_reads.pl` and ratchet R14 count by a
character-level scanner instead, and every figure here is theirs; the
count includes one definition, the non-Windows `GetTickCount` shim in
`VS_UI_widget.h`). **Most of it is not executable-side**: `VS_UI` is a
static library and holds 132 of them, 100 in the two `VS_UI_GameCommon`
sources alone (chat-spam
throttles, help and hide auto-timers, mining progress and double-click
timing), so those have a test path once `VS_UI` is added to the unit
binary's link line, as CLAUDE.md says. The executable's largest clusters
are `MTopView` (21) and `CGameUpdate` (3 live of 12 spellings). Each move
needs its quantisation statement, and the executable ones are verified
only by running the client. The one `packetwire` site, a byte-rate
probe in `SocketInputStream::fill`, sits under
`__TEST_PACKET_RECEIVED_SIZE_PER_SECOND__`, which nothing defines, and is
dead code rather than a clock to move; `gamemodel` is clean.

The fourth priority-5 slice (2026-09-10) is every VS_UI timer outside the
two `GameCommon` sources, and it gives `basic` the gate they all share.
Five live widget classes carried a `DWORD` pair and the three-line
`prev + interval <= GetTickCount()` over it - the event button's focus
fade, `C_ANIMATION`'s frame step, the shop's and the computer's scroll
timers, the mouse pointer's - and two functions kept a previous tick as
a static local against a delay, the file dialog's long-name latch and
the party cursor's frame step. A sixth class, the tutorial briefing,
carries the pair too and was converted in the slice's first version;
the review found its whole implementation inside a comment block and
its only instantiation commented out with it, so that conversion is
reverted and the class is a task 5.2 deletion candidate. The title
screen, converted inline by the first slice, is on the shared gate too. `MonotonicClock::IntervalTimer` is that gate over a time
point and a 64-bit duration: `Fire()`, `Restart()`, `SetIntervalMillis`,
and `Elapsed()` for the file dialog's strict comparison. Writing its
tests corrected what the first slice's title-screen comment said the
wrap did: the two `DWORD`s wrap independently, so when the sum wraps
before the tick the gate is open on every frame until the tick follows
(an early firing, up to one interval), and when the tick wraps first the
gate stays shut for 49.7 days for a widget not polled in the window - not
"open from then on". Both cases are pinned with the old gate worked out
beside them, along with the single clock read that keeps a caller's work
out of the interval. The quantisation statement is the one every move
off `GetTickCount` owes: on Windows it is kernel32's and steps in about
15.6 ms, so each timer fired on the first step at or past its interval
and now fires at the interval - the button fade goes from about 15.6 to
10 ms a step (a 320 ms fade instead of 500), the tutorial scrolls from
62.5 to 50, the animation frame and the shop from 109 to 100, the party
cursor from 156 to 150, the pointer from 312 to 300, and the button
class's own animation, which sets 75 through `SetSpeed`, from 78 to 75.
Every one of these gates is polled once per frame, so the period in
play is the larger of the interval and the frame period: at 60 fps the
button fade is 32 frames either way, and the shorter intervals show
their change only above the frame rate they are written for.
`C_ANIMATION` never initialised its previous tick, but never read it
before a `Play*` call reset it either; the timer constructs as "now".
Two `srand(GetTickCount())` seeds stay: they are not timers. `VS_UI`
goes from 132 live reads to 104, the tree from 214 to 186 (28 calls;
the counted total includes the shim definition), and R14 holds the
line. The timer is tested in `basic`
(`tests/unit/test_interval_timer.cpp`), including a fake source that
advances per read, so a two-read timer fails the single-read test; the
widget conversions are verified by the build, and the client was not
run. The review also recorded, for the slice that clears GameCommon:
the non-Windows `GetTickCount` shim in `VS_UI_widget.h` sits under
`Platform.h`'s macro of the same name off Windows and so defines a
second `platform_get_ticks` with `gettimeofday`'s epoch, a pre-existing
hazard that goes with its last users.

The fifth priority-5 slice (2026-09-10) is every `GetTickCount` site in
the two `GameCommon` sources (45), and with them the last three
`GetTickCount` spellings in `VS_UI` - two `srand` seeds and the
`VS_UI_widget.h` stub. The 45 were two shapes over the same `DWORD`
pair. Interval gates, as
the fourth slice's: the tribe window's timer, the minimap and world map
refresh, and the mining progress, which was a global previous tick and a
static interval set per mine level that two progress bars divided; that
is one global `IntervalTimer` now, and the bars draw `Elapsed()` over
`GetInterval()`. Windows, `prev + interval >= now` true while the window
is open: the party and death requests and the regen-tower minimap (their
interval a constructor argument), the skill window's 2 s, and the chat
window's lockout, one previous tick shared by three windows chosen by
mode; those are `Elapsed() <= interval`, read once per call where the old
chat condition read the tick three times and the call four.
The chat window's help and auto-hide timers were `prev = GetTickCount()
- X` at start so the first poll fires, which is `IntervalTimer::Expire()`
(added with `ExpireBy(d)`, and tested); its two spam throttles kept
vectors of ticks and keep vectors of `TimePoint`; the party manager's
face-large delay was a static local, converted as the file dialog's was;
and the crazy-mine board's double-click guard, an `int` last-click tick
against `GetDoubleClickTime()`, is a timer whose `Elapsed()` is compared
with that, its "first click is never a double click" and "after a
double click, five seconds ago" being `ExpireBy(5 s)`. With no
`GetTickCount` call left in `VS_UI`, the non-Windows stub in
`VS_UI_widget.h` - the second `platform_get_ticks` the fourth slice's
review found - is deleted, and the two `srand(GetTickCount())` seeds take
`MonotonicClock::LegacyTicks()`, which on Windows is SDL's tick rather
than kernel32's, a different epoch for a value that only has to vary
(one of the two, in `VS_UI_Item.cpp`, sits under `#ifndef _LIB` and is
compiled out of the library build). The quantisation statement: the 100 ms
timers go from about 109 to 100 ms; the second-scale windows and delays
close within a 15.6 ms step of where they did; the mining bar draws its
fraction from a 1 ms clock instead of a stepped one; and all of it is
bounded by the frame rate the gates are polled at. R14 goes from 186 to
138 (48: the 45 gates, the two seeds and the stub's definition). What
`VS_UI` holds now is 56 `timeGetTime()` sites in three files - the two
sources and the header's `C_VS_UI_BLOOD_BIBLE_STATUS::SetTimer` - four of
them `srand` seeds and the rest the deadline and elapsed-time shapes:
quest and mission deadlines from server data, effect status, notices, the
minigames' clocks, which want a `TimePoint` deadline rather than a gate
and are the next slice; `Client` holds 82. The conversions are verified
by the build on Windows and, by the review, on Linux GCC in the port
image; the client was not run. The review found no gate whose truth
differs from its pre-image, including the chat lockout's restructured
`else`, whose dropped clear on the reset path every caller overwrites
on the next line.

The sixth priority-5 slice (2026-09-10) is the deadlines in the same two
sources: `timeGetTime()` ticks held as "when this ends" or "when this
started" and compared against a fresh read. Each is a
`MonotonicClock::TimePoint` now, compared or subtracted on the 64-bit
rep: the quest status window's two countdowns (a deadline set as now
plus the delay, the milliseconds left returned, `-1` once past), the
blood bible's timer, the image notice's close delay, the resurrect
request's per-button delay and its percent bar, the timed missions'
start (with a `bool` for "no limit" where 0 meant that), the gamble
spin's two strict gates (`IntervalTimer`), and four `srand` seeds. One
of them the executable sets: the flag war's end, which
`GCFlagWarStatusHandler` builds from the seconds the server sends and
passes through three relays to the CTF status window, whose two
"remaining" displays share a `RemainingMillis()` that reads 0 once the
end has passed. The `DWORD` subtraction went round to 49.7 days there:
`Show()` had a "four hours or more reads as none" guard that caught it
(the comment said three; the arithmetic says four), and the hover
tooltip had none and printed the wrapped value, about 1,193 hours, once
a war ended - a small display defect this slice removes, which the
review found the first version of this record describing as no change.
Retyping the mission record found a trap worth its own
sentence: the wire layer's `UI_GMissionInfo` and `UI_GQuestInfo` were
redeclared field for field inside `C_VS_UI_QUEST_MANAGER`, and the
handlers passed one to the manager through a `void*` it cast to its own
copy - two layouts that had to agree with nothing to say so, and the
first field retyped on one side would have read garbage from the other.
The manager's names are typedefs of the wire structs now. No
quantisation change: `timeGetTime()` was already the 1 ms tick; the
epoch and the width change, and the width is what removes the wrap. R14
goes from 138 to 113 (25 calls, the handler's two among them). `VS_UI`
holds 33: the three minigames' clocks, and the three deadlines the
executable sets through shared structs - the quest status's
`quest_time`, the effect status's `delayFrame`, and the war list's
`left_time` (`WAR_INFO`, set in `MWarManager`, read in the effect status
window) - which want the same retyping on both sides of the boundary
and are the next slices. `Client` holds 80. The review added the one
test a binary can reach: the mission record is a packetwire header, and
`tests/unit/test_quest_status_record.cpp` pins its no-limit default and
that a quest holds its missions by the same type. The rest is verified
by the build on Windows and, by the review, on Linux GCC; the client
was not run.

The seventh priority-5 slice (2026-09-16) is the three minigames' clocks
in `vs_ui_gamecommon2.cpp`, the last `timeGetTime()` readers in `VS_UI`
that do not cross the executable boundary. The minesweeper
(`C_VS_UI_FINDING_MINE`) kept one `DWORD` for two things in turn - the
tick the game started at, then, once it ended, the milliseconds it had
taken - and prints whichever the status says it holds; that is a
`TimePoint` and a `Duration` now, each read where the old field was read
under that status. The arrow tile (`C_VS_UI_ARROW_TILE`) stamps each
character's start, end and last move, carries a trap delay nothing ever
sets (the trap status is never assigned; the field is only reset), and
gates its monster moves on the time since the last one; the four
`S_CHARACTER` fields and the gate's stamp are `TimePoint`s, the `= 0`
resets are the default time point (the epoch, which every comparison
treats as the zero tick did), and `TimerMonsterMove()` compares the
elapsed `Duration` against its millisecond argument through `Millis()`.
The crazy mine (`C_VS_UI_CRAZY_MINE`) stamps its start the same way. The
elapsed times the two that keep a score - the arrow tile and the crazy
mine; the minesweeper only displays its - send to `UI_CLEAR_STAGE` are
the same millisecond counts, 64-bit until the message's `intptr_t`,
where the handler already divides by ten and clamps to a `WORD` for the
score packet; the two "%5d.%2d" displays compute one 64-bit count and
split it, where they read the tick twice. No quantisation change:
`timeGetTime()` was already the 1 ms tick. The retyping supplies three
initialisers master lacked - the arrow tile's monster-move stamp, the
crazy mine's start and the monsters' end stamp - and only the first is
read before anything sets it: the gate compared its first read against
whatever the member held, and a default time point reads as the epoch,
so the gate is open before the first move, as it was for any stale
value more than 600 ms old - and no play reaches it either way, since
the move counter it runs under is only ever zeroed and decremented, on
master as here (the crazy mine's start is read only once `m_bStart` is
set, the monsters' end never). One display change the
split makes: a left click on the board after a win re-runs the clear
path, which has no status guard, and master's second `elapsed = tick -
elapsed` over a field already holding the elapsed time showed roughly
the seconds since boot; the branch recomputes from the start point that
still stands, so the timer resumes from where it stopped - a small
display defect removed, not equivalence.
R14 goes from 113 to 93 (20 calls). `VS_UI` holds 13: the three
deadlines the executable sets through shared structs (`quest_time`,
`delayFrame`, `left_time`), which want the same retyping on both sides
of the boundary and are the next slice. `Client` holds 80. No test
drives these windows (`user_option_tests` links `VS_UI`, so a test could
be written against it; none reaches the minigames); verified by the
build on Windows in both Debug trees and by the ratchet, the client not
run.

The eighth priority-5 slice (2026-09-16) is the three deadlines the
executable sets and `VS_UI` reads through shared structs, retyped on
both sides of the boundary at once. Two were counted in whole seconds -
`QUEST_STATUS::quest_time`, set by `UI_RunQuestStatus` from the quest
packet's remaining seconds, and `WAR_INFO::left_time`, set by
`MWarManager::SetWar` from the war info's - as `remaining +
timeGetTime()/1000` and read against `timeGetTime()/1000`. Those are
`MonotonicClock::SecondPoint` now, a time point at whole-second
resolution the header adds beside a `NowInSeconds()` that floors the
clock to it, so a deadline set as `NowInSeconds()` plus the seconds
left and read against `NowInSeconds()` counts down on the clock's second
boundary exactly as the divided tick did - the arithmetic is the same
integers, only 64-bit, over the steady clock's second boundaries rather
than the tick's, so a displayed count can sit one second out of phase
with what the tick would have shown at the same instant, as approximate
either way (`tests/unit/test_monotonic_clock.cpp` pins the floor, the
countdown and the count past the wrap). The time-item register of the
second slice had written the same typedef and the same floor as a
file-static of its own; both are the header's now, and its tests are
unchanged. The third,
`S_SLOT::UI_EFFECTSTATUS_STRUCT::delayFrame`, is a millisecond deadline
`UI_AddEffectStatus` sets as now plus the effect's frames scaled to
milliseconds, and is a `TimePoint`; the effect status window's three
readers - the tooltip's remaining seconds, and the two icon loops that
flash an icon under five seconds left - read the clock once and take
the difference, where the tooltip read the tick twice. The two "reset to
0" writes of `quest_time` are the default time point, which every
comparison treats as the zero second did (a quest with no deadline set
reads as timed out, as before). No quantisation change and no
resolution change: the second-counted deadlines still floor both ends
to the second, the millisecond one was already the 1 ms tick. R14 goes
from 93 to 72 (21 calls: 13 in `VS_UI`, the three setters' 8 in
`Client`). `VS_UI` holds no live tick read now - what remains is a dozen
mentions inside comments in five files, two of them in the commented-out
block in the effect status window's `Process` - and
`Client` holds 72: `MTopView` (21), `MPlayer` and `MFakeCreature` (7
each), and the rest in ones to fours. Verified by the build on Windows
in both Debug trees, the ratchet and the clock test; the client not run.

The ninth priority-5 slice (2026-09-16) is the event register's start
stamp and the HP-modify list's. `MEvent::eventStartTickCount` is set by
`MEventManager::AddEvent` and reset by the two ending cinematics in
`MTopView` (the advancement quest's and the Ousters') as each starts,
and was read as `GetTickCount() - eventStartTickCount` at ten sites: the
show-time blink and the expiry in the manager, the two countdown
captions, the two scroll positions, the two fades and two four-second
script gates in `MTopView`. It is a `TimePoint` now, and the ten reads
are one `MEvent::ElapsedMillis()`, which returns the elapsed count as
the `DWORD` every consumer's arithmetic already ran in - so the
captions' `(delay - elapsed + 999) / 1000`, the scrolls' `-500 + elapsed
/ 66`, the fades' `min(31, elapsed / speed)` and the gates' `elapsed /
4000 > 0` are the same expressions over the same width.
`MCreature::HPModify::TickCount`, stamped as a damage number is queued
and read once by the list drawer to drop numbers older than
`HPModifyListTime`, is a `TimePoint` the same way. **What this slice does
not do:** it removes no wrap defect. Every one of the eleven reads was a
plain unsigned subtraction, which is exact modulo 2^32 and so measured
the true elapsed time across the tick's wrap already; none had the
`previous + delay <= now` shape R14 exists for, and `ElapsedMillis()`
truncates to `DWORD`, so a span past 49.7 days wraps as it did. The
slice takes these stamps off the mixed counters and onto the one clock,
which is what the count measures. Four of the six resets and both
script gates sit in switch arms whose selector is never set to the
values they test - the cinematics' script stepping is commented out at
its two writers - and were retyped with the rest as dead code. The
quantisation statement this slice owes, hedged because it was not
measured: these sites read `GetTickCount()`, which on Windows is still
kernel32's counter (`basic/Platform.h` routes only `timeGetTime()` to
`platform_get_ticks()` there; off Windows both are the SDL tick), whose
step is the system timer's - up to 15.6 ms, and likely about 1 ms under
the timer resolution SDL2 asks for on Windows by default - and they
read a 1 ms clock now. Both ends of an interval were quantised, so a
gate, a fade step, a caption's second boundary or a damage number's
drop can fall up to one old step earlier or later than before; nothing
else changes. R14 goes from 72 to 53 (19 calls). `MTopView` holds 6:
the quest event's `parameter1` and `parameter4`, generic `DWORD` fields
the drawer compares against the tick and the second, which nothing
sets to a tick today (the writers that did are commented out in
`GCQuestStatusHandler`) and which want their own reading before they
are retyped; `MPlayer` holds 9 (two of them in its header, both feeding
`previous + delay < now` gates - the shape that does fail at the wrap)
and `MFakeCreature` 7. Verified by the build on Windows in both Debug
trees and the ratchet; the client not run.

The tenth priority-5 slice (2026-09-16) is the player's four stamps and
the fake creature's move deadline, with the two executable-side setters
that fed them. `MPlayer::m_RepeatTimer` and `m_LockTimer` (stamped by
`SetRepeatAction` and `SetLockMode` in the header) and
`m_PetDelayTime` (re-armed whenever its gate opens with a pet item
present, corpse or not, and by `SetPetDelay` from `PacketFunction`)
were gated as `stamp + delay <
now` - the sum shape that R14 exists for, open on every frame once the
sum carries past 2^32 and shut for 49.7 days once the tick does - and
`MCreature::m_TraceTimer` (stamped on a click-to-follow, `0` for none)
as `(now - stamp) / 1000 > limit`, a subtraction. `MFakeCreature::
m_nextMoveTime` was a deadline, `now + (2 to 10 s at random, by site)`,
compared as `now > deadline` - a sum shape again - and set from
`PacketFunction` for the rocket as `now + flight time` through
`SetTime`. All five are `TimePoint`s: the three sum gates read `now -
stamp > Millis(delay)`, which is the same strict comparison over
64-bit arithmetic, so a stamp left at the default point still opens
the gate at once, as the pet delay's `0` did on master - the repeat
and lock stamps master never initialised at all (read only under their
flags, which the constructor clears; `docs/skill-repeat-diagnostics-
2026-08-31.md` records it) and are the epoch now; the trace limit
keeps its `(DWORD)` elapsed and its default-point sentinel; the deadline
is `Now() + Millis(...)` and the two comparisons are unchanged. The two
setters take a `TimePoint`, and the repeat gate's diagnostic line logs
the elapsed count and the limit where it logged two raw ticks and the
limit. One trap the review caught before it shipped: the pet's delay is
computed from its level, a wire byte nothing clamps, and a level past
the table's maximum makes it negative; master's `DWORD` sum wrapped and
left that gate open, and `Millis(DWORD)` would have turned the negative
into 49.7 days and shut it - so the delay is floored at zero at the
gate, which keeps master's behaviour. `Millis()` taking a `DWORD` is
worth remembering for the sites still to come: a signed delay handed
to it does not warn under `/W3`. This is
the first `Client` slice that removes the wrap failure R14 describes:
four gates had it - three in the player, and the fake creature's
deadline at its two comparison sites.
Quantisation, hedged as in the ninth slice: the player's four stamps
and `SetPetDelay`'s caller read kernel32's `GetTickCount()` on Windows,
so those gates can open up to one old step earlier or later; the fake
creature's and the rocket's read `timeGetTime()`, already the 1 ms
tick. R14 goes from 53 to 35 (18 calls); `MTopView` holds 6, the quest
event's parameter fields, and the rest is ones to fours in twelve files.
Verified by the build on Windows in both Debug trees and the ratchet;
none of it has a test; the client not run.

The eleventh priority-5 slice (2026-09-17) is the scatter: every live
clock read outside the frame clock, 25 calls in eight files (the four calls that are not clocks stay), the one
`packetwire` site among them. The wait screens' double-click gate in
`CWaitUIUpdate` kept its last click as a `DWORD` and asked
`labs(GetTickCount() - last) <= g_double_click_time`, resetting the stamp
to 0 after a double click; it is a `TimePoint` now, the reset is the
epoch, and the gate is the same comparison over the 64-bit difference -
the `labs` guarded against a clock that runs backwards, which this one
does not. `CGameUpdate`'s copy of that stamp is write-only (its gate has
been commented out since the DirectInput original) and is retyped with
it. The title screen's fade kept a static start tick and computed `31 -
elapsed * 16 / 1000` in `DWORD`, cast to `int` - which is how a fade past
two seconds went negative and clamped to zero - and computes the same
expression over a `DWORD` elapsed count taken from the clock. The
`CGVerifyTime` deadline in `CGameUpdate::Update` was set as
`timeGetTime() + value` and read as `g_CurrentTime > nextTime`: a
sum-shaped deadline, the failing shape, and one that mixed a fresh tick
with the frame's stamp; both ends read `Now()` now, so the gate opens earlier than the frame stamp would have opened it, by the frame's work so far (the deadline is anchored at the same instant; the frame stamp is the older read), and `nextTimeValue` is a `DWORD`, so `Millis()` is never handed a
signed value (the tenth slice's lesson). `GCUpdateInfoHandler`'s loading
stopwatch measures from a `TimePoint` to `Now()` instead of to the
`g_CurrentTime` it has just refreshed, which stays, being the
frame clock's. `ProfilerInfo::m_StartTime` is a `TimePoint` whose epoch
is the "no pass open" sentinel that `0` was, and the total keeps
accumulating `DWORD` milliseconds; only the `OUTPUT_DEBUG` macros call
it. The name window's sixty-second auto-close parked its tick in
`TempInformation::Value1`, an `intptr_t` slot, through an `int` cast;
`TempInformation` has a `TimeValue1` for it. `LCReconnectHandler`'s two
stopwatches under `OUTPUT_DEBUG`, which nothing defines, and
`SocketInputStream::fill`'s byte-rate window under
`__TEST_PACKET_RECEIVED_SIZE_PER_SECOND__`, which the file's own first
line defines out - the `packetwire` site, which the survey after the
third slice called dead code rather than a clock to move - are converted
as dead code, the window to an `IntervalTimer` whose first second runs
from static initialisation rather than from the first `fill()`; each was
compiled once with its macro defined (the handler's converted lines
compile; the link then fails on `DEBUG_ADD` and `DEBUG_ADD_FORMAT`, which `DebugInfo.h`'s
`OUTPUT_DEBUG` branch declares and nothing defines - a pre-existing
property of the define, not of these lines) and the define reverted.
`MTopView`'s six, the quest caption that read the event register's
`parameter1` and `parameter4` as a tick and a second, are deleted with
the caption: its only writers are the commented-out event builders in
`GCQuestStatusHandler`, so `GetEventByFlag(EVENTFLAG_QUEST_INFO)` never
returns one, and upstream's own comment above it says the part below probably will not be used. That deletion takes six checked-format sites with it, so
`check_format_arity.pl`'s floors go from 301 and 289 to 295 and 283, and
the converted-site count C19 quotes from 321 to 315. Removed wrap
failure: the `CGVerifyTime` deadline; every other converted read was a subtraction, and the six deleted ones were bare comparisons against the tick.
Quantisation, hedged as before: the two click stamps read kernel32's
`GetTickCount()` on Windows, so a double click's window can close up to
one old step earlier or later; the rest read `timeGetTime()`, already
the 1 ms tick. The `MMSystem.h` notes in `CGameUpdate`, `CWaitUIUpdate`
and `Profiler` go with their last tick call (`CWaitUIUpdate`'s remaining
one sits in a block comment). R14 goes from 35 to 10 (25 calls), and what
is left is the frame clock - `g_CurrentTime`'s three live writers,
`g_StartTime` and `CWinUpdate::m_CurrentTime` - and four calls that are
not clocks: the two log-file names in `Client.cpp` and `GameMain`'s hack
check, which compares `timeGetTime()` against `GetTickCount()` on
purpose. Verified by the build on Windows in both Debug trees and the
test suite in both; the client not run.

The twelfth priority-5 slice (2026-09-17) opens the frame clock, the
last of the tick's readers. `g_CurrentTime` is a `DWORD` stamped from
`timeGetTime()` at the top of every frame and was read at 144 sites:
the static-local gates of the update loops, some forty member stamps in a dozen classes, three deadlines in `UserInformation`, `MGameTime`'s
base, and two library seams (`MItemHost::pCurrentTime`, the pointer the
item core's trade-accept and skill-use delays run on, and
`WireHost::CurrentTime`, the function the wire layer's timeouts run
on). Most of the stamps are `g_CurrentTime + delay`, the sum shape, so
this is where the wrap failure R14 describes lives at scale, and it
cannot be retyped in one step without touching every reader and both
seams at once. So the slice puts a `TimePoint` beside it: `g_FrameNow`,
stamped by one `StampFrameClock()` in `Client.cpp` that sets both from
one read each (the three writers - the frame loop,
`CWaitPacketUpdate::SetDelay` and the update-info handler - call it),
and a new ratchet, R16, counts what still reads the `DWORD`, so the
readers can move a group at a time and the `DWORD` goes when the count
reaches its definition, declarations and writer. The first group is
every static-local gate: `CGameUpdate`'s fixed-step accumulator
(`lastTime += delay` with its catch-up loop, its
`MAX_UPDATE_ONETIME_COUNT` reset and the draw interpolation's
`sinceTick`, the same arithmetic over 64-bit milliseconds, where an
accumulator ahead of the frame reads as a negative elapsed rather than
a huge one), the sound-per-second window, the anti-cheat's clamped
elapsed, the help scroll and the resurrect dialog's frame step;
`MTopView`'s four message scrolls and click frame; `GameMain`'s
request-manager tick and keep-alive; the wait screens' update gate;
`Client.cpp`'s log flush and the FPS window (`g_StartTime` a
`TimePoint`; `g_EndTime`, never read, deleted); the party status send and the conversion countdown's blink gate in `MPlayer`; and `MZone`'s personal-shop reset, whose interval is a
signed option value floored at zero, since a negative one made the
`DWORD` sum wrap so the reset fired every frame and `Millis()` would
have made it 49.7 days. With them the small stamp classes only the
frame clock fed: `SoundNode`'s play time, `ShowTimeChecker`'s next
show, `MHelpDisplayer`'s hold, `MParty`'s join time (its `0xFFFFFFFF`
"never joined" was a sum that wrapped to "kick allowed"; the epoch says
the same without the wrap), `MJusticeAttackManager`'s map value
(written, never read) and `MZoneSoundManager`'s last update, plus the
zone random-sound deadline shared by `GameMain` and `MZone`, and
`CWaitPacketUpdate`'s give-up deadline (`0xFFFFFFFF` for "not set" is
`TimePoint::max()`, spelled with the parentheses that keep
`<windows.h>`'s `max` macro off it; its two debug lines print the
delay and the overrun instead of two raw ticks). `CWinUpdate`, the base
of the update classes, kept a static tick, a last-update stamp and a
delay that no class outside it read; they are deleted rather than
retyped, with the two tick calls in them. A write-only static in `CGameUpdate` (`lastDisplayGameTime`) and a write-only pair of locals (`TempCurrentTime`, `TemplastTime`) went the same way. Gates that compared a `DWORD`
difference against an `int` delay (`g_UpdateDelay`, the config's
message delays, `tmp`) hand the `int` to `Millis(DWORD)` as they handed
it to the unsigned comparison: a negative delay never fired before and
never fires now. Quantisation: every one of these read the 1 ms
`timeGetTime()` through the frame stamp and reads the 1 ms clock
through the same stamp; the frame-consistent read (one stamp per
frame, not a fresh `Now()` per gate) is kept on purpose. R14 goes from
10 to 5 (the one remaining writer, the two log-file names, the hack
check); R16 starts at 57 from 144. What is left for the next slices:
the creatures' and the player's member stamps (chat fade, recovery,
regeneration, bleeding, action and death delays, the conversion
countdown), `UserInformation`'s deadlines with their `gamemodel` test
path, `MGameTime`, the two seams, then the `DWORD` itself. Verified by
the build on Windows in both Debug trees and the ctest suites in both
test trees (nothing a test binary links changed; the ratchets and the
include checker ran); the client not run.

The thirteenth priority-5 slice (2026-09-17) is the second frame-clock
group: the creatures' and the player's member stamps. `MCreature` kept
six `DWORD` deadlines on the frame clock - the chat string's fade
(`m_NextChatFadeTime`: now plus the config's keep delay on a new line,
now plus the fade delay on a fade step, now plus `0xffffff` for the
type-650 creature's permanent line, and cleared to 0), the HP and MP
recovery (`m_RecoveryHPNextTime`, `m_RecoveryMPNextTime`: now plus the
recovery delay, then advanced by the delay per step), the regeneration
and its bonus (`m_RegenNextTime`, `m_RegenBonusNextTime`: now when set,
advanced by the delay per step) and the bleeding (`m_NextBloodingTime`:
now plus a gap, or five gaps) - and `MPlayer` three more: the action
delay (`m_DelayTime`: now plus the action's delay from the table, 300
ms for a skill used standing still, cleared to 0; read by `IsNotDelay()` before every action), the
death delay (`m_DeadDelayTime`: now plus the config's delay, read by
`GetDeadDelayLast()` as whole seconds left) and the conversion
countdown (`m_ConversionDelayTime`: now plus the delay
`SetConversionDelay` gets from the effect's frames, cleared at five sites, the header's `UnSetConversionDelay()` among them; read as minutes left and by the blink schedule). All nine are
`TimePoint`s: every "now + delay" is `g_FrameNow` plus a duration,
every `+= delay` advances by one, every comparison is the same
comparison against `g_FrameNow`, and every clear is the epoch, which
the gates read as they read `0` (an epoch deadline has passed;
`IsNotDelay()`'s `m_DelayTime==0 ||` half folds into the `<=`, since
the epoch is below every stamp). `MFakeCreature`'s one read, the
inherited chat fade, moves with them. The delays split by signedness:
the config's `int` delays and `GetActionInfoDelay()`'s `int` go
through a signed `MonotonicClock::Duration`, so a negative value still
puts the deadline in the past as the `DWORD` sum's wrap did (the `int` converts unsigned in a sum too, but the sum's modular wrap restores now minus the delay, which a comparison cannot recover; the twelfth slice's comparison sites therefore took `Millis(DWORD)`); the
`DWORD` delays (recovery, regeneration, the bleeding gap, the blink
table, `SetDelay`'s and `SetConversionDelay`'s arguments) go through
`Millis`. `GetDeadDelayLast()`'s `int second = deadline - now` is the
same `int` over the 64-bit difference, 0 when negative as before; the
conversion countdown's `(int)` casts, master's way of comparing two
ticks as signed, go with the width, and its blink `timeGap` is a
`DWORD` of the same difference, read only when the deadline is ahead.
One reader changes: `m_DeadDelayTime` was never initialised -
`GetDeadDelayLast()` read whatever the allocation held until the first
death - and is the epoch now. The debug line that printed the tick and
the deadline prints the delay; `GetConversionDelayTime()` returns the
point (its one caller is a comment). Removed wrap failure: all nine
were sum-shaped deadlines, the shape R14 exists for. Quantisation:
unchanged, the frame stamp on both sides as in the twelfth slice. The
four `extern DWORD g_CurrentTime` declarations these files carried go
with their last reader, and the Korean comments beside the rewritten
lines are translated. R16 goes from 57 to 18: what is left is
`UserInformation`'s two live deadlines (`LogoutTime`,
`ItemDropEnableTime`, with their readers and writers in `CGameUpdate`, `UIMessageManager` and `GameUI`), `MGameTime`'s start and current time
(with `GCUpdateInfoHandler`'s and `CGameUpdate`'s calls), the two
library seams in `GameInit`, the definition, the one write and the log
flush's debug print in `Client.cpp`, and the externs in `Client.h`,
`MGameTime.cpp` and `GCUpdateInfoHandler.cpp`. Verified by the build on
Windows in both Debug trees and the ctest suites in both test trees
(nothing a test binary links changed); the client not run.

The fourteenth priority-5 slice (2026-09-17) is the third frame-clock
group, and the first with a test path: `UserInformation`'s deadlines,
a `gamemodel` member, and `MGameTime`. `UserInformation` kept three
`DWORD`s on the frame clock. `LogoutTime`, `0` for "none scheduled",
is set by the ESC handler to now plus five seconds (two in `_DEBUG`),
read by the same handler as whole seconds left for the countdown
caption, tested by the update loop as `!= 0 && now > it` to run the
logout, and cleared at three sites. `ItemDropEnableTime`, set after a
trade to now plus the config's delay, gates dropping an item as
`it < now`. `GlobalSayTime`'s three uses are commented out, and it is deleted with its declaration and constructor write. The two live ones
are `TimePoint`s with the epoch as the sentinel, and the class gains five small members - four one-line accessors and an out-of-line countdown - for what the executable's sites were spelling by hand - `IsLogoutScheduled()`, `IsLogoutDue(now)`,
`SecondsToLogout(now)`, `CancelLogout()`, `IsItemDropEnabled(now)` -
so the contract has a home a test can reach:
`tests/unit/test_user_information_deadlines.cpp` pins the defaults, the
whole-second countdown and its strict "due" edge, the cancel, the
strict item-drop edge, and the logout scheduled across the legacy
wrap, with the `DWORD` sum's failure worked out beside it (a deadline
set 1 s before the wrap for 5 s later came out as 4000, and `now >
4000` logged the player out at once). The executable's sites call the members or assign the point from `g_FrameNow` (the ESC handler's countdown branch lets `SecondsToLogout` answer instead of comparing by hand; it returns 0 once due, which the branch's `sec > 0` test already required); the strict comparisons are unchanged. `MGameTime` derived the game clock from `(now - start)
/ 1000 * ratio` over two `DWORD`s and re-based itself on the frame
clock at a month's turn; its start is a `TimePoint`, `SetStartTime`
and `SetCurrentTime` take one, the gap is the same `DWORD` over the
64-bit difference, and the re-base uses the `now` it was handed rather
than the global (the same frame's stamp). Its two callers, the update-info handler's start and the update loop's per-frame current time, pass `g_FrameNow`. Removed wrap failure:
the logout and item-drop deadlines were sum-shaped; the game clock's
subtraction was exact already and changes width and clock only.
Quantisation: unchanged, the frame stamp on both sides. R16 goes from
18 to 6: what is left is the two library seams in `GameInit`
(`MItemHost::pCurrentTime`, `WireHost::CurrentTime`), the definition,
the one write and the log flush's debug print in `Client.cpp`, and the
extern in `Client.h`. Verified by the build on Windows in both Debug
trees and the test suites in both test trees, the new tests included;
the client not run.

The fifteenth priority-5 slice (2026-09-17) is the last of the frame
clock: the two library seams that read it, and then the `DWORD`
itself. `MItemHost::pCurrentTime` handed the item core a `const DWORD*`
that `MItem::Clock()` returned to the skill delays (`SKILLINFO_NODE`'s
`m_AvailableTime`, "clock + delay", read as `*clock >= it` and as the
time left) and the trade accept delay (`MTradeManager`'s
`m_NextAcceptTime`, the same shape); `WireHost::CurrentTime` handed the
wire layer a `DWORD()` that `RequestServerPlayer`'s timeout ran on
(`CurrentTime() + EXPIRE_DELAY`, read as `CurrentTime() > it`). Both
seams carry a `TimePoint` now - a pointer to `g_FrameNow` and a
function returning it - and the three deadlines behind them are
`TimePoint`s: every sum is the point plus a duration (the skill's
signed `int` delay through `Duration`, the `DWORD` delays through
`Millis`), every comparison is unchanged, the "no host" cases read as
they did (`Clock()` NULL means no delay; the wire host's epoch plus a
minute never expires, as `0 + 60000` never did), and the skill's
time-left is the same `DWORD` over the 64-bit difference where it
was a signed `int` over two ticks. Both seams were tested already, so
the tests move with them: the fake clocks in the trade, skill, container-helper and wire-host tests are `TimePoint`s (the trade, skill and wire-host ones set through `FromMillis`; the container helpers never advance theirs), and two new cases pin the delays across the legacy wrap
with the `DWORD` sum's failure worked out beside each - a trade
refused 1 s before the wrap could be accepted again at once, a skill
used then was available at once. With no reader left, `g_CurrentTime`
goes: its definition, its extern, its write in `StampFrameClock()`
(which stamps `g_FrameNow` alone now) and its `timeGetTime()` call,
and the log flush's debug print prints the point's millisecond count.
R16 goes from 6 to 0 and stays as the guard that nothing brings the
name back; R14 goes from 5 to 4, and the four are the floor: the two
log-file names in `Client.cpp`, which want a number that differs per
run and are not clocks, and `GameMain`'s hack check, which compares
`timeGetTime()` against `GetTickCount()` on purpose and would measure
nothing on one clock. That closes priority 5: every tick read that was
a clock, in `Client`, `VS_UI`, `basic`, `gamemodel` and `packetwire`,
runs on `MonotonicClock` - the widgets' interval gates on
`IntervalTimer`, the deadlines and stamps on `TimePoint`, the
second-counted ones on `SecondPoint` - and the frame clock is one
`TimePoint` stamped once per frame. Verified by the build on Windows in
both Debug trees and the test suites in both test trees (674 tests,
297,466 checks, 0 failed in each); the client not run.

**Filesystem status (2026-09-05):** the first priority-6 slice is implemented.
`basic/DirectoryListing.{h,cpp}` lists a directory through
`std::filesystem::directory_iterator` against a DOS-style wildcard and returns
a snapshot sorted in the case-insensitive ordinal order an NTFS `_findnext`
walk produced, so a caller owns no search handle and cannot be handed a file it
created inside its own loop. The header carries a measured semantics table:
`*` and `?` matching, case-insensitive ASCII folding and the NTFS order are
preserved; 8.3 short-name aliases, `.` and `..`, and Win32's zero-or-one
trailing `?` are deliberately not. Profile discovery and deletion in
`Client/ProfileManager.cpp` and the log cleanup in `Client/Client.cpp` are the
migrated walks; the latter also drops a `long` that truncated the CRT's
`intptr_t` search handle on x64. `tests/unit/test_directory_listing.cpp` pins
the matcher, the order and the failure contract over a scratch directory. The
migrated callers are executable-only and need the runtime check that the login
screen still lists profiles. A second slice migrated the last three `_findfirst`
walks: the one that empties the `Update` directory in `Client/Client.cpp`, and
`UUFDeleteDirectory` and `UUFDeleteFiles` in `Client/Updater/UpdateUtility.cpp`,
all of which stored the CRT's `intptr_t` search handle in a `long`. Each keeps
its `_chdir` dance and its dotfile skip, asks for `*` where it asked for `*.*`,
and lists files only. `UpdateUtility.cpp` is compiled into no target and does
not build on its own for four pre-existing reasons in untouched functions; the
migrated code was proven to compile with those patched temporarily. No
`_findfirst` call remains in live code. A third slice took the four
`FindFirstFile` call sites that were left: the `Updater2.exe` existence test
in `Client/Client.cpp`, which never closed its search handle and is now a
`std::filesystem::exists` through the `std::error_code` overload; the startup
DLL whitelist in the same file; the older copy of that whitelist in
`Client/GameInit.cpp`, whose `g_wAuthKeyMap` assignment fires only when the
listing is non-empty and every name on it is whitelisted; and
`C_VS_UI_FILE_DIALOG::RefreshFileList` in `VS_UI/src/VS_UI_ExtraDialog.cpp`,
the profile-picture file dialog. The whitelist keeps its silent `return -1`
out of `WinMain` - no message box and no log line, because logging is not up
yet - and both whitelists list directories as well as files, because they
judge the name alone and a subdirectory whose name ends in `.dll` used to
reject the client too. The 8.3-alias deviation applies to them: a file such
as `foo.dllx` was matched through its `FOO~1.DLL` short name and stopped
startup, and is no longer listed, so the check only ever gets looser and no
name that was listed stops being listed. The file dialog synthesises the `..`
entry `directory_iterator` never yields, first in the sequence and only for a
directory that has a parent - `path::has_relative_path()` decides that, and
`dir /a` confirms it, listing `..` for `C:\Users` and not for `C:\`. Reading
that function turned up a pre-existing defect, fixed here: the inner
`for(int i = 0; ...)` shadowed the outer `i`, so the
`if(i == m_vs_file_list.size())` after it tested the filter loop's counter
instead, and since the inner loop almost never breaks for a file, every file was
dropped unless `m_filter.size()` happened to equal the list size. The suffix
filter beside it was unbounded twice over: it `strcpy`'d each entry into a
`char[20]` out of the `char[30]` `Start()` fills, and it indexed the name at
`size() - j - 1`, which wraps for a name shorter than the suffix. Both halves
are `basic/FileDialogListing.{h,cpp}` now - moved out of VS_UI so that they
have a test path at all, then fixed test-first
(`docs/reviews/RESTRUCTURING.md` task 3.1, `tests/unit/test_file_dialog_listing.cpp`).
The `..`
entry is synthesised outside the success branch, because the helper is all
or nothing where `FindFirstFile` had delivered `..` before any `FindNextFile`
could fail; the same all-or-nothing rule is a deviation of the two whitelists
too, and there it can only skip the check. A dangling junction is the one
entry the helper classifies differently from `FindFirstFile`: it follows the
link and reports a file where the legacy walk reported the reparse point's
own directory bit, so the dialog no longer shows one.
`tests/unit/test_directory_listing.cpp` now also pins the trailing-separator
directory string the dialog is the only caller to pass. No `FindFirstFile` call
remains in live code - only the commented-out loop in
`VS_UI/src/VS_UI_Tutorial.cpp` - and a fourth slice deleted the shims those
walks used to reach. `basic/Platform.h` no longer declares `FindFirstFileA`,
`FindNextFileA`, `FindClose`, `WIN32_FIND_DATA`, `_finddata_t`, `_findfirst`,
`_findnext` or `_findclose`. All of them sat in the `#ifndef PLATFORM_WINDOWS`
compatibility layer, which a Windows build never compiles, and nothing outside
that header named any of them in live code - only comments and the commented-out
tutorial loop. `FILETIME` stayed, because `Client/CrashReport.cpp` declares one,
and so did `INVALID_HANDLE_VALUE`, which `Client/Client.cpp`,
`Client/CGameUpdate.cpp` and `Client/CrashReport.cpp` still compare against.
Priority 6 now has no `_findfirst` or `FindFirstFile` residue left.

**Bounded-formatting status (2026-09-17):** the first priority-7 slice
is implemented, and it is mostly an instrument. The formats the data
files own are C19's and closed (`SafeFormat`, R7 and R8, the arity
audit); what priority 7 is about is the other kind - a developer-owned
literal or a plain copy into a fixed buffer, where the defect is the
destination, not the format. The packet tree has been at zero on that
since task 5.4 (R3). The rest of the tree measures **1,102** such lines
by R3's grep with `wsprintf` added and headers scanned beside sources - `sprintf`, `wsprintf`, `strcpy` and `strcat`, with the `//` tail of each line stripped, read with `grep -a` because of the NUL bytes in `VS_UI_GameCommon.cpp` - and **ratchet R17** holds that
count over `basic`, `VS_UI` and `Client` outside the two packet
directories, so each slice retreats along it and a new unbounded copy
fails the suite. A site leaves the count when its destination is a
real array and the call is `snprintf(dst, sizeof(dst), ...)` or an
exact-length `memcpy`, checked by hand - `sizeof()` on a pointer is
the mistake the packet-tree pass warned about, and the reason this is not scripted. What the instrument cannot see: `vsprintf`, eleven live lines in `MinTr.h` and the two `DebugInfo.cpp` that forward a vararg list into a fixed buffer (the `\b` that rejects its leading `v` is the same one that would reject `lstrcpy`, of which the tree has none), `strncpy`, and a copy spelled any other way. This slice takes `basic` and `SpriteLib` to zero (`SafeFormat`'s bounding is nominal - the `*w++` writes around the two `snprintf` are held inside the 32-byte spec by the flag, width and precision caps declared 300 lines away, which is why the sizes are passed down at all):
`C_DIRECTORY`, a class nothing constructs whose constructor calls a
POSIX-only function and so could not have linked into the Windows
client had anything referenced it, is deleted with its six lines and its two files (the PS.h umbrella loses the include; nothing else named
it); `PlatformSDL.cpp`'s three sites (four lines) are bounded by the size each already checked - the executable-directory join is one `snprintf` into
the caller's buffer behind the existing `len + 2 > size` test, the
"./" fallback is one into its array, and the config value's copy is a
`memcpy` of the length the guard above it has just admitted;
`SafeFormat`'s `BuildSpec` takes the spec buffer's size and writes its
width and precision with `snprintf` against the space left (the caller's
32-byte array, and the two values are capped well inside it, so
nothing changes but the spelling); and `SpriteLib`'s two size-mismatch captions in `CTypePack.h` are `snprintf(sizeof)`; the other two lines were in `CTypePackVector.h`, a header no translation unit includes (its Windows branch still asks for `<fstream.h>`, which no compiler here has shipped), and it is deleted with its listings rather than bounded - the first version of this slice bounded it and called its index file name a fixed overflow, which a review corrected: a site nothing compiles is not a live path. R17 goes from 1,102 to **1,086**; what
is left is `VS_UI` (783) and the rest of `Client` (303), most of them
`wsprintf` and `sprintf` into local `char` arrays, to be taken a file
group at a time with each destination read. Verified by the build on
Windows in both Debug trees (reconfigured, since a source left the
`basic` list) and the test suites in both test trees; the client not
run.

**Bounded-formatting second slice (2026-09-17):** ten Client utility
files to zero, 70 lines - nine converted, one deleted. `GetWinVersion` wrote its caller's buffer with
`strcpy`, `strcat` and `sprintf` through a bare `char*`; it takes the
buffer's size now and writes everything with it, the build-number
suffix appended behind a length check, and its one caller, the crash
report, passes `sizeof` of the 1,024-byte array it always was. `CMd5` is
deleted with its two files: nothing constructs it (one source
included its header and used nothing of it), and its 50-byte error
text was an overflow for any path over 21 bytes (`md5.h` defined
`ENGLISH`; the other text was 53 bytes before any path) - eight of
the lines, which the first version of this slice bounded and called a
live overflow. The crash report's four stamps (its fifth line, a
`strcpy` of the empty string, is a NUL store), the profile manager's
five file-name joins over six lines (the `.bmp` to `.spk` rewrite is one `snprintf` with a
`%.*s` where it was a `strncpy`, a NUL and a `strcat`), the utility
functions' four copies, the guild-mark manager's four index names and
the wait screen's one live caption are `snprintf(sizeof)`, every
destination a local `char` array read at the site. `MMusic` declared
its MCI error text as eighty `LPSTR`s and wrote it through a cast at
nine sites; it is the `char` array it was always used as, and the
empty test reads the first character rather than the first pointer.
The other sixteen lines the count held in these files were inside
block-commented debug code - three LRU dumps in `CPartManager.h`, four
light-sight traces in `MCreatureWear.cpp`, the server list and FPS
displays in `CWaitUIUpdate.cpp` (which the first version rewrote in
place) - R17 strips only `//` tails, as R3 does, so a `/* */` block
counts - and are deleted with their blocks. R17 goes from 1,086 to **1,016**. Verified by the build
on Windows in both Debug trees (reconfigured, a source having left
the executable's list) and the test suites in both test trees; the
client not run.

**Bounded-formatting third slice (2026-09-17):** `Client.cpp`,
`GameMain.cpp`, `MZone.cpp` and `MCreature.cpp` to zero, 46 lines.
Thirty-one are converted. In `Client.cpp` the Futec command-line
parser's two copies (a 4,096-byte scratch, then four 32-byte argument
slots that a `|`-separated token from the command line overran), the
restart path's join of the executable name onto the current directory
(a `sprintf` that read its own destination, undefined behaviour, now
an append behind `strlen`), the crash message, the log-file name (128
bytes built from a `_MAX_PATH` `g_CWD`, so an install directory over
about a hundred characters overran it), the log-directory join, the
host name, the check string, the two update-directory joins, the
`OUTPUT_DEBUG` dump, the two nProtect messages and the updater's
command line are `snprintf(sizeof)`. In `GameMain.cpp` the server-group
name - 80 bytes, from the group the login server's world list
supplied - the screenshot path, the three sound file names and the
two ghost-position XML keys (the second with its `size_t` counter cast
to the `int` its `%d` reads) are the same. In `MZone.cpp` the
image-object trace's five type labels and its two self-reading
appends are bounded under the `__OUTPUT_IMAGEOBJECT__` switch nothing
defines. In `MCreature.cpp` the creature name is a `memcpy` of the
length it was just allocated with, and the two chat-row copies - each
already held to the row by the wrap arithmetic above it - are bounded
by the configured row width. Fifteen lines were dead: `get_rand_str`,
which only commented-out lines called, is deleted with them, and
thirteen lines of block-commented debug dumps (three image-object
traces in `MZone.cpp`; seven light-sight and move traces in
`MCreature.cpp`, two of them inside larger commented-out move-logic
blocks) are deleted with their blocks. R17 goes from 1,016 to
**970**. Verified by the build on Windows in both Debug trees and the
test suites in both test trees; the client not run.

### Packet modernization guardrails

Packet I/O is the highest-value area but also the easiest place to cause a silent
compatibility break. A span/concepts change should:

- retain the existing packet wire inventory and golden-byte tests;
- use fixed-width wire types and add compile-time width checks;
- make byte order explicit with `std::endian` where multi-byte values cross the
  network boundary;
- avoid serializing whole C++ structs or using `std::bit_cast` across padding;
- add new overloads before migrating call sites, rather than changing every packet
  in one patch.

### Formatting guardrails

`basic/SafeFormat` intentionally tolerates malformed or data-controlled legacy
format strings. A blanket conversion to `std::format` would change that behavior
and may introduce exceptions. Keep `SafeFormat` for localized or server/data-owned
formats unless their grammar and failure policy are deliberately migrated. Begin
with compile-time, developer-owned log messages and bounded destinations.

### Suggested delivery order

1. Land `source_location` diagnostics and the simple C++20 container/string helpers.
2. Add a backward-compatible span layer to packet streams and migrate one packet
   family with unchanged wire goldens.
3. Add typed wire helpers and concepts after the span boundary is established.
4. Migrate clocks and filesystem operations one subsystem at a time.
5. Treat formatting, thread ownership, and raw-pointer ownership as dedicated
   stabilization projects with their own automated tests. Runtime smoke checks
   are optional.

Modules, coroutines, mass ranges rewrites, blanket `char8_t` conversion, and broad
syntax-only modernization are deliberately not priorities. They offer less defect
reduction than the boundary, lifetime, timing, and diagnostics work above.
