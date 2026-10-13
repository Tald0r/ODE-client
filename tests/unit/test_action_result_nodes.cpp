#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MInventory.h"
#include "MItemOptionTable.h"
#include "MorphState.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "SoundDef.h"
#include "VsUiPresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <vector>

// The results a skill reply queues, run when the action reaches them: a
// skill's result on its target, a creature or the player moved, the
// player's status, a death, a burrow, (in)visibility, snipping, an effect
// status, a creature-type change, a casket, a turn, and an inventory item
// removed, re-optioned or re-gendered. The player is the creature whose ID
// the player host gives; creatures are fakes behind opaque handles.
namespace {

std::vector<std::string> calls;
int alive = 0;

std::string N(long long value) { return std::to_string(value); }

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	bool underground = false;
	bool male = true;
	bool inCasket = false;
	std::uint16_t color1 = 11;
	std::uint16_t color2 = 22;
	TYPE_SECTORPOSITION serverX = 0;
	TYPE_SECTORPOSITION serverY = 0;
};

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
std::string Who(MCreature* pCreature) { return Fake(pCreature).name; }
void Record(MCreature* pCreature, const std::string& what) { calls.push_back(Who(pCreature) + "." + what); }

bool zoneExists = true;
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
FakeCreature* self = nullptr;

// The item the player holds while the server checks its use.
MItem* held = nullptr;
bool heldOnItem = false;

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

MStatus playerStatus;

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return self != nullptr ? Handle(*self) : nullptr; };
	host.GetID = []() { return self != nullptr ? self->id : OBJECTID_NULL; };
	host.Status = []() { return &playerStatus; };
	host.SetServerPosition = [](TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { calls.push_back("player.server|" + N(sX) + "|" + N(sY)); };
	host.SetUndergroundCreature = []() { calls.push_back("player.burrow"); };
	host.IsItemCheckBufferItemToItem = []() { return heldOnItem; };
	host.GetItemCheckBuffer = []() { return held; };
	host.ClearItemCheckBuffer = []() {
		calls.push_back("player.clearheld");
		held = nullptr;
		heldOnItem = false;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.SetDead = [](MCreature* p) { Record(p, "dead"); };
	host.SetStop = [](MCreature* p) { Record(p, "stop"); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "move|" + N(x) + "|" + N(y));
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "server|" + N(sX) + "|" + N(sY)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + (grade != 0 ? "|g" + N(grade) : ""));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetTurning = [](MCreature* p, std::uint16_t count) { Record(p, "turn|" + N(count)); };
	host.ReleaseMoveBuffer = [](MCreature* p) { Record(p, "release"); };
	host.SetVisible = [](MCreature* p) { Record(p, "visible"); };
	host.SetInvisible = [](MCreature* p) { Record(p, "invisible"); };
	host.IsUndergroundCreature = [](MCreature* p) { return Fake(p).underground; };
	host.SetUndergroundCreature = [](MCreature* p) { Record(p, "burrow"); };
	host.SetFlyingCreature = [](MCreature* p) { Record(p, "fly"); };
	host.SetGroundCreature = [](MCreature* p) { Record(p, "ground"); };
	host.SetCreatureType = [](MCreature* p, int type) {
		Record(p, "type|" + N(type));
		// A new type resets the gender and colors, as MCreature's does.
		Fake(p).male = false;
		Fake(p).color1 = Fake(p).color2 = 0;
	};
	host.IsMale = [](MCreature* p) { return Fake(p).male; };
	host.SetMale = [](MCreature* p, bool bMale) { Record(p, bMale ? "male" : "female"); };
	host.GetBodyColor1 = [](MCreature* p) { return Fake(p).color1; };
	host.GetBodyColor2 = [](MCreature* p) { return Fake(p).color2; };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t colorSet) { Record(p, "color1|" + N(colorSet)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t colorSet) { Record(p, "color2|" + N(colorSet)); };
	host.GetServerX = [](MCreature* p) { return Fake(p).serverX; };
	host.GetServerY = [](MCreature* p) { return Fake(p).serverY; };
	host.IsInCasket = [](MCreature* p) { return Fake(p).inCasket; };
	host.AddCasket = [](MCreature* p, int casketType) { Record(p, "casket|" + N(casketType)); };
	host.RemoveCasket = [](MCreature* p) { Record(p, "uncasket"); };
	host.SetBloodyZenith = [](MCreature* p, int delay) { Record(p, "zenith|" + N(delay)); };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.SelectSkill = [](int skillID) { calls.push_back("select|" + N(skillID)); };
	host.UnselectSkill = []() { calls.push_back("unselect"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	explicit Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0) : m_Class(itemClass)
	{
		alive++;
		SetID(id);
		SetItemType(type);
	}
	~Ware() { alive--; }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

const MItemHost items = []() {
	MItemHost host{};
	host.RefreshAffect = [](MItem* pItem) { calls.push_back("affect|" + N(pItem->GetID())); };
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
const std::string kFanfare = "sound|" + N(SOUND_EVENT_FANFARE);

struct World : GameModelWorld
{
	FakeCreature ogre, vanda, me;
	MActionInfoTable actions;
	CREATURE_TABLE creatureTypes;
	MInventory inventory;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	MInventory* oldInventory = g_pInventory;
	int oldMorph = g_MorphCreatureType;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World() : GameModelWorld(8)
	{
		ogre = {1001, "Ogre"};
		vanda = {1002, "Vanda"};
		me = {7, "Self"};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}};
		self = &me;
		zoneExists = true;
		calls.clear();
		alive = 0;
		held = nullptr;
		heldOnItem = false;
		playerStatus = MStatus();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
		creatureTypes.Init(CREATURETYPE_INSTALL_TURRET + 1);
		testfw::MutableRow(creatureTypes, CREATURETYPE_BAT).bFlyingCreature = true;
		g_pCreatureTable = &creatureTypes;
		for (ITEM_CLASS itemClass : {ITEM_CLASS_SWORD, ITEM_CLASS_PET_ITEM})
		{
			g_pItemTable->InitClass(itemClass, 3);
			for (int type = 0; type < 3; type++)
				testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
		}
		inventory.Init(4, 4);
		g_pInventory = &inventory;
		g_MorphCreatureType = 0;
	}
	~World()
	{
		inventory.Release();
		MItem::SetHost(nullptr);
		delete held;
		held = nullptr;
		WorldEvents::SetHost(oldEvents);
		VsUiPresentation::SetHost(oldUi);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_MorphCreatureType = oldMorph;
		g_pInventory = oldInventory;
		g_pCreatureTable = oldCreatureTypes;
		g_pActionInfoTable = oldActions;
		creatures.clear();
		self = nullptr;
	}
	// An item in the inventory; the item host is installed after it is
	// added, so adding it records nothing.
	Ware* Stock(ITEM_CLASS itemClass, TYPE_OBJECTID id, BYTE x, TYPE_ITEMTYPE type = 0)
	{
		Ware* ware = new Ware(itemClass, id, type);
		MItem::SetHost(nullptr);
		CHECK(inventory.AddItem(ware, x, 0));
		MItem::SetHost(&items);
		return ware;
	}
	// What the player holds for the check, being used on an item.
	static Ware* Hold(ITEM_CLASS itemClass, TYPE_ITEMTYPE type)
	{
		Ware* ware = new Ware(itemClass, 900, type);
		held = ware;
		heldOnItem = true;
		return ware;
	}
};

void Run(MActionResultNode&& node)
{
	node.Execute();
}

} // namespace

