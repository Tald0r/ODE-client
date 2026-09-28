//----------------------------------------------------------------------
// test_price_manager.cpp
//----------------------------------------------------------------------
//
// The price manager (docs/RESTRUCTURING.md task 4.2, third slice): what
// a shop charges or pays for an item. It reads the item and option
// tables, the timed-item register and the user's head-price rate, all
// library-defined; the player's race, level and stats, the half-price
// event and skills, and the shop tax come through MPriceHost, driven
// here by hand.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MPriceManager.h"

#include <cstdio>
#include <fstream>
#include <vector>

namespace {

//----------------------------------------------------------------------
// A host the tests drive by hand.
//----------------------------------------------------------------------
int		s_Race = -1;
int		s_Level = 0;
int		s_StatSum = 0;
int		s_BasicStatSum = 0;
bool	s_PotionHalf = false;
bool	s_GambleHalf = false;
int		s_TaxPercent = 100;

int		Race()				{ return s_Race; }
int		Level()				{ return s_Level; }
int		StatSum()			{ return s_StatSum; }
int		BasicStatSum()		{ return s_BasicStatSum; }
bool	IsPotionHalfPrice()	{ return s_PotionHalf; }
bool	IsGambleHalfPrice()	{ return s_GambleHalf; }
DWORD	ShopTaxPercent()	{ return (DWORD)s_TaxPercent; }

const MPriceHost	s_Host = { Race, Level, StatSum, BasicStatSum, IsPotionHalfPrice, IsGambleHalfPrice, ShopTaxPercent };

const char* const	kTempFile = "price_manager_test.bin";

// The class average the mysterious price scales comes only from a
// loaded table, so a class is written out and read back.
void	LoadClassFromFile(ITEM_CLASS itemClass, int firstPrice, int secondPrice)
{
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		const int count = 2;
		out.write((const char*)&count, 4);
		ITEMTABLE_INFO first;
		first.Price = firstPrice;
		first.SaveToFile(out);
		ITEMTABLE_INFO second;
		second.Price = secondPrice;
		second.SaveToFile(out);
	}
	std::ifstream in(kTempFile, std::ios::binary);
	testfw::MutableRow(*g_pItemTable, itemClass).LoadFromFile(in);
	in.close();
	std::remove(kTempFile);
}

//----------------------------------------------------------------------
// The globals the prices read, owned by one fixture per test.
//----------------------------------------------------------------------
struct PriceWorld : GameModelWorld
{
	PriceWorld() : GameModelWorld(6)
	{
		s_Race = -1;
		s_Level = 0;
		s_StatSum = 0;
		s_BasicStatSum = 0;
		s_PotionHalf = false;
		s_GambleHalf = false;
		s_TaxPercent = 100;

		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 2);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).Price = 1000;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SilverMax = 50;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).Price = 2000;
		g_pItemTable->InitClass(ITEM_CLASS_MACE, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MACE, 0).Price = 1000;		// SilverMax 0: cannot be coated
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 6);
		for (int i = 0; i < 6; i++)
			testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, i).Price = 100;
		g_pItemTable->InitClass(ITEM_CLASS_SERUM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SERUM, 0).Price = 200;
		g_pItemTable->InitClass(ITEM_CLASS_SKULL, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SKULL, 0).Price = 400;
		g_pItemTable->InitClass(ITEM_CLASS_MOON_CARD, 5);
		for (int i = 0; i < 5; i++)
			testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MOON_CARD, i).Price = 300;
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 0).Price = 500;
		g_pItemTable->InitClass(ITEM_CLASS_BLOOD_BIBLE_SIGN, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_BLOOD_BIBLE_SIGN, 0).Price = 500;
		g_pItemTable->InitClass(ITEM_CLASS_SLAYER_PORTAL_ITEM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SLAYER_PORTAL_ITEM, 0).Price = 1000;
		g_pItemTable->InitClass(ITEM_CLASS_OUSTERS_SUMMON_ITEM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_SUMMON_ITEM, 0).Price = 1000;
		// Blades: 4200 and 6000, so the class average is 5100 -> 500 in hundreds.
		LoadClassFromFile(ITEM_CLASS_BLADE, 4200, 6000);

		// Row 0 is the "no option" row; the others carry a part and a
		// price multiplier in percent.
		testfw::MutableRow(*g_pItemOptionTable, 0).Part = ITEMOPTION_TABLE::PART_HP;
		testfw::MutableRow(*g_pItemOptionTable, 1).Part = ITEMOPTION_TABLE::PART_DAMAGE;
		testfw::MutableRow(*g_pItemOptionTable, 1).PriceMultiplier = 150;
		testfw::MutableRow(*g_pItemOptionTable, 2).Part = ITEMOPTION_TABLE::PART_STR;
		testfw::MutableRow(*g_pItemOptionTable, 2).PriceMultiplier = 50;
		testfw::MutableRow(*g_pItemOptionTable, 3).Part = ITEMOPTION_TABLE::PART_ATTACK_SPEED;
		testfw::MutableRow(*g_pItemOptionTable, 3).PriceMultiplier = 100;
		testfw::MutableRow(*g_pItemOptionTable, 4).Part = ITEMOPTION_TABLE::PART_INT;
		testfw::MutableRow(*g_pItemOptionTable, 4).PriceMultiplier = 100;
		testfw::MutableRow(*g_pItemOptionTable, 5).Part = ITEMOPTION_TABLE::PART_DEX;
		testfw::MutableRow(*g_pItemOptionTable, 5).PriceMultiplier = 100;

		g_pUserInformation->HeadPrice = 100;

		MPriceManager::SetHost(&s_Host);
	}

	~PriceWorld()
	{
		MPriceManager::SetHost(NULL);
	}
};

