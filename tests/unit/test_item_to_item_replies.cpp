#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "TempInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddItemToItemVerify.h"
#include <string>
#include <vector>

// The server's verdict on an item put on an inventory item (an enchant, a
// sex change, a pet's revival, an option cleared or removed, a grade
// raised) or on two items mixed. The item is the one at the inventory
// cell the request remembered (two for a mix), and the request is over.
// A refusal says why, lets the item in hand go back and ends the item
// check; a result shows its effect on the item (an enchant that broke
// the item, an option lost or gained, a sex changed), lets the item in
// hand go, and some sound a chime or say the change worked. A three-way
// enchant changes up to three options, each byte pair of the parameters
// an option's new and old values.
namespace {

std::vector<std::string> calls;
bool playerIsSlayer = true;

std::string N(long long value) { return std::to_string(value); }

struct Sword : MItem
{
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.PopupMessage = [](int id) { calls.push_back("popup|" + N(id)); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.UnlockItem = []() { calls.push_back("unlock"); };
	host.DropItem = []() { calls.push_back("drop"); };
	host.OkMixingForge = [](std::uint32_t parameter, MItem* pItem, MItem* pItem2) {
		calls.push_back("mixed|" + N(parameter) + "|" + N(pItem->GetID()) + "|" + N(pItem2->GetID()));
	};
	host.OkRemoveOption = [](std::uint32_t parameter, MItem* pItem) {
		calls.push_back("option off|" + N(parameter) + "|" + N(pItem->GetID()));
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return reinterpret_cast<MCreature*>(&calls); };
	host.ClearItemCheckBuffer = []() { calls.push_back("check over"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsSlayer = [](MCreature*) { return playerIsSlayer; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.AddNewInventoryEffect = [](TYPE_OBJECTID id, TYPE_ACTIONINFO actionInfo, std::uint32_t delayFrame, std::uint32_t value) {
		calls.push_back("effect|" + N(id) + "|" + N(actionInfo) + "|" + N(delayFrame) + "|" + N(value));
	};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

struct World : GameModelWorld
{
	MInventory inventory;
	TempInformation temp;
	MInventory* oldInventory = g_pInventory;
	TempInformation* oldTemp = g_pTempInformation;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	Sword* pTarget = nullptr;
	Sword* pOther = nullptr;
	World()
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 3);
		for (int type = 0; type < 3; type++)
			testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, type).SetGrid(1, 1);
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		g_pTempInformation = &temp;
		pTarget = Put(501, 2, 3);
		pOther = Put(502, 5, 1);
		calls.clear();
		playerIsSlayer = true;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		DialogPresentation::SetHost(oldDialog);
		CommunicationPresentation::SetHost(oldCommunication);
		inventory.Release();
		g_pTempInformation = oldTemp;
		g_pInventory = oldInventory;
	}
	Sword* Put(ObjectID_t id, int x, int y)
	{
		Sword* pItem = new Sword;
		pItem->SetID(id);
		pItem->SetItemType(1);
		CHECK(inventory.AddItem(pItem, static_cast<BYTE>(x), static_cast<BYTE>(y)));
		return pItem;
	}
	// The request for the item at (2,3).
	void OnTarget()
	{
		temp.SetMode(TempInformation::MODE_ITEM_TO_ITEM);
		temp.Value1 = 2;
		temp.Value2 = 3;
	}
	// The mix of the items at (2,3) and (5,1).
	void Mixing()
	{
		temp.SetMode(TempInformation::MODE_ITEM_MIXING);
		temp.Value1 = 2;
		temp.Value2 = 3;
		temp.Value3 = 5;
		temp.Value4 = 1;
	}
};

void Receive(int code, unsigned parameter = 0, unsigned parameter2 = 0)
{
	GCAddItemToItemVerify packet;
	packet.setCode(static_cast<BYTE>(code));
	packet.setParameter(parameter);
	packet.setParameter2(parameter2);
	GCAddItemToItemVerifyHandler::execute(&packet, nullptr);
}

std::string Effect(int action, unsigned value = 0)
{
	return "effect|501|" + N(action) + "|0|" + N(value);
}

const std::string kChime = "sound|" + std::to_string(SOUND_XMAS_STAR);

} // namespace

TEST(ItemToItemReplies, ARefusalSaysWhyAndLetsTheItemGo)
{
	World world;
	const struct { int code; int text; } refusals[] = {
		{ADD_ITEM_TO_ITEM_VERIFY_ERROR, STRING_ERROR_ETC_ERROR},
		{ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_IMPOSSIBLE, STRING_MESSAGE_ITEM_TO_ITEM_IMPOSIBLE}};
	for (const auto& refusal : refusals)
	{
		calls.clear();
		world.OnTarget();
		Receive(refusal.code);
		CHECK(calls == std::vector<std::string>({"popup|" + N(refusal.text), "unlock", "check over"}));
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
	// Not premium: the race says which message.
	calls.clear();
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_IMPOSSIBLE_NO_PREMIUM);
	playerIsSlayer = false;
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_IMPOSSIBLE_NO_PREMIUM);
	CHECK(calls == std::vector<std::string>({"popup|" + N(STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_SLAYER), "unlock", "check over",
		"popup|" + N(STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_VAMPIRE), "unlock", "check over"}));
	// A mix of one option group, and a sex change refused.
	calls.clear();
	world.Mixing();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_MIXING_FAILED_SAME_OPTION_GROUP);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_TRANS_IMPOSSIBLE);
	Receive(250);
	CHECK(calls == std::vector<std::string>{"popup|" + N(STRING_MESSAGE_MIXING_FORGE_FAILED_SAME_OPTION_GROUP)});
}

