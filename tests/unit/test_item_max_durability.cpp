//----------------------------------------------------------------------
// test_item_max_durability.cpp
//----------------------------------------------------------------------
//
// A gear item's maximum durability must be the server's: the table
// durability moved by the grade and floored at 1000, then the
// durability options applied to that (decore::maxDurability; the
// server's ConcreteItem::getMaxDurability and computeMaxDurability).
// The expected values are rows of
// third_party/decore/domain/vectors/durability.tsv, reached through the
// real MSword (a weapon: 1000 a grade) and MHelm (500 a grade).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"

namespace {

// Option rows: every one a durability option with the plus-point
// below (100 is neutral), except row 4, which is a damage option. Row
// 0 is the empty slot.
const int	kPlusPoints[] = { 200, 150, 110, 0, 300, 120 };
const int	kOptionRows = (int)(sizeof(kPlusPoints) / sizeof(kPlusPoints[0]));

struct DurabilityWorld : GameModelWorld
{
	DurabilityWorld() : GameModelWorld(kOptionRows)
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		g_pItemTable->InitClass(ITEM_CLASS_HELM, 1);

		for (int i = 0; i < kOptionRows; i++)
		{
			testfw::MutableRow(*g_pItemOptionTable, i).Part = ITEMOPTION_TABLE::PART_DURABILITY;
			testfw::MutableRow(*g_pItemOptionTable, i).PlusPoint = kPlusPoints[i];
		}
		testfw::MutableRow(*g_pItemOptionTable, 4).Part = ITEMOPTION_TABLE::PART_DAMAGE;
	}

	void	SetDurability(ITEM_CLASS itemClass, int durability)
	{
		testfw::MutableRow(*g_pItemTable, itemClass, 0).Value1 = durability;
	}
};

} // namespace

//----------------------------------------------------------------------
// The floor comes before the options, and nothing caps the result
//----------------------------------------------------------------------
TEST(ItemMaxDurability, TheFloorComesBeforeTheOptionsAndNothingCapsIt)
{
	DurabilityWorld world;

	// floor-before-options: 500 at grade 0 is -3500, floored at 1000,
	// and a 150 option makes 1500 (not 1000 from flooring 750 after).
	world.SetDurability(ITEM_CLASS_SWORD, 500);
	MSword sword;
	sword.SetItemType(0);
	sword.SetGrade(0);
	sword.AddItemOption(1);
	CHECK_EQ(1500, sword.GetMaxDurability());

	// graded-no-options: 64000 at grade 7 is 67000, above the old 65000
	// cap the server never had.
	world.SetDurability(ITEM_CLASS_SWORD, 64000);
	MSword strong;
	strong.SetItemType(0);
	strong.SetGrade(7);
	CHECK_EQ(67000, strong.GetMaxDurability());

	// graded-then-options: 3000 at grade 6 is 5000, and 110 makes 5500.
	world.SetDurability(ITEM_CLASS_SWORD, 3000);
	MSword graded;
	graded.SetItemType(0);
	graded.SetGrade(6);
	graded.AddItemOption(2);
	CHECK_EQ(5500, graded.GetMaxDurability());

	// options-total-0: a total of 0% leaves a maximum of 0; the client
	// used to skip the options then and floor at 1000.
	MSword zeroed;
	zeroed.SetItemType(0);
	zeroed.SetGrade(4);
	zeroed.AddItemOption(3);
	CHECK_EQ(0, zeroed.GetMaxDurability());

	// A helm moves 500 a grade: 1500 at grade 0 is -500, floored at
	// 1000, and 110 makes 1100.
	world.SetDurability(ITEM_CLASS_HELM, 1500);
	MHelm helm;
	helm.SetItemType(0);
	helm.SetGrade(0);
	helm.AddItemOption(2);
	CHECK_EQ(1100, helm.GetMaxDurability());
}

//----------------------------------------------------------------------
// Which options count
//----------------------------------------------------------------------
TEST(ItemMaxDurability, OnlyDurabilityOptionsCountAndTheEmptySlotIsSkipped)
{
	DurabilityWorld world;
	world.SetDurability(ITEM_CLASS_SWORD, 3000);

	// A damage option does not move durability, and option 0, the
	// server's empty slot, never enters the item's list whatever its
	// row says (this already held; it is kept as adapter coverage).
	MSword sword;
	sword.SetItemType(0);
	sword.SetGrade(4);
	sword.AddItemOption(4);
	sword.AddItemOption(0);
	CHECK_EQ(3000, sword.GetMaxDurability());

	// options-add-their-excess: 110 and 120 are +30%, 3000 to 3900.
	sword.AddItemOption(2);
	sword.AddItemOption(5);
	CHECK_EQ(3900, sword.GetMaxDurability());
}
