//-----------------------------------------------------------------------------
// MPriceManager.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "MPriceManager.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "UserInformation.h"
#include "MTimeItemManager.h"
#include "RaceType.h"

#include "domain/ItemPrice.h"

#include <vector>

#define CHARGE_PRICE		5000

//-----------------------------------------------------------------------------
// The price rule is the server's (decore, third_party/decore/README.md),
// and it branches on the wire item-class ids: they must be this client's.
//-----------------------------------------------------------------------------
static_assert(decore::itemclass::Potion == ITEM_CLASS_POTION);
static_assert(decore::itemclass::Skull == ITEM_CLASS_SKULL);
static_assert(decore::itemclass::Serum == ITEM_CLASS_SERUM);
static_assert(decore::itemclass::SlayerPortalItem == ITEM_CLASS_SLAYER_PORTAL_ITEM);
static_assert(decore::itemclass::VampirePortalItem == ITEM_CLASS_VAMPIRE_PORTAL_ITEM);
static_assert(decore::itemclass::Larva == ITEM_CLASS_LARVA);
static_assert(decore::itemclass::Pupa == ITEM_CLASS_PUPA);
static_assert(decore::itemclass::ComposMei == ITEM_CLASS_COMPOS_MEI);
static_assert(decore::itemclass::OustersSummonItem == ITEM_CLASS_OUSTERS_SUMMON_ITEM);
static_assert(decore::itemclass::MoonCard == ITEM_CLASS_MOON_CARD);

namespace {

//-----------------------------------------------------------------------------
// What the price rule reads off the item: its class and type, the
// table price, the grade it is priced by, each option's price
// multiplier (kept in multipliers, which the input points into) and
// its durability. maxDurability is the one the caller settled on;
// every other field is left for the caller: 0, false, no race.
//-----------------------------------------------------------------------------
decore::ItemPriceInput
GatherPriceInput(const MItem* pItem, int maxDurability, std::vector<int>& multipliers)
{
	decore::ItemPriceInput input = {};
	input.itemClass = (int)pItem->GetItemClass();
	input.itemType = (int)pItem->GetItemType();
	input.basePrice = (unsigned)(*g_pItemTable)[pItem->GetItemClass()][pItem->GetItemType()].Price;
	input.grade = pItem->GetPriceGrade();

	const std::list<TYPE_ITEM_OPTION>& optionList = pItem->GetItemOptionList();
	std::list<TYPE_ITEM_OPTION>::const_iterator itr = optionList.begin();
	while (itr != optionList.end())
	{
		multipliers.push_back((*g_pItemOptionTable)[*itr].PriceMultiplier);
		itr++;
	}
	input.optionPriceMultipliers = multipliers.data();
	input.optionCount = (int)multipliers.size();

	input.curDurability = (unsigned)pItem->GetCurrentDurability();
	input.maxDurability = (unsigned)maxDurability;
	input.race = decore::PriceRace::None;
	return input;
}

//-----------------------------------------------------------------------------
// The price rule's race for RaceType.h's race, or the host's -1.
//-----------------------------------------------------------------------------
decore::PriceRace
PriceRaceOf(int race)
{
	switch (race)
	{
		case RACE_SLAYER :	return decore::PriceRace::Slayer;
		case RACE_VAMPIRE :	return decore::PriceRace::Vampire;
		case RACE_OUSTERS :	return decore::PriceRace::Ousters;
	}
	return decore::PriceRace::None;
}

//-----------------------------------------------------------------------------
// What repairing the item costs: the server's repair price
// (decore::repairPrice, PriceManager::getRepairPrice), with none of the
// buy and sell adjustments. A charged item is charged for the charges
// it lacks; anything else for a tenth of what the wear took.
//-----------------------------------------------------------------------------
int
RepairPrice(const MItem* pItem)
{
	// A vampire portal, a timed item and a blood bible sign are never
	// repaired.
	if (pItem->GetItemClass()==ITEM_CLASS_VAMPIRE_PORTAL_ITEM
		|| (g_pTimeItemManager != NULL && g_pTimeItemManager->IsExist( pItem->GetID() ))
		|| pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN)
	{
		return 0;
	}

	// Nor is a couple ring. The server builds it outside ConcreteItem,
	// with Item's durability and maximum of 1, so its repair price is 0
	// (the durability is the maximum): that is what its repair of all the
	// worn gear adds for one, and it refuses to repair one on its own
	// (isRepairableItem). The client's maximum is the ring's rule, at
	// least 1000 (MItem.cpp), which would quote nearly a tenth of the
	// price.
	if (pItem->GetItemClass() == ITEM_CLASS_COUPLE_RING
		|| pItem->GetItemClass() == ITEM_CLASS_VAMPIRE_COUPLE_RING)
	{
		return 0;
	}

	// An item whose class has no durability (MItem's -1) is not
	// repaired, unless it holds charges.
	int maxDurability = pItem->GetMaxDurability();
	if (maxDurability < 0)
	{
		if (!pItem->IsChargeItem())
		{
			return 0;
		}
		maxDurability = 0;
	}

	std::vector<int> multipliers;
	decore::ItemPriceInput input = GatherPriceInput(pItem, maxDurability, multipliers);

	if (pItem->IsChargeItem())
	{
		input.charge = (int)pItem->GetNumber();
		input.maxCharge = (int)pItem->GetMaxNumber();
	}

	return decore::repairPrice(input);
}

} // namespace