TEST(ItemToItemReplies, AnEnchantsResultShowsOnTheItem)
{
	World world;
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_CRASH);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_DECREASE, 77);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_OK, 78);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL);
	CHECK(calls == std::vector<std::string>({
		Effect(MAGIC_ENCHANT_REMOVE_ITEM), kChime, "drop",
		Effect(MAGIC_ENCHANT_OPTION_NULL, 77), kChime, "drop",
		Effect(MAGIC_ENCHANT_OPTION_PLUS, 78), "drop",
		Effect(MAGIC_UNKOWN1), "drop", "unlock", "check over"}));
}

TEST(ItemToItemReplies, AThreeWayEnchantChangesUpToThreeOptions)
{
	World world;
	world.pTarget->AddItemOption(5);
	world.pTarget->AddItemOption(6);
	world.pTarget->AddItemOption(7);
	// Byte pairs, low first: new 15 for old 5, new 16 for old 6; new 17 for
	// old 7.
	const unsigned first = 15u | (5u << 8) | (16u << 16) | (6u << 24);
	const unsigned second = 17u | (7u << 8);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, first, second);
	CHECK(calls == std::vector<std::string>({Effect(MAGIC_ENCHANT_OPTION_PLUS, first), "drop"}));
	const auto& options = world.pTarget->GetItemOptionList();
	CHECK(std::vector<TYPE_ITEM_OPTION>(options.begin(), options.end()) == std::vector<TYPE_ITEM_OPTION>({15, 16, 17}));
	// A new option of 0 changes nothing: only the third changes here.
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, 0u | (15u << 8) | (0u << 16) | (16u << 24), 27u | (17u << 8));
	const auto& after = world.pTarget->GetItemOptionList();
	CHECK(std::vector<TYPE_ITEM_OPTION>(after.begin(), after.end()) == std::vector<TYPE_ITEM_OPTION>({15, 16, 27}));
	// And none changes when all three are 0.
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, 0u | (15u << 8) | (0u << 16) | (16u << 24), 0u | (27u << 8));
	const auto& unchanged = world.pTarget->GetItemOptionList();
	CHECK(std::vector<TYPE_ITEM_OPTION>(unchanged.begin(), unchanged.end()) == std::vector<TYPE_ITEM_OPTION>({15, 16, 27}));
}

