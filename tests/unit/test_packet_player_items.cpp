#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectGeneratorTable.h"
#include "MEffectTarget.h"
#include "MScreenEffectManager.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MSkillManager.h"
#include "ModifyStatusManager.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MQuickSlot.h"
#include "MParty.h"
#include "MGameDef.h"
#include "ExperienceTable.h"
#include "UserOption.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "PlayerPresentationHost.h"
#include "DialogPresentationHost.h"
#include "PacketFunction.h"
#include "InventoryInfo.h"
#include "InventorySlotInfo.h"
#include "GearInfo.h"
#include "GearSlotInfo.h"
#include "ExtraInfo.h"
#include "ExtraSlotInfo.h"
#include "SubItemInfo.h"
#include "EffectInfo.h"
#include "BloodBibleSignInfo.h"
#include "PetInfo.h"
#include "VS_UI_title.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_item.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ASAN_SEES_FREES 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(ASAN_SEES_FREES)
#define ASAN_SEES_FREES 1
#endif
#ifdef ASAN_SEES_FREES
#include <sanitizer/asan_interface.h>
#endif

// MItemUse.cpp's, the factory GameInit installs.
MItem* NewItemOfClass(ITEM_CLASS itemClass);
extern S_SLOT g_char_slot_ingame;

// The player's things from the game server's word on them (task 4.253
// moved these into gameclient): SetInventoryInfo, SetGearInfo and
// SetExtraInfo make the items of the inventory, the gear and the mouse,
// SetBloodBibleSlot the blood bible's signs, SetEffectInfo a creature's
// effect statuses, and SetPetInfo a pet that follows its owner.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

bool Same(const std::vector<std::string>& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

int Count(const std::string& call) { return int(std::count(calls.begin(), calls.end(), call)); }

// Whether a SubItemInfo (no virtual destructor to watch) was freed. Only
// ASan can tell, by the poison its quarantine keeps on a freed block;
// without it this says yes, so the check is the sanitized trees' alone.
bool Freed(const void* p)
{
#ifdef ASAN_SEES_FREES
	return __asan_address_is_poisoned(p) != 0;
#else
	(void)p;
	return true;
#endif
}

// The class the factory cannot make (none when -1).
int refusedClass = -1;
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) { return int(itemClass) == refusedClass ? nullptr : NewItemOfClass(itemClass); };
	host.RefreshAffect = [](MItem* pItem) { calls.push_back("affect|" + N(pItem->GetID())); };
	host.ItemDestroyed = [](MItem* pItem) noexcept { calls.push_back("destroyed|" + N(pItem->GetID())); };
	return host;
}();

// The status values are the manager's business, not these functions'.
struct QuietStatus : ModifyStatusManager
{
	bool Execute(int, void*) const override { return true; }
};

// The effects started, with the target the action reaches.
struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return 7; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		// A delay node lasts the target's delay frames.
		calls.push_back("effect|" + N(info.nActionInfo) + "|" + N(info.creatureID) + "|delay " + N(info.count)
			+ "|phase " + (info.pEffectTarget != NULL ? N(info.pEffectTarget->GetCurrentPhase()) : std::string("end")));
		return false;
	}
};

struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

// An extra slot that says when it is freed.
struct WatchedExtraSlot : ExtraSlotInfo
{
	~WatchedExtraSlot() override { calls.push_back("slot freed|" + N(getObjectID())); }
};

struct InventoryEffects : MScreenEffectManager
{
	void Release() override { calls.push_back("inventory effects released"); MScreenEffectManager::Release(); }
};

// A creature that records the effect statuses it is given and its death.
struct EffectProbe : MCreature
{
	bool AddEffectStatus(EFFECTSTATUS status, DWORD delayFrame) override
	{
		calls.push_back("status|" + N(status) + "|" + N(delayFrame));
		return true;
	}
	void SetCorpse() override { calls.push_back("corpse"); }
};

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer->GetID(); };
	host.GetRace = []() { return g_pPlayer->GetRace(); };
	return host;
}();

// Zone 3001 has no skills, so the skill check only empties the set.
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return 3001; };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone->GetFakeCreature(id); };
	return host;
}();

// The zone's sectors hold its creatures as objects, as GameInit's entries
// convert them.
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return host;
}();

// The player's stats recomputed show its damage (the values are
// MPlayer::CalculateStatus's).
const PlayerPresentation::Host presentationHost = []() {
	PlayerPresentation::Host host{};
	host.SetCharInfoDAM = [](int, int) { calls.push_back("stats"); };
	return host;
}();

const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.PickUpItem = [](MItem* pItem) {
		calls.push_back("pick up|" + N(pItem->GetID()));
		gpC_mouse_pointer->PickUpItem(pItem);
	};
	host.DropItem = []() { calls.push_back("drop"); gpC_mouse_pointer->DropItem(); };
	host.UnlockItem = []() { calls.push_back("unlock item"); };
	return host;
}();

const MEffectTargetHost targetHost = []() {
	MEffectTargetHost host{};
	return host;
}();