//----------------------------------------------------------------------
// Test items: a plain one of any class, one that wears, one that
// holds charges.
//----------------------------------------------------------------------
struct Item : public MItem
{
	ITEM_CLASS	m_Class;

	Item(ITEM_CLASS itemClass, TYPE_ITEMTYPE type = 0, TYPE_OBJECTID id = 1)
		: m_Class(itemClass)
	{
		SetID(id);
		SetItemType(type);
	}

	ITEM_CLASS	GetItemClass() const	{ return m_Class; }
};

struct Gear : public Item
{
	int		m_MaxDurability;

	Gear(ITEM_CLASS itemClass, int maxDurability, TYPE_ITEM_DURATION current, TYPE_OBJECTID id = 1)
		: Item(itemClass, 0, id), m_MaxDurability(maxDurability)
	{
		SetCurrentDurability(current);
	}

	int		GetMaxDurability() const	{ return m_MaxDurability; }
};

struct Charged : public Item
{
	Charged(ITEM_CLASS itemClass, TYPE_ITEM_NUMBER charges)
		: Item(itemClass)
	{
		SetNumber(charges);
	}

	bool				IsChargeItem() const	{ return true; }
	TYPE_ITEM_NUMBER	GetMaxNumber() const	{ return 10; }
};

} // namespace

//----------------------------------------------------------------------
// The short circuits
//----------------------------------------------------------------------
TEST(PriceManager, NothingEventCardsAndUnidentifiedItemsShortCircuit)
{
	PriceWorld world;
	MPriceManager prices;

	CHECK_EQ(0, prices.GetItemPrice(NULL, MPriceManager::NPC_TO_PC));

	// The fourth moon card is worth whatever the server last said.
	Item eventCard(ITEM_CLASS_MOON_CARD, 4);
	Item otherCard(ITEM_CLASS_MOON_CARD, 3);
	prices.SetEventItemPrice(777);
	CHECK_EQ(777, prices.GetItemPrice(&eventCard, MPriceManager::NPC_TO_PC));
	CHECK_EQ(777, prices.GetItemPrice(&eventCard, MPriceManager::PC_TO_NPC));
	CHECK_EQ(300, prices.GetItemPrice(&otherCard, MPriceManager::NPC_TO_PC));

	// An unidentified item is priced as a gamble, whatever the trade.
	Item blade(ITEM_CLASS_BLADE);
	CHECK_EQ(4200, prices.GetItemPrice(&blade, MPriceManager::NPC_TO_PC));
	blade.UnSetIdentified();
	CHECK_EQ(500, prices.GetMysteriousPrice(&blade));
	CHECK_EQ(500, prices.GetItemPrice(&blade, MPriceManager::NPC_TO_PC));
	CHECK_EQ(500, prices.GetItemPrice(&blade, MPriceManager::REPAIR));
}

