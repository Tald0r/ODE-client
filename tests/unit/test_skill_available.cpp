#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MCreatureTable.h"
#include "UserInformation.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

// Which skills the player can use now (task 4.234, gamemodel's since
// then): MSkillSet::SetAvailableSkills rebuilds the set from the learned
// skills, what costs fit the player's MP (a vampire's HP), the weapon in
// hand, the inventory, the item at the mouse, the zone and the bonuses.
namespace {

// The player.
MStatus status;
Race race = RACE_SLAYER;
std::set<EFFECTSTATUS> effects;
int creatureType = CREATURETYPE_SLAYER_MALE;
bool casket = false, invisible = false, noPlayer = false;
int lightSights = 0;
MObject playerObject;
// The world around it.
int zoneID = 1;
MItem* mouseItem = NULL;
std::vector<int> selected;
std::map<const MItem*, bool> portalMarks;

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Status = []() -> MStatus* { return noPlayer ? NULL : &status; };
	host.GetRace = []() { return race; };
	host.HasEffectStatus = [](EFFECTSTATUS effect) { return effects.count(effect) != 0; };
	host.Creature = []() { return reinterpret_cast<MCreature*>(&playerObject); };
	host.IsInvisible = []() { return invisible; };
	host.CalculateLightSight = []() { ++lightSights; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* pCreature) {
		CHECK(pCreature == reinterpret_cast<MCreature*>(&playerObject));
		return creatureType;
	};
	host.IsInCasket = [](MCreature* pCreature) {
		CHECK(pCreature == reinterpret_cast<MCreature*>(&playerObject));
		return casket;
	};
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.GetID = []() { return zoneID; };
	return host;
}();
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.GetMouseItem = []() { return mouseItem; };
	return host;
}();
const VsUiPresentation::Host uiHost = []() {
	VsUiPresentation::Host host{};
	host.SelectSkill = [](int skillID) { selected.push_back(skillID); };
	return host;
}();
const MItemHost itemHost = []() {
	MItemHost host{};
	host.IsPortalMarked = [](const MItem* pPortal) {
		const auto found = portalMarks.find(pPortal);
		return found != portalMarks.end() && found->second;
	};
	return host;
}();

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	bool m_Gun;
	Ware(ITEM_CLASS itemClass, TYPE_ITEMTYPE type = 0, bool gun = false) : m_Class(itemClass), m_Gun(gun)
	{
		static TYPE_OBJECTID next = 5000;
		SetID(next++);
		SetItemType(type);
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGunItem() const override { return m_Gun; }
};

// The item classes the scenes use, each type a 1x1 item; bombs and mines
// use the action of their type plus 900.
const ITEM_CLASS kClasses[] = {
	ITEM_CLASS_SWORD, ITEM_CLASS_BLADE, ITEM_CLASS_SG, ITEM_CLASS_HOLYWATER, ITEM_CLASS_SLAYER_PORTAL_ITEM,
	ITEM_CLASS_BOMB, ITEM_CLASS_MINE, ITEM_CLASS_BOMB_MATERIAL, ITEM_CLASS_VAMPIRE_PORTAL_ITEM,
	ITEM_CLASS_VAMPIRE_ETC, ITEM_CLASS_SKULL, ITEM_CLASS_OUSTERS_SUMMON_ITEM, ITEM_CLASS_COUPLE_RING,
	ITEM_CLASS_VAMPIRE_COUPLE_RING, ITEM_CLASS_OUSTERS_CHAKRAM, ITEM_CLASS_OUSTERS_WRISTLET,
};

struct Scene
{
	GameModelWorld world;
	MSkillInfoTable info;
	MSkillManager manager;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MSkillSet set;
	MSkillInfoTable* oldInfo = g_pSkillInfoTable;
	MSkillManager* oldManager = g_pSkillManager;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreatures = CreatureState::SetHost(&creatureHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&uiHost);

	explicit Scene(Race playerRace = RACE_SLAYER)
	{
		status = MStatus();
		race = playerRace;
		creatureType = playerRace == RACE_VAMPIRE ? CREATURETYPE_VAMPIRE_MALE1
			: playerRace == RACE_OUSTERS ? CREATURETYPE_OUSTERS : CREATURETYPE_SLAYER_MALE;
		effects.clear();
		casket = invisible = noPlayer = false;
		lightSights = 0;
		zoneID = 1;
		mouseItem = NULL;
		selected.clear();
		portalMarks.clear();
		std::fill_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, false);
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, false);
		for (ITEM_CLASS itemClass : kClasses)
		{
			g_pItemTable->InitClass(itemClass, 40);
			for (int type = 0; type < 40; type++)
			{
				ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.SetGrid(1, 1);
				row.UseActionInfo = static_cast<TYPE_ACTIONINFO>(900 + type);
			}
		}
		MItem::SetHost(&itemHost);
		info.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);		// rows at their defaults
		g_pSkillInfoTable = &info;
		manager.Init();
		g_pSkillManager = &manager;
		inventory.Init(10, 6);
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pInventory = &inventory;
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
	}
	~Scene()
	{
		MItem::SetHost(NULL);
		VsUiPresentation::SetHost(oldUi);
		DialogPresentation::SetHost(oldDialog);
		ZoneState::SetHost(oldZone);
		CreatureState::SetHost(oldCreatures);
		PlayerState::SetHost(oldPlayer);
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pSkillManager = oldManager;
		g_pSkillInfoTable = oldInfo;
		inventory.Release();
		std::fill_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, false);
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, false);
	}

	SKILLINFO_NODE& Row(ACTIONINFO id) { return testfw::MutableRow(info, id); }
	void Points(int hp, int mp, int inte = 50, int level = 30)
	{
		status.SetStatus(MODIFY_CURRENT_HP, hp);
		status.SetStatus(MODIFY_CURRENT_MP, mp);
		status.SetStatus(MODIFY_CURRENT_INT, inte);
		status.SetStatus(MODIFY_LEVEL, level);
	}
	// A skill of the domain, learned, at that cost (and active or not).
	void Learn(int domain, ACTIONINFO id, int mp = 0, bool active = false)
	{
		Row(id).SetMP(mp);
		if (active) Row(id).SetActive(); else Row(id).UnSetActive();
		MSkillDomain& skills = *manager.GetMutable(domain);
		skills.AddSkill(id);
		skills.SetNewSkill();
		CHECK(skills.LearnSkill(id));
	}
	Ware* Carry(ITEM_CLASS itemClass, TYPE_ITEMTYPE type = 0)
	{
		Ware* pItem = new Ware(itemClass, type);
		for (int y = 0; y < 6; ++y)
			for (int x = 0; x < 10; ++x)
				if (inventory.GetItem(x, y) == NULL && inventory.AddItem(pItem, x, y))
					return pItem;
		CHECK(false);
		return pItem;
	}
	Ware* Hold(MSlotItemManager& gear, int slot, ITEM_CLASS itemClass, bool affect = true, bool gun = false)
	{
		Ware* pItem = new Ware(itemClass, 0, gun);
		if (!affect) pItem->UnSetAffectStatus();
		CHECK(gear.AddItem(pItem, slot));
		return pItem;
	}
	// The rebuilt set: each skill's flag.
	std::map<int, int> Rebuild()
	{
		set.SetAvailableSkills();
		std::map<int, int> flags;
		for (const auto& entry : set)
			flags[entry.first] = entry.second.Flag;
		return flags;
	}
	int VampireCost(ACTIONINFO id) const
	{
		return info.GetVampireConsumeMP(id, (int)status.GetINT(), (int)status.GetLEVEL());
	}
};