struct Scene
{
	GameModelWorld world{ 4 };
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MParty party;
	MSkillSet skills;
	MSkillInfoTable skillInfo;
	MSkillManager skillManager;
	QuietStatus quietStatus;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	Generators generators;
	InventoryEffects inventoryEffects;
	ExperienceTable experience;
	UserOption userOption;
	CMessageArray gameMessages;
	CMessageArray systemMessages;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MParty* oldParty = g_pParty;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MSkillManager* oldSkillManager = g_pSkillManager;
	ModifyStatusManager* oldModify = g_pModifyStatusManager;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	MScreenEffectManager* oldInventoryEffects = g_pInventoryEffectManager;
	ExperienceTable* oldExperience = g_pExperienceTable;
	UserOption* oldUserOption = g_pUserOption;
	CMessageArray* oldGameMessages = g_pGameMessage;
	CMessageArray* oldSystemMessages = g_pSystemMessage;
	C_VS_UI_ITEM* oldUiItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	MMagazine* oldMagazine = g_pCurrentMagazine;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MOustersArmsBand* oldArmsBand1 = g_pArmsBand1;
	MOustersArmsBand* oldArmsBand2 = g_pArmsBand2;
	int oldMode = g_Mode;
	S_SLOT oldSlot = g_char_slot_ingame;
	std::vector<bool> oldHolyLand = std::vector<bool>(g_abHolyLandBonusSkills, g_abHolyLandBonusSkills + HOLYLAND_BONUS_MAX);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const CreatureState::Host* oldCreatureHost = CreatureState::SetHost(&creatureHost);
	const PlayerPresentation::Host* oldPresentation = PlayerPresentation::SetHost(&presentationHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const MEffectTargetHost* oldTargetHost = MEffectTarget::SetHost(&targetHost);
	std::unique_ptr<MZone> zone;
	MPlayer* me = nullptr;

	Scene()
	{
		calls.clear();
		refusedClass = -1;
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Type(CREATURETYPE_SLAYER_MALE, CREATURETRIBE_SLAYER);
		Type(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
		Type(CREATURETYPE_OUSTERS, CREATURETRIBE_OUSTERS);
		// The pets' creature types.
		Type(700, CREATURETRIBE_NPC);
		Type(701, CREATURETRIBE_NPC);
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pParty = &party;
		g_pSkillAvailable = &skills;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		skillManager.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		g_pSkillManager = &skillManager;
		g_pModifyStatusManager = &quietStatus;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		generators.Add(new FakeGenerator);
		g_pEffectGeneratorTable = &generators;
		g_pInventoryEffectManager = &inventoryEffects;
		experience.m_PetExp.Init(60);
		g_pExperienceTable = &experience;
		g_pUserOption = &userOption;
		InitGameStringTable();
		gameMessages.Init(4, 256);
		g_pGameMessage = &gameMessages;
		systemMessages.Init(4, 256);
		g_pSystemMessage = &systemMessages;
		gpC_item = &uiItem;
		gpC_mouse_pointer = &mouse;
		g_Mode = MODE_WAIT_UPDATEINFO;
		g_char_slot_ingame = S_SLOT();
		Items();
		zone.reset(new MZone(40, 30));
		g_pZone = zone.get();
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
		me->SetID(77);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
		me->SetZone(g_pZone);
	}
	~Scene()
	{
		// The zone first: its pets ask for the player as they go.
		zone.reset();
		g_pZone = oldZone;
		delete me;
		g_pPlayer = oldPlayer;
		inventory.Release();
		slayerGear.Release();
		vampireGear.Release();
		oustersGear.Release();
		mouse.DropItem();
		MItem::SetHost(NULL);
		MEffectTarget::SetHost(oldTargetHost);
		DialogPresentation::SetHost(oldDialog);
		PlayerPresentation::SetHost(oldPresentation);
		CreatureState::SetHost(oldCreatureHost);
		ZoneState::SetHost(oldZoneHost);
		PlayerState::SetHost(oldPlayerHost);
		std::copy(oldHolyLand.begin(), oldHolyLand.end(), g_abHolyLandBonusSkills);
		g_char_slot_ingame = oldSlot;
		g_Mode = decltype(g_Mode)(oldMode);
		g_pArmsBand2 = oldArmsBand2;
		g_pArmsBand1 = oldArmsBand1;
		g_pQuickSlot = oldQuickSlot;
		g_pCurrentMagazine = oldMagazine;
		gpC_mouse_pointer = oldMouse;
		gpC_item = oldUiItem;
		g_pSystemMessage = oldSystemMessages;
		g_pGameMessage = oldGameMessages;
		g_pUserOption = oldUserOption;
		g_pExperienceTable = oldExperience;
		g_pInventoryEffectManager = oldInventoryEffects;
		g_pEffectGeneratorTable = oldGenerators;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pModifyStatusManager = oldModify;
		g_pSkillManager = oldSkillManager;
		g_pSkillInfoTable = oldSkillInfo;
		g_pSkillAvailable = oldSkills;
		g_pParty = oldParty;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	void Type(int type, CREATURETRIBE tribe)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, 0);
		testfw::MutableRow(creatureTypes, type).bMale = true;
	}
	// The item classes the tests make, each of the race that wears it.
	void Items()
	{
		Class(ITEM_CLASS_POTION, 2, 1, 1, 0);
		Class(ITEM_CLASS_SWORD, 1, 1, 3, FLAG_RACE_SLAYER);
		Class(ITEM_CLASS_HELM, 2, 2, 2, FLAG_RACE_SLAYER);
		// The frames a slayer wears them in.
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 0).AddonMaleFrameID = 40;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 1).AddonMaleFrameID = 41;
		Class(ITEM_CLASS_SG, 1, 1, 3, FLAG_RACE_SLAYER);
		Class(ITEM_CLASS_SMG, 1, 1, 3, FLAG_RACE_SLAYER);
		Class(ITEM_CLASS_AR, 1, 1, 3, FLAG_RACE_SLAYER);
		Class(ITEM_CLASS_SR, 1, 1, 3, FLAG_RACE_SLAYER);
		Class(ITEM_CLASS_BELT, 1, 2, 1, FLAG_RACE_SLAYER);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).Value3 = 3;
		Class(ITEM_CLASS_OUSTERS_ARMSBAND, 1, 2, 1, FLAG_RACE_OUSTERS);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 2;
		Class(ITEM_CLASS_PUPA, 1, 1, 1, FLAG_RACE_OUSTERS);
		Class(ITEM_CLASS_VAMPIRE_NECKLACE, 1, 1, 1, FLAG_RACE_VAMPIRE);
		Class(ITEM_CLASS_OUSTERS_CIRCLET, 1, 1, 1, FLAG_RACE_OUSTERS);
		Class(ITEM_CLASS_BLOOD_BIBLE_SIGN, HOLYLAND_BONUS_MAX, 1, 1, FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS);
		Class(ITEM_CLASS_PET_ITEM, 6, 1, 1, 0);
		// The magazines of the shotgun, the rifle and the sub-machine gun.
		Class(ITEM_CLASS_MAGAZINE, 3, 1, 1, FLAG_RACE_SLAYER);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 0).Value1 = ITEM_CLASS_SG;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_AR;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 2).Value1 = ITEM_CLASS_SMG;
	}
	void Class(int itemClass, int types, int width, int height, int race)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int type = 0; type < types; ++type)
		{
			testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(BYTE(width), BYTE(height));
			testfw::MutableRow(*g_pItemTable, itemClass, type).Race = BYTE(race);
		}
	}
	// An item of the class and type held in the inventory at (x, y).
	MItem* Hold(int itemClass, int type, TYPE_OBJECTID id, int x, int y)
	{
		MItem* pItem = MItem::NewItem(ITEM_CLASS(itemClass));
		pItem->SetItemType(TYPE_ITEMTYPE(type));
		pItem->SetID(id);
		inventory.AddItem(pItem, BYTE(x), BYTE(y));
		return pItem;
	}
	std::vector<std::string> Messages(CMessageArray& log)
	{
		std::vector<std::string> lines;
		for (int i = 0; i < log.GetSize(); i++)
			if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
		return lines;
	}
};

// A slot of the packet: class, type and object id first, the rest as the
// test sets it.
template <class Slot>
Slot* NewSlot(int itemClass, int type, ObjectID_t id)
{
	Slot* pSlot = new Slot;
	pSlot->setItemClass(BYTE(itemClass));
	pSlot->setItemType(ItemType_t(type));
	pSlot->setObjectID(id);
	return pSlot;
}

InventorySlotInfo* InventorySlot(int itemClass, int type, ObjectID_t id, int x, int y)
{
	InventorySlotInfo* pSlot = NewSlot<InventorySlotInfo>(itemClass, type, id);
	pSlot->setInvenX(CoordInven_t(x));
	pSlot->setInvenY(CoordInven_t(y));
	return pSlot;
}

GearSlotInfo* GearSlot(int itemClass, int type, ObjectID_t id, int slot)
{
	GearSlotInfo* pSlot = NewSlot<GearSlotInfo>(itemClass, type, id);
	pSlot->setSlotID(SlotID_t(slot));
	return pSlot;
}

SubItemInfo* SubItem(int itemClass, int type, ObjectID_t id, int number, int slot)
{
	SubItemInfo* pSub = new SubItemInfo();
	pSub->setItemClass(BYTE(itemClass));
	pSub->setItemType(ItemType_t(type));
	pSub->setObjectID(id);
	pSub->setItemNum(ItemNum_t(number));
	pSub->setSlotID(SlotID_t(slot));
	return pSub;
}

// A packet's list: the count it announces is the number of slots.
template <class Info, class Slot>
void Add(Info& info, std::initializer_list<Slot*> slots)
{
	for (Slot* pSlot : slots) info.addListElement(pSlot);
	info.setListNum(BYTE(slots.size()));
}

EffectInfo Effects(std::initializer_list<std::pair<int, int>> statuses)
{
	EffectInfo info;
	for (const auto& status : statuses) info.addListElement(EffectID_t(status.first), WORD(status.second));
	return info;
}

std::string Options(const MItem* pItem)
{
	std::string text;
	for (TYPE_ITEM_OPTION option : pItem->GetItemOptionList()) text += (text.empty() ? "" : ",") + N(option);
	return text;
}

} // namespace

//----------------------------------------------------------------------
// The inventory starts empty, of the game's size; each item takes the
// slot's id, type, options, place, number, colour, durability, silver,
// grade and enchantment. Then the player checks every item it holds, and
// the effects drawn on the inventory are let go.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventoryInfoPlacesEachItemAsTheSlotSays)
{
	Scene scene;
	scene.inventory.Init(3, 3);
	scene.Hold(ITEM_CLASS_POTION, 0, 900, 0, 0);
	InventorySlotInfo* pPotion = InventorySlot(ITEM_CLASS_POTION, 1, 501, 9, 5);
	pPotion->setOptionType({ 2, 3 });
	pPotion->setItemNum(7);
	pPotion->setMainColor(120);
	pPotion->setDurability(33);
	pPotion->setSilver(4);
	pPotion->setGrade(5);
	pPotion->setEnchantLevel(6);
	InventoryInfo info;
	Add(info, { pPotion, InventorySlot(ITEM_CLASS_SWORD, 0, 502, 0, 0) });
	calls.clear();

	SetInventoryInfo(&info);

	CHECK_EQ(INVENTORY_WIDTH, scene.inventory.GetWidth());
	CHECK_EQ(INVENTORY_HEIGHT, scene.inventory.GetHeight());
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(900)) == NULL);
	const MItem* pItem = scene.inventory.GetItem(TYPE_OBJECTID(501));
	CHECK(pItem != NULL && pItem == scene.inventory.GetItem(BYTE(9), BYTE(5)));
	if (pItem == NULL) return;
	CHECK_EQ(ITEM_CLASS_POTION, pItem->GetItemClass());
	CHECK_EQ(1, pItem->GetItemType());
	CHECK(Options(pItem) == "2,3");
	CHECK_EQ(7, pItem->GetNumber());
	CHECK_EQ(120, pItem->GetItemColorSet());
	CHECK_EQ(33, pItem->GetCurrentDurability());
	CHECK_EQ(4, pItem->GetSilver());
	CHECK_EQ(5, pItem->GetGrade());
	CHECK_EQ(6, pItem->GetEnchantLevel());
	CHECK(scene.inventory.GetItem(BYTE(0), BYTE(2)) != NULL && scene.inventory.GetItem(BYTE(0), BYTE(2))->GetID() == 502);
	// Each item is checked as it goes in, and all of them again after.
	CHECK(Same({ "destroyed|900", "affect|501", "affect|502", "affect|501", "affect|502", "inventory effects released" }));
}

