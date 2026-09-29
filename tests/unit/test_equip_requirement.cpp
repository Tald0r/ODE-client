//----------------------------------------------------------------------
// test_equip_requirement.cpp
//----------------------------------------------------------------------
//
// What an item asks of the character that uses it, and whether a
// character may. MItem's GetRequireSTR, DEX, INT, SUM and Level are
// the requirement the item descriptions show; MItem::IsUsableBy is the
// rule the player's CheckAffectStatus applies to each item it holds or
// wears. The server computes both with decore::requiredStats and
// meetsRequirement (third_party/decore/domain/EquipRequirement.h),
// pinned by the rows of third_party/decore/domain/vectors/equip.tsv,
// which decore_tests asserts; each check here names the row it takes
// its numbers from.
//
// The items are real MItem objects over a table row whose race flags,
// requirement and gender these tests set, and an option table whose
// rows carry the options' sum and level requirements.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "RaceType.h"

namespace {

//----------------------------------------------------------------------
// The option rows: each option's sum and level requirement (the
// server's OptionInfo ReqAbility "(SUM,n)(LEV,n)").
//----------------------------------------------------------------------
struct OptionRow
{
	int		sum;
	int		level;
};

const OptionRow	kOptions[] = {
	{ 0, 0 },		// 0: the "no option" row
	{ 5, 0 },		// 1
	{ 21, 0 },		// 2
	{ 0, 70 },		// 3
	{ 0, 10 },		// 4
	{ 5, 10 },		// 5
	{ 0, 120 },		// 6
	{ 0, 5 },		// 7
};
const int	kOptionRows = (int)(sizeof(kOptions) / sizeof(kOptions[0]));

struct EquipWorld : GameModelWorld
{
	EquipWorld() : GameModelWorld(kOptionRows)
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
		for (int i = 0; i < kOptionRows; i++)
		{
			testfw::MutableRow(*g_pItemOptionTable, i).RequireSUM = kOptions[i].sum;
			testfw::MutableRow(*g_pItemOptionTable, i).RequireLevel = kOptions[i].level;
		}
	}
};

// An item of a class whose MItem subclass adds nothing to the rule.
struct Gear : public MItem
{
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_SWORD; }
};

ITEMTABLE_INFO&	GearInfo()
{
	return testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0);
}

// The gear row made for `raceFlags` (FLAG_RACE_*), asking str, dex,
// int, their sum and a level, for either sex.
void	SetGear(BYTE raceFlags, int str, int dex, int intel, int sum, int level)
{
	ITEMTABLE_INFO& info = GearInfo();
	info.Race = raceFlags;
	info.SetRequireSTR((BYTE)str);
	info.SetRequireDEX((BYTE)dex);
	info.SetRequireINT((BYTE)intel);
	info.SetRequireSUM((WORD)sum);
	info.SetRequireLevel((BYTE)level);
	info.bMaleOnly = false;
	info.bFemaleOnly = false;
}

MItemUser	User(Race race, int str, int dex, int intel, int level, bool bMale)
{
	MItemUser user;
	user.race = race;
	user.str = str;
	user.dex = dex;
	user.inte = intel;
	user.level = level;
	user.bMale = bMale;
	return user;
}

} // namespace

//----------------------------------------------------------------------
// The requirement an item shows
//----------------------------------------------------------------------

// slayer-one-option: 50/40/30/120 with one option of sum 5 asks
// 60/50/40/125, twice the option's sum on each stat and once on the sum.
TEST(EquipRequirement, SlayerGearAddsTwiceEachOptionSum)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_SLAYER, 50, 40, 30, 120, 0);
	item.AddItemOption(1);

	CHECK_EQ(60, item.GetRequireSTR());
	CHECK_EQ(50, item.GetRequireDEX());
	CHECK_EQ(40, item.GetRequireINT());
	CHECK_EQ(125, item.GetRequireSUM());
	CHECK_EQ(0, item.GetRequireLevel());
}

// slayer-attr-new-cap-passed-by-2: a table asking more than 200 may be
// raised to the new cap, 290 (250 + 2 x 21 = 292).
TEST(EquipRequirement, SlayerGearAboveTheOldCapStopsAtTheNewCap)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_SLAYER, 250, 250, 250, 350, 0);
	item.AddItemOption(2);

	CHECK_EQ(290, item.GetRequireSTR());
	CHECK_EQ(290, item.GetRequireDEX());
	CHECK_EQ(290, item.GetRequireINT());
	CHECK_EQ(371, item.GetRequireSUM());
}

