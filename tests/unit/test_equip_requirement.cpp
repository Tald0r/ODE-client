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
// which decore_tests asserts. Each check of the requirement and of
// whether a character meets it names the equip.tsv row it takes its
// numbers from. The tests of what de-core leaves to its caller name
// none, since equip.tsv has no row for them: the gates before its rule
// (a quest item, another race's item, a dead pet, a couple ring) and
// the reading of an item with both of the client's gender flags set.
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
	{ 10, 10 },		// 8
	{ 0, 20 },		// 9: also the highest level a pet enchant option asks
};
const int	kOptionRows = (int)(sizeof(kOptions) / sizeof(kOptions[0]));

struct EquipWorld : GameModelWorld
{
	EquipWorld() : GameModelWorld(kOptionRows)
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
		g_pItemTable->InitClass(ITEM_CLASS_COUPLE_RING, 1);
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COUPLE_RING, 1);
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

// A slayer's and a vampire's couple ring. MCoupleRing and
// MVampireCoupleRing cannot be linked here (their UseGear is the
// executable's), and add nothing to the rule but their class.
struct CoupleRing : public MItem
{
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_COUPLE_RING; }
};

struct VampireCoupleRing : public MItem
{
	ITEM_CLASS	GetItemClass() const	{ return ITEM_CLASS_VAMPIRE_COUPLE_RING; }
};

ITEMTABLE_INFO&	RowOf(ITEM_CLASS itemClass)
{
	return testfw::MutableRow(*g_pItemTable, itemClass, 0);
}

ITEMTABLE_INFO&	GearInfo()
{
	return RowOf(ITEM_CLASS_SWORD);
}

// The row of `itemClass` made for `raceFlags` (FLAG_RACE_*), asking
// str, dex, int, their sum and a level, for either sex.
void	SetRow(ITEM_CLASS itemClass, BYTE raceFlags, int str, int dex, int intel, int sum, int level)
{
	ITEMTABLE_INFO& info = RowOf(itemClass);
	info.Race = raceFlags;
	info.SetRequireSTR((BYTE)str);
	info.SetRequireDEX((BYTE)dex);
	info.SetRequireINT((BYTE)intel);
	info.SetRequireSUM((WORD)sum);
	info.SetRequireLevel((BYTE)level);
	info.bMaleOnly = false;
	info.bFemaleOnly = false;
}

// The gear row, as SetRow sets it.
void	SetGear(BYTE raceFlags, int str, int dex, int intel, int sum, int level)
{
	SetRow(ITEM_CLASS_SWORD, raceFlags, str, dex, intel, sum, level);
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

// An item flagged a quest item asks nothing: the client's own rule,
// which the server has no counterpart for. (A time-limited item, which
// IsQuestItem also counts, takes the server's time-limited gate.)
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

// An item made for several races is checked by the wearer's race's
// rule, as the server's isRealWearing computes it, whichever race's
// rule the item's flags would pick.
TEST(EquipRequirement, AnItemForSeveralRacesIsCheckedByTheWearersRule)
{
	EquipWorld world;
	Gear item;
	item.AddItemOption(8);

	// slayer-attr-old-cap-passed-by-1: 181/181/181/250 raised by an
	// option of sum 10 asks a slayer 200/200/200/260, and an ousters
	// 201/201/201/260, since its STR, DEX and INT are not capped
	// (ousters-attrs-not-capped).
	SetGear(FLAG_RACE_SLAYER | FLAG_RACE_OUSTERS, 181, 181, 181, 250, 0);
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 200, 200, 200, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 199, 200, 200, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_OUSTERS, 200, 200, 200, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_OUSTERS, 201, 201, 201, 0, true)));

	// The same row for a slayer and a vampire (the shape of the
	// server's relics): a slayer still needs the raised 200, not the
	// table's 181 that the vampire rule passes through
	// (vampire-attrs-pass-through), and a vampire the level of 0 the
	// option raises to 10 (vampire-zero-level-raised).
	SetGear(FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE, 181, 181, 181, 250, 0);
	CHECK(!item.IsUsableBy(User(RACE_SLAYER, 181, 181, 181, 0, true)));
	CHECK(item.IsUsableBy(User(RACE_SLAYER, 200, 200, 200, 0, true)));
	CHECK(!item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 9, true)));
	CHECK(item.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 10, true)));

	// vampire-old-cap-passed: a level of 90 raised by 20 asks a vampire
	// 100, and an ousters 110, since its level has no lower cap
	// (ousters-no-old-level-cap).
	Gear levelled;
	levelled.AddItemOption(4);
	levelled.AddItemOption(4);
	SetGear(FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS, 0, 0, 0, 0, 90);
	CHECK(levelled.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 100, true)));
	CHECK(!levelled.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 99, true)));
	CHECK(!levelled.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 109, true)));
	CHECK(levelled.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 110, true)));
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