//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
MPriceManager*		g_pPriceManager = NULL;
const MPriceHost*	MPriceManager::s_pHost = NULL;

//-----------------------------------------------------------------------------
//
// contructor / destructor
// 
//-----------------------------------------------------------------------------
MPriceManager::MPriceManager()
{
	m_MarketCondBuy		= 25;		// when the NPC buys
	m_MarketCondSell	= 100;		// when the NPC sells
	m_EventFixPrice		= 0;
}

MPriceManager::~MPriceManager()
{
}

//-----------------------------------------------------------------------------
//
// member functions
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Get Item Price
//-----------------------------------------------------------------------------
int
MPriceManager::GetItemPrice(MItem* pItem, TRADE_TYPE type, bool bMysterious)
{
	(void)bMysterious;
	if (pItem==NULL)
	{
		return 0;
	}
	if(!pItem->IsIdentified())
		return GetMysteriousPrice(pItem);

	// A repair is the server's repair price and nothing below.
	if (type==REPAIR)
		return RepairPrice(pItem);

	__int64	finalPrice;

	// An item the game gave away, and a time-limited item (one the
	// timed-item register holds), are priced by the server's rule below,
	// which gives them a flat 1 and 50 ahead of the crown price and the
	// charges.
	const bool bCreateTypeGame = HostIsCreateTypeGame(pItem);
	const bool bTimeLimited = g_pTimeItemManager != NULL
							&& g_pTimeItemManager->IsExist( pItem->GetID() );
	const bool bFlatPrice = bCreateTypeGame || bTimeLimited;

	// The crown moon card is worth what the server last announced.
	if(!bFlatPrice
		&& pItem->GetItemClass() == ITEM_CLASS_MOON_CARD && pItem->GetItemType() == 4)
		return m_EventFixPrice;
	//-------------------------------------------------------
	// The rate, by what the player is doing
	//-------------------------------------------------------
	int nRatio = 100;

	switch (type)
	{
		//-------------------------------------------------------
		// Buying from the shop
		//-------------------------------------------------------
		case NPC_TO_PC :
			if (pItem->GetItemClass()==ITEM_CLASS_SKULL)
			{
				// A skull is only ever sold to the shop.
				nRatio = m_MarketCondBuy;
			}
			else
			{
				nRatio = m_MarketCondSell;
			}
		break;

		//-------------------------------------------------------
		// Selling to the shop
		//-------------------------------------------------------
		case PC_TO_NPC :
			nRatio = m_MarketCondBuy;
		break;

		//-------------------------------------------------------
		// Silver coating
		//-------------------------------------------------------
		case SILVERING :
		{
			ITEM_CLASS itemClass = pItem->GetItemClass();

			if (itemClass==ITEM_CLASS_BLADE
				|| itemClass==ITEM_CLASS_SWORD
				|| itemClass==ITEM_CLASS_CROSS
				|| itemClass==ITEM_CLASS_MACE)
			{
				double    maxSilver  = pItem->GetSilverMax();
				double    curSilver  = pItem->GetSilver();
				double    finalPrice = 0;

				// The coat is full.
				if (maxSilver==curSilver)
				{
					return 0;
				}

				// The price of the silver that fills the coat.
				finalPrice = maxSilver; 

				return (int)finalPrice;
			}
			else
			{
				return 0;
			}
		}
		break;

		//-------------------------------------------------------
		// Repair: priced above (RepairPrice)
		//-------------------------------------------------------
		case REPAIR :
		break;
	}

	//-------------------------------------------------------
	// A charged item is priced by its charges, and nothing below
	// applies to it.
	//-------------------------------------------------------
	if (pItem->IsChargeItem() && !bFlatPrice)
	{		
		int curCharge = pItem->GetNumber();
		
		int ChargePrice = CHARGE_PRICE;
		
		if( pItem->GetItemClass() == ITEM_CLASS_OUSTERS_SUMMON_ITEM )
			ChargePrice = 1000;

		int itemPrice = pItem->GetPrice();

		int finalPrice = itemPrice + curCharge * ChargePrice;
			
		finalPrice = finalPrice * nRatio / 100;

		return finalPrice;
	}	

	//-------------------------------------------------------
	// Everything else is the server's rule (decore::itemPrice): the
	// table price, the grade, the options, the wear and the rate, then
	// the player's adjustments - the weak slayer's potion discount,
	// the skull's race share and the half price - in double, truncated
	// once at the end and never below 1.
	//-------------------------------------------------------
	{
		int		itemDur = pItem->GetMaxDurability();

		if (itemDur<0)
		{
			itemDur = 0;
		}

		std::vector<int> multipliers;
		decore::ItemPriceInput input = GatherPriceInput(pItem, itemDur, multipliers);

		input.marketCond = nRatio;
		input.crownPrice = m_EventFixPrice;
		input.createTypeGame = bCreateTypeGame;
		input.timeLimited = bTimeLimited;
		input.race = PriceRaceOf(HostRace());
		input.currentStatSum = HostStatSum();
		// The consumables are half price under the premium event or the
		// NEMA blood bible, for a player who pays.
		input.premiumHalf = HostPotionHalfPrice() && HostIsPayPlaying();
		input.potionPriceRatio = HostPotionPriceRatio();
		finalPrice = decore::itemPrice(input);
	}

	// The tax-change event scales what the shop charges.
	if (type == NPC_TO_PC)
	{
		finalPrice = finalPrice * HostShopTaxPercent() / 100;
	}

	// A tax below 100% does not make it free either.
	if (finalPrice==0)
	{
		return 1;
	}

	// Then the head-price bonus the server sent at login, a percentage
	// it divides by 100 in integers before multiplying: 150% pays x1,
	// below 100% nothing (decore::skullSellTotal). The server applies it
	// to the price times the count, which comes to the same as applying
	// it to one skull and multiplying, as the callers do.
	if(pItem->GetItemClass() == ITEM_CLASS_SKULL)
	{
		finalPrice = (__int64)decore::skullSellTotal((unsigned)finalPrice, (unsigned)g_pUserInformation->HeadPrice);
	}


	return (int)finalPrice;
}

