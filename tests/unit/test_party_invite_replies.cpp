#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "ClientPlayer.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "GameMessages.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MHelpDef.h"
#include "MParty.h"
#include "PlayerStateHost.h"
#include "SafeFormat.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"
#include "Cpackets/CGPartyInvite.h"
#include "Gpackets/GCPartyInvite.h"
#include <string>
#include <vector>

// A party invitation's answers. Someone invites the player: when the
// player already invited that one, the party is formed (the player
// accepts); when the player invited someone else, the player is busy;
// otherwise the player is asked, unless waiting on something else,
// talking to an NPC, a bat or a wolf, or not taking invitations (then the
// player refuses). The inviter cancelling closes the asking windows. The
// one the player invited accepting forms the party; refusing, being busy,
// being full, or in another party (unless it is the player's own) ends
// the player's wait and says why. A party formed is announced with the
// inviter's name.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
int playerType = 1;
bool lockedInput = false;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;

struct FakeCreature
{
	const char* name;
	bool partyMember;
};
FakeCreature inviter{"Ruth", false}, member{"Ana", true};
int me;

std::string N(long long value) { return std::to_string(value); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Text(int id) { return (*g_pGameStringTable)[id].GetString(); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		return id == 1003 ? reinterpret_cast<MCreature*>(&inviter) : id == 1004 ? reinterpret_cast<MCreature*>(&member) : nullptr;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* p) { return p == reinterpret_cast<MCreature*>(&me) ? playerType : 1; };
	host.IsPlayerParty = [](MCreature* p) { return Fake(p).partyMember; };
	host.GetName = [](MCreature* p) { return Fake(p).name; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.IsLockInput = []() { return lockedInput; };
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.ClosePartyCancel = []() { calls.push_back("cancel? closed"); };
	host.ClosePartyAsk = []() { calls.push_back("ask closed"); };
	host.ClosePartyRequest = []() { calls.push_back("request closed"); };
	host.RunPartyRequest = [](std::uint32_t id) { calls.push_back("join " + N(id) + "?"); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	return host;
}();

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

std::vector<unsigned char> Answer(TYPE_OBJECTID target, int code)
{
	CGPartyInvite answer;
	answer.setTargetObjectID(target);
	answer.setCode(static_cast<BYTE>(code));
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&answer);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

struct World : GameModelWorld
{
	TempInformation temp;
	MParty party;
	CMessageArray gameLog;
	GamePlayer connection;
	TempInformation* oldTemp = g_pTempInformation;
	MParty* oldParty = g_pParty;
	CMessageArray* oldGameLog = g_pGameMessage;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	World()
	{
		InitGameStringTable();
		calls.clear();
		zoneExists = true;
		hasPlayer = true;
		playerType = 1;
		lockedInput = false;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		gameLog.Init(3, 512);
		party.AcceptInvite();
		g_pTempInformation = &temp;
		g_pParty = &party;
		g_pGameMessage = &gameLog;
	}
	~World()
	{
		GameInfoPresentation::SetHost(oldInfo);
		CommunicationPresentation::SetHost(oldCommunication);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pGameMessage = oldGameLog;
		g_pParty = oldParty;
		g_pTempInformation = oldTemp;
	}
	// The player invited this one and waits for the answer.
	void Inviting(TYPE_OBJECTID id)
	{
		waitVerify = PlayerState::WAIT_VERIFY_PARTY;
		temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
		temp.Value1 = id;
		temp.PartyInviter = id;
	}
	std::vector<std::string> Said()
	{
		std::vector<std::string> lines;
		for (int i = 0; i < gameLog.GetSize(); i++)
			if (gameLog[i] != nullptr && gameLog[i][0] != '\0') lines.push_back(gameLog[i]);
		return lines;
	}
};

void Receive(Player* pConnection, TYPE_OBJECTID target, int code)
{
	GCPartyInvite packet;
	packet.setTargetObjectID(target);
	packet.setCode(static_cast<BYTE>(code));
	GCPartyInviteHandler::execute(&packet, pConnection);
}

const std::string kHelp = "help|" + std::to_string(HELP_EVENT_PARTY);

std::string Joined(const char* name)
{
	char line[512];
	SafeFormat::Format(line, GetGameString(STRING_MESSAGE_SOMEONE_JOINED_PARTY), name);
	return line;
}

} // namespace

TEST(PartyInviteReplies, BeingInvitedByTheOneThePlayerInvitedFormsTheParty)
{
	World world;
	world.Inviting(1003);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REQUEST);
	CHECK(calls == std::vector<std::string>({"cancel? closed", kHelp}));
	CHECK(world.connection.Sent() == Answer(1003, CG_PARTY_INVITE_ACCEPT));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// The party-success packet is still awaited.
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_PARTY);
	CHECK(Joined("Ruth").find("Ruth") != std::string::npos);
	CHECK(world.Said() == std::vector<std::string>{Joined("Ruth")});
}

TEST(PartyInviteReplies, BeingInvitedWhileInvitingSomeoneElseIsBusy)
{
	World world;
	world.Inviting(1004);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REQUEST);
	CHECK(calls.empty());
	CHECK(world.connection.Sent() == Answer(1003, CG_PARTY_INVITE_BUSY));
	CHECK(world.temp.GetMode() == TempInformation::MODE_PARTY_REQUEST);
	CHECK(world.Said().empty());
}