TEST(ActionResultNodes, ASkillLandsAsItsResultOnTheTarget)
{
	World world;
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 1001, 40, 41, 30));
	CHECK(calls == std::vector<std::string>({"Ogre.delay|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|30",
		"Ogre.result|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|1001|40|41"}));
	// Without its user it still lands.
	calls.clear();
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 4242, 1001, 40, 41, 30));
	CHECK_EQ(2u, calls.size());
}

TEST(ActionResultNodes, ThreeSkillsLandAsAnotherSkillsResult)
{
	World world;
	const struct { TYPE_ACTIONINFO skill; TYPE_ACTIONINFO lands; } rows[] = {
		{SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW},
		{SKILL_ILLENDUE, SKILL_LIGHT_BALL},
		{SKILL_BLAZE_WALK, SKILL_BLAZE_WALK_ATTACK},
	};
	for (const auto& row : rows)
	{
		calls.clear();
		Run(MActionResultNodeActionInfo(row.skill, 1002, 1001, 1, 2, 0));
		CHECK(calls == std::vector<std::string>({"Ogre.delay|" + N(row.lands + kMin) + "|0",
			"Ogre.result|" + N(row.lands + kMin) + "|1001|1|2"}));
	}
}

TEST(ActionResultNodes, BloodyZenithMarksItsUserAndBlitzSlidingLandsOnTheUser)
{
	World world;
	Run(MActionResultNodeActionInfo(MAGIC_BLOODY_ZENITH, 1002, 1001, 1, 2, 5));
	CHECK(calls == std::vector<std::string>({"Ogre.delay|" + N(MAGIC_BLOODY_ZENITH + kMin) + "|5", "Vanda.zenith|8",
		"Ogre.result|" + N(MAGIC_BLOODY_ZENITH + kMin) + "|1001|1|2"}));
	calls.clear();
	Run(MActionResultNodeActionInfo(SKILL_BLITZ_SLIDING_ATTACK, 1002, 1001, 3, 4, 5));
	CHECK(calls == std::vector<std::string>({"Ogre.delay|" + N(SKILL_BLITZ_SLIDING_ATTACK + kMin) + "|5",
		"Vanda.result|" + N(SKILL_BLITZ_SLIDING_ATTACK + kMin) + "|1001|3|4"}));
	// Without its user, Blitz Sliding only delays the target.
	calls.clear();
	Run(MActionResultNodeActionInfo(SKILL_BLITZ_SLIDING_ATTACK, 4242, 1001, 3, 4, 5));
	CHECK(calls == std::vector<std::string>{"Ogre.delay|" + N(SKILL_BLITZ_SLIDING_ATTACK + kMin) + "|5"});
}

