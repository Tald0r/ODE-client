#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "BonusSkillHost.h"
#include "ClientConfig.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MorphState.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCSkillToInventoryOK1.h"
#include <algorithm>
#include <string>
#include <vector>

// A skill used on an inventory item, as the server confirms it. The
// player's wait ends. When the check buffer holds an item used that way,
// the buffer is cleared and, by the skill:
// - making holy water, a bomb, a mine, a pupa or a holy potion takes one
//   item from the stack (all of it when one is left or the made item
//   goes to its own cell) and puts the made item at the packet's cell, or
//   piles it on the same item there (holy water then numbering the stack
//   the source was);
// - installing a mine, opening a bloody tunnel or transforming makes the
//   item one used from the inventory; a mine refreshes the skills;
// - a bloody mark marks an unmarked portal item with the player's zone
//   and cell;
// - a transformation into a wolf, a bat or a werewolf changes the
//   player's type through its result action, delays the player a second,
//   remembers the type and refreshes the skills.
// The item then shows the skill's effect (the bloody mark's for a
// tunnel). A skill past the action table ends it there. Whatever was in
// the buffer, the skill's result delays the player's effects by the
// packet's duration, the packet's changes apply, and the effect status
// they carry, or else the skill's, is put on the player.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
MStatus playerStatus;
MItem* bufferItem = nullptr;
PlayerState::ITEM_CHECK_BUFFER bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
int me;

std::string N(long long value) { return std::to_string(value); }

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	bool marked = false;		// a portal item's mark
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0) : m_Class(itemClass)
	{
		SetID(id);
		SetItemType(type);
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override { return true; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return 50; }
};

MItemHost items = []() {
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { calls.push_back("made|" + N(itemClass)); return new Ware(itemClass, 0); };
	host.IsPortalMarked = [](const MItem* pPortal) { return static_cast<const Ware*>(pPortal)->marked; };
	host.MarkPortal = [](MItem* pPortal, int zoneID, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("marked|" + N(pPortal->GetID()) + "|" + N(zoneID) + "|" + N(x) + "," + N(y));
		static_cast<Ware*>(pPortal)->marked = true;
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	host.GetItemCheckBuffer = []() { return bufferItem; };
	host.GetItemCheckBufferStatus = []() { return bufferStatus; };
	host.ClearItemCheckBuffer = []() { calls.push_back("cleared"); bufferItem = nullptr; };
	host.SetItemCheckBuffer = [](MItem* pItem, PlayerState::ITEM_CHECK_BUFFER status) {
		calls.push_back("buffer|" + N(pItem->GetID()) + "|" + N(status));
		bufferItem = pItem;
		bufferStatus = status;
	};
	host.UseItemOK = []() { calls.push_back("used"); };
	host.GetID = []() { return TYPE_OBJECTID(7); };
	host.SetDelay = [](std::uint32_t ms) { calls.push_back("player delay|" + N(ms)); };
	host.Status = []() { return &playerStatus; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature*) { return TYPE_SECTORPOSITION(30); };
	host.GetY = [](MCreature*) { return TYPE_SECTORPOSITION(31); };
	host.GetServerX = [](MCreature*) { return TYPE_SECTORPOSITION(40); };
	host.GetServerY = [](MCreature*) { return TYPE_SECTORPOSITION(41); };
	host.GetDirection = [](MCreature*) { return 2; };
	host.SetEffectDelayFrame = [](MCreature*, TYPE_ACTIONINFO ai, std::uint32_t frames) { calls.push_back("delay|" + N(ai) + "|" + N(frames)); };
	host.AddEffectStatus = [](MCreature*, EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int sZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION tX, TYPE_SECTORPOSITION tY, int tZ, std::uint32_t delayFrame, MActionResult* pResult,
		bool bStartFromMainNode, int mainX, int mainY) {
		std::string nodes;
		for (auto it = pResult->GetIterator(); pResult->GetSize() > 0 && nodes.empty(); )
		{
			const auto* pNode = static_cast<const MActionResultNodeChangeCreatureType*>(*it);
			nodes = (*it)->IsNodeChangeCreatureType() ? "|becomes|" + N(pNode->m_TargetID) + "|" + N(pNode->m_CreatureType) : "|other";
		}
		calls.push_back("action|" + N(ai) + "|" + N(sX) + "," + N(sY) + "," + N(sZ) + "|" + N(direction) + "|" + N(targetID) + "|" + N(tX) + ","
			+ N(tY) + "," + N(tZ) + "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|start") + "|" + N(mainX) + "," + N(mainY) + nodes + "|"
			+ N(pResult->GetSize()));
		delete pResult;
	};
	host.AddNewInventoryEffect = [](TYPE_OBJECTID id, TYPE_ACTIONINFO ai, std::uint32_t delayFrame, std::uint32_t value) {
		calls.push_back("shows|" + N(id) + "|" + N(ai) + "|" + N(delayFrame) + "|" + N(value));
	};
	return host;
}();

