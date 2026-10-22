//----------------------------------------------------------------------
// test_item_containers.cpp
//----------------------------------------------------------------------
//
// The item managers (docs/reviews/RESTRUCTURING.md task 4.3, first slice):
// MItemManager, the id map every container is built on; MGridItemManager,
// the inventory-style grid where an item covers width x height cells;
// MSlotItemManager, the numbered slots a belt or an arms band offers;
// and MBelt, the gear item that is also a slot container. The bounds
// the managers enforce on network-supplied coordinates and slots are
// pinned here, and so is the ownership rule (Release deletes what the
// manager holds).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MItemManager.h"
#include "MGridItemManager.h"
#include "MSlotItemManager.h"
#include "MItemFinder.h"

namespace {

//----------------------------------------------------------------------
// The tables the items read: swords of three footprints, a belt with
// three pockets.
//----------------------------------------------------------------------
// Managers own their items, so tests hand them heap objects; the count
// of live ones proves Release's ownership. Each fixture starts it at 0.
int	s_Alive = 0;

struct ContainerWorld : GameModelWorld
{
	ContainerWorld()
	{
		s_Alive = 0;
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 3);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(2, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).SetGrid(1, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 2).SetGrid(3, 3);
		g_pItemTable->InitClass(ITEM_CLASS_BELT, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BELT, 0).SetValue(0, 0, 3);	// Value3: pockets
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).SetGrid(1, 1);
	}
};

struct Sword : public MItem
{
	Sword(TYPE_OBJECTID id, TYPE_ITEMTYPE type)	{ SetID(id); SetItemType(type); s_Alive++; }
	~Sword()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_SWORD; }
};

struct Quick : public MItem
{
	Quick(TYPE_OBJECTID id)						{ SetID(id); s_Alive++; }
	~Quick()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_POTION; }
	bool		IsQuickItem() const				{ return true; }
	TYPE_ITEM_NUMBER	GetMaxNumber() const	{ return 5; }
};

} // namespace

//----------------------------------------------------------------------
// The id map
//----------------------------------------------------------------------
TEST(ItemManager, HoldsOneItemPerIdAndFindsByPredicate)
{
	ContainerWorld world;
	MItemManager manager;

	Sword* a = new Sword(1, 0);
	Sword* twin = new Sword(1, 1);		// the same id again
	Quick* q = new Quick(2);

	CHECK(manager.AddItem(a));
	CHECK_EQ(false, manager.AddItem(twin));
	CHECK(manager.AddItem(q));
	CHECK_EQ(2, manager.GetItemNum());

	CHECK(manager.GetItem(1) == a);
	CHECK(manager.GetItem(2) == q);
	CHECK(manager.GetItem(3) == NULL);
	CHECK(manager.RemoveItem(3) == NULL);

	MItemClassFinder swords(ITEM_CLASS_SWORD);
	CHECK(manager.FindItem(swords) == a);
	MItemClassFinder belts(ITEM_CLASS_BELT);
	CHECK(manager.FindItem(belts) == NULL);

	// Removing hands the item back; the manager no longer owns it.
	CHECK(manager.RemoveItem(2) == q);
	CHECK(manager.GetItem(2) == NULL);
	CHECK_EQ(1, manager.GetItemNum());

	delete twin;
	delete q;
	CHECK_EQ(1, s_Alive);
	manager.Release();
	CHECK_EQ(0, s_Alive);
	CHECK_EQ(0, manager.GetItemNum());
}