using Flags = std::map<int, int>;

bool Has(const Flags& flags, int id) { return flags.count(id) != 0; }
int Flag(const Flags& flags, int id) { const auto found = flags.find(id); return found == flags.end() ? -1 : found->second; }

} // namespace

TEST(SkillAvailable, WithoutAPlayerOrTablesTheSetIsKept)
{
	Scene scene;
	scene.Points(100, 100);
	// The set as it was: one skill, rebuilt to nothing when the rebuild runs.
	auto kept = [&scene]() {
		scene.set.clear();
		scene.set.AddSkill(SKILL_SINGLE_BLOW);
		return scene.Rebuild() == Flags{{SKILL_SINGLE_BLOW, FLAG_SKILL_ENABLE}};
	};
	CHECK(!kept());
	CHECK_EQ(1, lightSights);
	noPlayer = true;
	CHECK(kept());
	noPlayer = false;
	// No skill info table, or an empty one; no skill manager, or an empty one.
	g_pSkillInfoTable = NULL;
	CHECK(kept());
	MSkillInfoTable empty;
	empty.Release();
	g_pSkillInfoTable = &empty;
	CHECK(kept());
	g_pSkillInfoTable = &scene.info;
	g_pSkillManager = NULL;
	CHECK(kept());
	MSkillManager none;
	g_pSkillManager = &none;
	CHECK(kept());
	CHECK_EQ(1, lightSights);
	g_pSkillManager = &scene.manager;
	CHECK(!kept());
	// Without a player host there is no player either.
	PlayerState::SetHost(nullptr);
	CHECK(kept());
}

TEST(SkillAvailable, Zone3001HasNoSkills)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER);
	scene.set.AddSkill(SKILL_SINGLE_BLOW);
	zoneID = 3001;
	CHECK(scene.Rebuild().empty());
	CHECK_EQ(0, lightSights);
	zoneID = 3002;
	CHECK((scene.Rebuild() == Flags{{MAGIC_CREATE_HOLY_WATER, FLAG_SKILL_ENABLE}}));
	CHECK_EQ(1, lightSights);
	// Without a zone host the zone is none, so not 3001.
	zoneID = 3001;
	CHECK(scene.Rebuild().empty());
	ZoneState::SetHost(nullptr);
	CHECK_EQ(1u, scene.Rebuild().size());
}

TEST(SkillAvailable, ASlayerWithoutGearOrInventoryHasNone)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER);
	g_abSweeperBonusSkills[0] = true;
	g_pSlayerGear = NULL;
	CHECK(scene.Rebuild().empty());
	g_pSlayerGear = &scene.slayerGear;
	g_pInventory = NULL;
	CHECK(scene.Rebuild().empty());
	CHECK_EQ(0, lightSights);
	g_pInventory = &scene.inventory;
	CHECK_EQ(2u, scene.Rebuild().size());
}

TEST(SkillAvailable, WillOfLifeCostsByLevel)
{
	// A vampire's Will of Life costs by its level, not its INT.
	Scene scene(RACE_VAMPIRE);
	scene.Learn(SKILLDOMAIN_VAMPIRE, SKILL_WILL_OF_LIFE, 10);
	scene.Points(1000, 0, 50, 30);
	const int atThirty = scene.VampireCost(SKILL_WILL_OF_LIFE);
	scene.Points(1000, 0, 50, 90);
	const int atNinety = scene.VampireCost(SKILL_WILL_OF_LIFE);
	CHECK(atThirty != atNinety);
	scene.Points(atNinety, 0, 50, 90);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_WILL_OF_LIFE));
	scene.Points(atNinety - 1, 0, 50, 90);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_WILL_OF_LIFE));
}

TEST(SkillAvailable, AnInstalledTurretHasItsOwnThree)
{
	Scene scene;
	scene.Points(100, 10);
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER);
	g_abSweeperBonusSkills[0] = true;
	effects = {EFFECTSTATUS_INSTALL_TURRET};
	scene.Row(SKILL_TURRET_FIRE).SetMP(10);
	CHECK((scene.Rebuild() == Flags{{MAGIC_UN_TRANSFORM, 1}, {SKILL_TURRET_FIRE, 1}, {SKILL_VIVID_MAGAZINE, 1}}));
	scene.Row(SKILL_TURRET_FIRE).SetMP(11);
	CHECK((scene.Rebuild() == Flags{{MAGIC_UN_TRANSFORM, 1}, {SKILL_TURRET_FIRE, 0}, {SKILL_VIVID_MAGAZINE, 1}}));
	CHECK_EQ(0, lightSights);
}

