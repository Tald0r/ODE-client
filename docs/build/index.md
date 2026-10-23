# Guides

* [Build commands](build-commands.md) - The two Windows Debug trees and why neither subsumes the other, the first configure's vcpkg toolchain, and the presets and verification scripts every platform's full check runs.

# Rules

* [Reading build output](reading-build-output.md) - A clean Debug build has warnings only from third-party code and the Windows SDK, so a warning in project code is new and fails its preset's budget; judge a build by error C####, LNK, MSB or fatal error, never by grepping for "error".
* [CI](ci.md) - The four workflows run on master pushes and manual dispatch only, never on a pull request, so a change is verified locally and dispatched by hand; the verification scripts assert the required ctests by name.

# Designs

* [Linux and macOS port assessment](linux-macos-port-assessment-2026-09-07.md) - What has to change for the DarkEden executable to build and run off Windows, in what order, and how much of it there is, by area.

# Reports

* [Compiler-warning cleanup, 2026-09-27](compiler-warnings-2026-09-27.md) - How about 6,100 project warning sites were cleared under Apple Clang, GCC, Clang and MSVC, what the budget still counts, and the defects found on the way.