//-----------------------------------------------------------------------------
// Get ItemPrice
//-----------------------------------------------------------------------------
// 뭔가 이상한 경우는..
// price.type = -1;
// price.number = 0;
//-----------------------------------------------------------------------------
void		
MPriceManager::GetItemPrice(MItem* pItem, STAR_ITEM_PRICE& price)
{
	if (pItem==NULL)
	{
		price.type = -1;
		price.number = 0;
		return;
	}

	// 옵션에 따라서 별의 type을 결정한다.
	switch (pItem->GetItemOptionPart())
    {
		case ITEMOPTION_TABLE::PART_DAMAGE		: price.type = 0; break;	// 검정
        case ITEMOPTION_TABLE::PART_STR			: price.type = 1; break;	// 빨강
        case ITEMOPTION_TABLE::PART_INT			: price.type = 2; break;	// 파랑
        case ITEMOPTION_TABLE::PART_DEX			: price.type = 3; break;	// 초록
        case ITEMOPTION_TABLE::PART_ATTACK_SPEED	: price.type = 4; break;	// 하늘

        default: 
			price.type = -1;
			price.number = 0;
		return;
	}

	// type에 따라서 별의 개수를 결정한다.
	price.number = (pItem->GetItemType() - 1) * 20;
}


//-----------------------------------------------------------------------------
// Get Mysterious Price
//-----------------------------------------------------------------------------
// The gamble: an unidentified item's price follows its class and the
// character buying it.
//-----------------------------------------------------------------------------
int MPriceManager::GetMysteriousPrice(MItem *pItem) const
{
	// A slayer pays by basic stats, everyone else by level: one to
	// twenty times the class's average price.
	int multiplier = 1;

	if (HostRace()==RACE_SLAYER)
	{
		int CSUM = HostBasicStatSum();

		if(CSUM > 0)
			multiplier = CSUM / 15;
	}
	else
	{
		multiplier = HostLevel() / 5;
	}

	multiplier = max(1, multiplier);

	int avr = (*g_pItemTable)[pItem->GetItemClass()].GetAveragePrice();

	__int64 final_price = (__int64)avr * multiplier;

	// Half under the JAVE blood bible, then the shop tax.
	if (HostGambleHalfPrice())
	{
		final_price /= 2;
	}

	final_price = final_price * HostShopTaxPercent() / 100;

	return (int)final_price;
}