TEST(SkillAvailable, MPPaysAndUnderSacrificeHPPaysTwice)
{
	Scene scene;
	scene.Points(3, 5);
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER, 5);
	scene.Learn(SKILLDOMAIN_HEAL, MAGIC_CURE_LIGHT_WOUNDS, 6);
	Flags flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, MAGIC_CREATE_HOLY_WATER));
	CHECK_EQ(0, Flag(flags, MAGIC_CURE_LIGHT_WOUNDS));
	// 5 MP and 3 HP twice: 11.
	effects = {EFFECTSTATUS_SACRIFICE};
	scene.Row(MAGIC_CURE_LIGHT_WOUNDS).SetMP(11);
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_CURE_LIGHT_WOUNDS));
	scene.Row(MAGIC_CURE_LIGHT_WOUNDS).SetMP(12);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_CURE_LIGHT_WOUNDS));
	// An ouster pays in MP the same way.
	race = RACE_OUSTERS;
	creatureType = CREATURETYPE_OUSTERS;
	scene.Learn(SKILLDOMAIN_OUSTERS, SKILL_FLOURISH, 11, true);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_FLOURISH));
	scene.Row(SKILL_FLOURISH).SetMP(12);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_FLOURISH));
}

TEST(SkillAvailable, AVampirePaysInHP)
{
	Scene scene(RACE_VAMPIRE);
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE, 20);
	const int cost = scene.VampireCost(MAGIC_HIDE);
	CHECK(cost > 0);
	scene.Points(cost, 0);
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_HIDE));
	scene.Points(cost - 1, 1000);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_HIDE));
	// Sacrifice does not change a vampire's.
	effects = {EFFECTSTATUS_SACRIFICE};
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_HIDE));
}

TEST(SkillAvailable, TheWeaponInHandEnablesItsDomain)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW);
	scene.Learn(SKILLDOMAIN_SWORD, SKILL_DOUBLE_IMPACT);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_FAST_RELOAD);
	scene.Learn(SKILLDOMAIN_BLADE, SKILL_SPIRAL_SLAY, 0, true);		// active: always
	const Flags none = scene.Rebuild();
	CHECK((none == Flags{{SKILL_SINGLE_BLOW, 0}, {SKILL_DOUBLE_IMPACT, 0}, {SKILL_FAST_RELOAD, 0}, {SKILL_SPIRAL_SLAY, 1}}));
	MSlotItemManager& gear = scene.slayerGear;
	const int hand = MSlayerGear::GEAR_SLAYER_RIGHTHAND;
	scene.Hold(gear, hand, ITEM_CLASS_BLADE);
	CHECK((scene.Rebuild() == Flags{{SKILL_SINGLE_BLOW, 1}, {SKILL_DOUBLE_IMPACT, 0}, {SKILL_FAST_RELOAD, 0}, {SKILL_SPIRAL_SLAY, 1}}));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	scene.Hold(gear, hand, ITEM_CLASS_SWORD);
	CHECK((scene.Rebuild() == Flags{{SKILL_SINGLE_BLOW, 0}, {SKILL_DOUBLE_IMPACT, 1}, {SKILL_FAST_RELOAD, 0}, {SKILL_SPIRAL_SLAY, 1}}));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	scene.Hold(gear, hand, ITEM_CLASS_SG, true, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_SINGLE_BLOW, 0}, {SKILL_DOUBLE_IMPACT, 0}, {SKILL_FAST_RELOAD, 1}, {SKILL_SPIRAL_SLAY, 1}}));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	// Any gun, by what the item says it is.
	scene.Hold(gear, hand, ITEM_CLASS_AR, true, true);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_FAST_RELOAD));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	scene.Hold(gear, hand, ITEM_CLASS_SG, true, false);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_FAST_RELOAD));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	// A weapon the player cannot use enables nothing.
	scene.Hold(gear, hand, ITEM_CLASS_BLADE, false);
	CHECK(scene.Rebuild() == none);
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	// At exactly the MP each weapon's skill is enabled.
	scene.Points(100, 7);
	for (ACTIONINFO id : {SKILL_SINGLE_BLOW, SKILL_DOUBLE_IMPACT, SKILL_FAST_RELOAD})
		scene.Row(id).SetMP(7);
	for (ITEM_CLASS weapon : {ITEM_CLASS_BLADE, ITEM_CLASS_SWORD, ITEM_CLASS_SG})
	{
		scene.Hold(gear, hand, weapon, true, weapon == ITEM_CLASS_SG);
		const ACTIONINFO skill = weapon == ITEM_CLASS_BLADE ? SKILL_SINGLE_BLOW
			: weapon == ITEM_CLASS_SWORD ? SKILL_DOUBLE_IMPACT : SKILL_FAST_RELOAD;
		CHECK_EQ(1, Flag(scene.Rebuild(), skill));
		delete gear.RemoveItem(static_cast<BYTE>(hand));
	}
	for (ACTIONINFO id : {SKILL_SINGLE_BLOW, SKILL_DOUBLE_IMPACT, SKILL_FAST_RELOAD})
		scene.Row(id).SetMP(0);
	// Past the MP, nothing, active or not.
	scene.Hold(gear, hand, ITEM_CLASS_BLADE);
	scene.Points(100, 0);
	scene.Row(SKILL_SINGLE_BLOW).SetMP(1);
	scene.Row(SKILL_SPIRAL_SLAY).SetMP(1);
	const Flags poor = scene.Rebuild();
	CHECK_EQ(0, Flag(poor, SKILL_SINGLE_BLOW));
	CHECK_EQ(0, Flag(poor, SKILL_SPIRAL_SLAY));
}

