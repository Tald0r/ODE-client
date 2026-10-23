//----------------------------------------------------------------------
// fuzz_client_login_stream_handlers.cpp
//----------------------------------------------------------------------
//
// Fuzz target: the bytes the login server sends the client over TCP,
// read by the client's own receive loop and dispatched to the packet's
// handler, in a world handler_world.h builds (whose header says what it
// holds, what is left out, how a run is reset and what counts as a
// finding). fuzz_client_login_stream.cpp reads the same bytes and stops
// before dispatch.
//
// The world is the one the login screen runs in: the objects
// InitGameObject made at start-up and no zone yet. Every input is read
// once from each of the seven statuses the player holds on the login
// connection, as fuzz_client_login_stream.cpp reads it (its header
// lists them and the ids each accepts); here a handler that moves the
// player to another status moves the rest of the stream with it, as in
// the client.
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_login_stream_handlers under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

#include "handler_world.h"

#include <cstddef>
#include <cstdint>

namespace {

const PlayerStatus	kLoginStatuses[] = {
	CPS_AFTER_SENDING_CL_LOGIN,
	CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID,
	CPS_AFTER_SENDING_CL_REGISTER_PLAYER,
	CPS_AFTER_SENDING_CL_GET_PC_LIST,
	CPS_AFTER_SENDING_CL_CREATE_PC,
	CPS_AFTER_SENDING_CL_DELETE_PC,
	CPS_AFTER_SENDING_CL_SELECT_PC,
};

} // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	HandlerFuzz::Initialize();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	for (PlayerStatus status : kLoginStatuses)
		HandlerFuzz::Run(data, size, status, HandlerFuzz::RACE_RUN_SLAYER, false);
	return 0;
}
