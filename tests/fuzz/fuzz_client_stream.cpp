//----------------------------------------------------------------------
// fuzz_client_stream.cpp
//----------------------------------------------------------------------
//
// Fuzz target: the bytes a game server sends the client over TCP, read
// through ClientPlayer::processCommand's gates by client_stream_reader.h
// (whose header lists them, the input format, the two passes over the
// input ring and what counts as a finding).
//
// The player is in CPS_NORMAL, the status of a session in the game:
// its validator set is PIST_ANY, so no id is refused there and every
// registered parser is reachable. The game connection's other statuses
// (CPS_AFTER_SENDING_CG_CONNECT, CPS_WAITING_FOR_GC_SET_POSITION,
// CPS_WAITING_FOR_GC_RECONNECT_LOGIN) accept fewer ids, never more.
// The encrypt code is the one GCUpdateInfoHandler sets after the zone
// loads, any byte.
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_stream under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

#include "client_stream_reader.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	ClientStreamFuzz::Initialize();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	ClientStreamFuzz::ReadInput(data, size, CPS_NORMAL);
	return 0;
}