const BonusSkills::Host skills = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("skills refreshed"); };
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
std::string Shows(TYPE_OBJECTID id, int skill, int delay) { return "shows|" + N(id) + "|" + N(skill + kMin) + "|" + N(delay) + "|0"; }
// The packet's duration, 20, is its frame count at 10 frames a second.
std::string Tail(int skill) { return "delay|" + N(skill + kMin) + "|20"; }

const ITEM_CLASS kClasses[] = {ITEM_CLASS_BOMB_MATERIAL, ITEM_CLASS_HOLYWATER, ITEM_CLASS_BOMB, ITEM_CLASS_MINE, ITEM_CLASS_PUPA,
	ITEM_CLASS_POTION, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, ITEM_CLASS_VAMPIRE_ETC};

struct World : GameModelWorld
{
	MActionInfoTable actions;
	MInventory inventory;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MInventory* oldInventory = g_pInventory;
	const int oldMorph = g_MorphCreatureType;
	const int oldLarge = g_nZoneLarge;
	const int oldSmall = g_nZoneSmall;
	const bool oldInLarge = g_bZonePlayerInLarge;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const BonusSkills::Host* oldSkills = BonusSkills::SetHost(&skills);
	World()
	{
		calls.clear();
		hasPlayer = true;
		bufferItem = nullptr;
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
		g_pClientConfig->FPS = 10;
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
		for (ITEM_CLASS itemClass : kClasses)
		{
			g_pItemTable->InitClass(itemClass, 10);
			for (int type = 0; type < 10; type++)
				testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
		}
		inventory.Init(4, 4);
		g_pInventory = &inventory;
		g_MorphCreatureType = 0;
		g_nZoneLarge = 60;
		g_nZoneSmall = 1007;
		g_bZonePlayerInLarge = true;
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		BonusSkills::SetHost(oldSkills);
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		g_bZonePlayerInLarge = oldInLarge;
		g_nZoneSmall = oldSmall;
		g_nZoneLarge = oldLarge;
		g_MorphCreatureType = oldMorph;
		g_pInventory = oldInventory;
		g_pActionInfoTable = oldActions;
		inventory.Release();
		bufferItem = nullptr;
	}
	// An item in the inventory at the cell, used by the skill.
	Ware* Using(ITEM_CLASS itemClass, TYPE_OBJECTID id, int number, BYTE x = 0, BYTE y = 0, TYPE_ITEMTYPE type = 0)
	{
		Ware* pItem = new Ware(itemClass, id, type);
		pItem->SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
		CHECK(inventory.AddItem(pItem, x, y));
		bufferItem = pItem;
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
		return pItem;
	}
};

struct Made
{
	TYPE_OBJECTID id = 3001;
	TYPE_ITEMTYPE type = 2;
	BYTE x = 1, y = 1;
	int effectStat = -1;
};

void Receive(int skill, const Made& made = {})
{
	GCSkillToInventoryOK1 packet;
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setObjectID(made.id);
	packet.setItemType(made.type);
	packet.setCEffectID(0);
	packet.setDuration(20);
	packet.setX(made.x);
	packet.setY(made.y);
	if (made.effectStat >= 0) packet.addLongData(MODIFY_EFFECT_STAT, static_cast<DWORD>(made.effectStat));
	GCSkillToInventoryOK1Handler::execute(&packet, nullptr);
}

} // namespace