TEST(SkillAvailable, EnchantHealAndOtherSkillsNeedOnlyTheMP)
{
	Scene scene;
	scene.Points(100, 7);
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER, 7);
	scene.Learn(SKILLDOMAIN_HEAL, MAGIC_CURE_LIGHT_WOUNDS, 8);
	scene.Learn(SKILLDOMAIN_ETC, SKILL_SOUL_CHAIN, 7);
	scene.Learn(SKILLDOMAIN_ETC, SKILL_SINGLE_BLOW, 8);
	CHECK((scene.Rebuild() == Flags{{MAGIC_CREATE_HOLY_WATER, 1}, {MAGIC_CURE_LIGHT_WOUNDS, 0}, {SKILL_SOUL_CHAIN, 1}, {SKILL_SINGLE_BLOW, 0}}));
	// A skill the blade domain gave without a blade is the other skills'
	// to decide: enabled when its cost fits.
	scene.Learn(SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW, 8);
	scene.Row(SKILL_SINGLE_BLOW).SetMP(7);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_SINGLE_BLOW));
}

TEST(SkillAvailable, APortalAndHolyWaterInTheInventory)
{
	Scene scene;
	scene.Points(100, 5);
	scene.Carry(ITEM_CLASS_SLAYER_PORTAL_ITEM);
	scene.Carry(ITEM_CLASS_SLAYER_PORTAL_ITEM);
	scene.Carry(ITEM_CLASS_HOLYWATER);
	scene.Carry(ITEM_CLASS_HOLYWATER);
	scene.Row(MAGIC_THROW_HOLY_WATER).SetMP(5);
	CHECK((scene.Rebuild() == Flags{{SUMMON_HELICOPTER, 1}, {MAGIC_THROW_HOLY_WATER, 1}}));
	scene.Row(MAGIC_THROW_HOLY_WATER).SetMP(6);
	CHECK((scene.Rebuild() == Flags{{SUMMON_HELICOPTER, 1}, {MAGIC_THROW_HOLY_WATER, 0}}));
}

TEST(SkillAvailable, BombsAndMinesShowTheirOwnActions)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Carry(ITEM_CLASS_BOMB, 2);
	scene.Carry(ITEM_CLASS_BOMB, 2);
	scene.Carry(ITEM_CLASS_MINE, 3);
	// Without either skill, neither counts.
	CHECK(scene.Rebuild().empty());
	// Throwing (gun domain, held or active): each bomb's action, the mine's
	// shown off.
	scene.Learn(SKILLDOMAIN_GUN, SKILL_THROW_BOMB, 0, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_THROW_BOMB, 1}, {902, 1}, {903, 0}}));
	// Laying mines too.
	scene.Learn(SKILLDOMAIN_GUN, SKILL_INSTALL_MINE, 0, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_THROW_BOMB, 1}, {SKILL_INSTALL_MINE, 1}, {902, 1}, {903, 1}}));
	// Disabled when nothing is at hand, or the skill itself is.
	delete scene.inventory.RemoveItem(scene.inventory.FindItem(ITEM_CLASS_MINE)->GetID());
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_INSTALL_MINE));
	scene.Row(SKILL_THROW_BOMB).UnSetActive();
	const Flags held = scene.Rebuild();
	CHECK_EQ(0, Flag(held, SKILL_THROW_BOMB));
	CHECK_EQ(1, Flag(held, 902));
}

TEST(SkillAvailable, OnlyLayingMinesStillShowsTheBombs)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Carry(ITEM_CLASS_BOMB, 1);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_INSTALL_MINE, 0, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_INSTALL_MINE, 0}, {901, 0}}));
}

TEST(SkillAvailable, ABombOrMineOfATypeTheTableLacksShowsNoAction)
{
	Scene scene;
	scene.Points(100, 100);
	// The type is the server's. Past the table's 40 rows it reads the default
	// row, whose action is ACTIONINFO_NULL: no skill row, so no icon to show
	// or describe.
	scene.Carry(ITEM_CLASS_BOMB, 40);
	scene.Carry(ITEM_CLASS_MINE, 40);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_THROW_BOMB, 0, true);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_INSTALL_MINE, 0, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_THROW_BOMB, 1}, {SKILL_INSTALL_MINE, 1}}));
}

TEST(SkillAvailable, MaterialsEnableMaking)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_MAKE_BOMB, 0, true);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_MAKE_MINE, 0, true);
	CHECK((scene.Rebuild() == Flags{{SKILL_MAKE_BOMB, 0}, {SKILL_MAKE_MINE, 0}}));
	// Types 0 to 4 make bombs, the rest mines.
	scene.Carry(ITEM_CLASS_BOMB_MATERIAL, 4);
	CHECK((scene.Rebuild() == Flags{{SKILL_MAKE_BOMB, 1}, {SKILL_MAKE_MINE, 0}}));
	scene.inventory.Release();
	scene.inventory.Init(10, 6);
	scene.Carry(ITEM_CLASS_BOMB_MATERIAL, 5);
	CHECK((scene.Rebuild() == Flags{{SKILL_MAKE_BOMB, 0}, {SKILL_MAKE_MINE, 1}}));
	// A skill its domain leaves disabled stays so with its material.
	scene.Row(SKILL_MAKE_MINE).UnSetActive();
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_MAKE_MINE));
}

TEST(SkillAvailable, MakingMinesAloneLooksForMaterials)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_MAKE_MINE, 0, true);
	scene.Carry(ITEM_CLASS_BOMB_MATERIAL, 6);
	CHECK((scene.Rebuild() == Flags{{SKILL_MAKE_MINE, 1}}));
}