// ousters-one-option: 50/40/30/120 at level 50 with one option of sum
// 5 and level 10 asks 60/50/40/125 at level 60.
TEST(EquipRequirement, OustersGearAddsEachOptionToEveryNonZeroValue)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_OUSTERS, 50, 40, 30, 120, 50);
	item.AddItemOption(5);

	CHECK_EQ(60, item.GetRequireSTR());
	CHECK_EQ(50, item.GetRequireDEX());
	CHECK_EQ(40, item.GetRequireINT());
	CHECK_EQ(125, item.GetRequireSUM());
	CHECK_EQ(60, item.GetRequireLevel());
}

// ousters-level-cap-passed: an ousters' level requirement stops at 150
// (90 + 70), whatever the table's level; ousters-no-old-level-cap: it
// has no lower cap (100 + 20).
TEST(EquipRequirement, OustersLevelStopsAt150)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_OUSTERS, 0, 0, 0, 0, 90);
	item.AddItemOption(3);

	CHECK_EQ(150, item.GetRequireLevel());

	SetGear(FLAG_RACE_OUSTERS, 0, 0, 0, 0, 100);
	item.RemoveItemOption(3);
	item.AddItemOption(4);
	item.AddItemOption(4);
	CHECK_EQ(120, item.GetRequireLevel());
}

// vampire-one-option: level 50 with one option of level 10 asks 60.
TEST(EquipRequirement, VampireGearAddsEachOptionLevel)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 50);
	item.AddItemOption(4);

	CHECK_EQ(60, item.GetRequireLevel());
}

// vampire-zero-level-raised: a vampire's table level of 0 is raised by
// its options (0 + 10), and by each of several, as in
// vampire-zero-level-raised-by-several (here 0 + 10 + 10).
TEST(EquipRequirement, VampireLevelZeroIsRaisedByTheOptions)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 0);
	item.AddItemOption(4);

	CHECK_EQ(10, item.GetRequireLevel());

	item.AddItemOption(4);
	CHECK_EQ(20, item.GetRequireLevel());
}

// A slayer's and an ousters' table level of 0 is not raised, though
// every option carries a level requirement (ousters-zero-level-not-
// raised; a slayer's level is not its rule's at all).
TEST(EquipRequirement, SlayerAndOustersLevelZeroStaysZero)
{
	EquipWorld world;
	Gear item;
	item.AddItemOption(5);

	SetGear(FLAG_RACE_SLAYER, 50, 40, 30, 120, 0);
	CHECK_EQ(0, item.GetRequireLevel());

	SetGear(FLAG_RACE_OUSTERS, 50, 40, 30, 120, 0);
	CHECK_EQ(0, item.GetRequireLevel());
}

// A level is kept in 8 bits and each option is added to the kept
// value: vampire-8-bit-wrap-under-cap asks 14 of a table level of 150
// raised by 120 (270 wraps to 14, under the cap); vampire-wrap-to-zero-
// still-raised asks 5 of 136 raised by 120 (0) and then 5; and
// ousters-wrap-to-zero-skips-later-options asks nothing of the same
// item, since an ousters' level of 0 is not raised.
TEST(EquipRequirement, OptionsAddToTheLevelAtItsWidth)
{
	EquipWorld world;
	Gear item;
	item.AddItemOption(6);

	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 150);
	CHECK_EQ(14, item.GetRequireLevel());

	item.AddItemOption(7);
	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 136);
	CHECK_EQ(5, item.GetRequireLevel());

	SetGear(FLAG_RACE_OUSTERS, 0, 0, 0, 0, 136);
	CHECK_EQ(0, item.GetRequireLevel());
}

// The client's own rule, which the server has no counterpart for: a
// quest item asks nothing.
TEST(EquipRequirement, QuestItemAsksNothing)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_OUSTERS, 50, 40, 30, 120, 50);
	item.AddItemOption(5);
	item.SetQuestFlag(true);

	CHECK_EQ(0, item.GetRequireSTR());
	CHECK_EQ(0, item.GetRequireDEX());
	CHECK_EQ(0, item.GetRequireINT());
	CHECK_EQ(0, item.GetRequireSUM());
	CHECK_EQ(0, item.GetRequireLevel());
}

//----------------------------------------------------------------------
// Whether a character may use it
//----------------------------------------------------------------------

// slayer-meets-exactly and the four one-short rows: a slayer needs each
// stat and the sum.
TEST(EquipRequirement, SlayerNeedsEachStatAndTheSum)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_SLAYER, 60, 50, 40, 150, 0);

	CHECK(item.IsUsableBy(User(RACE_SLAYER, 60, 50, 40, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 59, 51, 40, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 61, 49, 40, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 61, 50, 39, 0, true)));

	// slayer-sum-one-short
	SetGear(FLAG_RACE_SLAYER, 60, 50, 40, 151, 0);
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 60, 50, 40, 0, true)));

	// slayer-no-requirement
	SetGear(FLAG_RACE_SLAYER, 0, 0, 0, 0, 0);
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, false)));
}