//----------------------------------------------------------------------
// A gun takes a magazine instead of a number: the first magazine type of
// the gun's class (the last type when none is), eight types on for
// silver bullets, without options and holding the slot's number.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventoryGunsGetAMagazineOfTheirClass)
{
	Scene scene;
	InventorySlotInfo* pRifle = InventorySlot(ITEM_CLASS_AR, 0, 511, 0, 0);
	pRifle->setItemNum(30);
	InventorySlotInfo* pSilver = InventorySlot(ITEM_CLASS_SMG, 0, 512, 1, 0);
	pSilver->setItemNum(20);
	pSilver->setSilver(2);
	InventorySlotInfo* pSniper = InventorySlot(ITEM_CLASS_SR, 0, 513, 2, 0);
	pSniper->setItemNum(10);
	InventoryInfo info;
	Add(info, { pRifle, pSilver, pSniper });

	SetInventoryInfo(&info);

	const int expected[3][4] = { { 511, 1, 30, 0 }, { 512, 2 + 8, 20, 2 }, { 513, 2, 10, 0 } };
	for (const auto& gun : expected)
	{
		const MGunItem* pGun = dynamic_cast<const MGunItem*>(scene.inventory.GetItem(TYPE_OBJECTID(gun[0])));
		CHECK(pGun != NULL && pGun->GetMagazine() != NULL);
		if (pGun == NULL || pGun->GetMagazine() == NULL) continue;
		CHECK_EQ(0, pGun->GetMagazine()->GetID());
		CHECK_EQ(gun[1], pGun->GetMagazine()->GetItemType());
		CHECK_EQ(gun[2], pGun->GetMagazine()->GetNumber());
		CHECK_EQ(0, pGun->GetMagazine()->GetItemOptionListCount());
		CHECK_EQ(gun[3], pGun->GetSilver());
	}
}

//----------------------------------------------------------------------
// A belt's and an arms band's items go in the pockets their slots name,
// with their id, type and number; another item's are passed over, and a
// pocket the container does not have takes nothing: that item is freed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventoryContainersFillTheirPockets)
{
	Scene scene;
	InventorySlotInfo* pBelt = InventorySlot(ITEM_CLASS_BELT, 0, 521, 0, 0);
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 1, 601, 2, 0));
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 602, 5, 2));
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 605, 1, 3));
	InventorySlotInfo* pBand = InventorySlot(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 522, 3, 0);
	pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 603, 4, 1));
	pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 606, 1, 2));
	InventorySlotInfo* pSword = InventorySlot(ITEM_CLASS_SWORD, 0, 523, 6, 0);
	pSword->addListElement(SubItem(ITEM_CLASS_POTION, 0, 604, 1, 0));
	pSword->setItemNum(1);
	InventoryInfo info;
	Add(info, { pBelt, pBand, pSword });

	SetInventoryInfo(&info);

	const MBelt* pPlacedBelt = dynamic_cast<const MBelt*>(scene.inventory.GetItem(TYPE_OBJECTID(521)));
	CHECK(pPlacedBelt != NULL);
	if (pPlacedBelt != NULL)
	{
		CHECK_EQ(3, pPlacedBelt->GetSize());
		const MItem* pFirst = pPlacedBelt->GetItem(0);
		CHECK(pFirst != NULL && pFirst->GetID() == 601 && pFirst->GetItemType() == 1 && pFirst->GetNumber() == 2);
		CHECK(pPlacedBelt->GetItem(1) == NULL);
		const MItem* pThird = pPlacedBelt->GetItem(2);
		CHECK(pThird != NULL && pThird->GetID() == 602 && pThird->GetNumber() == 5);
	}
	const MOustersArmsBand* pPlacedBand = dynamic_cast<const MOustersArmsBand*>(scene.inventory.GetItem(TYPE_OBJECTID(522)));
	CHECK(pPlacedBand != NULL);
	if (pPlacedBand != NULL)
	{
		CHECK(pPlacedBand->GetItem(0) == NULL);
		const MItem* pPupa = pPlacedBand->GetItem(1);
		CHECK(pPupa != NULL && pPupa->GetID() == 603 && pPupa->GetItemClass() == ITEM_CLASS_PUPA && pPupa->GetNumber() == 4);
	}
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(523)) != NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(604)) == NULL);
	CHECK_EQ(1, Count("destroyed|605"));
	CHECK_EQ(1, Count("destroyed|606"));
}

//----------------------------------------------------------------------
// A slot that is missing, of a class or type the item table does not
// have, of a class the factory cannot make, or of an item that does not
// fit where it says is passed over (the last one freed), and the next one
// still placed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventorySkipsWhatCannotBePlaced)
{
	Scene scene;
	refusedClass = ITEM_CLASS_HELM;
	InventoryInfo info;
	info.addListElement(nullptr);
	Add(info, { InventorySlot(MAX_ITEM_CLASS, 0, 531, 0, 0), InventorySlot(ITEM_CLASS_SWORD, 1, 532, 0, 0),
		InventorySlot(ITEM_CLASS_HELM, 0, 533, 0, 0), InventorySlot(ITEM_CLASS_SWORD, 0, 534, 0, 0),
		InventorySlot(ITEM_CLASS_POTION, 0, 535, 0, 1), InventorySlot(ITEM_CLASS_POTION, 0, 536, 5, 5) });
	info.setListNum(7);
	calls.clear();

	SetInventoryInfo(&info);

	for (TYPE_OBJECTID id : { 531u, 532u, 533u, 535u })
		CHECK(scene.inventory.GetItem(id) == NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(534)) != NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(536)) != NULL);
	CHECK_EQ(1, Count("destroyed|535"));
	CHECK_EQ(2, scene.inventory.GetItemNum());
}

//----------------------------------------------------------------------
// No info still empties the inventory and lets its effects go; without an
// effect manager there is nothing to let go.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventoryInfoNoneStillEmptiesTheInventory)
{
	Scene scene;
	scene.Hold(ITEM_CLASS_POTION, 0, 900, 0, 0);
	calls.clear();

	SetInventoryInfo(NULL);

	CHECK_EQ(0, scene.inventory.GetItemNum());
	CHECK(Same({ "destroyed|900", "inventory effects released" }));

	g_pInventoryEffectManager = NULL;
	calls.clear();
	SetInventoryInfo(NULL);
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// A pocket item of a class or type outside the item table is freed and
// passed over like any other the pocket cannot take: its container and
// the pocket items after it still go in, the slots after it are read,
// the items are checked and the inventory's effects let go.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, InventorySkipsAPocketItemOutsideTheTable)
{
	Scene scene;
	SubItemInfo* pPastClass = SubItem(MAX_ITEM_CLASS, 0, 607, 1, 0);
	SubItemInfo* pPastType = SubItem(ITEM_CLASS_PUPA, 1, 608, 1, 0);
	InventorySlotInfo* pBelt = InventorySlot(ITEM_CLASS_BELT, 0, 541, 2, 0);
	pBelt->addListElement(pPastClass);
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 609, 1, 1));
	InventorySlotInfo* pBand = InventorySlot(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 543, 6, 0);
	pBand->addListElement(pPastType);
	InventoryInfo info;
	Add(info, { InventorySlot(ITEM_CLASS_POTION, 0, 540, 0, 0), pBelt, pBand, InventorySlot(ITEM_CLASS_POTION, 0, 542, 5, 0) });
	calls.clear();

	SetInventoryInfo(&info);

	const MBelt* pPlacedBelt = dynamic_cast<const MBelt*>(scene.inventory.GetItem(TYPE_OBJECTID(541)));
	CHECK(pPlacedBelt != NULL);
	if (pPlacedBelt != NULL)
	{
		CHECK(pPlacedBelt->GetItem(0) == NULL);
		CHECK(pPlacedBelt->GetItem(1) != NULL && pPlacedBelt->GetItem(1)->GetID() == 609);
	}
	const MOustersArmsBand* pPlacedBand = dynamic_cast<const MOustersArmsBand*>(scene.inventory.GetItem(TYPE_OBJECTID(543)));
	CHECK(pPlacedBand != NULL && pPlacedBand->GetItem(0) == NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(540)) != NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(542)) != NULL);
	CHECK_EQ(4, scene.inventory.GetItemNum());
	CHECK(Freed(pPastClass));
	CHECK(Freed(pPastType));
	// Each item checked as it goes in and again after; the effects let go.
	CHECK_EQ(2, Count("affect|542"));
	CHECK(!calls.empty() && calls.back() == "inventory effects released");
}

