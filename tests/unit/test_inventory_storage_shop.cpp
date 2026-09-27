//----------------------------------------------------------------------
// test_inventory_storage_shop.cpp
//----------------------------------------------------------------------
//
// The containers proper (docs/RESTRUCTURING.md task 4.3, second slice):
// MInventory, the player's grid, which merges piles and plays an
// item's landing sound; MStorage, the numbered storage boxes with
// their own wallet; MShopShelf, a shop's twenty slots and the factory
// that builds a shelf per type. What the containers ask of the
// executable - the player's affect check on an item, the landing
// sound - goes through MItem's host, which these tests drive by hand.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MInventory.h"
#include "MStorage.h"
#include "MShopShelf.h"
#include "MGameDef.h"
#include "MMoneyManager.h"

#include <vector>

namespace {

int	s_Alive = 0;

//----------------------------------------------------------------------
// A host that counts what the containers ask for.
//----------------------------------------------------------------------
std::vector<MItem*>		s_Refreshed;
std::vector<int>		s_Sounds;

DWORD	s_Frame = 0;
int		DropFrameCount(TYPE_FRAMEID)		{ return 0; }
void	RefreshAffect(MItem* pItem)			{ s_Refreshed.push_back(pItem); }
void	PlayItemSound(TYPE_SOUNDID sound)	{ s_Sounds.push_back(sound); }

const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, NULL,
							NULL, NULL, NULL, NULL, NULL, NULL };

struct ContainerWorld : GameModelWorld
{
	ContainerWorld()
	{
		s_Alive = 0;
		s_Refreshed.clear();
		s_Sounds.clear();

		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 2);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(2, 1);
		// (tile, inventory, gear, use): the slots differ, so a container
		// playing any slot but the inventory one fails the sound checks.
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetSoundID(170, 71, 270, 370);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).SetGrid(1, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).SetSoundID(171, 72, 271, 371);
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).SetGrid(1, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).SetSoundID(172, 73, 272, 372);

		MItem::SetHost(&s_Host);
	}
};

struct Sword : public MItem
{
	Sword(TYPE_OBJECTID id, TYPE_ITEMTYPE type)	{ SetID(id); SetItemType(type); s_Alive++; }
	~Sword()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_SWORD; }
};

// Gear that never goes in the inventory grid.
struct Worn : public Sword
{
	Worn(TYPE_OBJECTID id) : Sword(id, 1)		{}
	bool		IsInventoryItem() const			{ return false; }
};

// A pile that stacks up to five.
struct Potion : public MItem
{
	Potion(TYPE_OBJECTID id, int number)		{ SetID(id); SetNumber(number); s_Alive++; }
	~Potion()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_POTION; }
	bool		IsPileItem() const				{ return true; }
	TYPE_ITEM_NUMBER	GetMaxNumber() const	{ return 5; }
};