TEST(SkillAvailable, TheItemAtTheMouseCounts)
{
	Scene scene;
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_THROW_BOMB, 0, true);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_INSTALL_MINE, 0, true);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_MAKE_BOMB, 0, true);
	scene.Learn(SKILLDOMAIN_GUN, SKILL_MAKE_MINE, 0, true);
	struct { ITEM_CLASS itemClass; TYPE_ITEMTYPE type; int skill; } cases[] = {
		{ITEM_CLASS_BOMB, 0, SKILL_THROW_BOMB},
		{ITEM_CLASS_MINE, 0, SKILL_INSTALL_MINE},
		{ITEM_CLASS_BOMB_MATERIAL, 0, SKILL_MAKE_BOMB},
		{ITEM_CLASS_BOMB_MATERIAL, 7, SKILL_MAKE_MINE},
	};
	for (const auto& c : cases)
	{
		Ware item(c.itemClass, c.type);
		mouseItem = &item;
		const Flags flags = scene.Rebuild();
		for (int skill : {SKILL_THROW_BOMB, SKILL_INSTALL_MINE, SKILL_MAKE_BOMB, SKILL_MAKE_MINE})
			CHECK_EQ(skill == c.skill ? 1 : 0, Flag(flags, skill));
		// The mouse's item is not shown as an action.
		CHECK(!Has(flags, 900));
	}
	// Without a dialog host there is no mouse item.
	Ware bomb(ITEM_CLASS_BOMB, 0);
	mouseItem = &bomb;
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_THROW_BOMB));
	DialogPresentation::SetHost(nullptr);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_THROW_BOMB));
	mouseItem = NULL;
}

TEST(SkillAvailable, RestoreIsGivenByHand)
{
	Scene scene;
	scene.Points(100, 9);
	g_pUserInformation->HasSkillRestore = true;
	scene.Row(MAGIC_RESTORE).SetMP(9);
	CHECK((scene.Rebuild() == Flags{{MAGIC_RESTORE, 1}}));
	scene.Row(MAGIC_RESTORE).SetMP(10);
	CHECK((scene.Rebuild() == Flags{{MAGIC_RESTORE, 0}}));
	// Not for a vampire.
	race = RACE_VAMPIRE;
	creatureType = CREATURETYPE_BAT;
	CHECK(!Has(scene.Rebuild(), MAGIC_RESTORE));
}

TEST(SkillAvailable, InACasketOnlyOpeningIt)
{
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0);
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE);
	g_abHolyLandBonusSkills[0] = true;
	casket = true;
	CHECK((scene.Rebuild() == Flags{{MAGIC_OPEN_CASKET, 1}}));
	CHECK((selected == std::vector<int>{MAGIC_OPEN_CASKET}));
	CHECK_EQ(0, lightSights);
	// Without a UI host nothing is selected.
	VsUiPresentation::SetHost(nullptr);
	selected.clear();
	CHECK_EQ(1u, scene.Rebuild().size());
	CHECK(selected.empty());
}

TEST(SkillAvailable, OnlyAVampiresOwnFormUsesItsDomain)
{
	const int forms[] = {
		CREATURETYPE_VAMPIRE_MALE1, CREATURETYPE_VAMPIRE_FEMALE1, CREATURETYPE_VAMPIRE_MALE2, CREATURETYPE_VAMPIRE_FEMALE2,
		CREATURETYPE_VAMPIRE_MALE3, CREATURETYPE_VAMPIRE_FEMALE3, CREATURETYPE_VAMPIRE_MALE4, CREATURETYPE_VAMPIRE_FEMALE4,
		CREATURETYPE_VAMPIRE_MALE5, CREATURETYPE_VAMPIRE_FEMALE5, CREATURETYPE_VAMPIRE_MALE6, CREATURETYPE_VAMPIRE_FEMALE6,
		CREATURETYPE_VAMPIRE_OPERATOR,
	};
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0);
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE);
	for (int form : forms)
	{
		creatureType = form;
		const Flags flags = scene.Rebuild();
		CHECK_EQ(1, Flag(flags, MAGIC_HIDE));
		CHECK(!Has(flags, MAGIC_UN_TRANSFORM));
		CHECK_EQ(1, Flag(flags, SKILL_BLOOD_DRAIN));
	}
	creatureType = CREATURETYPE_BAT;
	const Flags bat = scene.Rebuild();
	CHECK(!Has(bat, MAGIC_HIDE));
	CHECK_EQ(1, Flag(bat, MAGIC_UN_TRANSFORM));
	CHECK(!Has(bat, SKILL_BLOOD_DRAIN));
}

TEST(SkillAvailable, InvisibilityIsRefusedInTheLairs)
{
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0);
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_INVISIBILITY);
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_HIDE);
	for (int lair : {1104, 1106, 1114, 1115})
	{
		zoneID = lair;
		const Flags flags = scene.Rebuild();
		CHECK_EQ(0, Flag(flags, MAGIC_INVISIBILITY));
		CHECK_EQ(1, Flag(flags, MAGIC_HIDE));
	}
	for (int other : {1103, 1105, 1107, 1113, 1116})
	{
		zoneID = other;
		CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_INVISIBILITY));
	}
}

