//----------------------------------------------------------------------
// test_pet_option_part.cpp
//----------------------------------------------------------------------
//
// A pet's attribute is the server's byte (PetInfo's PetAttr), which
// SetPetInfo stores as the pet item's enchant level; the pet's colour and
// its option names find the attribute's rows in the item option table by
// part. The part is compared as the number it is: ITEMOPTION_PART has no
// fixed type and its values end at 63, so converting 64 to it was
// undefined, which Clang's UBSan reported at the comparison's load.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MItem.h"
#include <string>

namespace {

const int kPart = 64;

// Row 1 is another part's; rows 2 and 3 are the attribute's, row 2 one
// that changes on an upgrade (no colour of its own).
struct PetWorld : GameModelWorld
{
	PetWorld() : GameModelWorld(4)
	{
		g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
		struct Row { int part; const char* name; int colour; int upgrade; };
		const Row rows[] = { { ITEMOPTION_TABLE::PART_VISION, "Far", 311, 0 },
			{ kPart, "Keen", 322, 2 }, { kPart, "Sharp", 333, 0 } };
		for (int i = 0; i < 3; ++i)
		{
			ITEMOPTION_INFO& row = testfw::MutableRow(*g_pItemOptionTable, i + 1);
			row.Part = rows[i].part;
			row.Name = rows[i].name;
			row.EName = rows[i].name;
			row.ColorSet = rows[i].colour;
			row.UpgradeOptionType = rows[i].upgrade;
		}
	}
};

// A pet whose attribute is `part`, at attribute level 2; `number` less
// ten, in tens, is how far into the part's rows its name is.
void	GiveAttribute(MPetItem& pet, int part, int number)
{
	pet.SetEnchantLevel(WORD(part));
	pet.SetSilver(2);
	pet.SetNumber(TYPE_ITEM_NUMBER(number));
}

} // namespace

TEST(PetOptionPart, AnAttributePastTheEnumTakesTheColourOfItsFirstUnchangingRow)
{
	PetWorld world;
	MPetItem pet;
	GiveAttribute(pet, kPart, 10);
	CHECK_EQ(333, (int)pet.GetItemOptionColorSet(0));
}

TEST(PetOptionPart, AnAttributePastTheEnumIsNamedByItsRows)
{
	PetWorld world;
	MPetItem pet;
	GiveAttribute(pet, kPart, 10);
	CHECK(pet.GetPetOptionName() == "Keen ");
	CHECK(pet.GetPetOptionEName() == "Keen ");
	GiveAttribute(pet, kPart, 20);
	CHECK(pet.GetPetOptionName() == "Sharp ");
	CHECK(pet.GetPetOptionEName() == "Sharp ");
}