//----------------------------------------------------------------------
// A slayer's gear starts empty; each item takes the slot's id, type,
// options, grade, slot, colour, durability and enchantment (a gun its
// magazine and not the silver, anything else its number and silver), and
// goes on the slayer's gear and on the player's body; one the gear refuses
// is freed, and so is a pocket item the belt refuses. The blood bible follows, then the player checks its gear, its
// skills and its stats, and the sheet shows its to-hit.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, GearInfoDressesTheSlayer)
{
	Scene scene;
	MItem* pOld = MItem::NewItem(ITEM_CLASS_HELM);
	pOld->SetItemType(0);
	pOld->SetID(950);
	scene.slayerGear.AddItem(pOld, MSlayerGear::GEAR_SLAYER_HELM);
	scene.skills.AddSkill(ACTIONINFO(SKILL_ATTACK_MELEE));
	scene.me->SetStatus(MODIFY_TOHIT, 66);
	GearSlotInfo* pHelm = GearSlot(ITEM_CLASS_HELM, 1, 701, MSlayerGear::GEAR_SLAYER_HELM);
	pHelm->setOptionType({ 3 });
	pHelm->setItemNum(1);
	pHelm->setSilver(3);
	pHelm->setGrade(4);
	pHelm->setMainColor(130);
	pHelm->setDurability(40);
	pHelm->setEnchantLevel(2);
	GearSlotInfo* pBelt = GearSlot(ITEM_CLASS_BELT, 0, 702, MSlayerGear::GEAR_SLAYER_BELT);
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 1, 703, 3, 1));
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 706, 1, 3));
	GearSlotInfo* pRifle = GearSlot(ITEM_CLASS_AR, 0, 704, MSlayerGear::GEAR_SLAYER_RIGHTHAND);
	pRifle->setItemNum(25);
	pRifle->setSilver(1);
	GearInfo info;
	Add(info, { pHelm, pBelt, pRifle, GearSlot(ITEM_CLASS_HELM, 0, 705, MSlayerGear::GEAR_SLAYER_COAT) });
	BloodBibleSignInfo bible;
	bible.setOpenNum(2);
	bible.getList() = { 4, 7 };
	calls.clear();

	SetGearInfo(&info, &bible);

	const MItem* pPlacedHelm = scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(pPlacedHelm != NULL && pPlacedHelm->GetID() == 701);
	if (pPlacedHelm != NULL)
	{
		CHECK_EQ(1, pPlacedHelm->GetItemType());
		CHECK(Options(pPlacedHelm) == "3");
		CHECK_EQ(1, pPlacedHelm->GetNumber());
		CHECK_EQ(3, pPlacedHelm->GetSilver());
		CHECK_EQ(4, pPlacedHelm->GetGrade());
		CHECK_EQ(MSlayerGear::GEAR_SLAYER_HELM, pPlacedHelm->GetItemSlot());
		CHECK_EQ(130, pPlacedHelm->GetItemColorSet());
		CHECK_EQ(40, pPlacedHelm->GetCurrentDurability());
		CHECK_EQ(2, pPlacedHelm->GetEnchantLevel());
	}
	const MBelt* pPlacedBelt = dynamic_cast<const MBelt*>(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BELT));
	CHECK(pPlacedBelt != NULL && g_pQuickSlot == pPlacedBelt);
	CHECK(pPlacedBelt != NULL && pPlacedBelt->GetItem(1) != NULL && pPlacedBelt->GetItem(1)->GetID() == 703);
	const MGunItem* pPlacedRifle = dynamic_cast<const MGunItem*>(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	CHECK(pPlacedRifle != NULL && pPlacedRifle->GetID() == 704 && pPlacedRifle->GetMagazine() != NULL);
	if (pPlacedRifle != NULL && pPlacedRifle->GetMagazine() != NULL)
	{
		CHECK_EQ(1 + 8, pPlacedRifle->GetMagazine()->GetItemType());
		CHECK_EQ(25, pPlacedRifle->GetMagazine()->GetNumber());
		CHECK(g_pCurrentMagazine == pPlacedRifle->GetMagazine());
		CHECK_EQ(0, pPlacedRifle->GetSilver());
	}
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_COAT) == NULL);
	CHECK_EQ(1, Count("destroyed|950"));
	CHECK_EQ(1, Count("destroyed|705"));
	CHECK_EQ(1, Count("destroyed|706"));
	// The body wears only what the gear took: the helm, not the one the
	// coat's slot refused.
	CHECK(scene.me->GetAddonInfo(ADDON_HELM).bAddon);
	CHECK_EQ(1, scene.me->GetAddonInfo(ADDON_HELM).ItemType);
	CHECK_EQ(41, scene.me->GetAddonInfo(ADDON_HELM).FrameID);

	const MItem* pFirstSign = scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1);
	const MItem* pSecondSign = scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE2);
	CHECK(pFirstSign != NULL && pFirstSign->GetItemType() == 4);
	CHECK(pSecondSign != NULL && pSecondSign->GetItemType() == 7);
	CHECK_EQ(2, scene.slayerGear.GetBloodBibleOpenSlot());
	CHECK(g_abHolyLandBonusSkills[4] && g_abHolyLandBonusSkills[7]);

	// Last, every item the gear holds is checked (the signs and the rest,
	// by id), then the stats.
	const std::vector<std::string> last = { "affect|0", "affect|1", "affect|701", "affect|702", "affect|704", "stats" };
	CHECK(calls.size() >= last.size() && std::equal(last.begin(), last.end(), calls.end() - static_cast<std::ptrdiff_t>(last.size())));
	CHECK_EQ(1, Count("stats"));
	CHECK(scene.skills.empty());
	CHECK_EQ(66, g_char_slot_ingame.TOHIT);
}

//----------------------------------------------------------------------
// A vampire's and an ousters' items go on their own race's gear, which
// starts empty; what their gear, or an arms band's pockets, refuse is
// freed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, GearInfoUsesThePlayersRaceGear)
{
	{
		Scene scene;
		scene.me->SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
		scene.vampireGear.SetBloodBibleOpenSlot(4);
		GearInfo info;
		Add(info, { GearSlot(ITEM_CLASS_VAMPIRE_NECKLACE, 0, 711, MVampireGear::GEAR_VAMPIRE_NECKLACE),
			GearSlot(ITEM_CLASS_HELM, 0, 712, MVampireGear::GEAR_VAMPIRE_COAT) });
		calls.clear();

		SetGearInfo(&info);

		const MItem* pNecklace = scene.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_NECKLACE);
		CHECK(pNecklace != NULL && pNecklace->GetID() == 711);
		CHECK(scene.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_COAT) == NULL);
		CHECK_EQ(1, Count("destroyed|712"));
		CHECK_EQ(0, scene.vampireGear.GetBloodBibleOpenSlot());
		CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_NECKLACE) == NULL);
		CHECK_EQ(1, Count("stats"));
	}
	{
		Scene scene;
		scene.me->SetCreatureType(CREATURETYPE_OUSTERS);
		scene.oustersGear.SetBloodBibleOpenSlot(4);
		GearSlotInfo* pBand = GearSlot(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 723, MOustersGear::GEAR_OUSTERS_ARMSBAND1);
		pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 724, 1, 0));
		pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 725, 1, 2));
		GearInfo info;
		Add(info, { GearSlot(ITEM_CLASS_OUSTERS_CIRCLET, 0, 721, MOustersGear::GEAR_OUSTERS_CIRCLET),
			GearSlot(ITEM_CLASS_VAMPIRE_NECKLACE, 0, 722, MOustersGear::GEAR_OUSTERS_COAT), pBand });
		calls.clear();

		SetGearInfo(&info);

		const MItem* pCirclet = scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET);
		CHECK(pCirclet != NULL && pCirclet->GetID() == 721);
		CHECK(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_COAT) == NULL);
		CHECK_EQ(1, Count("destroyed|722"));
		const MOustersArmsBand* pPlacedBand = dynamic_cast<const MOustersArmsBand*>(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_ARMSBAND1));
		CHECK(pPlacedBand != NULL && pPlacedBand->GetItem(0) != NULL && pPlacedBand->GetItem(0)->GetID() == 724);
		CHECK_EQ(1, Count("destroyed|725"));
		CHECK_EQ(0, scene.oustersGear.GetBloodBibleOpenSlot());
		CHECK_EQ(1, Count("stats"));
	}
}

//----------------------------------------------------------------------
// No info still empties the gear and checks the player, and leaves the
// blood bible alone.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, GearInfoNoneStillEmptiesTheGear)
{
	Scene scene;
	MItem* pOld = MItem::NewItem(ITEM_CLASS_HELM);
	pOld->SetItemType(0);
	pOld->SetID(950);
	scene.slayerGear.AddItem(pOld, MSlayerGear::GEAR_SLAYER_HELM);
	scene.skills.AddSkill(ACTIONINFO(SKILL_ATTACK_MELEE));
	scene.me->SetStatus(MODIFY_TOHIT, 65);
	g_abHolyLandBonusSkills[3] = true;
	BloodBibleSignInfo bible;
	bible.setOpenNum(1);
	bible.getList() = { 5 };
	calls.clear();

	SetGearInfo(NULL, &bible);

	CHECK(Same({ "destroyed|950", "stats" }));
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1) == NULL);
	CHECK(g_abHolyLandBonusSkills[3]);
	CHECK(!g_abHolyLandBonusSkills[5]);
	CHECK(scene.skills.empty());
	CHECK_EQ(65, g_char_slot_ingame.TOHIT);
}