TEST(SkillToInventoryReplies, MakingHolyWaterTakesOneAndNumbersTheMadeStack)
{
	World world;
	MItem* pSource = world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 3);
	Receive(MAGIC_CREATE_HOLY_WATER);
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", "made|" + N(ITEM_CLASS_HOLYWATER), Shows(3001, MAGIC_CREATE_HOLY_WATER, 32),
		Tail(MAGIC_CREATE_HOLY_WATER)}));
	CHECK_EQ(2, static_cast<int>(pSource->GetNumber()));
	const MItem* pMade = world.inventory.GetItem(1, 1);
	CHECK(pMade != nullptr && pMade->GetID() == 3001 && pMade->GetItemType() == 2 && pMade->GetItemClass() == ITEM_CLASS_HOLYWATER);
	// Holy water numbers the made stack as the source's was.
	CHECK(pMade != nullptr && pMade->GetNumber() == 3);
	// Again: it piles on the made stack, numbered as the source was.
	calls.clear();
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2002, 1, 2, 0);
	Receive(MAGIC_CREATE_HOLY_WATER);
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", Shows(3001, MAGIC_CREATE_HOLY_WATER, 32), Tail(MAGIC_CREATE_HOLY_WATER)}));
	CHECK(world.inventory.GetItem(1, 1) == pMade && pMade->GetNumber() == 1);
	// The last one goes from the inventory.
	CHECK(world.inventory.GetItem(2, 0) == nullptr);
}

TEST(SkillToInventoryReplies, MakingItemsPutsThemAtTheCellOrPilesThem)
{
	World world;
	const std::pair<int, ITEM_CLASS> makes[] = {{SKILL_MAKE_BOMB, ITEM_CLASS_BOMB}, {SKILL_MAKE_MINE, ITEM_CLASS_MINE}, {SKILL_ABSORB_SOUL, ITEM_CLASS_PUPA}};
	TYPE_OBJECTID id = 3001;
	for (const auto& [skill, itemClass] : makes)
	{
		calls.clear();
		MItem* pSource = world.Using(ITEM_CLASS_BOMB_MATERIAL, id - 1000, 5, 0, 0);
		Made made;
		made.id = id;
		made.x = static_cast<BYTE>(id - 3000);
		Receive(skill, made);
		CHECK(calls == (std::vector<std::string>{"verified", "cleared", "made|" + N(itemClass), Shows(id, skill, 32), Tail(skill)}));
		const MItem* pMade = world.inventory.GetItem(made.x, 1);
		CHECK(pMade != nullptr && pMade->GetItemClass() == itemClass && pMade->GetNumber() == 1);
		// Again onto the same item: it piles there.
		calls.clear();
		bufferItem = pSource;
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
		Receive(skill, made);
		CHECK(calls == (std::vector<std::string>{"verified", "cleared", Shows(id, skill, 32), Tail(skill)}));
		CHECK(pMade != nullptr && pMade->GetNumber() == 2);
		CHECK_EQ(3, static_cast<int>(pSource->GetNumber()));
		bufferItem = nullptr;
		delete world.inventory.RemoveItem(pSource->GetID());
		id++;
	}
}

TEST(SkillToInventoryReplies, AHolyPotionIsTheSourcesValue3sType)
{
	World world;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BOMB_MATERIAL, 4).Value3 = 6;
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 2, 0, 0, 4);
	Receive(SKILL_CREATE_HOLY_POTION);
	const MItem* pMade = world.inventory.GetItem(1, 1);
	CHECK(pMade != nullptr && pMade->GetItemClass() == ITEM_CLASS_POTION && pMade->GetItemType() == 6 && pMade->GetNumber() == 1);
}

TEST(SkillToInventoryReplies, AMadeItemForItsOwnCellReplacesTheWholeStack)
{
	World world;
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 5, 1, 1);
	Receive(SKILL_MAKE_BOMB);
	const MItem* pMade = world.inventory.GetItem(1, 1);
	CHECK(pMade != nullptr && pMade->GetID() == 3001 && pMade->GetItemClass() == ITEM_CLASS_BOMB && pMade->GetNumber() == 1);
	CHECK_EQ(1, world.inventory.GetItemNum());
}

TEST(SkillToInventoryReplies, OnlyTheSourcesOwnCellReplacesTheStack)
{
	World world;
	// The same column, another row: only one is taken.
	MItem* pSource = world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 5, 1, 0);
	Receive(SKILL_MAKE_BOMB);
	CHECK_EQ(4, static_cast<int>(pSource->GetNumber()));
	CHECK(world.inventory.GetItem(1, 0) == pSource);
	// The same row, another column.
	bufferItem = pSource;
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
	Made made;
	made.x = 2;
	made.y = 0;
	made.id = 3002;
	Receive(SKILL_MAKE_BOMB, made);
	CHECK_EQ(3, static_cast<int>(pSource->GetNumber()));
}