TEST(SkillAvailable, SomeVampireSkillsNeedAnItem)
{
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0);
	for (ACTIONINFO id : {MAGIC_BLOODY_MARK, MAGIC_BLOODY_TUNNEL, MAGIC_TRANSFORM_TO_WOLF, MAGIC_TRANSFORM_TO_BAT, SKILL_TRANSFORM_TO_WERWOLF, MAGIC_HOWL})
		scene.Learn(SKILLDOMAIN_VAMPIRE, id, 10);
	CHECK((scene.Rebuild() == Flags{{MAGIC_BLOODY_MARK, 0}, {MAGIC_BLOODY_TUNNEL, 0}, {MAGIC_TRANSFORM_TO_WOLF, 0},
		{MAGIC_TRANSFORM_TO_BAT, 0}, {SKILL_TRANSFORM_TO_WERWOLF, 0}, {MAGIC_HOWL, 0}, {SKILL_BLOOD_DRAIN, 1}}));
	// An unmarked portal marks, a marked one tunnels; the wolf and bat
	// take their own vampire items, the werewolf skull 39.
	Ware* portal = scene.Carry(ITEM_CLASS_VAMPIRE_PORTAL_ITEM);
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_BLOODY_MARK));
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_BLOODY_TUNNEL));
	portalMarks[portal] = true;
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_BLOODY_MARK));
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_BLOODY_TUNNEL));
	scene.Carry(ITEM_CLASS_VAMPIRE_ETC, 1);
	Flags flags = scene.Rebuild();
	CHECK_EQ(0, Flag(flags, MAGIC_TRANSFORM_TO_WOLF));
	CHECK_EQ(1, Flag(flags, MAGIC_TRANSFORM_TO_BAT));
	scene.Carry(ITEM_CLASS_VAMPIRE_ETC, 0);
	scene.Carry(ITEM_CLASS_SKULL, 38);
	flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, MAGIC_TRANSFORM_TO_WOLF));
	CHECK_EQ(0, Flag(flags, SKILL_TRANSFORM_TO_WERWOLF));
	scene.Carry(ITEM_CLASS_SKULL, 39);
	flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, SKILL_TRANSFORM_TO_WERWOLF));
	CHECK_EQ(0, Flag(flags, MAGIC_HOWL));
	// An item does not pay for a skill too dear.
	scene.Points(0, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_TRANSFORM_TO_WOLF));
}

TEST(SkillAvailable, AVampiresHandGivenSkills)
{
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0, 70, 40);
	g_pUserInformation->HasMagicGroundAttack = true;
	g_pUserInformation->HasMagicBloodySnake = true;
	g_pUserInformation->HasMagicBloodyWarp = true;
	for (ACTIONINFO id : {MAGIC_GROUND_ATTACK, MAGIC_BLOODY_SNAKE, MAGIC_BLOODY_WARP})
		scene.Row(id).SetMP(30);
	Flags flags;
	for (ACTIONINFO id : {MAGIC_GROUND_ATTACK, MAGIC_BLOODY_SNAKE, MAGIC_BLOODY_WARP})
	{
		CHECK_EQ(1, Flag(scene.Rebuild(), id));
		scene.Points(scene.VampireCost(id), 0, 70, 40);
		CHECK_EQ(1, Flag(scene.Rebuild(), id));
		// Each needs its own cost.
		scene.Points(scene.VampireCost(id) - 1, 0, 70, 40);
		const ACTIONINFO cheaper = id == MAGIC_GROUND_ATTACK ? MAGIC_BLOODY_SNAKE : MAGIC_GROUND_ATTACK;
		scene.Row(cheaper).SetMP(0);
		flags = scene.Rebuild();
		CHECK_EQ(0, Flag(flags, id));
		CHECK_EQ(1, Flag(flags, cheaper));
		scene.Row(cheaper).SetMP(30);
		scene.Points(1000, 0, 70, 40);
	}
	// Each by its own flag.
	bool* const given[] = {&g_pUserInformation->HasMagicGroundAttack, &g_pUserInformation->HasMagicBloodySnake, &g_pUserInformation->HasMagicBloodyWarp};
	const ACTIONINFO skills[] = {MAGIC_GROUND_ATTACK, MAGIC_BLOODY_SNAKE, MAGIC_BLOODY_WARP};
	for (int only = 0; only < 3; ++only)
	{
		for (int i = 0; i < 3; ++i) *given[i] = i == only;
		flags = scene.Rebuild();
		for (int i = 0; i < 3; ++i)
			CHECK_EQ(i == only, Has(flags, skills[i]));
	}
	// None without the user's flag.
	for (bool* flag : given) *flag = false;
	CHECK((scene.Rebuild() == Flags{{SKILL_BLOOD_DRAIN, 1}}));
}

TEST(SkillAvailable, BloodDrainIsAVampireFormsAndPaid)
{
	Scene scene(RACE_VAMPIRE);
	scene.Row(SKILL_BLOOD_DRAIN).SetMP(20);
	scene.Points(scene.VampireCost(SKILL_BLOOD_DRAIN), 0);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_BLOOD_DRAIN));
	scene.Points(scene.VampireCost(SKILL_BLOOD_DRAIN) - 1, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_BLOOD_DRAIN));
	for (int form : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF, CREATURETYPE_INSTALL_TURRET})
	{
		creatureType = form;
		CHECK(!Has(scene.Rebuild(), SKILL_BLOOD_DRAIN));
	}
}

TEST(SkillAvailable, AnInvisibleVampireCanBecomeVisible)
{
	Scene scene(RACE_VAMPIRE);
	scene.Points(1000, 0);
	CHECK(!Has(scene.Rebuild(), MAGIC_UN_INVISIBILITY));
	invisible = true;
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_UN_INVISIBILITY));
	PlayerState::Host withoutInvisibility = playerHost;
	withoutInvisibility.IsInvisible = nullptr;
	PlayerState::SetHost(&withoutInvisibility);
	CHECK(!Has(scene.Rebuild(), MAGIC_UN_INVISIBILITY));
}

TEST(SkillAvailable, AWolfEatsAndHowls)
{
	Scene scene(RACE_VAMPIRE);
	creatureType = CREATURETYPE_WOLF;
	scene.Row(MAGIC_EAT_CORPSE).SetMP(10);
	scene.Points(scene.VampireCost(MAGIC_EAT_CORPSE), 0);
	CHECK((scene.Rebuild() == Flags{{MAGIC_EAT_CORPSE, 1}, {MAGIC_UN_TRANSFORM, 1}}));
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_HOWL, 10);
	// The same cost as eating, so the same HP pays it.
	CHECK_EQ(scene.VampireCost(MAGIC_EAT_CORPSE), scene.VampireCost(MAGIC_HOWL));
	CHECK((scene.Rebuild() == Flags{{MAGIC_EAT_CORPSE, 1}, {MAGIC_HOWL, 1}, {MAGIC_UN_TRANSFORM, 1}}));
	scene.Points(scene.VampireCost(MAGIC_HOWL), 0);
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_HOWL));
	scene.Points(scene.VampireCost(MAGIC_HOWL) - 1, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_HOWL));
	scene.Points(scene.VampireCost(MAGIC_EAT_CORPSE) - 1, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_EAT_CORPSE));
}