TEST(ActionResultNodes, AGradedSkillLandsWithItsStep)
{
	World world;
	testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT).SetUseActionGrade();
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 1001, 1, 2, 0, 3));
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 1001, 1, 2, 0, 0));
	// An ungraded skill ignores a step.
	Run(MActionResultNodeActionInfo(SKILL_SINGLE_BLOW, 1002, 1001, 1, 2, 0, 3));
	const std::string impact = N(SKILL_DOUBLE_IMPACT + kMin);
	CHECK(calls == std::vector<std::string>({"Ogre.delay|" + impact + "|0", "Ogre.result|" + impact + "|1001|1|2|g3",
		"Ogre.delay|" + impact + "|0", "Ogre.result|" + impact + "|1001|1|2",
		"Ogre.delay|" + N(SKILL_SINGLE_BLOW + kMin) + "|0", "Ogre.result|" + N(SKILL_SINGLE_BLOW + kMin) + "|1001|1|2"}));
}

TEST(ActionResultNodes, ASkillWithoutATargetZoneOrResultRowDoesNothing)
{
	World world;
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 4242, 1, 2, 0));
	Run(MActionResultNodeActionInfo(static_cast<TYPE_ACTIONINFO>(MAX_ACTIONINFO - kMin), 1002, 1001, 1, 2, 0));
	zoneExists = false;
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 1001, 1, 2, 0));
	CHECK(calls.empty());
}

TEST(ActionResultNodes, APositionChangeMovesThePlayerOrACreature)
{
	World world;
	Run(MActionResultNodeChangePosition(7, 12, 13));
	CHECK(calls == std::vector<std::string>({"Self.move|12|13", "player.server|12|13"}));
	calls.clear();
	Run(MActionResultNodeChangePosition(1001, 14, 15));
	CHECK(calls == std::vector<std::string>({"Ogre.release", "Ogre.move|14|15", "Ogre.server|14|15"}));
	calls.clear();
	Run(MActionResultNodeChangePosition(4242, 1, 1));
	zoneExists = false;
	Run(MActionResultNodeChangePosition(7, 1, 1));
	zoneExists = true;
	self = nullptr;
	Run(MActionResultNodeChangePosition(1001, 1, 1));
	CHECK(calls.empty());
}

TEST(ActionResultNodes, AStatusChangeAppliesToThePlayerOnly)
{
	World world;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	playerStatus.SetStatus(MODIFY_MAX_HP, 50);
	auto Changed = [](std::uint32_t hp) {
		MStatus* status = new MStatus;
		status->SetStatus(MODIFY_CURRENT_HP, hp);
		return status;
	};
	Run(MActionResultNodeChangeStatus(1001, Changed(20)));
	CHECK_EQ(10, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
	// Only the values the change carries are applied.
	Run(MActionResultNodeChangeStatus(7, Changed(30)));
	CHECK_EQ(30, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
	CHECK_EQ(50, static_cast<long long>(playerStatus.GetStatus(MODIFY_MAX_HP)));
	self = nullptr;
	Run(MActionResultNodeChangeStatus(7, Changed(40)));
	Run(MActionResultNodeChangeStatus(7, nullptr));
	CHECK_EQ(30, static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)));
}