TEST(SkillToInventoryReplies, AMadeItemTheInventoryRefusesIsDropped)
{
	World world;
	MItem* pSource = world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 5);
	Made made;
	made.x = 9;
	made.y = 9;
	Receive(SKILL_MAKE_BOMB, made);
	// The source's item shows the effect; nothing is left of the made one.
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", "made|" + N(ITEM_CLASS_BOMB), Shows(2001, SKILL_MAKE_BOMB, 32),
		Tail(SKILL_MAKE_BOMB)}));
	CHECK_EQ(4, static_cast<int>(pSource->GetNumber()));
	CHECK_EQ(1, world.inventory.GetItemNum());
}

TEST(SkillToInventoryReplies, AnotherItemAtTheCellTakesNothing)
{
	World world;
	MItem* pSource = world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 5);
	// Another ID, another type, another class.
	for (const auto& [made, thereClass] : {std::pair<Made, ITEM_CLASS>{Made{3002}, ITEM_CLASS_BOMB}, {Made{3001, 3}, ITEM_CLASS_BOMB},
		{Made{}, ITEM_CLASS_MINE}})
	{
		Ware* pThere = new Ware(thereClass, 3001, 2);
		CHECK(world.inventory.AddItem(pThere, 1, 1));
		calls.clear();
		bufferItem = pSource;
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
		Receive(SKILL_MAKE_BOMB, made);
		// The source's item shows the effect.
		CHECK(calls == (std::vector<std::string>{"verified", "cleared", Shows(2001, SKILL_MAKE_BOMB, 32), Tail(SKILL_MAKE_BOMB)}));
		CHECK_EQ(1, static_cast<int>(pThere->GetNumber()));
		delete world.inventory.RemoveItem(pThere->GetID());
	}
	CHECK_EQ(2, static_cast<int>(pSource->GetNumber()));
}

TEST(SkillToInventoryReplies, AMineTunnelOrTransformationUsesTheItem)
{
	World world;
	const std::string use = "buffer|2001|" + N(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	world.Using(ITEM_CLASS_MINE, 2001, 1);
	Receive(SKILL_INSTALL_MINE);
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", use, "used", "skills refreshed", Shows(2001, SKILL_INSTALL_MINE, 14),
		Tail(SKILL_INSTALL_MINE)}));
	calls.clear();
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
	Receive(MAGIC_BLOODY_TUNNEL);
	// The tunnel shows the mark's effect.
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", use, "used", Shows(2001, MAGIC_BLOODY_MARK, 14), Tail(MAGIC_BLOODY_TUNNEL)}));
}

TEST(SkillToInventoryReplies, ABloodyMarkMarksAnUnmarkedPortal)
{
	World world;
	Ware* pPortal = world.Using(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 2001, 1);
	Receive(MAGIC_BLOODY_MARK);
	CHECK(calls == (std::vector<std::string>{"verified", "cleared", "marked|2001|60|40,41", Shows(2001, MAGIC_BLOODY_MARK, 14),
		Tail(MAGIC_BLOODY_MARK)}));
	// Marked already, or another item: not marked again.
	calls.clear();
	bufferItem = pPortal;
	g_bZonePlayerInLarge = false;
	Receive(MAGIC_BLOODY_MARK);
	world.Using(ITEM_CLASS_VAMPIRE_ETC, 2002, 1, 1, 0);
	Receive(MAGIC_BLOODY_MARK);
	CHECK(std::count_if(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("marked|", 0) == 0; }) == 0);
	// The small zone is the player's when it is not in the large one.
	pPortal->marked = false;
	calls.clear();
	bufferItem = pPortal;
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
	Receive(MAGIC_BLOODY_MARK);
	CHECK(calls.size() > 2 && calls[2] == "marked|2001|1007|40,41");
}

