//----------------------------------------------------------------------
// test_price_grade.cpp
//----------------------------------------------------------------------
//
// A graded item's shop quote must be what the server charges or pays.
// The server's rule (decore::itemPrice and repairPrice, vendored in
// third_party/decore) scales the table price by (80 + 5 * grade) / 100.0
// in double whenever the item's grade is not -1, before the options,
// the wear and the market rate, and truncates once at the end. Its
// parity vectors (third_party/decore/domain/vectors/price.tsv and
// repair_price.tsv, asserted by decore_tests) are the reference: the
// named checks below are their seed-* rows. The sweep checks the
// adapter instead - that MPriceManager hands the rule the grade it
// prices by, the options, the durability and the rate - by comparing
// every quote with the rule called on inputs built by hand.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MPriceManager.h"

#include "domain/ItemPrice.h"

#include <cstdio>
#include <vector>

namespace {

//----------------------------------------------------------------------
// The server's rule on a sword with no charges, given the table price,
// the grade, the options' multipliers and the durability by hand.
//----------------------------------------------------------------------
decore::ItemPriceInput	RuleInput(int price, int grade, const std::vector<int>& multipliers, int cur, int max)
{
	decore::ItemPriceInput input = {};
	input.itemClass = ITEM_CLASS_SWORD;
	input.basePrice = (unsigned)price;
	input.grade = grade;
	input.optionPriceMultipliers = multipliers.data();
	input.optionCount = (int)multipliers.size();
	input.curDurability = (unsigned)cur;
	input.maxDurability = (unsigned)max;
	return input;
}

int		RulePrice(int price, int grade, const std::vector<int>& multipliers, int cur, int max, int nDiscount)
{
	decore::ItemPriceInput input = RuleInput(price, grade, multipliers, cur, max);
	input.marketCond = nDiscount;
	return decore::itemPrice(input);
}

int		RuleRepairPrice(int price, int grade, const std::vector<int>& multipliers, int cur, int max)
{
	return decore::repairPrice(RuleInput(price, grade, multipliers, cur, max));
}

//----------------------------------------------------------------------
// The tables: one sword, and options whose multipliers are the row
// number's entry in kMultipliers.
//----------------------------------------------------------------------
const int	kMultipliers[] = { 0, 150, 50, 110 };
const int	kOptionRows = (int)(sizeof(kMultipliers) / sizeof(kMultipliers[0]));

struct GradeWorld : GameModelWorld
{
	GradeWorld() : GameModelWorld(kOptionRows)
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		SetSwordPrice(1000);

		for (int i = 1; i < kOptionRows; i++)
		{
			testfw::MutableRow(*g_pItemOptionTable, i).Part = ITEMOPTION_TABLE::PART_DAMAGE;
			testfw::MutableRow(*g_pItemOptionTable, i).PriceMultiplier = kMultipliers[i];
		}

		g_pUserInformation->HeadPrice = 100;

		// No host: no race, no event, no tax.
		MPriceManager::SetHost(NULL);
	}

	void	SetSwordPrice(int price)
	{
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).Price = price;
	}
};

// A sword that is worn as gear and wears. The maximum durability is
// given directly, so the grade's durability offset does not move the
// wear ratio under test.
struct GradedGear : public MItem
{
	int		m_MaxDurability;

	GradedGear(int grade, int maxDurability, TYPE_ITEM_DURATION current)
		: m_MaxDurability(maxDurability)
	{
		SetID(1);
		SetItemType(0);
		SetGrade(grade);
		SetCurrentDurability(current);
	}

	ITEM_CLASS	GetItemClass() const		{ return ITEM_CLASS_SWORD; }
	bool		IsGearItem() const			{ return true; }
	int			GetMaxDurability() const	{ return m_MaxDurability; }
};

// An item that is not gear but carries a value in its grade field, as
// a pet item does (the server sends the days since its last feeding
// there, while its own pet item has no grade to price by).
struct GradeFieldNonGear : public MItem
{
	GradeFieldNonGear(int grade, TYPE_ITEM_DURATION current)
	{
		SetID(1);
		SetItemType(0);
		SetGrade(grade);
		SetCurrentDurability(current);
	}

	ITEM_CLASS	GetItemClass() const		{ return ITEM_CLASS_SWORD; }
	int			GetMaxDurability() const	{ return 100; }
};

} // namespace

//----------------------------------------------------------------------
// The grade scales the price before it is truncated
//----------------------------------------------------------------------
TEST(PriceGrade, GradedPricesMatchTheServer)
{
	GradeWorld world;
	MPriceManager prices;

	// 27 at grade 3 is 25.65, and two thirds of that is 17.1: the
	// server pays 17, not 16 from two thirds of a truncated 25
	// (price.tsv, seed-grade-3-on-27-two-thirds).
	world.SetSwordPrice(27);
	GradedGear worn(3, 3, 2);
	CHECK_EQ(17, prices.GetItemPrice(&worn, MPriceManager::NPC_TO_PC));

	// 1003 at grade 9 is 1253.75. Repairing 3 of 4 is a tenth of the
	// 940.3125 the wear took: the server charges 94, not 93 from a
	// truncated 1253 (repair_price.tsv,
	// seed-grade-9-on-1003-three-quarters-lost).
	world.SetSwordPrice(1003);
	GradedGear dented(9, 4, 1);
	CHECK_EQ(94, prices.GetItemPrice(&dented, MPriceManager::REPAIR));
}

