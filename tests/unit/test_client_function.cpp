#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ClientFunction.h"
#include "MEffectGeneratorTable.h"
#include "MEffectGenerator.h"
#include "MActionInfoTable.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "SkillDef.h"
#include "UserInformation.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"

#include <memory>
#include <string>
#include <vector>

// The whisper target, whether the player stands somewhere safe, and the
// inventory effects (task 4.236, gamemodel's since then).
// AddNewInventoryEffect starts an item action's effect at the item's grid
// cell, with what the action does to the item as its result: here a fake
// generator declines, so the result runs at once.
namespace {

std::vector<EFFECTGENERATOR_INFO> generated;
std::vector<std::string> calls;
int safe = 0;
MItem* bufferItem = nullptr;
bool itemToItem = false;

struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return 7; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override { generated.push_back(info); return false; }
};

struct Table : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0) : m_Class(itemClass) { SetID(id); SetItemType(type); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.IsInSafeSector = []() { return safe; };
	host.IsItemCheckBufferItemToItem = []() { return itemToItem; };
	host.GetItemCheckBuffer = []() { return bufferItem; };
	host.ClearItemCheckBuffer = []() { calls.push_back("clear buffer"); itemToItem = false; bufferItem = nullptr; };
	return host;
}();
const WorldEvents::Host worldHost{};
const MEffectTargetHost targetHost{};
const MEffectHost effectHost{
	.CurrentFrame = []() -> DWORD { return 100; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};

struct Scene
{
	GameModelWorld world;
	MActionInfoTable actions;
	Table table;
	MInventory inventory;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MEffectGeneratorTable* oldTable = g_pEffectGeneratorTable;
	MInventory* oldInventory = g_pInventory;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	Scene()
	{
		generated.clear();
		calls.clear();
		safe = 0;
		bufferItem = nullptr;
		itemToItem = false;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		table.Add(new FakeGenerator);
		g_pEffectGeneratorTable = &table;
		for (ITEM_CLASS itemClass : {ITEM_CLASS_POTION, ITEM_CLASS_PET_ITEM})
		{
			g_pItemTable->InitClass(itemClass, 4);
			for (int type = 0; type < 4; ++type)
				testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
		}
		inventory.Init(4, 4);
		g_pInventory = &inventory;
	}
	~Scene()
	{
		g_pInventory = oldInventory;
		g_pEffectGeneratorTable = oldTable;
		g_pActionInfoTable = oldActions;
		MEffect::SetHost(oldEffect);
		MEffectTarget::SetHost(oldTarget);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayer);
		inventory.Release();
	}
	// An item action of `phases` phases by the fake generator.
	void Action(TYPE_ACTIONINFO action, int phases, BYTE target = FLAG_ACTIONINFO_TARGET_ITEM)
	{
		MActionInfo* info = actions.GetMutable(action);
		info->Set("action", 0, 0, 1, target);
		if (phases == 0) info->Release(); else info->Init(phases);
		for (int phase = 0; phase < phases; ++phase)
			info->GetMutable(phase)->EffectGeneratorID = 7;
	}
	Ware* Put(ITEM_CLASS itemClass, TYPE_OBJECTID id, int x, int y, TYPE_ITEMTYPE type = 0)
	{
		Ware* pItem = new Ware(itemClass, id, type);
		CHECK(inventory.AddItem(pItem, x, y));
		return pItem;
	}
};

} // namespace

TEST(ClientFunction, TheWhisperTargetIsTheUsers)
{
	Scene scene;
	g_pUserInformation->WhisperID = "Lantern";
	CHECK(std::string(GetWhisperID()) == "Lantern");
}

TEST(ClientFunction, SafetyIsThePlayersSector)
{
	Scene scene;
	for (int value : {0, 1, 2})
	{
		safe = value;
		CHECK_EQ(value, IsPlayerInSafePosition());
	}
	PlayerState::SetHost(nullptr);
	safe = 2;
	CHECK_EQ(0, IsPlayerInSafePosition());
}

TEST(ClientFunction, AnItemEffectStartsAtItsCellAndRemovesIt)
{
	Scene scene;
	scene.Action(MAGIC_ENCHANT_REMOVE_ITEM, 2);
	scene.Put(ITEM_CLASS_POTION, 50, 2, 1);
	AddNewInventoryEffect(50, MAGIC_ENCHANT_REMOVE_ITEM, 33);
	CHECK_EQ(1u, generated.size());
	CHECK_EQ(MAGIC_ENCHANT_REMOVE_ITEM, generated[0].nActionInfo);
	CHECK_EQ(2, generated[0].x0);
	CHECK_EQ(1, generated[0].y0);
	CHECK_EQ(1, generated[0].power);
	CHECK_EQ(50, generated[0].creatureID);
	CHECK_EQ(2, generated[0].x1);
	CHECK_EQ(1, generated[0].y1);
	// The generator declined: the result ran, and the item is gone.
	CHECK(scene.inventory.GetItem(50) == nullptr);
}

