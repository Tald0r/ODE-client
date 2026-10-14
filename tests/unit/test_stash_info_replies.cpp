#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCOtherModifyInfo.h"
#include "Gpackets/GCStashSell.h"
#include <string>
#include <vector>

// The storage price an NPC asks answers the player's question to it: a
// price of 0 means no more storage can be bought, so the talk box comes
// back with a message saying so; any other price asks whether to buy one
// and keeps the price for the answer. Another creature's changed status
// is applied to it, and when the other-info window shows that creature
// (by name) and it is of the player's race, the window shows it again.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool zoneExists = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
const char* otherInfoName = nullptr;

struct FakeCreature
{
	const char* name;
	int race;
	MStatus status;
};
FakeCreature other{"Lina", RACE_SLAYER, {}};
int me;

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string N(long long value) { return std::to_string(value); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	host.GetRace = []() { return RACE_SLAYER; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) { return id == 1003 ? Handle(other) : nullptr; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.Status = [](MCreature* p) { return &Fake(p).status; };
	host.GetName = [](MCreature* p) { return Fake(p).name; };
	host.GetRace = [](MCreature* p) { return Fake(p).race; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.ShowPCTalkDlg = []() { calls.push_back("talk box again"); };
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	host.RunStorageBuy = [](int price) { calls.push_back("buy storage?|" + N(price)); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.GetOtherInfoName = []() { return otherInfoName; };
	host.RunOtherInfo = [](MCreature* p) { calls.push_back(std::string("other info|") + Fake(p).name); };
	return host;
}();

struct World : GameModelWorld
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	World()
	{
		InitGameStringTable();
		calls.clear();
		hasPlayer = true;
		zoneExists = true;
		waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
		otherInfoName = nullptr;
		other = FakeCreature{"Lina", RACE_SLAYER, {}};
		g_pTempInformation = &temp;
	}
	~World()
	{
		GameInfoPresentation::SetHost(oldInfo);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
	}
};

void ReceivePrice(Gold_t price)
{
	GCStashSell packet;
	packet.setPrice(price);
	GCStashSellHandler::execute(&packet, nullptr);
}

void ReceiveOtherStatus(ObjectID_t id, int hp)
{
	GCOtherModifyInfo original;
	original.setObjectID(id);
	original.addShortData(MODIFY_CURRENT_HP, static_cast<WORD>(hp));
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCOtherModifyInfo packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCOtherModifyInfoHandler::execute(&packet, nullptr);
}

long long HitPoints(FakeCreature& creature) { return static_cast<long long>(creature.status.GetStatus(MODIFY_CURRENT_HP)); }

} // namespace

TEST(StashInfoReplies, AStoragePriceAsksWhetherToBuyAndKeepsThePrice)
{
	World world;
	ReceivePrice(5000);
	CHECK(calls == std::vector<std::string>({"verified", "buy storage?|5000"}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_STORAGE_BUY);
	CHECK_EQ(5000, static_cast<long long>(world.temp.Value1));
}

TEST(StashInfoReplies, APriceOfNothingSaysNoMoreCanBeBought)
{
	World world;
	ReceivePrice(0);
	CHECK(calls == std::vector<std::string>({"verified", "talk box again",
		std::string("message|") + (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_MORE].GetString()}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(StashInfoReplies, APriceNobodyAskedForOrWithoutAPlayerIsIgnored)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	ReceivePrice(5000);
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	hasPlayer = false;
	ReceivePrice(5000);
	ReceivePrice(0);
	CHECK(calls.empty());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_NPC_ASK);
}

TEST(StashInfoReplies, AnotherCreaturesStatusChangesItAndRefreshesItsWindow)
{
	World world;
	ReceiveOtherStatus(1003, 40);
	CHECK_EQ(40, HitPoints(other));
	CHECK(calls.empty());
	// The window shows it, and it is of the player's race.
	otherInfoName = "Lina";
	ReceiveOtherStatus(1003, 30);
	CHECK_EQ(30, HitPoints(other));
	CHECK(calls == std::vector<std::string>{"other info|Lina"});
	// A creature without a name is not the window's.
	calls.clear();
	other.name = nullptr;
	ReceiveOtherStatus(1003, 25);
	CHECK(calls.empty());
	other.name = "Lina";
	// Another race, another name, or no player: the window is left alone.
	calls.clear();
	other.race = RACE_VAMPIRE;
	ReceiveOtherStatus(1003, 20);
	other.race = RACE_SLAYER;
	otherInfoName = "Lin";
	ReceiveOtherStatus(1003, 10);
	otherInfoName = "Lina";
	hasPlayer = false;
	ReceiveOtherStatus(1003, 5);
	CHECK(calls.empty());
	CHECK_EQ(5, HitPoints(other));
}

TEST(StashInfoReplies, AStatusForNobodyInTheZoneIsIgnored)
{
	World world;
	const long long untouched = HitPoints(other);
	otherInfoName = "Lina";
	ReceiveOtherStatus(4242, 40);
	zoneExists = false;
	ReceiveOtherStatus(1003, 40);
	CHECK(calls.empty());
	CHECK_EQ(untouched, HitPoints(other));
	// Without the creature's status, nothing is applied, but the window
	// still follows.
	zoneExists = true;
	const CreatureState::Host nameOnly = []() {
		CreatureState::Host host{};
		host.GetName = [](MCreature* p) { return Fake(p).name; };
		host.GetRace = [](MCreature* p) { return Fake(p).race; };
		return host;
	}();
	CreatureState::SetHost(&nameOnly);
	ReceiveOtherStatus(1003, 40);
	CHECK_EQ(untouched, HitPoints(other));
	CHECK(calls == std::vector<std::string>{"other info|Lina"});
}

TEST(StashInfoReplies, WithoutHostsTheWindowsAreLeftAlone)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	GameInfoPresentation::SetHost(nullptr);
	CHECK(GameInfoPresentation::GetOtherInfoName() == nullptr);
	GameInfoPresentation::RunOtherInfo(Handle(other));
	ReceivePrice(5000);
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	ReceivePrice(0);
	const GameInfoPresentation::Host bare{};
	const DialogPresentation::Host bareDialog{};
	GameInfoPresentation::SetHost(&bare);
	DialogPresentation::SetHost(&bareDialog);
	CHECK(GameInfoPresentation::GetOtherInfoName() == nullptr);
	GameInfoPresentation::RunOtherInfo(Handle(other));
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	ReceivePrice(5000);
	waitVerify = PlayerState::WAIT_VERIFY_NPC_ASK;
	ReceivePrice(0);
	// Each price still ends the wait.
	CHECK(calls == std::vector<std::string>(4, "verified"));
}

TEST(StashInfoReplies, AnEmptyMessageStringStillReadsAsText)
{
	World world;
	// An empty entry holds NULL; the dialog's message box takes its length.
	g_pGameStringTable->Set(STRING_MESSAGE_CANNOT_BUY_MORE, "");
	ReceivePrice(0);
	CHECK(calls == std::vector<std::string>({"verified", "talk box again", "message|"}));
}
