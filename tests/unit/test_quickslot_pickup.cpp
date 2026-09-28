//----------------------------------------------------------------------
// test_quickslot_pickup.cpp
//----------------------------------------------------------------------
//
// CanPickupItemToQuickslot decides whether an item picked up from the
// zone may go straight into the slayer belt or an ousters armsband.
// Both containers take the same checks: the item check buffer must be
// empty, no temporary mode may be pending, and the item must be usable
// by the player's race.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MQuickSlot.h"
#include "RaceType.h"

namespace {

struct QuickSlotWorld : GameModelWorld
{
	QuickSlotWorld()
	{
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
	}
};

struct QuickItem : public MItem
{
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_POTION; }
};

void	SetItemRace(BYTE race)
{
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).Race = race;
}

} // namespace

TEST(QuickSlotPickup, SlayerBeltTakesASlayerItemWhenNothingIsPending)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_SLAYER);

	CHECK(CanPickupItemToQuickslot(RACE_SLAYER, true, false, true, true, &item));
}

TEST(QuickSlotPickup, SlayerBeltRefusesWhileATempModeIsPending)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_SLAYER);

	CHECK(!CanPickupItemToQuickslot(RACE_SLAYER, true, false, true, false, &item));
}

TEST(QuickSlotPickup, SlayerBeltRefusesWhileTheItemCheckBufferIsInUse)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_SLAYER);

	CHECK(!CanPickupItemToQuickslot(RACE_SLAYER, true, false, false, true, &item));
}

TEST(QuickSlotPickup, SlayerBeltRefusesAnOustersOnlyItem)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_OUSTERS);

	CHECK(!CanPickupItemToQuickslot(RACE_SLAYER, true, false, true, true, &item));
}

TEST(QuickSlotPickup, SlayerWithoutABeltIsRefused)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_SLAYER);

	CHECK(!CanPickupItemToQuickslot(RACE_SLAYER, false, true, true, true, &item));
}

TEST(QuickSlotPickup, OustersArmsBandAppliesTheSameChecks)
{
	QuickSlotWorld world;
	QuickItem item;

	SetItemRace(FLAG_RACE_OUSTERS);
	CHECK(CanPickupItemToQuickslot(RACE_OUSTERS, false, true, true, true, &item));
	CHECK(!CanPickupItemToQuickslot(RACE_OUSTERS, false, true, true, false, &item));
	CHECK(!CanPickupItemToQuickslot(RACE_OUSTERS, false, true, false, true, &item));
	CHECK(!CanPickupItemToQuickslot(RACE_OUSTERS, true, false, true, true, &item));

	SetItemRace(FLAG_RACE_SLAYER);
	CHECK(!CanPickupItemToQuickslot(RACE_OUSTERS, false, true, true, true, &item));
}

TEST(QuickSlotPickup, VampireHasNoQuickslot)
{
	QuickSlotWorld world;
	QuickItem item;
	SetItemRace(FLAG_RACE_VAMPIRE);

	CHECK(!CanPickupItemToQuickslot(RACE_VAMPIRE, true, true, true, true, &item));
}