// A slayer's or a vampire's couple ring is usable whatever its table
// and its options ask, and whatever its gender: the server's Slayer and
// Vampire::isRealWearing let one through (isCoupleRing) before they
// compute a requirement. The gates before it still apply: another
// race's ring is refused, and a quest item, which counts a
// time-limited one, asks the gender first, as isRealWearing's
// time-limited gate does. Ousters::isRealWearing has no such case; no
// ousters couple ring exists, so its check is of a made-up row.
TEST(EquipRequirement, ACoupleRingAsksASlayerOrAVampireNothing)
{
	EquipWorld world;

	CoupleRing ring;
	ring.AddItemOption(8);
	SetRow(ITEM_CLASS_COUPLE_RING, FLAG_RACE_SLAYER, 100, 100, 100, 300, 0);
	RowOf(ITEM_CLASS_COUPLE_RING).bMaleOnly = true;
	CHECK(ring.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, false)));
	CHECK(ring.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
	CHECK(!ring.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 100, false)));

	VampireCoupleRing vampireRing;
	vampireRing.AddItemOption(8);
	SetRow(ITEM_CLASS_VAMPIRE_COUPLE_RING, FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 50);
	RowOf(ITEM_CLASS_VAMPIRE_COUPLE_RING).bFemaleOnly = true;
	CHECK(vampireRing.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 1, true)));
	CHECK(vampireRing.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 1, false)));
	CHECK(!vampireRing.IsUsableBy(User(RACE_SLAYER, 100, 100, 100, 0, false)));

	ring.SetQuestFlag(true);
	CHECK(!ring.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, false)));
	CHECK(ring.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
	ring.SetQuestFlag(false);

	// 100/100/100/300 at level 100 raised by the option asks an ousters
	// 120/120/120/310 at level 110.
	SetRow(ITEM_CLASS_COUPLE_RING, FLAG_RACE_OUSTERS, 100, 100, 100, 300, 100);
	CHECK(!ring.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 1, true)));
	CHECK(!ring.IsUsableBy(User(RACE_OUSTERS, 120, 120, 120, 109, true)));
	CHECK(ring.IsUsableBy(User(RACE_OUSTERS, 120, 120, 120, 110, true)));
}

// A pet whose life has run out lends nothing; a living one asks
// nothing (ALivingPetAsksNothing).
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

// A living pet is usable by any race, whatever its race flags, its
// options and its gender flags ask. The server's isUsableItem lets any
// race use a pet item, where it asks a couple ring's or a pupa's user
// for its race, and executePetItem asks only the pet's HP and, for a
// second-stage pet, the owner's quest level; the server does not read
// PetItemInfo's Race column. Its seed's pets 0-2 are made for all three
// races and pet 4, the Stirge Bag, for vampires only. An option from
// the pet enchant asks a level of at most 20 (option row 9), which the
// vampire rule would raise the pet's level of 0 to
// (vampire-zero-level-raised's rule).
TEST(EquipRequirement, ALivingPetAsksNothing)
{
	EquipWorld world;
	MPetItem pet;
	pet.AddItemOption(9);
	pet.SetCurrentDurability(100);

	SetRow(ITEM_CLASS_PET_ITEM, FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS, 0, 0, 0, 0, 0);
	CHECK(pet.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 5, true)));
	CHECK(pet.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
	CHECK(pet.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 1, true)));

	// The Stirge Bag's row: its own race's low-level vampire, and the
	// two races its flags leave out.
	SetRow(ITEM_CLASS_PET_ITEM, FLAG_RACE_VAMPIRE, 0, 0, 0, 0, 0);
	CHECK(pet.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 5, true)));
	CHECK(pet.IsUsableBy(User(RACE_SLAYER, 0, 0, 0, 0, true)));
	CHECK(pet.IsUsableBy(User(RACE_OUSTERS, 0, 0, 0, 1, true)));

	RowOf(ITEM_CLASS_PET_ITEM).bFemaleOnly = true;
	CHECK(pet.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 5, true)));

	pet.SetCurrentDurability(0);
	CHECK(!pet.IsUsableBy(User(RACE_VAMPIRE, 0, 0, 0, 100, false)));
}