TEST(SkillToInventoryReplies, ATransformationChangesThePlayersType)
{
	World world;
	const std::pair<int, std::pair<int, int>> turns[] = {{MAGIC_TRANSFORM_TO_WOLF, {RESULT_MAGIC_TRANSFORM_TO_WOLF, CREATURETYPE_WOLF}},
		{MAGIC_TRANSFORM_TO_BAT, {RESULT_MAGIC_TRANSFORM_TO_BAT, CREATURETYPE_BAT}},
		{SKILL_TRANSFORM_TO_WERWOLF, {RESULT_SKILL_TRANSFORM_TO_WERWOLF, CREATURETYPE_WER_WOLF}}};
	for (const auto& [skill, result] : turns)
	{
		calls.clear();
		world.Using(ITEM_CLASS_VAMPIRE_ETC, 2001, 1);
		Receive(skill);
		CHECK(calls == (std::vector<std::string>{"verified", "cleared", "buffer|2001|" + N(PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), "used",
			"action|" + N(result.first) + "|30,31,0|2|" + N(OBJECTID_NULL) + "|30,31,0|0|start|0,0|becomes|7|" + N(result.second) + "|1",
			"player delay|1000", "skills refreshed", Shows(2001, skill, 32), Tail(skill)}));
		CHECK_EQ(result.second, g_MorphCreatureType);
		delete world.inventory.RemoveItem(TYPE_OBJECTID(2001));
		bufferItem = nullptr;
	}
}

TEST(SkillToInventoryReplies, ASkillPastTheActionTableEndsAtTheBuffer)
{
	World world;
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 3);
	Receive(MAX_ACTIONINFO);
	CHECK(calls == (std::vector<std::string>{"verified", "cleared"}));
	CHECK_EQ(1, world.inventory.GetItemNum());
}

TEST(SkillToInventoryReplies, AnotherBufferOnlyDelaysTheEffects)
{
	World world;
	Receive(SKILL_MAKE_BOMB);
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 3);
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY;
	Receive(SKILL_MAKE_BOMB);
	CHECK(calls == (std::vector<std::string>{"verified", Tail(SKILL_MAKE_BOMB), "verified", Tail(SKILL_MAKE_BOMB)}));
	CHECK(bufferItem != nullptr);
	CHECK_EQ(1, world.inventory.GetItemNum());
}

TEST(SkillToInventoryReplies, ThePacketsOrTheSkillsEffectStatusIsPutOn)
{
	World world;
	Made made;
	made.effectStat = EFFECTSTATUS_CURSE_PARALYSIS;
	Receive(SKILL_MAKE_BOMB, made);
	CHECK(calls == (std::vector<std::string>{"verified", Tail(SKILL_MAKE_BOMB), "effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|20"}));
	CHECK(playerStatus.GetEFFECT_STAT() == EFFECTSTATUS_NULL);
	calls.clear();
	testfw::MutableRow(world.actions, SKILL_MAKE_BOMB).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Receive(SKILL_MAKE_BOMB);
	CHECK(calls == (std::vector<std::string>{"verified", Tail(SKILL_MAKE_BOMB), "effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|20"}));
	// Without the status array the skill's is still put on.
	calls.clear();
	PlayerState::Host noStatus = player;
	noStatus.Status = nullptr;
	PlayerState::SetHost(&noStatus);
	Receive(SKILL_MAKE_BOMB, made);
	CHECK(calls == (std::vector<std::string>{"verified", Tail(SKILL_MAKE_BOMB), "effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|20"}));
}

TEST(SkillToInventoryReplies, TheDurationIsConvertedAndThePacketsStatusWins)
{
	World world;
	// At 16 frames a second the duration of 20 is 32 frames.
	g_pClientConfig->FPS = 16;
	testfw::MutableRow(world.actions, SKILL_MAKE_BOMB).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Made made;
	made.effectStat = EFFECTSTATUS_INSTALL_TURRET;
	Receive(SKILL_MAKE_BOMB, made);
	CHECK(calls == (std::vector<std::string>{"verified", "delay|" + N(SKILL_MAKE_BOMB + kMin) + "|32",
		"effect|" + N(EFFECTSTATUS_INSTALL_TURRET) + "|32"}));
}

TEST(SkillToInventoryReplies, WithoutAPlayerNothingHappens)
{
	World world;
	world.Using(ITEM_CLASS_BOMB_MATERIAL, 2001, 3);
	hasPlayer = false;
	Receive(SKILL_MAKE_BOMB);
	CHECK(calls.empty());
	CHECK(bufferItem != nullptr);
}
