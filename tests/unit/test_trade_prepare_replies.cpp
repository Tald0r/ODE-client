#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "TempInformation.h"
#include "TradePresentationHost.h"
#include "ZoneStateHost.h"
#include "Cpackets/CGTradePrepare.h"
#include "Gpackets/GCTradePrepare.h"
#include <string>
#include <vector>

// A trade's preparation. Someone asks the player to trade: when the
// player already asked that one, the exchange opens (the player accepts);
// when the player asked someone else, the player is busy; otherwise the
// player is asked. The player then faces the one asking, if the zone
// knows it. A player waiting on something else, repeating an action,
// talking to an NPC, a bat or a wolf refuses whatever comes. The one
// asking cancelling closes the exchange window, or the asking one. The
// one the player asked accepting opens the exchange; refusing or being
// busy ends the player's wait, closes the windows and says why.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
bool repeating = false;
bool lockedInput = false;
bool exchangeOpen = false;
int playerType = 1;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;

int me;
int trader;

std::string N(long long value) { return std::to_string(value); }
MCreature* Me() { return reinterpret_cast<MCreature*>(&me); }
std::string Who(MCreature* p) { return p == Me() ? std::string("me") : p == reinterpret_cast<MCreature*>(&trader) ? std::string("1003") : "?"; }
std::string Text(int id) { return (*g_pGameStringTable)[id].GetString(); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Me() : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	host.IsRepeatAction = []() { return repeating; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) { return id == 1003 ? reinterpret_cast<MCreature*>(&trader) : nullptr; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* p) { return p == Me() ? playerType : 1; };
	host.GetX = [](MCreature* p) { return static_cast<TYPE_SECTORPOSITION>(p == Me() ? 40 : 0); };
	host.GetY = [](MCreature* p) { return static_cast<TYPE_SECTORPOSITION>(p == Me() ? 41 : 0); };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("faces|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.IsLockInput = []() { return lockedInput; };
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	return host;
}();

const TradePresentation::Host trade = []() {
	TradePresentation::Host host{};
	host.IsRunningExchange = []() { return exchangeOpen; };
	host.CloseExchange = []() { calls.push_back("exchange closed"); };
	host.CloseExchangeAsk = []() { calls.push_back("ask closed"); };
	host.CloseExchangeCancel = []() { calls.push_back("cancel? closed"); };
	host.RunExchange = [](std::uint32_t id) { calls.push_back("exchange with " + N(id)); };
	host.RunExchangeAsk = [](std::uint32_t id) { calls.push_back("trade with " + N(id) + "?"); };
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
	CGTradePrepare answer;
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
	GamePlayer connection;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const TradePresentation::Host* oldTrade = TradePresentation::SetHost(&trade);
	World()
	{
		InitGameStringTable();
		calls.clear();
		zoneExists = true;
		hasPlayer = true;
		repeating = false;
		lockedInput = false;
		exchangeOpen = false;
		playerType = 1;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		g_pTempInformation = &temp;
	}
	~World()
	{
		TradePresentation::SetHost(oldTrade);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
	}
	// The player asked this one to trade and waits for the answer.
	void Asking(TYPE_OBJECTID id)
	{
		waitVerify = PlayerState::WAIT_VERIFY_TRADE;
		temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		temp.Value1 = id;
	}
};

void Receive(Player* pConnection, TYPE_OBJECTID target, int code)
{
	GCTradePrepare packet;
	packet.setTargetObjectID(target);
	packet.setCode(static_cast<BYTE>(code));
	GCTradePrepareHandler::execute(&packet, pConnection);
}

} // namespace

TEST(TradePrepareReplies, BeingAskedByTheOneThePlayerAskedOpensTheExchange)
{
	World world;
	world.Asking(1003);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls == std::vector<std::string>({"exchange with 1003", "faces|1003|40,41"}));
	CHECK(world.connection.Sent() == Answer(1003, CG_TRADE_PREPARE_CODE_ACCEPT));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
}

TEST(TradePrepareReplies, BeingAskedWhileAskingSomeoneElseIsBusy)
{
	World world;
	world.Asking(1004);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls.empty());
	CHECK(world.connection.Sent() == Answer(1003, CG_TRADE_PREPARE_CODE_BUSY));
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
}