TEST(SkillAvailable, AWerewolfBitesAndGlides)
{
	Scene scene(RACE_VAMPIRE);
	creatureType = CREATURETYPE_WER_WOLF;
	scene.Row(SKILL_BITE_OF_DEATH).SetMP(10);
	scene.Points(scene.VampireCost(SKILL_BITE_OF_DEATH), 0);
	CHECK((scene.Rebuild() == Flags{{SKILL_BITE_OF_DEATH, 1}, {MAGIC_UN_TRANSFORM, 1}}));
	scene.Learn(SKILLDOMAIN_VAMPIRE, MAGIC_RAPID_GLIDING, 10);
	scene.Points(scene.VampireCost(MAGIC_RAPID_GLIDING), 0);
	CHECK_EQ(1, Flag(scene.Rebuild(), MAGIC_RAPID_GLIDING));
	scene.Points(scene.VampireCost(MAGIC_RAPID_GLIDING) - 1, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), MAGIC_RAPID_GLIDING));
	scene.Points(scene.VampireCost(SKILL_BITE_OF_DEATH) - 1, 0);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_BITE_OF_DEATH));
	// A bat neither bites nor glides.
	creatureType = CREATURETYPE_BAT;
	CHECK((scene.Rebuild() == Flags{{MAGIC_UN_TRANSFORM, 1}}));
}

TEST(SkillAvailable, AnOusterNeedsTheRightWeaponOrElement)
{
	Scene scene(RACE_OUSTERS);
	scene.Points(100, 100);
	status.SetStatus(MODIFY_ELEMENTAL_FIRE, 5);
	status.SetStatus(MODIFY_ELEMENTAL_WATER, 6);
	status.SetStatus(MODIFY_ELEMENTAL_EARTH, 7);
	auto learn = [&](ACTIONINFO id, int domain, int fire = 0, int water = 0, int earth = 0) {
		scene.Learn(SKILLDOMAIN_OUSTERS, id);
		scene.Row(id).ElementalDomain = domain;
		scene.Row(id).Fire = fire;
		scene.Row(id).Water = water;
		scene.Row(id).Earth = earth;
		scene.Row(id).SetSkillStep(SKILL_STEP_OUSTERS_COMBAT);
	};
	learn(SKILL_FLOURISH, SKILLINFO_NODE::ELEMENTAL_DOMAIN_COMBAT);
	learn(SKILL_EVADE, SKILLINFO_NODE::ELEMENTAL_DOMAIN_FIRE, 5);
	learn(SKILL_SHARP_ROUND, SKILLINFO_NODE::ELEMENTAL_DOMAIN_WATER, 0, 7);
	learn(SKILL_HIDE_SIGHT, SKILLINFO_NODE::ELEMENTAL_DOMAIN_EARTH, 0, 0, 7);
	learn(SKILL_BACK_STAB, SKILLINFO_NODE::ELEMENTAL_DOMAIN_NO_DOMAIN);
	learn(SKILL_BLITZ_SLIDING, SKILLINFO_NODE::ELEMENTAL_DOMAIN_WIND);
	learn(SKILL_BLAZE_BOLT, SKILLINFO_NODE::ELEMENTAL_DOMAIN_ETC);
	learn(SKILL_DESTRUCTION_SPEAR, SKILLINFO_NODE::ELEMENTAL_DOMAIN_ELEMENTAL_COMBAT);
	learn(SKILL_ICE_FIELD, SKILLINFO_NODE::ELEMENTAL_DOMAIN_FIRE);
	scene.Row(SKILL_ICE_FIELD).SetSkillStep(SKILL_STEP_ETC);
	// Bare-handed: the domainless, the wind, the other and the step-less.
	Flags flags;
	const Flags bare = scene.Rebuild();
	CHECK((bare == Flags{{SKILL_FLOURISH, 0}, {SKILL_EVADE, 0}, {SKILL_SHARP_ROUND, 0}, {SKILL_HIDE_SIGHT, 0},
		{SKILL_BACK_STAB, 1}, {SKILL_BLITZ_SLIDING, 1}, {SKILL_BLAZE_BOLT, 1}, {SKILL_DESTRUCTION_SPEAR, 0}, {SKILL_ICE_FIELD, 1}}));
	MSlotItemManager& gear = scene.oustersGear;
	const int hand = MOustersGear::GEAR_OUSTERS_RIGHTHAND;
	// A chakram: combat.
	scene.Hold(gear, hand, ITEM_CLASS_OUSTERS_CHAKRAM);
	flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, SKILL_FLOURISH));
	CHECK_EQ(0, Flag(flags, SKILL_EVADE));
	CHECK_EQ(0, Flag(flags, SKILL_DESTRUCTION_SPEAR));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	// A wristlet: the elements the player's levels reach (fire 5 of 5,
	// water 7 of 6 not, earth 7 of 7).
	scene.Hold(gear, hand, ITEM_CLASS_OUSTERS_WRISTLET);
	flags = scene.Rebuild();
	CHECK_EQ(0, Flag(flags, SKILL_FLOURISH));
	CHECK_EQ(1, Flag(flags, SKILL_EVADE));
	CHECK_EQ(0, Flag(flags, SKILL_SHARP_ROUND));
	CHECK_EQ(1, Flag(flags, SKILL_HIDE_SIGHT));
	status.SetStatus(MODIFY_ELEMENTAL_WATER, 7);
	status.SetStatus(MODIFY_ELEMENTAL_FIRE, 4);
	flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, SKILL_SHARP_ROUND));
	CHECK_EQ(0, Flag(flags, SKILL_EVADE));
	// Above what each needs, still; earth below, not.
	status.SetStatus(MODIFY_ELEMENTAL_FIRE, 6);
	status.SetStatus(MODIFY_ELEMENTAL_WATER, 8);
	status.SetStatus(MODIFY_ELEMENTAL_EARTH, 6);
	flags = scene.Rebuild();
	CHECK_EQ(1, Flag(flags, SKILL_EVADE));
	CHECK_EQ(1, Flag(flags, SKILL_SHARP_ROUND));
	CHECK_EQ(0, Flag(flags, SKILL_HIDE_SIGHT));
	status.SetStatus(MODIFY_ELEMENTAL_EARTH, 8);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_HIDE_SIGHT));
	delete gear.RemoveItem(static_cast<BYTE>(hand));
	// A wristlet the player cannot use counts for nothing.
	scene.Hold(gear, hand, ITEM_CLASS_OUSTERS_WRISTLET, false);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_SHARP_ROUND));
	// Past the MP, nothing.
	scene.Row(SKILL_BACK_STAB).SetMP(101);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_BACK_STAB));
}