//----------------------------------------------------------------------
// The market conditions
//----------------------------------------------------------------------
TEST(PriceManager, BuyingAndSellingFollowTheMarketConditions)
{
	PriceWorld world;
	MPriceManager prices;
	Item sword(ITEM_CLASS_SWORD);
	Item skull(ITEM_CLASS_SKULL);

	// The NPC sells at 100% and buys at 25% until the server says otherwise.
	CHECK_EQ(100, prices.GetMarketCondSell());
	CHECK_EQ(25, prices.GetMarketCondBuy());
	CHECK_EQ(1000, prices.GetItemPrice(&sword, MPriceManager::NPC_TO_PC));
	CHECK_EQ(250, prices.GetItemPrice(&sword, MPriceManager::PC_TO_NPC));

	prices.SetMarketCondSell(120);
	prices.SetMarketCondBuy(30);
	CHECK_EQ(1200, prices.GetItemPrice(&sword, MPriceManager::NPC_TO_PC));
	CHECK_EQ(300, prices.GetItemPrice(&sword, MPriceManager::PC_TO_NPC));

	// A skull is only ever sold to the shop, so both directions use the
	// buying rate.
	CHECK_EQ(120, prices.GetItemPrice(&skull, MPriceManager::NPC_TO_PC));
	CHECK_EQ(120, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
}

//----------------------------------------------------------------------
// Options and wear
//----------------------------------------------------------------------
TEST(PriceManager, OptionsRaiseThePriceAndWearLowersIt)
{
	PriceWorld world;
	MPriceManager prices;

	// The option multipliers add up: 150% + 50% = 200%.
	Item optioned(ITEM_CLASS_SWORD);
	optioned.AddItemOption(1);
	optioned.AddItemOption(2);
	CHECK_EQ(2000, prices.GetItemPrice(&optioned, MPriceManager::NPC_TO_PC));
	CHECK_EQ(500, prices.GetItemPrice(&optioned, MPriceManager::PC_TO_NPC));

	// Half worn is half price; worn out is never free; an item with no
	// durability to speak of (a negative maximum) does not wear at all.
	Gear halfWorn(ITEM_CLASS_SWORD, 100, 50);
	Gear wornOut(ITEM_CLASS_SWORD, 100, 0);
	Gear fresh(ITEM_CLASS_SWORD, 100, 100);
	Gear noWear(ITEM_CLASS_SWORD, -5, 50);
	CHECK_EQ(500, prices.GetItemPrice(&halfWorn, MPriceManager::NPC_TO_PC));
	CHECK_EQ(1, prices.GetItemPrice(&wornOut, MPriceManager::NPC_TO_PC));
	CHECK_EQ(1000, prices.GetItemPrice(&fresh, MPriceManager::NPC_TO_PC));
	CHECK_EQ(1000, prices.GetItemPrice(&noWear, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Repair
//----------------------------------------------------------------------
TEST(PriceManager, RepairCostsATenthOfTheDamageAndSomeItemsAreNeverRepaired)
{
	PriceWorld world;
	MPriceManager prices;

	// A quarter of the wear at a tenth of the price: 1000 * 0.25 * 10%.
	Gear dented(ITEM_CLASS_SWORD, 100, 75);
	Gear fresh(ITEM_CLASS_SWORD, 100, 100);
	Item noDurability(ITEM_CLASS_SWORD);
	CHECK_EQ(25, prices.GetItemPrice(&dented, MPriceManager::REPAIR));
	CHECK_EQ(0, prices.GetItemPrice(&fresh, MPriceManager::REPAIR));
	CHECK_EQ(0, prices.GetItemPrice(&noDurability, MPriceManager::REPAIR));

	// A vampire portal, a blood bible sign and a timed item are never repaired.
	Gear portal(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 100, 50);
	Gear sign(ITEM_CLASS_BLOOD_BIBLE_SIGN, 100, 50);
	Gear timed(ITEM_CLASS_SWORD, 100, 50, 77);
	CHECK_EQ(0, prices.GetItemPrice(&portal, MPriceManager::REPAIR));
	CHECK_EQ(0, prices.GetItemPrice(&sign, MPriceManager::REPAIR));
	CHECK_EQ(50, prices.GetItemPrice(&timed, MPriceManager::REPAIR));
	g_pTimeItemManager->AddTimeItem(77, 60);
	CHECK_EQ(0, prices.GetItemPrice(&timed, MPriceManager::REPAIR));
}

//----------------------------------------------------------------------
// A repair quote is the server's repair price (decore::repairPrice,
// PriceManager::getRepairPrice) and nothing else: none of the buy and
// sell adjustments, and no early return that only a buy or sell has.
//----------------------------------------------------------------------
TEST(PriceManager, RepairQuotesAreTheServersRepairPriceAlone)
{
	PriceWorld world;
	MPriceManager prices;

	// A worn potion as a weak slayer under the half price, and a worn
	// skull as a vampire at a 40% head price: the server charges a
	// tenth of the wear, 5 and 20, with no discount, share or rate.
	Gear potion(ITEM_CLASS_POTION, 100, 50);
	Gear skull(ITEM_CLASS_SKULL, 100, 50);
	s_Race = RACE_SLAYER;
	s_StatSum = 40;
	s_PotionHalf = true;
	CHECK_EQ(5, prices.GetItemPrice(&potion, MPriceManager::REPAIR));
	s_Race = RACE_VAMPIRE;
	g_pUserInformation->HeadPrice = 40;
	CHECK_EQ(20, prices.GetItemPrice(&skull, MPriceManager::REPAIR));

	// A maximum durability of 0 is "no durability" to the server, and
	// repairing such an item costs 1 even at 0 of 0 (repair_price.tsv,
	// no-durability-costs-1).
	Gear zero(ITEM_CLASS_SWORD, 0, 0);
	CHECK_EQ(1, prices.GetItemPrice(&zero, MPriceManager::REPAIR));

	// The crown moon card's price is a buy and sell price; a moon card
	// has no durability, so there is nothing to repair.
	Item crown(ITEM_CLASS_MOON_CARD, 4);
	prices.SetEventItemPrice(777);
	CHECK_EQ(777, prices.GetItemPrice(&crown, MPriceManager::PC_TO_NPC));
	CHECK_EQ(0, prices.GetItemPrice(&crown, MPriceManager::REPAIR));
}

//----------------------------------------------------------------------
// Charges
//----------------------------------------------------------------------
TEST(PriceManager, ChargedItemsPriceEveryChargeAndRepairRefillsThem)
{
	PriceWorld world;
	MPriceManager prices;

	// A slayer portal: 1000 for the item and 5000 a charge, three of ten
	// held. (The server charges for charges only on the two portals and
	// the Ousters summon item, the classes the client makes charged.)
	Charged charged(ITEM_CLASS_SLAYER_PORTAL_ITEM, 3);
	CHECK_EQ(16000, prices.GetItemPrice(&charged, MPriceManager::NPC_TO_PC));
	CHECK_EQ(4000, prices.GetItemPrice(&charged, MPriceManager::PC_TO_NPC));
	CHECK_EQ(35000, prices.GetItemPrice(&charged, MPriceManager::REPAIR));

	// An Ousters summon item's charges cost 1000 each.
	Charged summon(ITEM_CLASS_OUSTERS_SUMMON_ITEM, 3);
	CHECK_EQ(4000, prices.GetItemPrice(&summon, MPriceManager::NPC_TO_PC));
	CHECK_EQ(7000, prices.GetItemPrice(&summon, MPriceManager::REPAIR));

	// A charged consumable is priced by its charges alone: no half
	// price, no tax (today's behaviour, pinned).
	Charged chargedPotion(ITEM_CLASS_POTION, 2);
	s_PotionHalf = true;
	s_TaxPercent = 200;
	CHECK_EQ(10100, prices.GetItemPrice(&chargedPotion, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Silvering
//----------------------------------------------------------------------
TEST(PriceManager, SilveringCostsTheFullCoatUntilTheCoatIsFull)
{
	PriceWorld world;
	MPriceManager prices;

	Item sword(ITEM_CLASS_SWORD);
	CHECK_EQ(50, prices.GetItemPrice(&sword, MPriceManager::SILVERING));
	sword.SetSilver(20);
	CHECK_EQ(50, prices.GetItemPrice(&sword, MPriceManager::SILVERING));
	sword.SetSilver(50);
	CHECK_EQ(0, prices.GetItemPrice(&sword, MPriceManager::SILVERING));

	// A mace with no coat capacity reads as already full; a potion is
	// not a coatable class at all.
	Item mace(ITEM_CLASS_MACE);
	Item potion(ITEM_CLASS_POTION);
	CHECK_EQ(0, prices.GetItemPrice(&mace, MPriceManager::SILVERING));
	CHECK_EQ(0, prices.GetItemPrice(&potion, MPriceManager::SILVERING));
}

//----------------------------------------------------------------------
// What the host changes
//----------------------------------------------------------------------
TEST(PriceManager, TheHostShapesPotionSkullAndTaxedPrices)
{
	PriceWorld world;
	MPriceManager prices;
	Item potion(ITEM_CLASS_POTION, 0);
	Item fifthPotion(ITEM_CLASS_POTION, 5);
	Item otherPotion(ITEM_CLASS_POTION, 3);
	Item serum(ITEM_CLASS_SERUM);
	Item sword(ITEM_CLASS_SWORD);
	Item skull(ITEM_CLASS_SKULL);

	// Without a host nothing about the player or the world applies,
	// however much the answers it would give would move the price.
	s_Race = RACE_VAMPIRE;
	s_StatSum = 10;
	s_PotionHalf = true;
	s_TaxPercent = 200;
	MPriceManager::SetHost(NULL);
	CHECK_EQ(100, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));
	CHECK_EQ(100, prices.GetItemPrice(&skull, MPriceManager::NPC_TO_PC));
	MPriceManager::SetHost(&s_Host);
	CHECK_EQ(100, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));		// 100 / 2 * 200%
	CHECK_EQ(100, prices.GetItemPrice(&skull, MPriceManager::NPC_TO_PC));		// 100 * 200% / 2
	s_PotionHalf = false;
	s_TaxPercent = 100;

	// A slayer with 40 or fewer stat points pays 70% for the two basic
	// potions, the first and the sixth.
	s_Race = RACE_SLAYER;
	s_StatSum = 40;
	CHECK_EQ(70, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));
	CHECK_EQ(70, prices.GetItemPrice(&fifthPotion, MPriceManager::NPC_TO_PC));
	CHECK_EQ(100, prices.GetItemPrice(&otherPotion, MPriceManager::NPC_TO_PC));
	s_StatSum = 41;
	CHECK_EQ(100, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));
	s_Race = RACE_VAMPIRE;
	s_StatSum = 10;
	CHECK_EQ(100, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));

	// The consumables go to half price; a sword does not.
	s_Race = RACE_SLAYER;
	s_StatSum = 40;
	s_PotionHalf = true;
	CHECK_EQ(35, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));
	CHECK_EQ(100, prices.GetItemPrice(&serum, MPriceManager::NPC_TO_PC));
	CHECK_EQ(1000, prices.GetItemPrice(&sword, MPriceManager::NPC_TO_PC));

	// The shop tax scales what the shop charges, not what it pays.
	s_TaxPercent = 120;
	CHECK_EQ(42, prices.GetItemPrice(&potion, MPriceManager::NPC_TO_PC));
	CHECK_EQ(1200, prices.GetItemPrice(&sword, MPriceManager::NPC_TO_PC));
	CHECK_EQ(250, prices.GetItemPrice(&sword, MPriceManager::PC_TO_NPC));
	s_TaxPercent = 100;
	s_PotionHalf = false;

	// A skull (400 at the 25% buying rate: 100) is worth half to a
	// vampire and three quarters to an Ousters, then the server's
	// head-price rate applies.
	CHECK_EQ(100, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
	s_Race = RACE_VAMPIRE;
	CHECK_EQ(50, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
	s_Race = RACE_OUSTERS;
	CHECK_EQ(75, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
	g_pUserInformation->HeadPrice = 40;
	CHECK_EQ(30, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
	s_Race = RACE_SLAYER;
	CHECK_EQ(40, prices.GetItemPrice(&skull, MPriceManager::PC_TO_NPC));
	CHECK_EQ(1000, prices.GetItemPrice(&sword, MPriceManager::NPC_TO_PC));	// the rate is for skulls only
}

//----------------------------------------------------------------------
// Star prices
//----------------------------------------------------------------------
TEST(PriceManager, StarPriceFollowsTheFirstOptionsPartAndTheItemType)
{
	PriceWorld world;
	MPriceManager prices;
	STAR_ITEM_PRICE price;

	prices.GetItemPrice(NULL, price);
	CHECK_EQ(-1, price.type);
	CHECK_EQ(0, price.number);

	// No option reads the "no option" row, whose part earns no stars.
	Item plain(ITEM_CLASS_SWORD, 3);
	prices.GetItemPrice(&plain, price);
	CHECK_EQ(-1, price.type);
	CHECK_EQ(0, price.number);

	// Damage, strength, intelligence, dexterity and attack speed are
	// star types 0 to 4; the item type minus one, in twenties, is the
	// count.
	const int parts[5] = { 1, 2, 4, 5, 3 };
	for (int i = 0; i < 5; i++)
	{
		Item item(ITEM_CLASS_SWORD, 3);
		item.AddItemOption(parts[i]);
		prices.GetItemPrice(&item, price);
		CHECK_EQ(i, price.type);
		CHECK_EQ(40, price.number);
	}

	// Only the first option counts.
	Item two(ITEM_CLASS_SWORD, 5);
	two.AddItemOption(2);
	two.AddItemOption(1);
	prices.GetItemPrice(&two, price);
	CHECK_EQ(1, price.type);
	CHECK_EQ(80, price.number);
}

//----------------------------------------------------------------------
// The gamble
//----------------------------------------------------------------------
TEST(PriceManager, MysteriousPriceScalesTheClassAverageByTheCharacter)
{
	PriceWorld world;
	MPriceManager prices;
	Item blade(ITEM_CLASS_BLADE);
	blade.UnSetIdentified();

	// Without a host: once the average, whatever the host would have said.
	s_Race = RACE_SLAYER;
	s_BasicStatSum = 45;
	s_GambleHalf = true;
	s_TaxPercent = 200;
	MPriceManager::SetHost(NULL);
	CHECK_EQ(500, prices.GetMysteriousPrice(&blade));
	MPriceManager::SetHost(&s_Host);
	CHECK_EQ(1500, prices.GetMysteriousPrice(&blade));		// 500 * 3 / 2 * 200%
	s_GambleHalf = false;
	s_TaxPercent = 100;

	// A slayer pays by basic stats in fifteens, never less than once.
	s_Race = RACE_SLAYER;
	s_BasicStatSum = 45;
	s_Level = 100;											// ignored for a slayer
	CHECK_EQ(1500, prices.GetMysteriousPrice(&blade));
	s_BasicStatSum = 10;
	CHECK_EQ(500, prices.GetMysteriousPrice(&blade));
	s_BasicStatSum = 0;
	CHECK_EQ(500, prices.GetMysteriousPrice(&blade));

	// Everyone else by level in fives.
	s_Race = RACE_VAMPIRE;
	s_Level = 50;
	CHECK_EQ(5000, prices.GetMysteriousPrice(&blade));
	s_Level = 4;
	CHECK_EQ(500, prices.GetMysteriousPrice(&blade));
	s_Race = RACE_OUSTERS;
	s_Level = 100;
	CHECK_EQ(10000, prices.GetMysteriousPrice(&blade));

	// Half under the gamble skill, then the shop tax, in every trade.
	s_Race = RACE_VAMPIRE;
	s_Level = 50;
	s_GambleHalf = true;
	CHECK_EQ(2500, prices.GetMysteriousPrice(&blade));
	s_TaxPercent = 150;
	CHECK_EQ(3750, prices.GetMysteriousPrice(&blade));
	CHECK_EQ(3750, prices.GetItemPrice(&blade, MPriceManager::PC_TO_NPC));
	CHECK_EQ(3750, prices.GetItemPrice(&blade, MPriceManager::REPAIR));

	// A class with no loaded average is worth nothing as a gamble.
	Item sword(ITEM_CLASS_SWORD);
	sword.UnSetIdentified();
	CHECK_EQ(0, prices.GetMysteriousPrice(&sword));

	// The tax is applied in 64 bits: an expensive class at the top
	// multiplier is 120,000,000, which times a percentage does not fit
	// a 32-bit int on the way to the division.
	LoadClassFromFile(ITEM_CLASS_CROSS, 60000000, 60000000);
	Item cross(ITEM_CLASS_CROSS);
	cross.UnSetIdentified();
	s_Race = RACE_OUSTERS;
	s_Level = 100;
	s_GambleHalf = false;
	s_TaxPercent = 100;
	CHECK_EQ(120000000, prices.GetMysteriousPrice(&cross));
	s_TaxPercent = 150;
	CHECK_EQ(180000000, prices.GetMysteriousPrice(&cross));
}