//----------------------------------------------------------------------
// A slot of a class or type the item table does not have, or of a class
// the factory cannot make, is passed over and the next one still worn.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, GearSkipsWhatCannotBeMade)
{
	Scene scene;
	refusedClass = ITEM_CLASS_SWORD;
	GearInfo info;
	Add(info, { GearSlot(MAX_ITEM_CLASS, 0, 731, MSlayerGear::GEAR_SLAYER_HELM), GearSlot(ITEM_CLASS_HELM, 2, 732, MSlayerGear::GEAR_SLAYER_HELM),
		GearSlot(ITEM_CLASS_SWORD, 0, 733, MSlayerGear::GEAR_SLAYER_RIGHTHAND), GearSlot(ITEM_CLASS_HELM, 1, 734, MSlayerGear::GEAR_SLAYER_HELM) });

	SetGearInfo(&info);

	const MItem* pHelm = scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(pHelm != NULL && pHelm->GetID() == 734);
	CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == NULL);
	CHECK_EQ(1, scene.slayerGear.GetItemNum());
}

//----------------------------------------------------------------------
// A pocket item outside the item table is freed and passed over: the belt
// and the pocket items after it still go on, the slots after it are read,
// the blood bible is applied and the player's gear, skills and stats are
// checked; an ousters' arms band likewise.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, GearSkipsAPocketItemOutsideTheTable)
{
	{
		Scene scene;
		scene.skills.AddSkill(ACTIONINFO(SKILL_ATTACK_MELEE));
		scene.me->SetStatus(MODIFY_TOHIT, 64);
		SubItemInfo* pPastType = SubItem(ITEM_CLASS_POTION, 5, 743, 1, 0);
		GearSlotInfo* pBelt = GearSlot(ITEM_CLASS_BELT, 0, 742, MSlayerGear::GEAR_SLAYER_BELT);
		pBelt->addListElement(pPastType);
		pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 745, 1, 1));
		GearInfo info;
		Add(info, { GearSlot(ITEM_CLASS_HELM, 0, 741, MSlayerGear::GEAR_SLAYER_HELM), pBelt,
			GearSlot(ITEM_CLASS_SWORD, 0, 744, MSlayerGear::GEAR_SLAYER_RIGHTHAND) });
		BloodBibleSignInfo bible;
		bible.setOpenNum(1);
		bible.getList() = { 5 };
		calls.clear();

		SetGearInfo(&info, &bible);

		CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) != NULL);
		const MBelt* pPlacedBelt = dynamic_cast<const MBelt*>(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BELT));
		CHECK(pPlacedBelt != NULL);
		if (pPlacedBelt != NULL)
		{
			CHECK(pPlacedBelt->GetItem(0) == NULL);
			CHECK(pPlacedBelt->GetItem(1) != NULL && pPlacedBelt->GetItem(1)->GetID() == 745);
		}
		CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) != NULL);
		CHECK(scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1) != NULL);
		CHECK(Freed(pPastType));
		const std::vector<std::string> last = { "affect|0", "affect|741", "affect|742", "affect|744", "stats" };
		CHECK(calls.size() >= last.size() && std::equal(last.begin(), last.end(), calls.end() - static_cast<std::ptrdiff_t>(last.size())));
		CHECK(scene.skills.empty());
		CHECK_EQ(64, g_char_slot_ingame.TOHIT);
	}
	{
		Scene scene;
		scene.me->SetCreatureType(CREATURETYPE_OUSTERS);
		SubItemInfo* pPastType = SubItem(ITEM_CLASS_PUPA, 1, 747, 1, 0);
		GearSlotInfo* pBand = GearSlot(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 746, MOustersGear::GEAR_OUSTERS_ARMSBAND1);
		pBand->addListElement(pPastType);
		GearInfo info;
		Add(info, { pBand, GearSlot(ITEM_CLASS_OUSTERS_CIRCLET, 0, 748, MOustersGear::GEAR_OUSTERS_CIRCLET) });
		calls.clear();

		SetGearInfo(&info);

		const MOustersArmsBand* pPlacedBand = dynamic_cast<const MOustersArmsBand*>(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_ARMSBAND1));
		CHECK(pPlacedBand != NULL && pPlacedBand->GetItem(0) == NULL);
		CHECK(scene.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET) != NULL);
		CHECK(Freed(pPastType));
		CHECK_EQ(1, Count("stats"));
	}
}

//----------------------------------------------------------------------
// The blood bible: the signs in the open slots are taken off and freed,
// the open count is the packet's, a sign of each listed type goes in the
// next slot (its id the slot's number) and only the listed bonuses hold.
// No info changes nothing.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, BloodBibleReplacesTheSignsOfEachRace)
{
	struct RaceGear { int creatureType; MPlayerGear* (*gear)(Scene&); int (*open)(Scene&); int first; };
	const RaceGear races[] = {
		{ CREATURETYPE_SLAYER_MALE, [](Scene& s) -> MPlayerGear* { return &s.slayerGear; },
			[](Scene& s) { return int(s.slayerGear.GetBloodBibleOpenSlot()); }, MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1 },
		{ CREATURETYPE_VAMPIRE_MALE1, [](Scene& s) -> MPlayerGear* { return &s.vampireGear; },
			[](Scene& s) { return int(s.vampireGear.GetBloodBibleOpenSlot()); }, MVampireGear::GEAR_VAMPIRE_BLOOD_BIBLE1 },
		{ CREATURETYPE_OUSTERS, [](Scene& s) -> MPlayerGear* { return &s.oustersGear; },
			[](Scene& s) { return int(s.oustersGear.GetBloodBibleOpenSlot()); }, MOustersGear::GEAR_OUSTERS_BLOOD_BIBLE1 },
	};
	for (const RaceGear& race : races)
	{
		Scene scene;
		scene.me->SetCreatureType(TYPE_CREATURETYPE(race.creatureType));
		MPlayerGear* pGear = race.gear(scene);
		BloodBibleSignInfo old;
		old.setOpenNum(2);
		old.getList() = { 1, 2 };
		SetBloodBibleSlot(&old);
		for (int i = 0; i < HOLYLAND_BONUS_MAX; ++i) g_abHolyLandBonusSkills[i] = true;
		SetBloodBibleSlot(NULL);
		CHECK(g_abHolyLandBonusSkills[0]);
		BloodBibleSignInfo info;
		info.setOpenNum(3);
		info.getList() = { 4, 7, 9 };
		calls.clear();

		SetBloodBibleSlot(&info);

		CHECK_EQ(1, Count("destroyed|0"));
		CHECK_EQ(1, Count("destroyed|1"));
		CHECK_EQ(3, race.open(scene));
		const int types[] = { 4, 7, 9 };
		for (int i = 0; i < 3; ++i)
		{
			const MItem* pSign = pGear->GetItem(BYTE(race.first + i));
			CHECK(pSign != NULL && pSign->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN && pSign->GetItemType() == types[i] && pSign->GetID() == TYPE_OBJECTID(i));
		}
		for (int i = 0; i < HOLYLAND_BONUS_MAX; ++i)
			CHECK_EQ(i == 4 || i == 7 || i == 9, g_abHolyLandBonusSkills[i]);
	}
}

//----------------------------------------------------------------------
// The signs' types and count are the server's: a type past the twelve
// bonuses marks none, and a sign past the six slots is freed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, BloodBibleSignsPastTheBonusesOrTheSlotsAreKeptOut)
{
	Scene scene;
	scene.me->SetCreatureType(TYPE_CREATURETYPE(CREATURETYPE_SLAYER_MALE));
	BloodBibleSignInfo info;
	info.setOpenNum(6);
	info.getList() = { 3, HOLYLAND_BONUS_MAX, 255, 1, 2, 4, 5 };
	calls.clear();
	SetBloodBibleSlot(&info);
	for (int i = 0; i < HOLYLAND_BONUS_MAX; ++i)
		CHECK_EQ(i == 1 || i == 2 || i == 3 || i == 4 || i == 5, g_abHolyLandBonusSkills[i]);
	// The gear takes the signs the item table knows; the ones it refuses
	// (types 12 and 255, past the table) and the seventh, which no slot
	// takes, are made and freed.
	for (int i = 0; i < 6; ++i)
		CHECK_EQ(i != 1 && i != 2, scene.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER(MSlayerGear::GEAR_SLAYER_BLOOD_BIBLE1 + i)) != NULL);
	CHECK_EQ(1, Count("destroyed|1"));
	CHECK_EQ(1, Count("destroyed|2"));
	CHECK_EQ(1, Count("destroyed|6"));
	// The signs the gear took stay.
	for (const char* kept : {"destroyed|0", "destroyed|3", "destroyed|4", "destroyed|5"})
		CHECK_EQ(0, Count(kept));
}