//----------------------------------------------------------------------
// The grid
//----------------------------------------------------------------------
TEST(GridItemManager, PlacesAtTheGivenCellAndRejectsOverlapSpillAndOutOfRange)
{
	ContainerWorld world;
	MGridItemManager grid;
	grid.Init(4, 3);
	CHECK_EQ(4, (int)grid.GetWidth());
	CHECK_EQ(3, (int)grid.GetHeight());

	Sword* wide = new Sword(1, 0);		// 2 x 1
	CHECK(grid.AddItem(wide, 0, 0));
	CHECK(grid.GetItem(0, 0) == wide);
	CHECK(grid.GetItem(1, 0) == wide);
	CHECK(grid.GetItem(2, 0) == NULL);
	CHECK(grid.GetItem(1) == wide);
	CHECK_EQ(0, (int)wide->GetGridX());
	CHECK_EQ(0, (int)wide->GetGridY());

	Sword* small = new Sword(2, 1);		// 1 x 1
	CHECK_EQ(false, grid.AddItem(small, 1, 0));		// occupied
	CHECK_EQ(false, grid.AddItem(small, 4, 0));		// past the width
	CHECK_EQ(false, grid.AddItem(small, 0, 3));		// past the height
	CHECK(grid.GetItem(4, 0) == NULL);
	CHECK(grid.GetItem(0, 3) == NULL);

	Sword* wide2 = new Sword(3, 0);
	CHECK_EQ(false, grid.AddItem(wide2, 3, 2));		// would spill past the edge
	CHECK(grid.AddItem(wide2, 2, 2));
	CHECK(grid.GetItem(3, 2) == wide2);

	CHECK(grid.AddItem(small, 3, 0));
	CHECK_EQ(3, grid.GetItemNum());

	grid.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(GridItemManager, AutomaticPlacementFillsColumnsTopDown)
{
	ContainerWorld world;
	MGridItemManager grid;
	grid.Init(2, 2);

	Sword* wide = new Sword(1, 0);		// 2 x 1 takes the whole first row
	CHECK(grid.AddItem(wide));
	CHECK_EQ(0, (int)wide->GetGridX());
	CHECK_EQ(0, (int)wide->GetGridY());

	Sword* a = new Sword(2, 1);
	CHECK(grid.AddItem(a));
	CHECK_EQ(0, (int)a->GetGridX());
	CHECK_EQ(1, (int)a->GetGridY());

	Sword* b = new Sword(3, 1);
	CHECK(grid.AddItem(b));
	CHECK_EQ(1, (int)b->GetGridX());
	CHECK_EQ(1, (int)b->GetGridY());

	POINT where;
	Sword* c = new Sword(4, 1);
	CHECK_EQ(false, grid.GetFitPosition(c, where));
	CHECK_EQ(false, grid.AddItem(c));
	delete c;

	// A footprint larger than the grid never fits.
	Sword* huge = new Sword(5, 2);		// 3 x 3
	CHECK_EQ(false, grid.GetFitPosition(huge, where));
	CHECK_EQ(false, grid.AddItem(huge));
	CHECK_EQ(false, grid.AddItem(huge, 0, 0));
	delete huge;

	grid.Release();
	CHECK_EQ(0, s_Alive);
}

TEST(GridItemManager, RemovingClearsEveryCellTheItemCovered)
{
	ContainerWorld world;
	MGridItemManager grid;
	grid.Init(4, 2);

	Sword* wide = new Sword(1, 0);
	Sword* wide2 = new Sword(2, 0);
	CHECK(grid.AddItem(wide, 0, 0));
	CHECK(grid.AddItem(wide2, 0, 1));

	// By cell: either cell of the item names it.
	CHECK(grid.RemoveItem(1, 0) == wide);
	CHECK(grid.GetItem(0, 0) == NULL);
	CHECK(grid.GetItem(1, 0) == NULL);
	CHECK(grid.GetItem(1) == NULL);
	CHECK(grid.RemoveItem(1, 0) == NULL);
	CHECK(grid.RemoveItem(4, 0) == NULL);

	// By id.
	CHECK(grid.RemoveItem(2) == wide2);
	CHECK(grid.GetItem(0, 1) == NULL);
	CHECK(grid.GetItem(1, 1) == NULL);
	CHECK(grid.RemoveItem(2) == NULL);
	CHECK_EQ(0, grid.GetItemNum());

	// The cells are free again.
	POINT where;
	CHECK(grid.GetFitPosition(wide, where));
	CHECK_EQ(0, where.x);
	CHECK_EQ(0, where.y);

	delete wide;
	delete wide2;
	CHECK_EQ(0, s_Alive);
}

TEST(GridItemManager, ReplacementCoversAtMostOneOldItem)
{
	ContainerWorld world;
	MGridItemManager grid;
	grid.Init(3, 1);

	Sword* a = new Sword(1, 1);
	Sword* b = new Sword(2, 1);
	CHECK(grid.AddItem(a, 0, 0));
	CHECK(grid.AddItem(b, 1, 0));

	MItem* old = NULL;
	Sword* wide = new Sword(3, 0);		// 2 x 1

	// Over a and b: two different items, no replacement.
	CHECK_EQ(false, grid.CanReplaceItem(wide, 0, 0, old));
	CHECK(old == NULL);

	// Over b and an empty cell: b is the one to hand out.
	CHECK(grid.CanReplaceItem(wide, 1, 0, old));
	CHECK(old == b);

	// Out of range or spilling: no.
	CHECK_EQ(false, grid.CanReplaceItem(wide, 3, 0, old));
	CHECK_EQ(false, grid.CanReplaceItem(wide, 2, 0, old));

	CHECK(grid.ReplaceItem(wide, 1, 0, old));
	CHECK(old == b);
	CHECK(grid.GetItem(1, 0) == wide);
	CHECK(grid.GetItem(2, 0) == wide);
	CHECK(grid.GetItem(2) == NULL);

	delete b;
	grid.Release();
	CHECK_EQ(0, s_Alive);
}

// The grid, the same rule: a refused newcomer must not cost the occupant
// its place, and the call must say so.
TEST(GridItemManager, RefusedReplacementKeepsTheOccupant)
{
	ContainerWorld world;
	MGridItemManager grid;
	grid.Init(3, 1);

	Sword* a = new Sword(1, 1);
	Sword* b = new Sword(2, 1);
	Sword* twinOfB = new Sword(2, 1);
	CHECK(grid.AddItem(a, 0, 0));
	CHECK(grid.AddItem(b, 1, 0));

	MItem* old = NULL;
	CHECK_EQ(false, grid.ReplaceItem(twinOfB, 0, 0, old));
	CHECK(old == NULL);
	CHECK(grid.GetItem(0, 0) == a);
	CHECK(grid.GetItem((TYPE_OBJECTID)1) == a);
	CHECK_EQ(2, grid.GetItemNum());

	delete twinOfB;
	grid.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// The slots
//----------------------------------------------------------------------
TEST(SlotItemManager, AddsGetsAndRemovesByIndexAndById)
{
	ContainerWorld world;
	MSlotItemManager slots;
	slots.Init(3);
	CHECK_EQ(3, (int)slots.GetSize());

	Quick* a = new Quick(1);
	Quick* b = new Quick(2);

	CHECK_EQ(false, slots.AddItem(a, 3));			// past the last slot
	CHECK(slots.AddItem(a, 1));
	CHECK(slots.GetItem(1) == a);
	CHECK(slots.GetItem(3) == NULL);
	CHECK_EQ(1, (int)a->GetItemSlot());
	CHECK_EQ(false, slots.AddItem(b, 1));			// occupied
	CHECK(slots.AddItem(b, 2));
	CHECK_EQ(2, slots.GetItemNum());

	CHECK(slots.RemoveItem((BYTE)1) == a);
	CHECK(slots.GetItem(1) == NULL);
	CHECK(slots.GetItem(1) == NULL);
	CHECK(slots.RemoveItem((BYTE)1) == NULL);
	CHECK(slots.RemoveItem((BYTE)3) == NULL);

	CHECK(slots.RemoveItem((TYPE_OBJECTID)2) == b);
	CHECK(slots.GetItem(2) == NULL);
	CHECK_EQ(0, slots.GetItemNum());

	delete a;
	delete b;
	CHECK_EQ(0, s_Alive);
}

TEST(SlotItemManager, ReplacementHandsTheOccupantOut)
{
	ContainerWorld world;
	MSlotItemManager slots;
	slots.Init(2);

	Quick* a = new Quick(1);
	Quick* b = new Quick(2);
	MItem* old = a;

	CHECK_EQ(false, slots.CanReplaceItem(b, 2, old));
	CHECK(old == NULL);

	// Into an empty slot: nothing comes out.
	CHECK(slots.CanReplaceItem(a, 0, old));
	CHECK(old == NULL);
	CHECK(slots.ReplaceItem(a, 0, old));
	CHECK(old == NULL);
	CHECK(slots.GetItem(0) == a);

	// Into an occupied slot: the occupant comes out and is no longer held.
	CHECK(slots.CanReplaceItem(b, 0, old));
	CHECK(old == a);
	CHECK(slots.ReplaceItem(b, 0, old));
	CHECK(old == a);
	CHECK(slots.GetItem(0) == b);
	CHECK(slots.GetItem(1) == NULL);
	CHECK_EQ(1, slots.GetItemNum());

	delete a;
	slots.Release();
	CHECK_EQ(0, s_Alive);
	CHECK_EQ(0, (int)slots.GetSize());
}

// The slot array mirrors the id map: an item the map refuses (its id is
// already held) must not be left in a slot the map knows nothing about.
TEST(SlotItemManager, RefusedIdLeavesTheSlotEmpty)
{
	ContainerWorld world;
	MSlotItemManager slots;
	slots.Init(2);

	Quick* a = new Quick(1);
	Quick* twin = new Quick(1);
	CHECK(slots.AddItem(a, 0));
	CHECK_EQ(false, slots.AddItem(twin, 1));
	CHECK(slots.GetItem(1) == NULL);
	CHECK_EQ(1, slots.GetItemNum());

	// The slot is still free for an item the map will take.
	Quick* b = new Quick(2);
	CHECK(slots.AddItem(b, 1));
	CHECK(slots.GetItem(1) == b);

	delete twin;
	slots.Release();
	CHECK_EQ(0, s_Alive);
}

// The same rule for replacement: when the map refuses the newcomer, the
// occupant stays where it was instead of being dropped on the floor.
TEST(SlotItemManager, RefusedReplacementKeepsTheOccupant)
{
	ContainerWorld world;
	MSlotItemManager slots;
	slots.Init(2);

	Quick* a = new Quick(1);
	Quick* b = new Quick(2);
	Quick* twinOfB = new Quick(2);
	CHECK(slots.AddItem(a, 0));
	CHECK(slots.AddItem(b, 1));

	MItem* old = NULL;
	CHECK_EQ(false, slots.ReplaceItem(twinOfB, 0, old));
	CHECK(old == NULL);
	CHECK(slots.GetItem(0) == a);
	CHECK(slots.MItemManager::GetItem(1) == a);	// the slot overload hides the id lookup
	CHECK_EQ(2, slots.GetItemNum());

	delete twinOfB;
	slots.Release();
	CHECK_EQ(0, s_Alive);
}

//----------------------------------------------------------------------
// A belt: gear that is a slot container sized by its table entry.
//----------------------------------------------------------------------
TEST(Belt, SizesItsPocketsFromTheTableAndTakesOnlyQuickItems)
{
	ContainerWorld world;
	MBelt belt;
	belt.SetItemType(0);
	CHECK_EQ(3, (int)belt.GetSize());
	CHECK(belt.IsContainerItem());
	CHECK(belt.IsGearSlotBelt());

	Sword* sword = new Sword(1, 1);
	CHECK_EQ(false, belt.AddItem(sword));
	CHECK_EQ(false, belt.AddItem(sword, 0));
	delete sword;

	Quick* a = new Quick(2);
	CHECK(belt.AddItem(a));
	CHECK(belt.GetItem(0) == a);

	// The next free pocket, or one holding the same kind with room.
	int slot = -1;
	Quick* b = new Quick(3);
	CHECK(belt.FindSlotToAddItem(b, slot));
	CHECK_EQ(0, slot);			// a is the same kind and 1 + 1 <= 5
	a->SetNumber(5);
	CHECK(belt.FindSlotToAddItem(b, slot));
	CHECK_EQ(1, slot);			// a is full, so the next empty pocket
	CHECK(belt.AddItem(b, 1));
	CHECK_EQ(false, belt.AddItem(b, 3));

	Quick* c = new Quick(4);
	Quick* d = new Quick(5);
	CHECK(belt.AddItem(c));
	CHECK_EQ(false, belt.AddItem(d));		// no pocket left
	delete d;

	belt.Release();
	CHECK_EQ(0, s_Alive);
}