TEST(ActionResultNodes, TheSimpleCreatureResultsReachThePlayerOrTheCreature)
{
	World world;
	Run(MActionResultNodeCreatureDie(7));
	Run(MActionResultNodeCreatureDie(1001));
	Run(MActionResultNodeCreatureBurrow(7));
	Run(MActionResultNodeCreatureBurrow(1001));
	Run(MActionResultNodeCreatureInvisible(7));
	Run(MActionResultNodeCreatureInvisible(1001));
	Run(MActionResultNodeCreatureVisible(7));
	Run(MActionResultNodeCreatureVisible(1001));
	Run(MActionResultNodeCreatureSnipping(7));
	Run(MActionResultNodeCreatureSnipping(1001));
	Run(MActionResultNodeCreatureTurning(7, 9));
	Run(MActionResultNodeCreatureTurning(1001, 6));
	Run(MActionResultNodeAddEffectStatus(7, EFFECTSTATUS_CURSE_PARALYSIS, 40));
	Run(MActionResultNodeAddEffectStatus(1001, EFFECTSTATUS_CURSE_PARALYSIS, 41));
	const std::string snipping = "effect|" + N(EFFECTSTATUS_SNIPPING_MODE) + "|65535";
	const std::string paralysis = "effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|";
	CHECK(calls == std::vector<std::string>({"Self.dead", "Ogre.dead", "player.burrow", "Ogre.burrow",
		"Self.invisible", "Ogre.invisible", "Self.visible", "Ogre.visible",
		"Self." + snipping, "Self.invisible", "Ogre." + snipping, "Ogre.invisible",
		"Self.turn|9", "Ogre.turn|6", "Self." + paralysis + "40", "Ogre." + paralysis + "41"}));
}

TEST(ActionResultNodes, TheSimpleCreatureResultsNeedAZoneAPlayerAndTheCreature)
{
	World world;
	auto RunAll = [](TYPE_OBJECTID id) {
		Run(MActionResultNodeCreatureDie(id));
		Run(MActionResultNodeCreatureBurrow(id));
		Run(MActionResultNodeCreatureInvisible(id));
		Run(MActionResultNodeCreatureVisible(id));
		Run(MActionResultNodeCreatureSnipping(id));
		Run(MActionResultNodeCreatureTurning(id, 1));
		Run(MActionResultNodeAddEffectStatus(id, EFFECTSTATUS_CURSE_PARALYSIS, 1));
		Run(MActionResultNodeChangeCreatureType(id, CREATURETYPE_BAT));
		Run(MActionResultNodeSummonCasket(id, 1));
		Run(MActionResultNodeOpenCasket(id));
		Run(MActionResultNodeFakeDie(id));
		Run(MActionResultNodeEvent(1));
	};
	RunAll(4242);
	zoneExists = false;
	RunAll(7);
	RunAll(1001);
	CHECK(calls.empty());
	// Without a player only the effect status and the caskets, which do not
	// ask for one, still land.
	zoneExists = true;
	self = nullptr;
	world.ogre.inCasket = true;
	RunAll(1001);
	CHECK(calls == std::vector<std::string>({"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|1", "Ogre.uncasket"}));
}

TEST(ActionResultNodes, ACreatureTypeChangeKeepsTheGenderAndColors)
{
	World world;
	Run(MActionResultNodeChangeCreatureType(1001, CREATURETYPE_BAT));
	Run(MActionResultNodeChangeCreatureType(1002, CREATURETYPE_WOLF));
	// A werewolf takes its second color as its first.
	world.ogre.male = false;
	world.ogre.color1 = 33;
	world.ogre.color2 = 44;
	Run(MActionResultNodeChangeCreatureType(1001, CREATURETYPE_WER_WOLF));
	CHECK(calls == std::vector<std::string>({
		"Ogre.type|" + N(CREATURETYPE_BAT), "Ogre.male", "Ogre.color1|11", "Ogre.color2|22", "Ogre.fly",
		"Vanda.type|" + N(CREATURETYPE_WOLF), "Vanda.male", "Vanda.color1|11", "Vanda.color2|22", "Vanda.ground",
		"Ogre.type|" + N(CREATURETYPE_WER_WOLF), "Ogre.female", "Ogre.color1|44", "Ogre.color2|44", "Ogre.ground"}));
	// A buried creature keeps its type.
	calls.clear();
	world.vanda.underground = true;
	Run(MActionResultNodeChangeCreatureType(1002, CREATURETYPE_BAT));
	CHECK(calls.empty());
}