TEST(SkillAvailable, SummoningASylphNeedsTheGem)
{
	Scene scene(RACE_OUSTERS);
	scene.Points(100, 100);
	scene.Learn(SKILLDOMAIN_OUSTERS, SKILL_SUMMON_SYLPH, 0, true);
	CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_SUMMON_SYLPH));
	scene.Carry(ITEM_CLASS_OUSTERS_SUMMON_ITEM, 3);
	CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_SUMMON_SYLPH));
}

TEST(SkillAvailable, TheBonusSkillsAreAddedForEveryRace)
{
	for (Race playerRace : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
	{
		Scene scene(playerRace);
		scene.Points(1000, 1000);
		g_abHolyLandBonusSkills[0] = true;
		g_abHolyLandBonusSkills[HOLYLAND_BONUS_MAX - 1] = true;
		g_abSweeperBonusSkills[1] = true;
		g_abSweeperBonusSkills[SWEEPER_BONUS_MAX - 1] = true;
		const Flags flags = scene.Rebuild();
		CHECK_EQ(1, Flag(flags, SKILL_HOLYLAND_BLOOD_BIBLE_ARMEGA));
		CHECK_EQ(1, Flag(flags, SKILL_HOLYLAND_BLOOD_BIBLE_ARMEGA + HOLYLAND_BONUS_MAX - 1));
		CHECK(!Has(flags, SKILL_HOLYLAND_BLOOD_BIBLE_ARMEGA + 1));
		CHECK(!Has(flags, SKILL_SWEEPER_BONUS_1));
		CHECK_EQ(1, Flag(flags, SKILL_SWEEPER_BONUS_1 + 1));
		CHECK_EQ(1, Flag(flags, SKILL_SWEEPER_BONUS_1 + SWEEPER_BONUS_MAX - 1));
		CHECK_EQ(1, lightSights);
	}
}

TEST(SkillAvailable, ACoupleRingGivesTheLoveChain)
{
	struct { Race race; ITEM_CLASS ring; ITEM_CLASS otherRing; } cases[] = {
		{RACE_SLAYER, ITEM_CLASS_COUPLE_RING, ITEM_CLASS_VAMPIRE_COUPLE_RING},
		{RACE_VAMPIRE, ITEM_CLASS_VAMPIRE_COUPLE_RING, ITEM_CLASS_COUPLE_RING},
		{RACE_OUSTERS, ITEM_CLASS_COUPLE_RING, ITEM_CLASS_VAMPIRE_COUPLE_RING},
	};
	for (const auto& c : cases)
	{
		Scene scene(c.race);
		scene.Points(1000, 1000);
		MSlotItemManager& gear = c.race == RACE_SLAYER ? static_cast<MSlotItemManager&>(scene.slayerGear)
			: c.race == RACE_VAMPIRE ? static_cast<MSlotItemManager&>(scene.vampireGear)
			: static_cast<MSlotItemManager&>(scene.oustersGear);
		// Another race's ring, or a ring in another race's gear, gives none.
		scene.Hold(gear, 0, c.otherRing);
		MSlotItemManager& elsewhere = c.race == RACE_SLAYER ? static_cast<MSlotItemManager&>(scene.oustersGear)
			: static_cast<MSlotItemManager&>(scene.slayerGear);
		scene.Hold(elsewhere, 1, c.ring);
		CHECK(!Has(scene.Rebuild(), SKILL_LOVE_CHAIN));
		// Its own: enabled while the player can use it.
		Ware* ring = scene.Hold(gear, 1, c.ring);
		CHECK_EQ(1, Flag(scene.Rebuild(), SKILL_LOVE_CHAIN));
		ring->UnSetAffectStatus();
		CHECK_EQ(0, Flag(scene.Rebuild(), SKILL_LOVE_CHAIN));
		ring->SetAffectStatus();
		// Not as a bat or a wolf.
		for (int form : {CREATURETYPE_BAT, CREATURETYPE_WOLF})
		{
			creatureType = form;
			CHECK(!Has(scene.Rebuild(), SKILL_LOVE_CHAIN));
		}
	}
}

TEST(SkillAvailable, EachRebuildRecomputesTheLightSight)
{
	Scene scene;
	scene.Points(100, 100);
	scene.set.SetAvailableSkills();
	scene.set.CheckMP();
	CHECK_EQ(2, lightSights);
	// CheckMP rebuilds the whole set.
	scene.Learn(SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER, 101);
	scene.set.CheckMP();
	CHECK_EQ(0, scene.set.find(MAGIC_CREATE_HOLY_WATER)->second.Flag);
	CHECK_EQ(1u, scene.set.size());
}