TEST(ItemToItemReplies, OtherResultsShowOnTheItemToo)
{
	World world;
	world.pTarget->AddItemOption(5);
	world.pTarget->AddItemOption(6);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_TRANS_OK);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_REVIVAL_OK, 79);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_CLEAR_OPTION_OK, 80);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK, 7);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_REMOVE_OPTION_OK, 5);
	const std::string worked = "popup|" + N(STRING_MESSAGE_SUCCESS_CHANGE);
	CHECK(calls == std::vector<std::string>({
		Effect(MAGIC_TRANS_ITEM_OK), "drop",
		Effect(MAGIC_ENCHANT_OPTION_PLUS, 79), "drop",
		Effect(MAGIC_ENCHANT_OPTION_NULL, 80), kChime, "drop", worked,
		Effect(MAGIC_ENCHANT_OPTION_PLUS), kChime, "drop", worked,
		Effect(MAGIC_ENCHANT_OPTION_PLUS), kChime, "drop", worked}));
	CHECK_EQ(7, world.pTarget->GetGrade());
	const auto& options = world.pTarget->GetItemOptionList();
	CHECK(std::vector<TYPE_ITEM_OPTION>(options.begin(), options.end()) == std::vector<TYPE_ITEM_OPTION>{6});
}

TEST(ItemToItemReplies, AMixOrAnOptionTakenOffGoesToItsWindow)
{
	World world;
	world.temp.pValue = &world;
	world.Mixing();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_MIXING_OK, 33);
	CHECK(calls == std::vector<std::string>{"mixed|33|501|502"});
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(world.temp.Value1 == 0 && world.temp.Value2 == 0 && world.temp.Value3 == 0 && world.temp.Value4 == 0);
	CHECK(world.temp.pValue == nullptr);
	calls.clear();
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_DETACHING_OK, 34);
	CHECK(calls == std::vector<std::string>({"option off|34|501", "drop", "check over"}));
	// An item gone: nothing.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_ITEM_MIXING);
	world.temp.Value1 = 9;
	world.temp.Value2 = 5;
	world.temp.Value3 = 5;
	world.temp.Value4 = 1;
	Receive(ADD_ITEM_TO_ITEM_VERIFY_MIXING_OK, 33);
	world.temp.SetMode(TempInformation::MODE_ITEM_MIXING);
	world.temp.Value1 = 2;
	world.temp.Value2 = 3;
	world.temp.Value3 = 9;
	world.temp.Value4 = 5;
	Receive(ADD_ITEM_TO_ITEM_VERIFY_MIXING_OK, 33);
	world.temp.SetMode(TempInformation::MODE_ITEM_TO_ITEM);
	world.temp.Value1 = 9;
	world.temp.Value2 = 5;
	Receive(ADD_ITEM_TO_ITEM_VERIFY_DETACHING_OK, 34);
	Receive(ADD_ITEM_TO_ITEM_VERIFY_REVIVAL_OK, 79);
	CHECK(calls.empty());
}

TEST(ItemToItemReplies, WithoutTheHostsNothingIsShown)
{
	World world;
	WorldEvents::SetHost(nullptr);
	DialogPresentation::SetHost(nullptr);
	WorldEvents::AddNewInventoryEffect(501, MAGIC_ENCHANT_OPTION_PLUS, 0, 0);
	DialogPresentation::OkMixingForge(1, world.pTarget, world.pOther);
	DialogPresentation::OkRemoveOption(1, world.pTarget);
	world.OnTarget();
	Receive(ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_OK, 78);
	CHECK(calls.empty());
}

TEST(ItemToItemReplies, AResultForAnItemNoLongerThereShowsNothing)
{
	World world;
	// No request remembered, or its cell empty: each result about the item
	// does nothing, as the revival and the option taken off always did.
	const int results[] = {ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_CRASH, ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_DECREASE,
		ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_OK, ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK, ADD_ITEM_TO_ITEM_VERIFY_TRANS_OK,
		ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL, ADD_ITEM_TO_ITEM_VERIFY_CLEAR_OPTION_OK, ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK,
		ADD_ITEM_TO_ITEM_REMOVE_OPTION_OK};
	for (int result : results)
	{
		Receive(result, 0x0F05u, 0x0F07u);
		world.temp.SetMode(TempInformation::MODE_ITEM_TO_ITEM);
		world.temp.Value1 = 9;
		world.temp.Value2 = 5;
		Receive(result, 0x0F05u, 0x0F07u);
	}
	CHECK(calls.empty());
}