TEST(ActionResultNodes, ThePlayersNewFormPicksItsSkillAndEndsTheMorph)
{
	World world;
	const struct { int type; int skill; } rows[] = {
		{CREATURETYPE_BAT, MAGIC_UN_TRANSFORM},
		{CREATURETYPE_WOLF, MAGIC_EAT_CORPSE},
		{CREATURETYPE_WER_WOLF, MAGIC_UN_TRANSFORM},
		{CREATURETYPE_INSTALL_TURRET, MAGIC_UN_TRANSFORM},
		{1, ACTIONINFO_NULL},
	};
	for (const auto& row : rows)
	{
		calls.clear();
		g_MorphCreatureType = 77;
		world.me.underground = true;
		Run(MActionResultNodeChangeCreatureType(7, row.type));
		CHECK(calls == std::vector<std::string>{row.skill == ACTIONINFO_NULL ? "unselect" : "select|" + N(row.skill)});
		CHECK_EQ(0, g_MorphCreatureType);
	}
	// Another creature's change leaves the morph.
	g_MorphCreatureType = 77;
	Run(MActionResultNodeChangeCreatureType(1001, CREATURETYPE_BAT));
	CHECK_EQ(77, g_MorphCreatureType);
}

TEST(ActionResultNodes, ACasketClosesOnAStoppedCreatureAndOpens)
{
	World world;
	world.ogre.serverX = 31;
	world.ogre.serverY = 32;
	Run(MActionResultNodeSummonCasket(1001, 2));
	Run(MActionResultNodeOpenCasket(1001));
	// One already in its casket stays, and only it opens.
	world.vanda.inCasket = true;
	Run(MActionResultNodeSummonCasket(1002, 2));
	Run(MActionResultNodeOpenCasket(1002));
	CHECK(calls == std::vector<std::string>({"Ogre.stop", "Ogre.move|31|32", "Ogre.casket|2", "Vanda.uncasket"}));
}

TEST(ActionResultNodes, ARemovedInventoryItemIsFreedWithTheHeldItem)
{
	World world;
	world.Stock(ITEM_CLASS_SWORD, 501, 0);
	world.Hold(ITEM_CLASS_SWORD, 0);
	CHECK_EQ(2, alive);
	Run(MActionResultNodeRemoveItemInInventory(501));
	CHECK(world.inventory.GetItem(static_cast<TYPE_OBJECTID>(501)) == nullptr);
	CHECK(calls == std::vector<std::string>{"player.clearheld"});
	CHECK_EQ(0, alive);
	// A held item not used on another item stays held.
	world.Stock(ITEM_CLASS_SWORD, 502, 0);
	world.Hold(ITEM_CLASS_SWORD, 0);
	heldOnItem = false;
	Run(MActionResultNodeRemoveItemInInventory(502));
	CHECK(held != nullptr);
	CHECK_EQ(1, alive);
}

TEST(ActionResultNodes, AnOptionChangeChangesAddsOrUpgradesOptions)
{
	World world;
	testfw::MutableRow(*g_pItemOptionTable, 3).UpgradeOptionType = 5;
	Ware* sword = world.Stock(ITEM_CLASS_SWORD, 501, 0);
	sword->AddItemOption(3);
	// 3 becomes 5, its upgrade: the fanfare plays, and the held item goes.
	world.Hold(ITEM_CLASS_SWORD, 0);
	Run(MActionResultNodeChangeItemOptionInInventory(501, (3 << 8) | 5));
	CHECK(sword->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({5}));
	CHECK(calls == std::vector<std::string>({"player.clearheld", kFanfare}));
	// The low word, then the high word: 1 is added, 5 becomes 6; no upgrade.
	calls.clear();
	Run(MActionResultNodeChangeItemOptionInInventory(501, (((5u << 8) | 6u) << 16) | 1u));
	CHECK(sword->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({6, 1}));
	CHECK(calls.empty());
	CHECK_EQ(1, alive);
}