//----------------------------------------------------------------------
// What has a grade: gear whose grade is not -1
//----------------------------------------------------------------------
TEST(PriceGrade, EveryGradeButNoneScalesAGearPrice)
{
	GradeWorld world;
	MPriceManager prices;

	world.SetSwordPrice(1000);

	// No grade: the table price (price.tsv, seed-no-grade).
	GradedGear ungraded(-1, 100, 100);
	CHECK_EQ(1000, prices.GetItemPrice(&ungraded, MPriceManager::NPC_TO_PC));

	// Grade 0 is a grade to the server: 80% (seed-grade-0-is-80pct).
	GradedGear zero(0, 100, 100);
	CHECK_EQ(800, prices.GetItemPrice(&zero, MPriceManager::NPC_TO_PC));

	// So is a grade past 10: grade 11 is 135% (seed-grade-11-is-135pct).
	GradedGear eleven(11, 100, 100);
	CHECK_EQ(1350, prices.GetItemPrice(&eleven, MPriceManager::NPC_TO_PC));

	// An item that is not gear is not priced by its grade field.
	GradeFieldNonGear pet(8, 100);
	CHECK_EQ(1000, prices.GetItemPrice(&pet, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Every grade and durability, a spread of prices, options and rates
//----------------------------------------------------------------------
TEST(PriceGrade, EveryGradedQuoteInASweepMatchesTheServer)
{
	GradeWorld world;
	MPriceManager prices;

	const int kPrices[] = { 1, 7, 27, 99, 1000, 1001, 1999, 4999, 12345, 99999, 1234567 };
	const int kRates[] = { 25, 30, 100, 120 };

	std::vector< std::vector<int> > optionSets;
	optionSets.push_back(std::vector<int>());
	optionSets.push_back(std::vector<int>(1, 1));			// 150%
	{
		std::vector<int> rows;
		rows.push_back(3);
		rows.push_back(2);									// 110% + 50%
		optionSets.push_back(rows);
	}

	long long	checked = 0;
	long long	mismatches = 0;
	int			firstServer = 0;
	int			firstClient = 0;

	for (int grade = -1; grade <= 11; grade++)
	{
		for (size_t p = 0; p < sizeof(kPrices) / sizeof(kPrices[0]); p++)
		{
			world.SetSwordPrice(kPrices[p]);

			for (size_t o = 0; o < optionSets.size(); o++)
			{
				std::vector<int> multipliers;
				for (size_t r = 0; r < optionSets[o].size(); r++)
				{
					multipliers.push_back(kMultipliers[optionSets[o][r]]);
				}

				for (int max = 0; max <= 40; max++)
				{
					for (int cur = 0; cur <= max; cur++)
					{
						GradedGear item(grade, max, (TYPE_ITEM_DURATION)cur);
						for (size_t r = 0; r < optionSets[o].size(); r++)
						{
							item.AddItemOption((TYPE_ITEM_OPTION)optionSets[o][r]);
						}

						for (size_t k = 0; k < sizeof(kRates) / sizeof(kRates[0]); k++)
						{
							prices.SetMarketCondSell(kRates[k]);
							prices.SetMarketCondBuy(kRates[k]);

							const int expected = RulePrice(kPrices[p], grade, multipliers, cur, max, kRates[k]);
							const int bought = prices.GetItemPrice(&item, MPriceManager::NPC_TO_PC);
							const int sold = prices.GetItemPrice(&item, MPriceManager::PC_TO_NPC);
							checked += 2;

							if (bought != expected || sold != expected)
							{
								if (mismatches == 0)
								{
									firstServer = expected;
									firstClient = bought != expected ? bought : sold;
									std::printf("  first price mismatch: price %d, grade %d, options %d, %d of %d at %d%%: server %d, client %d\n",
										kPrices[p], grade, (int)o, cur, max, kRates[k], firstServer, firstClient);
								}
								mismatches++;
							}
						}

						// A maximum of 0 is in the sweep: the server's repair
						// price gives an item that keeps no durability 1, and the
						// client quotes the same through decore::repairPrice.
						{
							const int expected = RuleRepairPrice(kPrices[p], grade, multipliers, cur, max);
							const int repaired = prices.GetItemPrice(&item, MPriceManager::REPAIR);
							checked++;

							if (repaired != expected)
							{
								if (mismatches == 0)
								{
									firstServer = expected;
									firstClient = repaired;
									std::printf("  first repair mismatch: price %d, grade %d, options %d, %d of %d: server %d, client %d\n",
										kPrices[p], grade, (int)o, cur, max, firstServer, firstClient);
								}
								mismatches++;
							}
						}
					}
				}
			}
		}
	}

	if (mismatches != 0)
	{
		std::printf("  %lld of %lld quotes differ from the server\n", mismatches, checked);
	}

	CHECK(checked > 0);
	CHECK_EQ(0, mismatches);
	CHECK_EQ(firstServer, firstClient);
}