TEST(TradePrepareReplies, BeingAskedOtherwiseAsksThePlayer)
{
	World world;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls == std::vector<std::string>({"trade with 1003?", "faces|1003|40,41"}));
	CHECK(world.connection.Sent().empty());
	// In the request mode but not waiting on a trade, or waiting on one in
	// another mode: asked.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	world.temp.Value1 = 1003;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	world.temp.SetMode(TempInformation::MODE_NULL);
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls == std::vector<std::string>({"trade with 1003?", "faces|1003|40,41", "trade with 1003?", "faces|1003|40,41"}));
	CHECK(world.connection.Sent().empty());
	// One the zone does not know, or no zone: asked, but not faced.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	Receive(&world.connection, 1009, GC_TRADE_PREPARE_CODE_REQUEST);
	zoneExists = false;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls == std::vector<std::string>({"trade with 1009?", "trade with 1003?"}));
}

TEST(TradePrepareReplies, ABusyPlayerRefusesWhateverComes)
{
	World world;
	auto refuses = [&](int code) {
		calls.clear();
		GamePlayer connection;
		Receive(&connection, 1003, code);
		return calls.empty() && connection.Sent() == Answer(1003, CG_TRADE_PREPARE_CODE_REJECT);
	};
	waitVerify = PlayerState::WAIT_VERIFY_PARTY;
	CHECK(refuses(GC_TRADE_PREPARE_CODE_REQUEST));
	CHECK(refuses(GC_TRADE_PREPARE_CODE_CANCEL));
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	repeating = true;
	CHECK(refuses(GC_TRADE_PREPARE_CODE_REQUEST));
	repeating = false;
	lockedInput = true;
	CHECK(refuses(GC_TRADE_PREPARE_CODE_ACCEPT));
	lockedInput = false;
	playerType = CREATURETYPE_BAT;
	CHECK(refuses(GC_TRADE_PREPARE_CODE_REQUEST));
	playerType = CREATURETYPE_WOLF;
	CHECK(refuses(GC_TRADE_PREPARE_CODE_REQUEST));
	// Waiting on the trade itself is no reason to refuse.
	playerType = 1;
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	CHECK(!refuses(GC_TRADE_PREPARE_CODE_REQUEST));
}

TEST(TradePrepareReplies, CancellingClosesTheExchangeOrTheAsking)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_CANCEL);
	CHECK(calls == std::vector<std::string>{"ask closed"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	calls.clear();
	exchangeOpen = true;
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_CANCEL);
	CHECK(calls == std::vector<std::string>{"exchange closed"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_PARTY_REQUEST);
	CHECK(world.connection.Sent().empty());
}

TEST(TradePrepareReplies, AcceptingOpensTheExchange)
{
	World world;
	world.Asking(1003);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_ACCEPT);
	CHECK(calls == std::vector<std::string>{"exchange with 1003"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
	// Not asking anyone: nothing.
	calls.clear();
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_ACCEPT);
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_ACCEPT);
	CHECK(calls.empty());
	CHECK(world.connection.Sent().empty());
}

TEST(TradePrepareReplies, RefusingOrBeingBusyEndsTheWaitAndSaysWhy)
{
	World world;
	world.Asking(1003);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REJECT);
	CHECK(calls == std::vector<std::string>({"verified", "cancel? closed", "message|" + Text(STRING_MESSAGE_TRADE_REJECTED)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_NULL);
	calls.clear();
	world.Asking(1003);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_BUSY);
	CHECK(calls == std::vector<std::string>({"verified", "ask closed", "cancel? closed", "message|" + Text(STRING_MESSAGE_TRADE_BUSY)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// Not asking anyone: nothing, and nothing for a code past the last.
	calls.clear();
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REJECT);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_BUSY);
	world.Asking(1003);
	world.temp.SetMode(TempInformation::MODE_NULL);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REJECT);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_BUSY);
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REJECT);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_BUSY);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_MAX);
	CHECK(calls.empty());
	CHECK(world.connection.Sent().empty());
	CHECK(Text(STRING_MESSAGE_TRADE_REJECTED) != Text(STRING_MESSAGE_TRADE_BUSY));
}

TEST(TradePrepareReplies, WithoutAPlayerOrTheTradeRegisterNothingHappens)
{
	World world;
	world.Asking(1003);
	hasPlayer = false;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REJECT);
	hasPlayer = true;
	g_pTempInformation = nullptr;
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls.empty());
	CHECK(world.connection.Sent().empty());
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_TRADE);
	// Without the trade host the windows are skipped and the answer still
	// goes.
	g_pTempInformation = &world.temp;
	TradePresentation::SetHost(nullptr);
	Receive(&world.connection, 1003, GC_TRADE_PREPARE_CODE_REQUEST);
	CHECK(calls == std::vector<std::string>{"faces|1003|40,41"});
	CHECK(world.connection.Sent() == Answer(1003, CG_TRADE_PREPARE_CODE_ACCEPT));
}