TEST(PartyInviteReplies, AnInvitationIsAskedAboutOrRefused)
{
	World world;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REQUEST);
	CHECK(calls == std::vector<std::string>{"join 1003?"});
	CHECK(world.connection.Sent().empty());
	// Each reason to refuse.
	const auto refuses = [&](const char* why) {
		calls.clear();
		GamePlayer connection;
		Receive(&connection, 1003, GC_PARTY_INVITE_REQUEST);
		CHECK(calls.empty());
		CHECK(connection.Sent() == Answer(1003, CG_PARTY_INVITE_REJECT));
		if (!calls.empty() || connection.Sent() != Answer(1003, CG_PARTY_INVITE_REJECT)) calls.push_back(why);
	};
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	refuses("waiting");
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	lockedInput = true;
	refuses("talking");
	lockedInput = false;
	playerType = CREATURETYPE_BAT;
	refuses("bat");
	playerType = CREATURETYPE_WOLF;
	refuses("wolf");
	playerType = 1;
	world.party.RefuseInvite();
	refuses("not taking invitations");
	// Waiting on a party answer, but not in the request mode: asked.
	calls.clear();
	world.party.AcceptInvite();
	waitVerify = PlayerState::WAIT_VERIFY_PARTY;
	GamePlayer asked;
	Receive(&asked, 1003, GC_PARTY_INVITE_REQUEST);
	CHECK(calls == std::vector<std::string>{"join 1003?"});
	CHECK(asked.Sent().empty());
}

TEST(PartyInviteReplies, TheInviterCancellingClosesTheAsking)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_CANCEL);
	CHECK(calls == std::vector<std::string>({"request closed", "ask closed"}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// In another mode it stays.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_CANCEL);
	CHECK(calls == std::vector<std::string>({"request closed", "ask closed"}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
}

TEST(PartyInviteReplies, TheInvitedAcceptingFormsTheParty)
{
	World world;
	world.Inviting(1003);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_ACCEPT);
	CHECK(calls == std::vector<std::string>({"cancel? closed", kHelp}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_PARTY);
	CHECK(world.Said() == std::vector<std::string>{Joined("Ruth")});
	// Not waiting on it: the party is still formed; the mode stays.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_ACCEPT);
	CHECK(calls == std::vector<std::string>({"cancel? closed", kHelp}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_PARTY_REQUEST);
	// An inviter no longer in the zone is not announced.
	calls.clear();
	world.temp.PartyInviter = 4242;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_ACCEPT);
	CHECK_EQ(2, static_cast<int>(world.Said().size()));
}

TEST(PartyInviteReplies, ARefusalEndsTheWaitAndSaysWhy)
{
	World world;
	const struct { int code; int text; } refusals[] = {
		{GC_PARTY_INVITE_REJECT, STRING_MESSAGE_PARTY_REJECTED},
		{GC_PARTY_INVITE_BUSY, STRING_MESSAGE_PARTY_BUSY},
		{GC_PARTY_INVITE_MEMBER_FULL, STRING_MESSAGE_PARTY_FULL}};
	for (const auto& r : refusals)
	{
		calls.clear();
		world.Inviting(1003);
		Receive(&world.connection, 1003, r.code);
		CHECK(calls == std::vector<std::string>({"verified", "cancel? closed", "ask closed", "request closed", "message|" + Text(r.text)}));
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
	// Not waiting on it: the windows close and it is said, the mode stays.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	CHECK(calls == std::vector<std::string>({"cancel? closed", "ask closed", "request closed", "message|" + Text(STRING_MESSAGE_PARTY_REJECTED)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_PARTY_REQUEST);
	CHECK(world.Said().empty());
}

TEST(PartyInviteReplies, AnotherPartyIsSaidUnlessItIsThePlayers)
{
	World world;
	world.Inviting(1003);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_ANOTHER_PARTY);
	CHECK(calls == std::vector<std::string>({"verified", "cancel? closed", "ask closed", "request closed",
		"message|" + Text(STRING_MESSAGE_IN_ANOTHER_PARTY)}));
	// The player's own party member: closed without a word.
	calls.clear();
	world.Inviting(1004);
	Receive(&world.connection, 1004, GC_PARTY_INVITE_ANOTHER_PARTY);
	CHECK(calls == std::vector<std::string>({"verified", "cancel? closed", "ask closed", "request closed"}));
	// Nobody by that id, or an unknown code: nothing.
	calls.clear();
	world.Inviting(4242);
	Receive(&world.connection, 4242, GC_PARTY_INVITE_ANOTHER_PARTY);
	Receive(&world.connection, 1003, GC_PARTY_INVITE_MAX);
	CHECK(calls.empty());
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_PARTY);
}

TEST(PartyInviteReplies, WithoutItsPiecesNothingHappens)
{
	World world;
	world.Inviting(1003);
	zoneExists = false;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	zoneExists = true;
	hasPlayer = false;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	hasPlayer = true;
	g_pTempInformation = nullptr;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	g_pTempInformation = &world.temp;
	g_pParty = nullptr;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	g_pParty = &world.party;
	g_pGameMessage = nullptr;
	Receive(&world.connection, 1003, GC_PARTY_INVITE_REJECT);
	g_pGameMessage = &world.gameLog;
	CHECK(calls.empty());
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_PARTY);
	// Without the hosts, the windows are left alone and input is unlocked.
	CommunicationPresentation::SetHost(nullptr);
	DialogPresentation::SetHost(nullptr);
	CommunicationPresentation::ClosePartyAsk();
	CommunicationPresentation::ClosePartyRequest();
	CommunicationPresentation::RunPartyRequest(1003);
	CHECK(!DialogPresentation::IsLockInput());
	CreatureState::SetHost(nullptr);
	CHECK(!CreatureState::IsPlayerParty(reinterpret_cast<MCreature*>(&member)));
	CHECK(calls.empty());
}
