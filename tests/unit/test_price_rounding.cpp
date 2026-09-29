//----------------------------------------------------------------------
// test_price_rounding.cpp
//----------------------------------------------------------------------
//
// What a shop quotes for an item with options and wear must be what
// the server charges or pays. The server's rule (decore::itemPrice and
// repairPrice, vendored in third_party/decore) works in double from the
// table price through the options, the wear and the market rate, and
// truncates once at the end. Its parity vectors
// (third_party/decore/domain/vectors/price.tsv and repair_price.tsv,
// asserted by decore_tests) are the reference: the named checks below
// are their seed-* rows. The sweep checks the adapter instead - that
// MPriceManager hands the rule the item's price, options, durability
// and rate - by comparing every quote with the rule called on inputs
// built by hand.
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
// The server's rule on a sword with no grade and no charges, given
// the table price, the options' multipliers and the durability by hand.
//----------------------------------------------------------------------
decore::ItemPriceInput	RuleInput(int price, const std::vector<int>& multipliers, int cur, int max)
{
	decore::ItemPriceInput input = {};
	input.itemClass = ITEM_CLASS_SWORD;
	input.basePrice = (unsigned)price;
	input.grade = -1;
	input.optionPriceMultipliers = multipliers.data();
	input.optionCount = (int)multipliers.size();
	input.curDurability = (unsigned)cur;
	input.maxDurability = (unsigned)max;
	return input;
}

// nDiscount is the market rate the shop list sent, which the client
// holds as its buying and selling conditions.
int		RulePrice(int price, const std::vector<int>& multipliers, int cur, int max, int nDiscount)
{
	decore::ItemPriceInput input = RuleInput(price, multipliers, cur, max);
	input.marketCond = nDiscount;
	return decore::itemPrice(input);
}

int		RuleRepairPrice(int price, const std::vector<int>& multipliers, int cur, int max)
{
	return decore::repairPrice(RuleInput(price, multipliers, cur, max));
}

//----------------------------------------------------------------------
// The tables: one sword, and options whose multipliers are the row
// number's entry in kMultipliers.
//----------------------------------------------------------------------
const int	kMultipliers[] = { 0, 150, 50, 110, 33, 100 };
const int	kOptionRows = (int)(sizeof(kMultipliers) / sizeof(kMultipliers[0]));

struct RoundingWorld : GameModelWorld
{
	RoundingWorld() : GameModelWorld(kOptionRows)
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

// A sword that wears.
struct Gear : public MItem
{
	int		m_MaxDurability;

	Gear(int maxDurability, TYPE_ITEM_DURATION current)
		: m_MaxDurability(maxDurability)
	{
		SetID(1);
		SetItemType(0);
		SetCurrentDurability(current);
	}

	ITEM_CLASS	GetItemClass() const		{ return ITEM_CLASS_SWORD; }
	int			GetMaxDurability() const	{ return m_MaxDurability; }
};

// The options an item carries, by row.
std::vector<int>	Multipliers(const std::vector<int>& rows)
{
	std::vector<int> multipliers;
	for (size_t i = 0; i < rows.size(); i++)
	{
		multipliers.push_back(kMultipliers[rows[i]]);
	}
	return multipliers;
}

} // namespace