bool	Refreshed(const MItem* pItem)
{
	for (size_t i = 0; i < s_Refreshed.size(); i++)
	{
		if (s_Refreshed[i] == pItem)
			return true;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------
// The inventory
//----------------------------------------------------------------------
TEST(Inventory, TakesInventoryItemsOnlyAndPlaysTheirSound)
{
	ContainerWorld world;
	MInventory inventory;
	inventory.Init(4, 2);

	Worn* worn = new Worn(1);
	CHECK_EQ(false, inventory.AddItem(worn));
	CHECK_EQ(false, inventory.AddItem(worn, 0, 0));
	CHECK_EQ(0, (int)s_Sounds.size());
	delete worn;

	Sword* wide = new Sword(2, 0);
	CHECK(inventory.AddItem(wide));
	CHECK_EQ(1, (int)s_Sounds.size());
	CHECK_EQ(71, s_Sounds[0]);

	Sword* small = new Sword(3, 1);
	CHECK(inventory.AddItem(small, 0, 1));
	CHECK_EQ(2, (int)s_Sounds.size());
	CHECK_EQ(72, s_Sounds[1]);

	// A refused placement makes no sound.
	Sword* small2 = new Sword(4, 1);
	CHECK_EQ(false, inventory.AddItem(small2, 0, 1));
	CHECK_EQ(2, (int)s_Sounds.size());

	MItem* old = NULL;
	CHECK(inventory.ReplaceItem(small2, 0, 1, old));
	CHECK(old == small);
	CHECK_EQ(3, (int)s_Sounds.size());

	delete small;
	inventory.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(Inventory, AddingAsksThePlayerAboutTheItem)
{
	ContainerWorld world;
	MInventory inventory;
	inventory.Init(4, 2);

	Sword* a = new Sword(1, 1);
	Sword* b = new Sword(2, 1);
	CHECK(inventory.AddItem(a));
	CHECK(inventory.AddItem(b));
	CHECK(Refreshed(a));
	CHECK(Refreshed(b));

	// Removing re-checks what remains; the whole-inventory check asks
	// about every item once more.
	s_Refreshed.clear();
	CHECK(inventory.RemoveItem((TYPE_OBJECTID)1) == a);
	CHECK(Refreshed(b));
	CHECK_EQ(false, Refreshed(a));

	s_Refreshed.clear();
	inventory.CheckAffectStatusAll();
	CHECK_EQ(1, (int)s_Refreshed.size());
	CHECK(s_Refreshed[0] == b);

	delete a;
	inventory.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(Inventory, FindsByClassAndTypeAndMergesPiles)
{
	ContainerWorld world;
	MInventory inventory;
	inventory.Init(3, 1);

	Sword* sword = new Sword(1, 1);
	Potion* pile = new Potion(2, 3);
	CHECK(inventory.AddItem(sword, 0, 0));
	CHECK(inventory.AddItem(pile, 2, 0));

	CHECK(inventory.FindItem(ITEM_CLASS_SWORD) == sword);
	CHECK(inventory.FindItem(ITEM_CLASS_SWORD, 1) == sword);
	CHECK(inventory.FindItem(ITEM_CLASS_SWORD, 0) == NULL);
	CHECK(inventory.FindItem(ITEM_CLASS_POTION) == pile);
	CHECK(inventory.FindItem(ITEM_CLASS_BELT) == NULL);

	// A pile that fits on top of the held one lands on it, not in the gap.
	POINT where;
	Potion* two = new Potion(3, 2);
	CHECK(inventory.GetFitPosition(two, where));
	CHECK_EQ(2, where.x);
	CHECK_EQ(0, where.y);

	// One that would overflow the pile takes the free cell instead.
	Potion* three = new Potion(4, 3);
	CHECK(inventory.GetFitPosition(three, where));
	CHECK_EQ(1, where.x);
	CHECK_EQ(0, where.y);

	// A quest pile is never merged onto.
	pile->SetQuestFlag(true);
	CHECK(inventory.GetFitPosition(two, where));
	CHECK_EQ(1, where.x);

	delete two;
	delete three;
	inventory.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The storage boxes
//----------------------------------------------------------------------
TEST(Storage, HoldsTwentySlotsPerBoxAndOwnsWhatItHolds)
{
	ContainerWorld world;
	MStorage storage;

	// Before Init nothing can be stored or read.
	Sword* early = new Sword(9, 1);
	CHECK_EQ(false, storage.SetItem(0, early));
	CHECK(storage.GetItem(0) == NULL);
	CHECK(storage.GetMoneyManager() == NULL);
	delete early;

	storage.Init(2);
	CHECK_EQ(2, (int)storage.GetSize());
	CHECK(storage.GetMoneyManager() != NULL);
	CHECK_EQ(0, (int)storage.GetCurrent());

	Sword* a = new Sword(1, 1);
	CHECK_EQ(false, storage.SetItem(STORAGE_SLOT, a));
	CHECK(storage.GetItem(STORAGE_SLOT) == NULL);
	CHECK(storage.SetItem(3, a));
	CHECK(storage.GetItem(3) == a);

	// Setting over an occupant deletes the occupant.
	Sword* b = new Sword(2, 1);
	CHECK(storage.SetItem(3, b));
	CHECK(storage.GetItem(3) == b);
	CHECK_EQ(1, s_Alive);

	// Removing hands the item back.
	CHECK(storage.RemoveItem(3) == b);
	CHECK(storage.GetItem(3) == NULL);
	CHECK(storage.RemoveItem(3) == NULL);
	CHECK(storage.RemoveItem(STORAGE_SLOT) == NULL);
	CHECK(storage.SetItem(5, b));

	storage.Release();
	CHECK_EQ(0, s_Alive);
	CHECK_EQ(0, (int)storage.GetSize());
	CHECK(storage.GetMoneyManager() == NULL);
}

TEST(Storage, SwitchingBoxesReChecksTheirItemsWithThePlayer)
{
	ContainerWorld world;
	MStorage storage;
	storage.Init(2);

	Sword* inFirst = new Sword(1, 1);
	CHECK(storage.SetItem(0, inFirst));

	storage.SetCurrent(1);
	CHECK_EQ(1, (int)storage.GetCurrent());
	CHECK(storage.GetItem(0) == NULL);		// the second box is empty
	Sword* inSecond = new Sword(2, 1);
	CHECK(storage.SetItem(0, inSecond));

	s_Refreshed.clear();
	storage.SetCurrent(0);
	CHECK_EQ(0, (int)storage.GetCurrent());
	CHECK(storage.GetItem(0) == inFirst);
	CHECK_EQ(1, (int)s_Refreshed.size());
	CHECK(s_Refreshed[0] == inFirst);

	// A box that does not exist is ignored.
	storage.SetCurrent(2);
	CHECK_EQ(0, (int)storage.GetCurrent());

	storage.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The shop shelves
//----------------------------------------------------------------------
TEST(ShopShelf, FactoryBuildsEachShelfType)
{
	ContainerWorld world;

	MShopShelf* fixed = MShopShelf::NewShelf(MShopShelf::SHELF_FIXED);
	MShopShelf* special = MShopShelf::NewShelf(MShopShelf::SHELF_SPECIAL);
	MShopShelf* unknown = MShopShelf::NewShelf(MShopShelf::SHELF_UNKNOWN);
	CHECK(fixed != NULL && fixed->GetShelfType() == MShopShelf::SHELF_FIXED);
	CHECK(special != NULL && special->GetShelfType() == MShopShelf::SHELF_SPECIAL);
	CHECK(unknown != NULL && unknown->GetShelfType() == MShopShelf::SHELF_UNKNOWN);
	CHECK_EQ(false, fixed->IsEnable());
	CHECK_EQ(0, fixed->GetVersion());

	delete fixed;
	delete special;
	delete unknown;
}

// The shelf type in a shop list comes off the wire; one past the table
// must not be called through.
TEST(ShopShelf, FactoryRefusesAnUnknownType)
{
	ContainerWorld world;

	CHECK(MShopShelf::NewShelf(MShopShelf::MAX_SHELF) == NULL);
	CHECK(MShopShelf::NewShelf(MShopShelf::SHELF_BASE) == NULL);
	// As the handlers pass it: the raw wire byte, never cast to the enum
	// (that conversion is undefined for an out-of-range value).
	CHECK(MShopShelf::NewShelf(255) == NULL);
	CHECK(MShopShelf::NewShelf(-1) == NULL);
	CHECK(MShopShelf::NewShelf(0x7fffffff) == NULL);
}

TEST(ShopShelf, FillsTheFirstEmptySlotAndReplacesByDeleting)
{
	ContainerWorld world;
	MShopFixedShelf shelf;

	for (unsigned int i = 0; i < SHOP_SHELF_SLOT; i++)
	{
		CHECK(shelf.IsEmptySlot(i));
	}
	CHECK(shelf.GetItem(SHOP_SHELF_SLOT) == NULL);

	Sword* a = new Sword(1, 1);
	Sword* b = new Sword(2, 1);
	CHECK(shelf.AddItem(a));
	CHECK(shelf.AddItem(b));
	CHECK(shelf.GetItem(0) == a);
	CHECK(shelf.GetItem(1) == b);
	CHECK_EQ(false, shelf.IsEmptySlot(0));
	CHECK(Refreshed(a));
	CHECK(Refreshed(b));

	// Out of range: refused, and nothing deleted.
	Sword* c = new Sword(3, 1);
	CHECK_EQ(false, shelf.SetItem(SHOP_SHELF_SLOT, c));
	CHECK_EQ(3, s_Alive);

	// Over an occupant: the occupant is deleted.
	CHECK(shelf.SetItem(0, c));
	CHECK(shelf.GetItem(0) == c);
	CHECK_EQ(2, s_Alive);

	// Removing hands the item back; deleting does not.
	CHECK(shelf.RemoveItem(1) == b);
	CHECK(shelf.IsEmptySlot(1));
	CHECK(shelf.RemoveItem(1) == NULL);
	CHECK(shelf.RemoveItem(SHOP_SHELF_SLOT) == NULL);
	delete b;
	shelf.DeleteItem(0);
	CHECK(shelf.IsEmptySlot(0));
	CHECK_EQ(0, s_Alive);
	shelf.DeleteItem(SHOP_SHELF_SLOT);

	// A full shelf takes nothing more.
	for (unsigned int i = 0; i < SHOP_SHELF_SLOT; i++)
	{
		Sword* s = new Sword(10 + i, 1);
		CHECK(shelf.AddItem(s));
	}
	Sword* extra = new Sword(99, 1);
	CHECK_EQ(false, shelf.AddItem(extra));
	delete extra;

	shelf.Release();
	CHECK_EQ(0, s_Alive);
}
