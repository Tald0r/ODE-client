#include "test_framework.h"
#include "gamemodel_world.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "MItemManager.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCCannotUse.h"
#include <string>
#include <vector>

// A refused item use ends what the player was waiting on. Waiting on a
// love chain, it ends that wait when the refused item is the couple ring
// the player wears (by race), lifting the chain and saying the partner
// cannot be found. Otherwise it ends the item check: an item used on
// another says the option could not be removed (but for a code sheet);
// one used from the inventory says why where the item has a reason (a
// guild tree, a mixing item, which also closes the forge, a sex-change
// dye); one used from the quick slot unlocks the gear; a couple ring used
// from the gear says the partner cannot be found.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
Race race = RACE_SLAYER;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
PlayerState::ITEM_CHECK_BUFFER checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
MItem* checkItem = nullptr;
MItemManager* gear = nullptr;

int me;
MCreature* const player = reinterpret_cast<MCreature*>(&me);

std::string N(long long value) { return std::to_string(value); }
std::string Text(int id) { return (*g_pGameStringTable)[id].GetString(); }

struct Goods : MItem
{
	ITEM_CLASS m_Class;
	Goods(ITEM_CLASS itemClass, TYPE_OBJECTID id, int type = 0) : m_Class(itemClass)
	{
		SetID(id);
		SetItemType(static_cast<TYPE_ITEMTYPE>(type));
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? player : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	host.GetRace = []() { return race; };
	host.GetGear = []() { return gear; };
	host.GetItemCheckBuffer = []() { return checkItem; };
	host.GetItemCheckBufferStatus = []() { return checkStatus; };
	host.ClearItemCheckBuffer = []() { calls.push_back("buffer cleared"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.RemoveEffectStatus = [](MCreature* p, EFFECTSTATUS status) {
		calls.push_back(std::string(p == player ? "player" : "other") + " loses|" + N(status));
		return true;
	};
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.PopupMessage = [](int id) { calls.push_back("popup|" + N(id)); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.UnlockGear = []() { calls.push_back("gear unlocked"); };
	host.CloseMixingForge = []() { calls.push_back("forge closed"); };
	return host;
}();

struct World : GameModelWorld
{
	CMessageArray systemLog;
	TempInformation temp;
	MItemManager playerGear;
	CMessageArray* oldSystem = g_pSystemMessage;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		systemLog.Init(3, 512);
		g_pSystemMessage = &systemLog;
		g_pTempInformation = &temp;
		calls.clear();
		hasPlayer = true;
		race = RACE_SLAYER;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		checkItem = nullptr;
		gear = &playerGear;
	}
	~World()
	{
		gear = nullptr;
		DialogPresentation::SetHost(oldDialog);
		CommunicationPresentation::SetHost(oldCommunication);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
		g_pSystemMessage = oldSystem;
	}
	// The system messages from oldest to newest, empty rows left out.
	std::vector<std::string> System()
	{
		std::vector<std::string> lines;
		for (int i = 0; i < systemLog.GetSize(); i++)
			if (systemLog[i] != nullptr && systemLog[i][0] != '\0') lines.push_back(systemLog[i]);
		return lines;
	}
	// The item in the check buffer, under that status.
	void Checking(PlayerState::ITEM_CHECK_BUFFER status, MItem* pItem)
	{
		checkStatus = status;
		checkItem = pItem;
	}
};

void Receive(TYPE_OBJECTID id)
{
	GCCannotUse packet;
	packet.setObjectID(id);
	GCCannotUseHandler::execute(&packet, nullptr);
}

const std::string kLoveChainEnds = "player loses|" + N(EFFECTSTATUS_LOVE_CHAIN);

} // namespace

TEST(CannotUseReplies, ARefusedCoupleRingEndsTheLoveChainWait)
{
	World world;
	Goods other(ITEM_CLASS_COUPLE_RING, 0);
	world.playerGear.AddItem(new Goods(ITEM_CLASS_VAMPIRE_COUPLE_RING, 30));
	world.playerGear.AddItem(new Goods(ITEM_CLASS_COUPLE_RING, 31));
	waitVerify = PlayerState::WAIT_VERIFY_LOVE_CHAIN;
	// The check buffer is left alone.
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT, &other);
	Receive(31);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "verified", kLoveChainEnds}));
	CHECK(world.System() == std::vector<std::string>{Text(STRING_MESSAGE_COUPLE_CAN_NOT_FIND)});
	// A vampire wears the vampire ring; an ouster the slayer one.
	calls.clear();
	race = RACE_VAMPIRE;
	Receive(30);
	race = RACE_OUSTERS;
	Receive(31);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "verified", kLoveChainEnds,
		"buffer cleared", "verified", kLoveChainEnds}));
	CHECK_EQ(3, static_cast<int>(world.System().size()));
}

TEST(CannotUseReplies, AnotherItemLeavesTheLoveChainWait)
{
	World world;
	world.playerGear.AddItem(new Goods(ITEM_CLASS_VAMPIRE_COUPLE_RING, 30));
	world.playerGear.AddItem(new Goods(ITEM_CLASS_COUPLE_RING, 31));
	waitVerify = PlayerState::WAIT_VERIFY_LOVE_CHAIN;
	Goods sword(ITEM_CLASS_SWORD, 40);
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT, &sword);
	// Another item, the other race's ring: nothing, not even the check.
	Receive(40);
	Receive(30);
	race = RACE_VAMPIRE;
	Receive(31);
	CHECK(calls.empty());
	CHECK(world.System().empty());
	// No ring worn: nothing.
	race = RACE_SLAYER;
	world.playerGear.Release();
	Receive(31);
	CHECK(calls.empty());
}