//----------------------------------------------------------------------
// Ordinary cases that came out one unit low
//----------------------------------------------------------------------
TEST(PriceRounding, WornPricesAndRepairsMatchTheServer)
{
	RoundingWorld world;
	MPriceManager prices;

	// Repairing 40 of 100 on a 1000 item: the server charges 40
	// (repair_price.tsv, seed-40-of-100-lost-on-1000).
	Gear worn(100, 60);
	CHECK_EQ(40, prices.GetItemPrice(&worn, MPriceManager::REPAIR));

	// A 5000 item at 39 of 50 sells for 3900 (price.tsv,
	// seed-worn-5000-at-39-of-50).
	world.SetSwordPrice(5000);
	Gear g(50, 39);
	CHECK_EQ(3900, prices.GetItemPrice(&g, MPriceManager::NPC_TO_PC));

	// Half of 27 at a 30% rate is 4.05: 4, not 3 from truncating 13.5
	// before the rate (seed-half-of-27-at-30pct).
	world.SetSwordPrice(27);
	Gear half(2, 1);
	prices.SetMarketCondBuy(30);
	CHECK_EQ(4, prices.GetItemPrice(&half, MPriceManager::PC_TO_NPC));

	// 1001 at 150% is 1501.5, and two thirds of that is 1001: the
	// half is not dropped before the wear (seed-1001-at-150pct-two-thirds).
	world.SetSwordPrice(1001);
	Gear optioned(3, 2);
	optioned.AddItemOption(1);
	CHECK_EQ(1001, prices.GetItemPrice(&optioned, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Precision
//----------------------------------------------------------------------
TEST(PriceRounding, PricesPast2To24KeepEveryUnit)
{
	RoundingWorld world;
	MPriceManager prices;

	// price.tsv, seed-price-past-2-to-24.
	world.SetSwordPrice(16777217);
	Gear fresh(100, 100);
	CHECK_EQ(16777217, prices.GetItemPrice(&fresh, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Every durability, a spread of prices, options and rates
//----------------------------------------------------------------------
TEST(PriceRounding, EveryQuoteInASweepMatchesTheServer)
{
	RoundingWorld world;
	MPriceManager prices;

	const int kPrices[] = { 1, 7, 27, 99, 1000, 1001, 1005, 4999, 5000, 12345, 99999, 1234567, 16777217 };
	const int kRates[] = { 25, 30, 100, 120 };

	std::vector< std::vector<int> > optionSets;
	optionSets.push_back(std::vector<int>());
	optionSets.push_back(std::vector<int>(1, 1));			// 150%
	optionSets.push_back(std::vector<int>(1, 3));			// 110%
	{
		std::vector<int> rows;
		rows.push_back(1);
		rows.push_back(2);									// 150% + 50%
		optionSets.push_back(rows);
	}
	{
		std::vector<int> rows;
		rows.push_back(4);
		rows.push_back(3);
		rows.push_back(5);									// 33% + 110% + 100%
		optionSets.push_back(rows);
	}

	long long	checked = 0;
	long long	mismatches = 0;
	int			firstServer = 0;
	int			firstClient = 0;

	for (size_t p = 0; p < sizeof(kPrices) / sizeof(kPrices[0]); p++)
	{
		world.SetSwordPrice(kPrices[p]);

		for (size_t o = 0; o < optionSets.size(); o++)
		{
			const std::vector<int> multipliers = Multipliers(optionSets[o]);

			for (int max = 0; max <= 120; max++)
			{
				for (int cur = 0; cur <= max; cur++)
				{
					Gear item(max, (TYPE_ITEM_DURATION)cur);
					for (size_t r = 0; r < optionSets[o].size(); r++)
					{
						item.AddItemOption((TYPE_ITEM_OPTION)optionSets[o][r]);
					}

					for (size_t k = 0; k < sizeof(kRates) / sizeof(kRates[0]); k++)
					{
						prices.SetMarketCondSell(kRates[k]);
						prices.SetMarketCondBuy(kRates[k]);

						const int expected = RulePrice(kPrices[p], multipliers, cur, max, kRates[k]);
						const int bought = prices.GetItemPrice(&item, MPriceManager::NPC_TO_PC);
						const int sold = prices.GetItemPrice(&item, MPriceManager::PC_TO_NPC);
						checked += 2;

						if (bought != expected || sold != expected)
						{
							if (mismatches == 0)
							{
								firstServer = expected;
								firstClient = bought != expected ? bought : sold;
								std::printf("  first price mismatch: price %d, options %d, %d of %d at %d%%: server %d, client %d\n",
									kPrices[p], (int)o, cur, max, kRates[k], firstServer, firstClient);
							}
							mismatches++;
						}
					}

					// A maximum of 0 is in the sweep: the server's repair
					// price gives an item that keeps no durability 1, and the
					// client quotes the same through decore::repairPrice.
					{
						const int expected = RuleRepairPrice(kPrices[p], multipliers, cur, max);
						const int repaired = prices.GetItemPrice(&item, MPriceManager::REPAIR);
						checked++;

						if (repaired != expected)
						{
							if (mismatches == 0)
							{
								firstServer = expected;
								firstClient = repaired;
								std::printf("  first repair mismatch: price %d, options %d, %d of %d: server %d, client %d\n",
									kPrices[p], (int)o, cur, max, firstServer, firstClient);
							}
							mismatches++;
						}
					}
				}
			}
		}
	}

	CHECK(checked > 0);
	CHECK_EQ(0, mismatches);
	CHECK_EQ(firstServer, firstClient);
}
