//----------------------------------------------------------------------
// test_price_rounding.cpp
//----------------------------------------------------------------------
//
// What a shop quotes for an item with options and wear must be what
// the server charges or pays. The server (opendarkeden-server,
// PriceManager::getPrice and getRepairPrice) works in double from the
// table price through the options, the wear and the market rate, and
// truncates once at the end; ServerPrice and ServerRepairPrice below
// transcribe that arithmetic and are the reference every quote here is
// checked against.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MPriceManager.h"

#include <cstdio>
#include <vector>

namespace {

//----------------------------------------------------------------------
// The server's arithmetic, for an item with no grade and no charges.
//----------------------------------------------------------------------
double	ServerOptionedPrice(int price, const std::vector<int>& multipliers)
{
	double originalPrice = price;

	if (!multipliers.empty())
	{
		double finalPrice = 0;
		for (size_t i = 0; i < multipliers.size(); i++)
		{
			double priceMultiplier = (double)multipliers[i];
			finalPrice += (originalPrice * priceMultiplier / 100);
		}
		originalPrice = finalPrice;
	}

	return originalPrice;
}

// PriceManager::getPrice: nDiscount is the market rate the shop list
// sent, which the client holds as its buying and selling conditions.
int		ServerPrice(int price, const std::vector<int>& multipliers, int cur, int max, int nDiscount)
{
	double originalPrice = ServerOptionedPrice(price, multipliers);
	double finalPrice = 0;

	double maxDurability = (double)max;
	double curDurability = (double)cur;

	if (maxDurability > 1)
		finalPrice = originalPrice * curDurability / maxDurability;
	else
		finalPrice = originalPrice;

	finalPrice = finalPrice * nDiscount / 100;

	int result = (int)finalPrice;
	return result > 1 ? result : 1;
}

// PriceManager::getRepairPrice.
int		ServerRepairPrice(int price, const std::vector<int>& multipliers, int cur, int max)
{
	double originalPrice = ServerOptionedPrice(price, multipliers);
	double finalPrice = 0;

	double maxDurability = (double)max;
	double curDurability = (double)cur;

	if (maxDurability != 0)
	{
		if (curDurability == maxDurability)
		{
			return 0;
		}
		finalPrice = originalPrice * curDurability / maxDurability;
	}
	else
	{
		finalPrice = originalPrice;
	}

	finalPrice = (originalPrice - finalPrice) / 10.0;

	if (finalPrice < 1.0)
	{
		return 1;
	}

	int result = (int)finalPrice;
	return result > 0 ? result : 0;
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
	const std::vector<int> none;

	// Repairing 40 of 100 on a 1000 item: the server charges 40.
	Gear worn(100, 60);
	CHECK_EQ(ServerRepairPrice(1000, none, 60, 100), 40);
	CHECK_EQ(40, prices.GetItemPrice(&worn, MPriceManager::REPAIR));

	// A 5000 item at 39 of 50 sells for 3900.
	world.SetSwordPrice(5000);
	Gear g(50, 39);
	CHECK_EQ(ServerPrice(5000, none, 39, 50, 100), 3900);
	CHECK_EQ(3900, prices.GetItemPrice(&g, MPriceManager::NPC_TO_PC));

	// Half of 27 at a 30% rate is 4.05: 4, not 3 from truncating 13.5
	// before the rate.
	world.SetSwordPrice(27);
	Gear half(2, 1);
	prices.SetMarketCondBuy(30);
	CHECK_EQ(ServerPrice(27, none, 1, 2, 30), 4);
	CHECK_EQ(4, prices.GetItemPrice(&half, MPriceManager::PC_TO_NPC));

	// 1001 at 150% is 1501.5, and two thirds of that is 1001: the
	// half is not dropped before the wear.
	world.SetSwordPrice(1001);
	Gear optioned(3, 2);
	optioned.AddItemOption(1);
	std::vector<int> rows(1, 1);
	CHECK_EQ(ServerPrice(1001, Multipliers(rows), 2, 3, 100), 1001);
	CHECK_EQ(1001, prices.GetItemPrice(&optioned, MPriceManager::NPC_TO_PC));
}

//----------------------------------------------------------------------
// Precision
//----------------------------------------------------------------------
TEST(PriceRounding, PricesPast2To24KeepEveryUnit)
{
	RoundingWorld world;
	MPriceManager prices;
	const std::vector<int> none;

	world.SetSwordPrice(16777217);
	Gear fresh(100, 100);
	CHECK_EQ(ServerPrice(16777217, none, 100, 100, 100), 16777217);
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

						const int expected = ServerPrice(kPrices[p], multipliers, cur, max, kRates[k]);
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

					// The server repairs only what has a durability.
					if (max > 0)
					{
						const int expected = ServerRepairPrice(kPrices[p], multipliers, cur, max);
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