TEST(ClientFunction, ATransformedItemTurnsItsGender)
{
	Scene scene;
	scene.Action(MAGIC_TRANS_ITEM_OK, 1);
	MItem* pItem = scene.Put(ITEM_CLASS_POTION, 51, 0, 0, 2);
	AddNewInventoryEffect(51, MAGIC_TRANS_ITEM_OK, 0);
	CHECK_EQ(1u, generated.size());
	CHECK_EQ(3, pItem->GetItemType());
}

TEST(ClientFunction, AnEnchantedItemTakesItsOption)
{
	Scene scene;
	scene.Action(MAGIC_ENCHANT_OPTION_PLUS, 1);
	MItem* pItem = scene.Put(ITEM_CLASS_POTION, 52, 1, 1);
	AddNewInventoryEffect(52, MAGIC_ENCHANT_OPTION_PLUS, 0, 9);
	CHECK_EQ(1u, generated.size());
	CHECK((pItem->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>{9}));
	// Each word of the value is an option and what it becomes (from 0: a
	// new one). OPTION_NULL takes the same node, for anything but a pet.
	scene.Action(MAGIC_ENCHANT_OPTION_NULL, 1);
	AddNewInventoryEffect(52, MAGIC_ENCHANT_OPTION_NULL, 0, (9 << 8) | 12);
	CHECK_EQ(2u, generated.size());
	CHECK((pItem->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>{12}));
}

TEST(ClientFunction, APetsOptionRemovalOnlyDropsTheHeldItem)
{
	// A pet's option is not removed here; the item the player put on it
	// is freed and the buffer cleared, and the effect plays without a
	// result.
	Scene scene;
	scene.Action(MAGIC_ENCHANT_OPTION_NULL, 1);
	MItem* pPet = scene.Put(ITEM_CLASS_PET_ITEM, 53, 3, 3);
	pPet->AddItemOption(4);
	pPet->SetSilver(77);
	pPet->SetEnchantLevel(5);
	bufferItem = new Ware(ITEM_CLASS_POTION, 99);
	itemToItem = true;
	AddNewInventoryEffect(53, MAGIC_ENCHANT_OPTION_NULL, 0);
	CHECK((calls == std::vector<std::string>{"clear buffer"}));
	CHECK(bufferItem == nullptr);
	CHECK_EQ(1u, generated.size());
	CHECK((pPet->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>{4}));
	CHECK_EQ(77, pPet->GetSilver());
	CHECK_EQ(5, pPet->GetEnchantLevel());
	// Without an item to put on it, nothing is freed.
	AddNewInventoryEffect(53, MAGIC_ENCHANT_OPTION_NULL, 0);
	CHECK_EQ(1u, calls.size());
	CHECK_EQ(2u, generated.size());
}

TEST(ClientFunction, AnyOtherItemActionPlaysWithoutAResult)
{
	Scene scene;
	scene.Action(MAGIC_ENCHANT_OPTION_PLUS, 1);
	scene.Action(MAGIC_CREATE_HOLY_WATER, 1);
	MItem* pItem = scene.Put(ITEM_CLASS_POTION, 54, 0, 2, 2);
	AddNewInventoryEffect(54, MAGIC_CREATE_HOLY_WATER, 0, 9);
	CHECK_EQ(1u, generated.size());
	CHECK(scene.inventory.GetItem(54) == pItem);
	CHECK(pItem->GetItemOptionList().empty());
	CHECK_EQ(2, pItem->GetItemType());
}

TEST(ClientFunction, NoEffectWithoutATargetItemOrPhases)
{
	Scene scene;
	scene.Put(ITEM_CLASS_POTION, 55, 0, 0);
	// An action past the table.
	AddNewInventoryEffect(55, MAX_ACTIONINFO, 0);
	// An item not in the inventory.
	scene.Action(MAGIC_ENCHANT_REMOVE_ITEM, 1);
	AddNewInventoryEffect(56, MAGIC_ENCHANT_REMOVE_ITEM, 0);
	// An action not on items.
	scene.Action(MAGIC_ENCHANT_REMOVE_ITEM, 1, FLAG_ACTIONINFO_TARGET_OTHER);
	AddNewInventoryEffect(55, MAGIC_ENCHANT_REMOVE_ITEM, 0);
	// An action without phases.
	scene.Action(MAGIC_ENCHANT_REMOVE_ITEM, 0);
	AddNewInventoryEffect(55, MAGIC_ENCHANT_REMOVE_ITEM, 0);
	CHECK(generated.empty());
	CHECK(scene.inventory.GetItem(55) != nullptr);
}