// vampire-meets-exactly and vampire-level-one-short.
TEST(EquipRequirement, VampireNeedsTheLevel)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 60);

	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 60, true)));
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 59, true)));
}

// ousters-meets-exactly and its one-short rows: an ousters needs each
// stat, the sum and the level.
TEST(EquipRequirement, OustersNeedsEachStatTheSumAndTheLevel)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_OUSTERS, 60, 50, 40, 150, 60);

	CHECK(item.IsUsableBy(User(RACE_OUSTERS, 60, 50, 40, 60, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 60, 50, 40, 59, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 59, 51, 40, 60, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 61, 49, 40, 60, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 61, 50, 39, 60, true)));
}

// The option-raised requirement is the one checked: 60/50/40/125 for
// slayer-one-option's item.
TEST(EquipRequirement, TheCheckReadsTheRaisedRequirement)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_SLAYER, 50, 40, 30, 120, 0);
	item.AddItemOption(1);

	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 50, 40, 30, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 60, 50, 40, 0, true)));
}

// The slayer and vampire gender rows (slayer-male-wearer-female-item and
// its kin), and ousters-ignores-gender.
TEST(EquipRequirement, GenderRestrictsSlayersAndVampiresOnly)
{
	EquipWorld world;
	Gear item;

	SetGear(FLAG_RACE_SLAYER, 10, 10, 10, 30, 0);
	GearInfo().bFemaleOnly = true;
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, false)));
	GearInfo().bFemaleOnly = false;
	GearInfo().bMaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, false)));

	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 10);
	GearInfo().bFemaleOnly = true;
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 10, true)));
	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 10, false)));

	// vampire-gender-without-level: the gender binds without a level.
	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 0);
	GearInfo().bFemaleOnly = true;
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 0, true)));

	SetGear(FLAG_RACE_OUSTERS, 10, 10, 10, 30, 10);
	GearInfo().bFemaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_OUSTERS, 10, 10, 10, 10, true)));
}

// The client's item table marks a gender with two flags, where the
// server keeps one value (decore::gender): neither is Both, one alone
// is that sex. An item with both flags set names both sexes; no server
// value says that, and it is read as Both, so either sex may use it, as
// before the client asked the server's rule.
TEST(EquipRequirement, BothGenderFlagsAllowEitherSex)
{
	EquipWorld world;
	Gear item;
	SetGear(FLAG_RACE_SLAYER, 10, 10, 10, 30, 0);
	GearInfo().bMaleOnly = true;
	GearInfo().bFemaleOnly = true;

	CHECK(item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 10, 10, 10, 0, false)));

	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 10);
	GearInfo().bMaleOnly = true;
	GearInfo().bFemaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 10, true)));
	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 10, false)));
}

// The client's own gates, which the server applies elsewhere or not at
// all: an item for another race is refused, whatever it asks.
TEST(EquipRequirement, AnotherRacesItemIsRefused)
{
	EquipWorld world;
	Gear item;

	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 0);
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 100, 100, 100, 100, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 100, 100, 100, 100, true)));

	SetGear(FLAG_RACE_SLAYER | FLAG_RACE_OUSTERS, 0, 0, 0, 0, 0);
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 100, 100, 100, 100, true)));
}

// A quest item is usable whatever it asks: by a slayer or a vampire
// whose sex the item allows, and by any ousters.
TEST(EquipRequirement, QuestItemIsUsableWhereTheGenderAllows)
{
	EquipWorld world;
	Gear item;
	item.SetQuestFlag(true);

	SetGear(FLAG_RACE_SLAYER, 100, 100, 100, 300, 0);
	GearInfo().bFemaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, false)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));

	SetGear(FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 100);
	GearInfo().bMaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 1, true)));
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 1, false)));

	SetGear(FLAG_RACE_OUSTERS, 100, 100, 100, 300, 100);
	GearInfo().bFemaleOnly = true;
	CHECK(item.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 1, true)));
}

// A pet whose life has run out lends nothing; a living one is judged
// like any item.
TEST(EquipRequirement, DeadPetIsNotUsable)
{
	EquipWorld world;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_PET_ITEM, 0).Race = FLAG_RACE_SLAYER;
	MPetItem pet;

	pet.SetCurrentDurability(0);
	CHECK(!pet.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));

	pet.SetCurrentDurability(100);
	CHECK(pet.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
}