//----------------------------------------------------------------------
// The item the mouse held is freed and let go; then each slot's item is
// made like the inventory's and picked up, less a pocket item its
// container refuses, which is freed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, ExtraInfoPutsTheItemOnTheMouse)
{
	Scene scene;
	MItem* pHeld = MItem::NewItem(ITEM_CLASS_POTION);
	pHeld->SetItemType(0);
	pHeld->SetID(990);
	scene.mouse.PickUpItem(pHeld);
	calls.clear();
	SetExtraInfo(NULL);
	CHECK(Same({ "destroyed|990", "drop" }));
	CHECK(scene.mouse.GetPickUpItem() == NULL);

	ExtraSlotInfo* pBelt = NewSlot<ExtraSlotInfo>(ITEM_CLASS_BELT, 0, 751);
	pBelt->setOptionType({ 1 });
	pBelt->setItemNum(1);
	pBelt->setMainColor(140);
	pBelt->setDurability(50);
	pBelt->setSilver(6);
	pBelt->setGrade(7);
	pBelt->setEnchantLevel(8);
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 1, 752, 9, 2));
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 0, 753, 1, 3));
	ExtraInfo info;
	Add(info, { pBelt });
	calls.clear();

	SetExtraInfo(&info);

	CHECK(Same({ "destroyed|753", "pick up|751" }));
	const MBelt* pPicked = dynamic_cast<const MBelt*>(scene.mouse.GetPickUpItem());
	CHECK(pPicked != NULL && pPicked->GetID() == 751);
	if (pPicked == NULL) return;
	CHECK_EQ(0, pPicked->GetItemType());
	CHECK(Options(pPicked) == "1");
	CHECK_EQ(1, pPicked->GetNumber());
	CHECK_EQ(140, pPicked->GetItemColorSet());
	CHECK_EQ(50, pPicked->GetCurrentDurability());
	CHECK_EQ(6, pPicked->GetSilver());
	CHECK_EQ(7, pPicked->GetGrade());
	CHECK_EQ(8, pPicked->GetEnchantLevel());
	CHECK(pPicked->GetItem(2) != NULL && pPicked->GetItem(2)->GetID() == 752 && pPicked->GetItem(2)->GetNumber() == 9);
}

//----------------------------------------------------------------------
// A gun on the mouse takes its magazine; an arms band its pockets.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, ExtraInfoGunsAndArmsBandsAreFilled)
{
	Scene scene;
	ExtraSlotInfo* pRifle = NewSlot<ExtraSlotInfo>(ITEM_CLASS_AR, 0, 761);
	pRifle->setItemNum(12);
	pRifle->setSilver(1);
	ExtraInfo rifle;
	Add(rifle, { pRifle });

	SetExtraInfo(&rifle);

	const MGunItem* pGun = dynamic_cast<const MGunItem*>(scene.mouse.GetPickUpItem());
	CHECK(pGun != NULL && pGun->GetMagazine() != NULL);
	if (pGun != NULL && pGun->GetMagazine() != NULL)
	{
		CHECK_EQ(1 + 8, pGun->GetMagazine()->GetItemType());
		CHECK_EQ(12, pGun->GetMagazine()->GetNumber());
		CHECK_EQ(1, pGun->GetSilver());
	}

	ExtraSlotInfo* pBand = NewSlot<ExtraSlotInfo>(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 762);
	pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 763, 2, 0));
	pBand->addListElement(SubItem(ITEM_CLASS_PUPA, 0, 764, 1, 2));
	ExtraInfo band;
	Add(band, { pBand });
	calls.clear();

	SetExtraInfo(&band);

	// The rifle frees its magazine as it goes; the pocket the band does
	// not have takes nothing.
	CHECK(Same({ "destroyed|0", "destroyed|761", "drop", "destroyed|764", "pick up|762" }));
	const MOustersArmsBand* pPicked = dynamic_cast<const MOustersArmsBand*>(scene.mouse.GetPickUpItem());
	CHECK(pPicked != NULL && pPicked->GetItem(0) != NULL && pPicked->GetItem(0)->GetID() == 763);
}

//----------------------------------------------------------------------
// A class the factory cannot make, or one outside the item table, is
// passed over (its slot freed) and the next one still picked up; a pocket
// item outside the table is freed and its container picked up without it.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, ExtraInfoSkipsAnItemOutsideTheTable)
{
	Scene scene;
	refusedClass = ITEM_CLASS_SWORD;
	ExtraSlotInfo* pRefused = NewSlot<ExtraSlotInfo>(ITEM_CLASS_SWORD, 0, 771);
	ExtraSlotInfo* pPastType = NewSlot<WatchedExtraSlot>(ITEM_CLASS_HELM, 7, 773);
	ExtraSlotInfo* pPotion = NewSlot<ExtraSlotInfo>(ITEM_CLASS_POTION, 0, 772);
	ExtraInfo info;
	Add(info, { pRefused, pPastType, pPotion });
	calls.clear();

	SetExtraInfo(&info);

	CHECK(Same({ "slot freed|773", "pick up|772" }));
	CHECK(scene.mouse.GetPickUpItem() != NULL && scene.mouse.GetPickUpItem()->GetID() == 772);

	SubItemInfo* pPastClass = SubItem(MAX_ITEM_CLASS, 0, 776, 1, 0);
	ExtraSlotInfo* pBelt = NewSlot<ExtraSlotInfo>(ITEM_CLASS_BELT, 0, 775);
	pBelt->addListElement(pPastClass);
	pBelt->addListElement(SubItem(ITEM_CLASS_POTION, 1, 777, 1, 1));
	ExtraInfo pocket;
	Add(pocket, { pBelt });
	calls.clear();

	SetExtraInfo(&pocket);

	CHECK(Same({ "destroyed|772", "drop", "pick up|775" }));
	const MBelt* pPickedBelt = dynamic_cast<const MBelt*>(scene.mouse.GetPickUpItem());
	CHECK(pPickedBelt != NULL && pPickedBelt->GetItem(0) == NULL && pPickedBelt->GetItem(1) != NULL && pPickedBelt->GetItem(1)->GetID() == 777);
	CHECK(Freed(pPastClass));

	SubItemInfo* pPupaPastType = SubItem(ITEM_CLASS_PUPA, 1, 779, 1, 0);
	ExtraSlotInfo* pBand = NewSlot<ExtraSlotInfo>(ITEM_CLASS_OUSTERS_ARMSBAND, 0, 778);
	pBand->addListElement(pPupaPastType);
	ExtraInfo band;
	Add(band, { pBand });
	calls.clear();

	SetExtraInfo(&band);

	CHECK(Same({ "destroyed|777", "destroyed|775", "drop", "pick up|778" }));
	const MOustersArmsBand* pPickedBand = dynamic_cast<const MOustersArmsBand*>(scene.mouse.GetPickUpItem());
	CHECK(pPickedBand != NULL && pPickedBand->GetItem(0) == NULL);
	CHECK(Freed(pPupaPastType));
}
//----------------------------------------------------------------------
// Each status lasts its duration in frames less the frames the loading
// took, and only while some remain; a blood drain's duration counts ten
// times, and a coma lays the creature dead first.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, EffectInfoAddsWhatRemainsOfEachStatus)
{
	Scene scene;
	std::unique_ptr<EffectProbe> creature(new EffectProbe);
	EffectInfo none;
	SetEffectInfo(creature.get(), NULL, 0);
	SetEffectInfo(creature.get(), &none, 0);
	CHECK(Same({}));

	// 16 frames a second: 10 tenths are 16 frames, 6 are 9, 5 are 8.
	EffectInfo info = Effects({ { EFFECTSTATUS_POISON, 10 }, { EFFECTSTATUS_DARKNESS, 5 }, { EFFECTSTATUS_CURSE_DOOM, 6 },
		{ EFFECTSTATUS_BLOOD_DRAIN, 3 }, { EFFECTSTATUS_COMA, 20 } });
	SetEffectInfo(creature.get(), &info, 8);

	CHECK(Same({ "status|" + N(EFFECTSTATUS_POISON) + "|8", "status|" + N(EFFECTSTATUS_CURSE_DOOM) + "|1",
		"status|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|40", "corpse", "status|" + N(EFFECTSTATUS_COMA) + "|24" }));
	CHECK_EQ(0, info.GetList().size());
}

