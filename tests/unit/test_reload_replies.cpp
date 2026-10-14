#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MQuickSlot.h"
#include "MSlayerGear.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "Gpackets/GCReloadOK.h"
#include <memory>
#include <string>
#include <vector>

// A gun reloaded, as the server confirms it. A vampire takes no reload.
// The magazine waiting in the player's check buffer, used from the
// inventory or the quick slot, sounds its use and leaves the buffer. One
// of its stack goes into the gun the player holds in the right hand,
// holding the packet's bullets: the last one leaves its container, and
// of a larger stack a new magazine of the same type and options is made
// and the stack is one fewer. The player's status is calculated again.
// Without a gun in the right hand the magazine is spent all the same:
// the last one is freed, and of a stack only the new one. An item that
// is no magazine only clears the buffer; another buffer status only
// sounds.
namespace {

std::vector<std::string> calls;
Race race = RACE_SLAYER;
MItem* bufferItem = nullptr;
PlayerState::ITEM_CHECK_BUFFER bufferStatus = PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY;
// The magazines the gun has held; it frees the one it lets go of.
std::vector<std::unique_ptr<MItem>> loaded;
// Whether freeing an item is recorded: only while a reply runs.
bool recordFrees = false;

std::string N(long long value) { return std::to_string(value); }

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	bool m_Gun;
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0, bool gun = false) : m_Class(itemClass), m_Gun(gun)
	{
		SetID(id);
		SetItemType(type);
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override { return m_Class == ITEM_CLASS_MAGAZINE; }
	bool IsQuickItem() const override { return m_Class == ITEM_CLASS_MAGAZINE; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return 9; }
	bool IsGunItem() const override { return m_Gun; }
	bool IsGearItem() const override { return m_Gun || m_Class == ITEM_CLASS_SWORD; }
	bool IsGearSlotRightHand() const override { return IsGearItem(); }
};

MItemHost items = []() {
	MItemHost host{};
	// A made item comes with an ID the reply replaces.
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { calls.push_back("made|" + N(itemClass)); return new Ware(itemClass, 999); };
	// Freeing an item forgets its description (the executable's host).
	host.ItemDestroyed = [](MItem* pItem) noexcept { if (recordFrees) calls.push_back("freed|" + N(pItem->GetID())); };
	host.ReloadGun = [](MItem* pGun, MItem* pMagazine) {
		calls.push_back("reload|" + N(pGun->GetID()) + "|" + N(pMagazine->GetID()) + "|" + N(pMagazine->GetItemType()) + "|"
			+ N(pMagazine->GetNumber()) + "|" + N(pMagazine->GetItemOptionListCount()));
		loaded.clear();
		loaded.emplace_back(pMagazine);
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.GetRace = []() { return race; };
	host.GetItemCheckBuffer = []() { return bufferItem; };
	host.GetItemCheckBufferStatus = []() { return bufferStatus; };
	host.ClearItemCheckBuffer = []() { calls.push_back("cleared"); bufferItem = nullptr; };
	host.CalculateStatus = []() { calls.push_back("calculated"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

constexpr TYPE_SOUNDID kUseSound = 77;

struct World : GameModelWorld
{
	MInventory inventory;
	MBelt belt;
	MSlayerGear gear;
	MInventory* oldInventory = g_pInventory;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MSlayerGear* oldGear = g_pSlayerGear;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		loaded.clear();
		race = RACE_SLAYER;
		bufferItem = nullptr;
		for (ITEM_CLASS itemClass : {ITEM_CLASS_MAGAZINE, ITEM_CLASS_SWORD, ITEM_CLASS_SG})
		{
			g_pItemTable->InitClass(itemClass, 5);
			for (int type = 0; type < 5; type++)
			{
				ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.SetGrid(1, 1);
				row.UseSoundID = kUseSound;
			}
		}
		inventory.Init(4, 4);
		static_cast<MSlotItemManager&>(belt).Init(4);
		gear.Init();
		g_pInventory = &inventory;
		g_pQuickSlot = &belt;
		g_pSlayerGear = &gear;
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		g_pSlayerGear = oldGear;
		g_pQuickSlot = oldQuickSlot;
		g_pInventory = oldInventory;
		inventory.Release();
		bufferItem = nullptr;
		loaded.clear();
	}
	// An item in the right hand, put in the slot directly: the gear's own
	// add reads a gun as an MGunItem, which a fake is not.
	void Holding(MItem* pItem) { CHECK(static_cast<MSlotItemManager&>(gear).AddItem(pItem, MSlayerGear::GEAR_SLAYER_RIGHTHAND)); }
	// A magazine stack, in the inventory or the belt, used for the reload.
	Ware* Magazines(int number, bool quickSlot = false)
	{
		Ware* pMagazine = new Ware(ITEM_CLASS_MAGAZINE, 2001, 3);
		pMagazine->SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
		std::list<TYPE_ITEM_OPTION> options = {1, 2};
		pMagazine->SetItemOptionList(options);
		CHECK(quickSlot ? belt.AddItem(pMagazine, 1) : inventory.AddItem(pMagazine, 0, 0));
		bufferItem = pMagazine;
		bufferStatus = quickSlot ? PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT : PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY;
		return pMagazine;
	}
};

void Receive(int bullets)
{
	GCReloadOK packet;
	packet.setBulletNum(static_cast<BYTE>(bullets));
	recordFrees = true;
	GCReloadOKHandler::execute(&packet, nullptr);
	recordFrees = false;
}

} // namespace

TEST(ReloadReplies, TheLastMagazineInTheInventoryGoesInTheGun)
{
	World world;
	world.Holding(new Ware(ITEM_CLASS_SG, 1001, 0, true));
	Ware* pMagazine = world.Magazines(1);
	Receive(30);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "reload|1001|2001|3|30|2", "calculated"}));
	CHECK(world.inventory.GetItem(0, 0) == nullptr);
	CHECK(loaded.size() == 1 && loaded[0].get() == pMagazine);
}

