#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCPartyError.h"
#include <string>
#include <vector>

// A refused party request says why. When the invited player is not there,
// is of another race, or the place is not a safety zone, the request this
// player made ends: its "cancel?" box closes, its wait is answered and the
// temp mode clears. The other refusals close that box too, except when the
// server says it is not inviting, which shows nothing, and when there is no
// right to expel or an unknown error, which only say so.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
int playerHandle = 0;

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&playerHandle) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.ClosePartyCancel = []() { calls.push_back("closecancel"); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back("popup|" + std::string(message)); };
	return host;
}();

struct World : GameModelWorld
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		g_pTempInformation = &temp;
		calls.clear();
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		CommunicationPresentation::SetHost(oldCommunication);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
	}
};

void Receive(BYTE code)
{
	GCPartyError original;
	original.setCode(code);
	original.setTargetObjectID(77);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCPartyError packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCPartyErrorHandler::execute(&packet, nullptr);
}

std::string Popup(int stringID) { return "popup|" + std::string((*g_pGameStringTable)[stringID].GetString()); }

} // namespace

TEST(PartyErrorHandler, AnImpossibleInviteEndsThePendingRequestThenSaysWhy)
{
	const struct { BYTE code; int stringID; } rows[] = {
		{GC_PARTY_ERROR_TARGET_NOT_EXIST, STRING_MESSAGE_PARTY_NOBODY},
		{GC_PARTY_ERROR_RACE_DIFFER, STRING_MESSAGE_RACE_DIFFER},
		{GC_PARTY_ERROR_NOT_SAFE, STRING_MESSAGE_PARTY_SAFETY_ZONE_ONLY},
	};
	for (const auto& row : rows)
	{
		World world;
		world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
		waitVerify = PlayerState::WAIT_VERIFY_PARTY;
		Receive(row.code);
		CHECK(calls == std::vector<std::string>({"closecancel", "verified", "closecancel", Popup(row.stringID)}));
		CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	}
}

TEST(PartyErrorHandler, WithoutAPendingRequestOnlyTheBoxClosesAndTheReasonShows)
{
	World world;
	// Another request's mode, and another wait, stay.
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	waitVerify = PlayerState::WAIT_VERIFY_PARTY;
	Receive(GC_PARTY_ERROR_TARGET_NOT_EXIST);
	CHECK(calls == std::vector<std::string>({"closecancel", Popup(STRING_MESSAGE_PARTY_NOBODY)}));
	CHECK_EQ(static_cast<int>(TempInformation::MODE_TRADE_REQUEST), static_cast<int>(world.temp.GetMode()));
	// A pending party request with another wait clears the mode only.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Receive(GC_PARTY_ERROR_RACE_DIFFER);
	CHECK(calls == std::vector<std::string>({"closecancel", "closecancel", Popup(STRING_MESSAGE_RACE_DIFFER)}));
	CHECK_EQ(static_cast<int>(PlayerState::WAIT_VERIFY_TRADE), static_cast<int>(waitVerify));
}

TEST(PartyErrorHandler, TheOtherRefusalsKeepTheRequestAndSayWhy)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(GC_PARTY_ERROR_NOT_NORMAL_FORM);
	Receive(GC_PARTY_ERROR_ALREADY_INVITING);
	Receive(GC_PARTY_ERROR_NOT_INVITING);
	Receive(GC_PARTY_ERROR_NO_AUTHORITY);
	Receive(GC_PARTY_ERROR_UNKNOWN);
	Receive(GC_PARTY_ERROR_MAX);
	CHECK(calls == std::vector<std::string>({"closecancel", Popup(STRING_MESSAGE_NOT_NORMAL_FORM), "closecancel",
		Popup(STRING_MESSAGE_PARTY_BUSY), Popup(STRING_MESSAGE_NO_AUTHORITY), Popup(STRING_MESSAGE_ERROR_PARTY)}));
	CHECK_EQ(static_cast<int>(TempInformation::MODE_PARTY_REQUEST), static_cast<int>(world.temp.GetMode()));
}

TEST(PartyErrorHandler, NothingHappensWithoutAPlayerOrTheTempModeAndMissingServicesSkipTheUI)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	hasPlayer = false;
	Receive(GC_PARTY_ERROR_TARGET_NOT_EXIST);
	hasPlayer = true;
	g_pTempInformation = nullptr;
	Receive(GC_PARTY_ERROR_TARGET_NOT_EXIST);
	g_pTempInformation = &world.temp;
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_PARTY_REQUEST), static_cast<int>(world.temp.GetMode()));
	CommunicationPresentation::SetHost(nullptr);
	DialogPresentation::SetHost(nullptr);
	Receive(GC_PARTY_ERROR_TARGET_NOT_EXIST);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
}