//----------------------------------------------------------------------
// The five statuses with an effect of their own start it at the creature
// when the status table names one: a back stab and the ground elemental
// for 18 frames and 0xFFFF, the soul stone's ice for 19, a trap for its
// duration in frames; only the ground elemental starts at the action's
// main node. Each then lasts the effect's time in frames.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, EffectInfoStartsTheEffectOfTheFiveThatShowOne)
{
	Scene scene;
	const int statuses[] = { EFFECTSTATUS_BACK_STAB_3, EFFECTSTATUS_ICE_OF_SOUL_STONE, EFFECTSTATUS_TRAP_TRIGGERED,
		EFFECTSTATUS_TRAPPED, EFFECTSTATUS_GROUND_ELEMENTAL_CENTER };
	for (int i = 0; i < 5; ++i)
	{
		const int action = 200 + i;
		testfw::MutableRow(scene.effectStatuses, statuses[i]).ActionInfo = TYPE_ACTIONINFO(action);
		scene.actions.GetMutable(action)->Init(4);
		for (int node = 0; node < 4; ++node)
		{
			scene.actions.GetMutable(action)->GetMutable(node)->EffectGeneratorID = 7;
			scene.actions.GetMutable(action)->GetMutable(node)->bDelayNode = true;
		}
		scene.actions.GetMutable(action)->SetMainNode(2);
	}
	std::unique_ptr<EffectProbe> creature(new EffectProbe);
	creature->SetID(55);
	EffectInfo info = Effects({ { EFFECTSTATUS_BACK_STAB_3, 100 }, { EFFECTSTATUS_ICE_OF_SOUL_STONE, 100 },
		{ EFFECTSTATUS_TRAP_TRIGGERED, 10 }, { EFFECTSTATUS_TRAPPED, 20 }, { EFFECTSTATUS_GROUND_ELEMENTAL_CENTER, 100 } });

	SetEffectInfo(creature.get(), &info, 2);

	CHECK(Same({
		"effect|200|55|delay 18|phase 1", "status|" + N(EFFECTSTATUS_BACK_STAB_3) + "|26",
		"effect|201|55|delay 19|phase 1", "status|" + N(EFFECTSTATUS_ICE_OF_SOUL_STONE) + "|28",
		"effect|202|55|delay 16|phase 1", "status|" + N(EFFECTSTATUS_TRAP_TRIGGERED) + "|23",
		"effect|203|55|delay 32|phase 1", "status|" + N(EFFECTSTATUS_TRAPPED) + "|49",
		"effect|204|55|delay 65535|phase 3", "status|" + N(EFFECTSTATUS_GROUND_ELEMENTAL_CENTER) + "|104854",
	}));
}

//----------------------------------------------------------------------
// Without an action in the status table they start nothing and keep the
// packet's duration.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, EffectInfoWithoutAnActionKeepsTheDuration)
{
	Scene scene;
	std::unique_ptr<EffectProbe> creature(new EffectProbe);
	EffectInfo info = Effects({ { EFFECTSTATUS_BACK_STAB_3, 10 }, { EFFECTSTATUS_ICE_OF_SOUL_STONE, 10 },
		{ EFFECTSTATUS_TRAP_TRIGGERED, 10 }, { EFFECTSTATUS_TRAPPED, 10 }, { EFFECTSTATUS_GROUND_ELEMENTAL_CENTER, 10 } });

	SetEffectInfo(creature.get(), &info, 0);

	CHECK(Same({ "status|" + N(EFFECTSTATUS_BACK_STAB_3) + "|16", "status|" + N(EFFECTSTATUS_ICE_OF_SOUL_STONE) + "|16",
		"status|" + N(EFFECTSTATUS_TRAP_TRIGGERED) + "|16", "status|" + N(EFFECTSTATUS_TRAPPED) + "|16",
		"status|" + N(EFFECTSTATUS_GROUND_ELEMENTAL_CENTER) + "|16" }));
}

namespace {

// A creature of the zone, which owns it.
MCreature* Owner(Scene& scene, TYPE_OBJECTID id)
{
	MCreature* pOwner = new MCreature;
	pOwner->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	pOwner->SetID(id);
	pOwner->SetPosition(10, 12);
	pOwner->SetServerPosition(10, 12);
	scene.zone->AddCreature(pOwner);
	return pOwner;
}

// A pet following the creature, as the zone holds it.
MFakeCreature* PetOf(Scene& scene, MCreature* pOwner, MPetItem* pItem = NULL)
{
	MFakeCreature* pPet = new MFakeCreature;
	pPet->SetOwnerID(pOwner->GetID());
	pPet->SetPetItem(pItem);
	scene.zone->AddFakeCreature(pPet);
	pOwner->SetPetID(pPet->GetID());
	return pPet;
}

PetInfo Pet(int type, int level)
{
	PetInfo pet;
	pet.setPetType(PetType_t(type));
	pet.setPetCreatureType(MonsterType_t(700));
	pet.setPetLevel(PetLevel_t(level));
	pet.setPetExp(30);
	pet.setPetHP(80);
	pet.setPetAttr(0);
	pet.setPetAttrLevel(0);
	pet.setPetOption(0);
	pet.setFoodType(6);
	pet.setGamble(0);
	pet.setCutHead(0);
	pet.setAttack(0);
	pet.setSummonInfo(0);
	pet.setNickname("Rex");
	return pet;
}

} // namespace

//----------------------------------------------------------------------
// Pet info about a creature the zone does not have changes nothing.
// "No pet" takes a creature's pet out of the zone (or finds none), and
// for the player recomputes its stats.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoNoneTakesThePetAway)
{
	Scene scene;
	MCreature* pOwner = Owner(scene, 300);
	PetOf(scene, pOwner);
	PetInfo none;
	calls.clear();

	SetPetInfo(&none, 12345);
	CHECK(Same({}));
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());

	SetPetInfo(&none, 300);
	CHECK_EQ(0, scene.zone->GetFakeCreatureNumber());
	CHECK_EQ(OBJECTID_NULL, pOwner->GetPetID());
	// A pet id the zone has no pet for stays.
	pOwner->SetPetID(4242);
	SetPetInfo(&none, 300);
	CHECK_EQ(4242, pOwner->GetPetID());
	CHECK(Same({}));

	scene.zone->AddCreature(scene.me);
	SetPetInfo(&none, 77);
	CHECK(Same({ "stats" }));
	scene.zone->RemoveCreature(77);
}

//----------------------------------------------------------------------
// Another creature's pet is made from the info: a new pet item of the
// pet's type and a fake creature of its creature type where the owner
// stands, following it, coloured by the "no option" row, walking (a type
// below 4) with the info's nickname; the item takes the pet's HP,
// attribute, level, food, experience left to the next level, abilities
// and option, and names the creature. A summoned pet comes out of a
// portal.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoMakesAnotherCreaturesPet)
{
	Scene scene;
	MCreature* pOwner = Owner(scene, 300);
	testfw::MutableRow(scene.creatureTypes, 693).Name = "Wolf";
	testfw::MutableRow(*g_pItemOptionTable, 0).ColorSet = 222;
	testfw::MutableRow(scene.experience.m_PetExp, 5).AccumExp = 100;
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->Init(1);
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->GetMutable(0)->EffectGeneratorID = 7;
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->GetMutable(0)->Count = 5;
	PetInfo pet = Pet(2, 5);
	pet.setPetOption(3);
	pet.setGamble(1);
	pet.setAttack(1);
	pet.setSummonInfo(1);
	calls.clear();

	SetPetInfo(&pet, 300);

	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
	MFakeCreature* pPet = dynamic_cast<MFakeCreature*>(scene.zone->GetFakeCreature(pOwner->GetPetID()));
	CHECK(pPet != NULL);
	if (pPet == NULL) return;
	CHECK_EQ(700, pPet->GetCreatureType());
	CHECK_EQ(10, pPet->GetX());
	CHECK_EQ(12, pPet->GetY());
	CHECK_EQ(300, pPet->GetOwnerID());
	CHECK_EQ(MFakeCreature::FAKE_CREATURE_TRACE, pPet->GetFakeCreatureType());
	CHECK_EQ(MCreature::CREATURE_FAKE_GROUND, pPet->GetMoveType());
	CHECK(pPet->GetNickName() == "Rex");
	CHECK_EQ(NicknameInfo::NICK_CUSTOM, pPet->GetNickNameType());
	CHECK_EQ(222, pPet->GetBodyColor1());
	CHECK_EQ(222, pPet->GetBodyColor2());
	CHECK(std::string("Wolf") == pPet->GetName());
	MPetItem* pItem = pPet->GetPetItem();
	CHECK(pItem != NULL);
	if (pItem == NULL) return;
	CHECK_EQ(2, pItem->GetItemType());
	CHECK_EQ(222, pItem->GetItemColorSet());
	CHECK_EQ(80, pItem->GetCurrentDurability());
	CHECK_EQ(0, pItem->GetEnchantLevel());
	CHECK_EQ(0, pItem->GetSilver());
	CHECK_EQ(5, pItem->GetNumber());
	CHECK_EQ(6, pItem->GetPetFoodType());
	CHECK_EQ(70, pItem->GetPetExpRemain());
	CHECK(!pItem->IsCanCutHead());
	CHECK(pItem->IsCanGamble());
	CHECK(pItem->IsCanAttack());
	CHECK(Options(pItem) == "3");
	CHECK(Same({ "effect|" + N(RESULT_MAGIC_BLOODY_TUNNEL_INTO) + "|" + N(pPet->GetID()) + "|delay 5|phase end" }));
	CHECK(scene.Messages(scene.systemMessages).empty());
}