TEST(ReloadReplies, OneOfAStackInTheInventoryIsAnewMagazine)
{
	World world;
	world.Holding(new Ware(ITEM_CLASS_SG, 1001, 0, true));
	Ware* pStack = world.Magazines(3);
	Receive(25);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "made|" + N(ITEM_CLASS_MAGAZINE), "reload|1001|0|3|25|2",
		"calculated"}));
	CHECK(world.inventory.GetItem(0, 0) == pStack && pStack->GetNumber() == 2);
	// Two are a stack too.
	calls.clear();
	bufferItem = pStack;
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY;
	Receive(24);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "made|" + N(ITEM_CLASS_MAGAZINE), "reload|1001|0|3|24|2",
		"freed|0", "calculated"}));
	CHECK(world.inventory.GetItem(0, 0) == pStack && pStack->GetNumber() == 1);
}

TEST(ReloadReplies, TheQuickSlotsMagazinesReloadAlike)
{
	World world;
	world.Holding(new Ware(ITEM_CLASS_SG, 1001, 0, true));
	Ware* pStack = world.Magazines(2, true);
	Receive(20);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "made|" + N(ITEM_CLASS_MAGAZINE), "reload|1001|0|3|20|2",
		"calculated"}));
	CHECK(world.belt.GetItem(1) == pStack && pStack->GetNumber() == 1);
	calls.clear();
	bufferItem = pStack;
	Receive(19);
	CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "reload|1001|2001|3|19|2", "freed|0", "calculated"}));
	CHECK(world.belt.GetItem(1) == nullptr && loaded.size() == 1 && loaded[0].get() == pStack);
}

TEST(ReloadReplies, AnItemThatIsNoMagazineOnlyClearsTheBuffer)
{
	World world;
	world.Holding(new Ware(ITEM_CLASS_SG, 1001, 0, true));
	for (bool quickSlot : {false, true})
	{
		calls.clear();
		Ware* pSword = new Ware(ITEM_CLASS_SWORD, 2002);
		world.inventory.AddItem(pSword, 1, 1);
		bufferItem = pSword;
		bufferStatus = quickSlot ? PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT : PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY;
		Receive(30);
		CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "calculated"}));
		CHECK(world.inventory.GetItem(1, 1) == pSword);
		delete world.inventory.RemoveItem(pSword->GetID());
	}
}

TEST(ReloadReplies, AVampireAnEmptyBufferOrAnotherStatusReloadsNothing)
{
	World world;
	world.Holding(new Ware(ITEM_CLASS_SG, 1001, 0, true));
	Ware* pMagazine = world.Magazines(1);
	race = RACE_VAMPIRE;
	Receive(30);
	CHECK(calls.empty() && bufferItem == pMagazine);
	race = RACE_OUSTERS;
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE;
	Receive(30);
	CHECK(calls == std::vector<std::string>{"sound|" + N(kUseSound)});
	CHECK(bufferItem == pMagazine && world.inventory.GetItem(0, 0) == pMagazine && loaded.empty());
	calls.clear();
	bufferItem = nullptr;
	Receive(30);
	CHECK(calls.empty());
}

TEST(ReloadReplies, WithoutAGunTheSpentMagazineIsDropped)
{
	// No gun in the right hand, or another item there.
	for (bool sword : {false, true})
	{
		for (bool quickSlot : {false, true})
		{
			World world;
			if (sword) world.Holding(new Ware(ITEM_CLASS_SWORD, 1002));
			// The last one: out of its container, its description forgotten.
			Ware* pLast = world.Magazines(1, quickSlot);
			(void)pLast;
			Receive(30);
			CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "freed|2001", "calculated"}));
			CHECK((quickSlot ? world.belt.GetItem(1) : world.inventory.GetItem(0, 0)) == nullptr && loaded.empty());
			// A stack keeps the rest.
			calls.clear();
			Ware* pStack = world.Magazines(3, quickSlot);
			Receive(30);
			CHECK(calls == (std::vector<std::string>{"sound|" + N(kUseSound), "cleared", "made|" + N(ITEM_CLASS_MAGAZINE), "freed|0", "calculated"}));
			CHECK((quickSlot ? world.belt.GetItem(1) : world.inventory.GetItem(0, 0)) == pStack && pStack->GetNumber() == 2 && loaded.empty());
		}
	}
}
