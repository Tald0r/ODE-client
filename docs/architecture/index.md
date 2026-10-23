# Rules

* [Class splits](class-splits.md) - A class split across a library and the executable moves whole, unless it has no virtuals or the library constructs none of the split classes.
* [Host interfaces](host-interfaces.md) - A library reaches the executable only through a designated host; a reader guards the pointer and re-reads it every call, a missing host answers with the value a missing config would give, and an installer is designated-initialised.
* [Library boundaries](library-boundaries.md) - Only code compiled into a static library links into a test binary; membership is the four tests/arch lists CMake reads, the executable's own source is SDLMain.cpp alone, and a new target names its sources, never a glob.
* [The vendored de-core](decore.md) - third_party/decore is a byte-identical copy of the server's src/domain, resynced with tools/decore/sync.pl and never edited by hand; a shared formula proves parity of the formula, not of the gameplay around it.

# Designs

* [The gamemodel library](gamemodel.md) - The game model extracted from the executable, every packet handler under Client/PacketHandler among it, each piece reaching the running game through a named host; membership is tests/arch/gamemodel_files.txt.
* [The gameview and gameclient libraries](gameview-and-gameclient.md) - gameview draws the model with SpriteLib over the packs MTopView hands it; gameclient puts the game together, from GameInitInfo to ClientMain, each piece behind a host the executable installs.
* [The packetwire library](packetwire.md) - Every .cpp under Client/Packet is compiled once as packetwire, the wire layer that reaches the program around it only through WireHost.h's entries the executable installs in GameInit.cpp.

# Reference

* [Client traps](client-traps.md) - The build and runtime facts that have cost time to rediscover: _DEBUG is live in every Debug build, an unlisted DLL exits before main, 5:5:5 sprites are latent, sprite rejection is silent, and grep stops at the NUL bytes in VS_UI_GameCommon.cpp.