//----------------------------------------------------------------------
// A pet of type 4 or more flies; one not summoned comes without a portal;
// option 0 is none. A pet with an attribute takes the colour of the first
// option row of that part that does not upgrade.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoFlyingPetsAndAttributeColours)
{
	Scene scene;
	MCreature* pOwner = Owner(scene, 300);
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->Init(1);
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->GetMutable(0)->EffectGeneratorID = 7;
	scene.actions.GetMutable(RESULT_MAGIC_BLOODY_TUNNEL_INTO)->GetMutable(0)->Count = 5;
	// The pet item names a type 4 pet after creature type 706.
	testfw::MutableRow(scene.creatureTypes, 706).Name = "Hawk";
	const int part = ITEMOPTION_TABLE::PART_VISION;
	testfw::MutableRow(*g_pItemOptionTable, 1).Part = part;
	testfw::MutableRow(*g_pItemOptionTable, 1).UpgradeOptionType = 2;
	testfw::MutableRow(*g_pItemOptionTable, 1).ColorSet = 311;
	testfw::MutableRow(*g_pItemOptionTable, 2).Part = part;
	testfw::MutableRow(*g_pItemOptionTable, 2).ColorSet = 322;
	testfw::MutableRow(*g_pItemOptionTable, 3).Part = part;
	testfw::MutableRow(*g_pItemOptionTable, 3).ColorSet = 333;
	PetInfo pet = Pet(4, 5);
	pet.setPetAttr(PetAttr_t(part));
	pet.setPetAttrLevel(2);
	calls.clear();

	SetPetInfo(&pet, 300);

	MFakeCreature* pPet = dynamic_cast<MFakeCreature*>(scene.zone->GetFakeCreature(pOwner->GetPetID()));
	CHECK(pPet != NULL && pPet->GetPetItem() != NULL);
	if (pPet == NULL || pPet->GetPetItem() == NULL) return;
	CHECK_EQ(MCreature::CREATURE_FAKE_FLYING, pPet->GetMoveType());
	CHECK_EQ(322, pPet->GetBodyColor1());
	CHECK_EQ(322, pPet->GetBodyColor2());
	CHECK_EQ(322, pPet->GetPetItem()->GetItemColorSet());
	CHECK_EQ(part, pPet->GetPetItem()->GetEnchantLevel());
	CHECK_EQ(2, pPet->GetPetItem()->GetSilver());
	CHECK_EQ(0, pPet->GetPetItem()->GetItemOptionListCount());
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// The player's own pet is the pet item in its inventory; when the info is
// about the pet it already has, a level gained (the message by the
// level's last digit, an attribute at 10, an option at 49, training while
// it may gamble without the second ability), a second or third ability
// won, or a gamble lost is told, and the item lock lifted. The pet takes
// the info's creature type, a summoned one says so, and the player's
// stats are recomputed.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoTellsThePlayerAboutItsPet)
{
	struct Case
	{
		int level, gamble, cutHead, attack, summon;
		bool itemGamble, itemCutHead, itemAttack, teen;
		std::vector<std::string> messages;
		bool unlocked;
	};
	const Case cases[] = {
		{ 10, 1, 0, 0, 1, true, false, false, false, { "two Cat 10", "You can give your pet an attribute.", "You can train your pet." }, false },
		{ 12, 1, 1, 0, 0, true, false, false, false, { "one Cat 12", "You gave Cat a secondary ability.", "You can loot monster heads along with items." }, true },
		{ 12, 1, 1, 0, 0, true, false, false, true, { "one Cat 12", "You gave Cat a secondary ability.", "You can loot soul stones along with items." }, true },
		{ 49, 1, 1, 1, 0, true, true, false, false, { "one Cat 49", "You can give Cat an option.", "You gave Cat a third ability." }, true },
		{ 9, 0, 1, 0, 0, true, true, false, false, { "Granting the third ability failed." }, true },
		{ 9, 0, 0, 0, 0, true, false, false, false, { "Granting the secondary ability failed." }, true },
	};
	for (const Case& c : cases)
	{
		Scene scene;
		g_pGameStringTable->Set(STRING_MESSAGE_NEW_PET_LEVEL_1, "one %s %d");
		g_pGameStringTable->Set(STRING_MESSAGE_NEW_PET_LEVEL_2, "two %s %d");
		testfw::MutableRow(scene.creatureTypes, 700).Name = "Cat";
		scene.userOption.UseTeenVersion = c.teen ? TRUE : FALSE;
		scene.zone->AddCreature(scene.me);
		// PetInfo has no setter for its item's id, so the item is id 0.
		MPetItem* pItem = dynamic_cast<MPetItem*>(scene.Hold(ITEM_CLASS_PET_ITEM, 3, 0, 0, 0));
		pItem->SetNumber(9 == c.level ? 9 : c.level - 1);
		pItem->SetPetGamble(c.itemGamble);
		pItem->SetPetCutHead(c.itemCutHead);
		pItem->SetPetAttack(c.itemAttack);
		MFakeCreature* pPet = PetOf(scene, scene.me, pItem);
		pPet->SetCreatureType(CREATURETYPE_SLAYER_MALE);
		PetInfo pet = Pet(3, c.level);
		pet.setPetCreatureType(701);
		pet.setGamble(BYTE(c.gamble));
		pet.setCutHead(BYTE(c.cutHead));
		pet.setAttack(BYTE(c.attack));
		pet.setSummonInfo(BYTE(c.summon));
		calls.clear();

		SetPetInfo(&pet, 77);

		CHECK(scene.Messages(scene.gameMessages) == c.messages);
		CHECK_EQ(c.unlocked ? 1 : 0, Count("unlock item"));
		CHECK_EQ(701, pPet->GetCreatureType());
		CHECK(pPet->GetPetItem() == pItem);
		CHECK_EQ(c.level, pItem->GetNumber());
		CHECK_EQ(c.cutHead != 0, pItem->IsCanCutHead());
		CHECK_EQ(1, Count("stats"));
		const std::vector<std::string> summoned = { "You summoned Cat." };
		CHECK(scene.Messages(scene.systemMessages) == (c.summon ? summoned : std::vector<std::string>()));
		scene.zone->RemoveCreature(77);
	}
}

//----------------------------------------------------------------------
// The player's pet info naming an item its inventory does not hold makes
// no pet, and still recomputes the player's stats.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoWithoutThePlayersItemMakesNoPet)
{
	Scene scene;
	scene.zone->AddCreature(scene.me);
	PetInfo pet = Pet(3, 9);
	calls.clear();

	SetPetInfo(&pet, 77);

	CHECK_EQ(0, scene.zone->GetFakeCreatureNumber());
	CHECK_EQ(OBJECTID_NULL, scene.me->GetPetID());
	CHECK(Same({ "stats" }));
	scene.zone->RemoveCreature(77);
}

//----------------------------------------------------------------------
// Another creature's pet it already has keeps its creature type and takes
// the new item: the item it had is not freed (the test frees it). A pet
// id the zone has no pet for ends the info with nothing made.
//----------------------------------------------------------------------
TEST(PacketPlayerItems, PetInfoForAnotherCreaturesPetItAlreadyHas)
{
	Scene scene;
	MCreature* pOwner = Owner(scene, 300);
	testfw::MutableRow(scene.creatureTypes, 693).Name = "Wolf";
	MPetItem* pOld = dynamic_cast<MPetItem*>(MItem::NewItem(ITEM_CLASS_PET_ITEM));
	pOld->SetItemType(1);
	pOld->SetID(880);
	MFakeCreature* pPet = PetOf(scene, pOwner, pOld);
	pPet->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	PetInfo pet = Pet(2, 7);
	calls.clear();

	SetPetInfo(&pet, 300);

	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
	CHECK_EQ(CREATURETYPE_SLAYER_MALE, pPet->GetCreatureType());
	CHECK(pPet->GetPetItem() != NULL && pPet->GetPetItem() != pOld);
	CHECK(pPet->GetPetItem() != NULL && pPet->GetPetItem()->GetItemType() == 2 && pPet->GetPetItem()->GetNumber() == 7);
	CHECK_EQ(0, Count("destroyed|880"));
	delete pOld;

	pOwner->SetPetID(4242);
	calls.clear();
	SetPetInfo(&pet, 300);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
	CHECK_EQ(4242, pOwner->GetPetID());
	CHECK(Same({}));
}