TEST(ActionResultNodes, TheHeldItemDecidesAPetsOrAnItemsChange)
{
	World world;
	Ware* pet = world.Stock(ITEM_CLASS_PET_ITEM, 601, 0);
	Ware* sword = world.Stock(ITEM_CLASS_SWORD, 602, 1);
	// Revival sets the pet's life.
	world.Hold(ITEM_CLASS_PET_ENCHANT_ITEM, 13);
	Run(MActionResultNodeChangeItemOptionInInventory(601, 90));
	CHECK_EQ(90, static_cast<long long>(pet->GetCurrentDurability()));
	world.Hold(ITEM_CLASS_PET_ENCHANT_ITEM, 14);
	Run(MActionResultNodeChangeItemOptionInInventory(601, 95));
	CHECK_EQ(95, static_cast<long long>(pet->GetCurrentDurability()));
	// The second enchant gives the pet one option.
	pet->AddItemOption(2);
	world.Hold(ITEM_CLASS_PET_ENCHANT_ITEM, 15);
	Run(MActionResultNodeChangeItemOptionInInventory(601, 4));
	CHECK(pet->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({4}));
	// The first: the attribute type and level, and the color reset.
	world.Hold(ITEM_CLASS_PET_ENCHANT_ITEM, 1);
	Run(MActionResultNodeChangeItemOptionInInventory(601, (3u << 16) | 7u));
	CHECK_EQ(3, static_cast<long long>(pet->GetEnchantLevel()));
	CHECK_EQ(7, pet->GetSilver());
	CHECK_EQ(0xFFFF, static_cast<long long>(pet->GetItemColorSet()));
	// A mixing item clears any item's options; a yellow drop sets its grade.
	sword->AddItemOption(2);
	world.Hold(ITEM_CLASS_MIXING_ITEM, 9);
	Run(MActionResultNodeChangeItemOptionInInventory(602, 0));
	CHECK(sword->GetItemOptionList().empty());
	world.Hold(ITEM_CLASS_EVENT_STAR, 23);
	Run(MActionResultNodeChangeItemOptionInInventory(602, 6));
	CHECK_EQ(6, sword->GetGrade());
	CHECK(calls == std::vector<std::string>({"player.clearheld", kFanfare, "player.clearheld", kFanfare,
		"player.clearheld", kFanfare, "player.clearheld", kFanfare, "player.clearheld", kFanfare,
		"player.clearheld", kFanfare}));
	CHECK_EQ(2, alive);
}

TEST(ActionResultNodes, AnOptionOrGenderChangeOnNoItemOnlyUsesTheHeldItem)
{
	World world;
	world.Hold(ITEM_CLASS_SWORD, 0);
	Run(MActionResultNodeChangeItemOptionInInventory(4242, 5));
	world.Hold(ITEM_CLASS_SWORD, 0);
	Run(MActionResultNodeChangeItemGenderInInventory(4242));
	CHECK(calls == std::vector<std::string>({"player.clearheld", "player.clearheld"}));
	CHECK_EQ(0, alive);
	// Without an inventory the held item stays.
	g_pInventory = nullptr;
	world.Hold(ITEM_CLASS_SWORD, 0);
	Run(MActionResultNodeChangeItemOptionInInventory(501, 5));
	Run(MActionResultNodeChangeItemGenderInInventory(501));
	Run(MActionResultNodeRemoveItemInInventory(501));
	g_pInventory = &world.inventory;
	CHECK(held != nullptr);
	CHECK_EQ(2u, calls.size());
}

TEST(ActionResultNodes, AGenderChangeFlipsTheTypeAndRefreshesTheItem)
{
	World world;
	Ware* coat = world.Stock(ITEM_CLASS_SWORD, 501, 0, 2);
	Run(MActionResultNodeChangeItemGenderInInventory(501));
	CHECK_EQ(3, static_cast<long long>(coat->GetItemType()));
	Run(MActionResultNodeChangeItemGenderInInventory(501));
	CHECK_EQ(2, static_cast<long long>(coat->GetItemType()));
	CHECK(calls == std::vector<std::string>({"affect|501", kFanfare, "affect|501", kFanfare}));
}

TEST(ActionResultNodes, MissingServicesSkipTheirPart)
{
	World world;
	WorldEvents::SetHost(nullptr);
	VsUiPresentation::SetHost(nullptr);
	world.Stock(ITEM_CLASS_SWORD, 501, 0, 2);
	Run(MActionResultNodeChangeItemGenderInInventory(501));
	g_MorphCreatureType = 77;
	world.me.underground = true;
	Run(MActionResultNodeChangeCreatureType(7, CREATURETYPE_BAT));
	CHECK(calls == std::vector<std::string>{"affect|501"});
	CHECK_EQ(0, g_MorphCreatureType);
	// Without the creature service nothing is found to act on.
	CreatureState::SetHost(nullptr);
	ZoneState::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	calls.clear();
	Run(MActionResultNodeActionInfo(SKILL_DOUBLE_IMPACT, 1002, 1001, 1, 2, 0));
	Run(MActionResultNodeCreatureDie(1001));
	CHECK(calls.empty());
}