TEST(CannotUseReplies, AnItemUsedOnAnotherCouldNotRemoveTheOption)
{
	World world;
	Goods stone(ITEM_CLASS_EVENT_STAR, 40);
	world.temp.SetMode(TempInformation::MODE_ITEM_MIXING);
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_ITEM_TO_ITEM, &stone);
	Receive(40);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "popup|" + N(STRING_MESSAGE_FAILED_REMOVE_OPTION)}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// A code sheet says nothing.
	calls.clear();
	Goods sheet(ITEM_CLASS_CODE_SHEET, 41);
	world.temp.SetMode(TempInformation::MODE_ITEM_MIXING);
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_ITEM_TO_ITEM, &sheet);
	Receive(41);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(CannotUseReplies, AnItemUsedFromTheInventorySaysWhyWhereItHasAReason)
{
	World world;
	const struct { ITEM_CLASS itemClass; int type; TYPE_OBJECTID packetID; std::vector<std::string> says; } cases[] = {
		{ITEM_CLASS_EVENT_TREE, 26, 0, {"popup|" + N(STRING_MESSAGE_USE_GUILD_MEMBER_ONLY)}},
		{ITEM_CLASS_EVENT_TREE, 28, 0, {"popup|" + N(STRING_MESSAGE_USE_GUILD_MEMBER_ONLY)}},
		{ITEM_CLASS_EVENT_TREE, 25, 0, {}},
		{ITEM_CLASS_EVENT_TREE, 29, 0, {}},
		{ITEM_CLASS_MIXING_ITEM, 0, 0, {"forge closed", "popup|" + N(UI_STRING_MESSAGE_FAIL_MERGE_ITEM)}},
		{ITEM_CLASS_DYE_POTION, 48, 1, {"popup|" + N(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_WEAR)}},
		{ITEM_CLASS_DYE_POTION, 48, 2, {"popup|" + N(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_COUPLE)}},
		{ITEM_CLASS_DYE_POTION, 48, 3, {"popup|" + N(STRING_ERROR_ETC_ERROR)}},
		{ITEM_CLASS_DYE_POTION, 47, 1, {}},
		{ITEM_CLASS_POTION, 48, 1, {}}};
	for (const auto& c : cases)
	{
		calls.clear();
		Goods item(c.itemClass, 40, c.type);
		world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, &item);
		Receive(c.packetID);
		std::vector<std::string> expected = c.says;
		expected.push_back("buffer cleared");
		CHECK(calls == expected);
	}
}

TEST(CannotUseReplies, EachOtherUseEndsTheCheck)
{
	World world;
	Goods ring(ITEM_CLASS_COUPLE_RING, 40);
	Goods vampireRing(ITEM_CLASS_VAMPIRE_COUPLE_RING, 41);
	Goods sword(ITEM_CLASS_SWORD, 42);
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT, &sword);
	Receive(42);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "gear unlocked"}));
	// A couple ring used from the gear: the partner cannot be found.
	calls.clear();
	for (MItem* pItem : {static_cast<MItem*>(&ring), static_cast<MItem*>(&vampireRing), static_cast<MItem*>(&sword)})
	{
		world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR, pItem);
		Receive(pItem->GetID());
	}
	CHECK(calls == std::vector<std::string>(3, "buffer cleared"));
	CHECK(world.System() == std::vector<std::string>(2, Text(STRING_MESSAGE_COUPLE_CAN_NOT_FIND)));
	calls.clear();
	for (PlayerState::ITEM_CHECK_BUFFER status : {PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY,
		PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE})
	{
		world.Checking(status, &sword);
		Receive(42);
	}
	CHECK(calls == std::vector<std::string>(2, "buffer cleared"));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
}

TEST(CannotUseReplies, AnotherCheckOrNoneIsLeftAlone)
{
	World world;
	Goods sword(ITEM_CLASS_SWORD, 42);
	for (PlayerState::ITEM_CHECK_BUFFER status : {PlayerState::ITEM_CHECK_BUFFER_DROP_TO_ZONE,
		PlayerState::ITEM_CHECK_BUFFER_DROP_TO_INVENTORY, PlayerState::ITEM_CHECK_BUFFER_NULL})
	{
		world.Checking(status, &sword);
		Receive(42);
	}
	// An empty check buffer, whatever its status.
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY, nullptr);
	Receive(42);
	CHECK(calls.empty());
	// Without a player, nothing, though the refused ring is worn.
	hasPlayer = false;
	world.playerGear.AddItem(new Goods(ITEM_CLASS_COUPLE_RING, 42));
	waitVerify = PlayerState::WAIT_VERIFY_LOVE_CHAIN;
	world.Checking(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT, &sword);
	Receive(42);
	CHECK(calls.empty());
	CHECK(world.System().empty());
}

TEST(CannotUseReplies, WithoutADialogHostTheGearAndForgeAreLeftAlone)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	DialogPresentation::UnlockGear();
	DialogPresentation::CloseMixingForge();
	const DialogPresentation::Host bare{};
	DialogPresentation::SetHost(&bare);
	DialogPresentation::UnlockGear();
	DialogPresentation::CloseMixingForge();
	CHECK(calls.empty());
}
