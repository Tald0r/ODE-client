//----------------------------------------------------------------------
// fuzz_client_login_stream.cpp
//----------------------------------------------------------------------
//
// Fuzz target: the bytes the login server sends the client over TCP,
// read through ClientPlayer::processCommand's gates by
// client_stream_reader.h (whose header lists them, the input format,
// the two passes over the input ring and what counts as a finding).
//
// The client talks to the login server through the same ClientPlayer
// (g_pSocket, created in GameInit.cpp) that later reconnects to a game
// server, so the receive path is the game target's; what differs is
// the player status, and with it the ids the validator accepts. On
// the login connection the player is in one of seven statuses, each
// set by the client right after the request it answers
// (UIMessageManager.cpp and the LC handlers), and each accepting a
// handful of LC ids (PacketValidator::init):
//
//   CPS_AFTER_SENDING_CL_LOGIN            version check, login, world
//                                         and server lists, name query
//   CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID  LCQueryResultPlayerID
//   CPS_AFTER_SENDING_CL_REGISTER_PLAYER  version check, registration
//   CPS_AFTER_SENDING_CL_GET_PC_LIST      LCPCList, server list, name
//                                         query
//   CPS_AFTER_SENDING_CL_CREATE_PC        creation, name query
//   CPS_AFTER_SENDING_CL_DELETE_PC        deletion, name query
//   CPS_AFTER_SENDING_CL_SELECT_PC        LCReconnect, selection
//                                         error, name query
//
// Every input is read once in each of them. CPS_NONE and
// CPS_BEGIN_SESSION, the statuses before CLLogin, are left out: their
// sets are PIST_NONE, so every frame is refused before a parser runs.
// All seven sets are PIST_NORMAL, so the reader's IgnorePacketException
// catch is never taken here. An id outside the status's set ends the
// stream as a protocol error, so a GC frame reaches no parser here.
//
// The encrypt code byte is faithful for this connection too: a fresh
// ClientPlayer's stream starts at code 0 (Encrypter's constructor),
// and after GCReconnectLogin the same player returns to the login
// server still holding the game session's code.
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_login_stream under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

#include "client_stream_reader.h"

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
	ClientStreamFuzz::Initialize();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	for (PlayerStatus status : kLoginStatuses)
		ClientStreamFuzz::ReadInput(data, size, status);
	return 0;
}
