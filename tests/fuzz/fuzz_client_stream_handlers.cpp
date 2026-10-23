//----------------------------------------------------------------------
// fuzz_client_stream_handlers.cpp
//----------------------------------------------------------------------
//
// Fuzz target: the bytes a game server sends the client over TCP, read
// by the client's own receive loop and dispatched to the packet's
// handler, in a world handler_world.h builds (whose header says what it
// holds, what is left out, how a run is reset and what counts as a
// finding). fuzz_client_stream.cpp reads the same bytes and stops
// before dispatch.
//
// The player is in CPS_NORMAL, as fuzz_client_stream.cpp's is: every
// registered id is accepted, so every handler the registry binds is
// reachable. Each input runs three times, the player a slayer, a
// vampire and an ousters, each in a world of its own, since many
// handlers take a branch per race.
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_stream_handlers under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

#include "handler_world.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	HandlerFuzz::Initialize();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	for (int race = 0; race < HandlerFuzz::RACE_RUN_MAX; race++)
		HandlerFuzz::Run(data, size, CPS_NORMAL, HandlerFuzz::Race(race), true);
	return 0;
}
