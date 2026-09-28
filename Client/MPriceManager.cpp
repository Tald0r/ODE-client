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

#define CHARGE_PRICE		5000

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

	__int64	finalPrice;


	// 2004, 08, 02, sobeit add start
	if(pItem->GetItemClass() == ITEM_CLASS_MOON_CARD && pItem->GetItemType() == 4)
		return m_EventFixPrice;
	// 2004, 08, 02, sobeit add end				
	//-------------------------------------------------------
	// The rate, by what the player is doing
	//-------------------------------------------------------
	int nRatio;

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
		// Repair
		//-------------------------------------------------------
		case REPAIR :
			// A vampire portal, a timed item and a blood bible sign
			// are never repaired.
			if (pItem->GetItemClass()==ITEM_CLASS_VAMPIRE_PORTAL_ITEM
				|| g_pTimeItemManager->IsExist( pItem->GetID() )
				|| pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN
				)
			{
				return 0;	
			}

			// A repair costs a tenth of the price.
			nRatio = 10;
		break;
	}

	//-------------------------------------------------------
	// A charged item is priced by its charges, and nothing below
	// applies to it.
	//-------------------------------------------------------
	if (pItem->IsChargeItem())
	{		
		int curCharge = pItem->GetNumber();
		int maxCharge = pItem->GetMaxNumber();
		
		int ChargePrice = CHARGE_PRICE;
		
		if( pItem->GetItemClass() == ITEM_CLASS_OUSTERS_SUMMON_ITEM )
			ChargePrice = 1000;

		if (type==REPAIR)
		{
			// Refill the missing charges.
			int charge = maxCharge - curCharge;

			return charge * ChargePrice;
		}
		
		int itemPrice = pItem->GetPrice();

		int finalPrice = itemPrice + curCharge * ChargePrice;
			
		finalPrice = finalPrice * nRatio / 100;

		return finalPrice;
	}	

	//-------------------------------------------------------
	// Everything else: the table price, by options and by wear.
	//-------------------------------------------------------
	{
		int		itemDur = pItem->GetMaxDurability();
		long	curDurability = pItem->GetCurrentDurability();
		
		//--------------------------------------------------
		// Nothing to repair on a whole item, or on one with no
		// durability to lose.
		//--------------------------------------------------
		if (type==REPAIR)
		{
			if (itemDur<0 || itemDur==curDurability)
			{
				return 0;
			}
		}
		

		if (itemDur<0)
		{
			itemDur = 0;
		}

		long	originalPrice = pItem->GetPrice();
		long	maxDurability = itemDur;
			
		finalPrice = originalPrice;
		
		{
			// The options add their multipliers.
			if (!pItem->IsEmptyItemOptionList())
			{
				int priceMult = pItem->GetItemOptionPriceMultiplier();//(*g_pItemOptionTable)[pItem->GetItemOption()].PriceMultiplier;
				finalPrice = finalPrice * priceMult / 100;
			}
			
			// Wear takes its share of the price...
			float damaged;
			if (maxDurability==0)
			{
				damaged = 1;
			}
			else
			{
				damaged = (float)curDurability / (float)maxDurability;
			}

			//--------------------------------------------------
			// ...and a repair charges for that share.
			//--------------------------------------------------
			if (type==REPAIR)
			{
				damaged = 1.0f - damaged;
			}
			
			finalPrice = static_cast<__int64>(finalPrice * damaged);
			
			// Then the rate.
			finalPrice = finalPrice * nRatio / 100;
		}	
	}

	
	// A weak slayer pays 70% for the two basic potions.
	if (pItem->GetItemClass()==ITEM_CLASS_POTION)
	{
		if ((pItem->GetItemType()==0 || pItem->GetItemType()==5)
			&& HostRace()==RACE_SLAYER
			&& HostStatSum() <= 40)
		{
			finalPrice = finalPrice * 70 / 100;
		}
	}

	// The consumables are half price under the premium event or the
	// NEMA blood bible.
	if ((pItem->GetItemClass() == ITEM_CLASS_POTION ||
		 pItem->GetItemClass() == ITEM_CLASS_SERUM ||
		 pItem->GetItemClass() == ITEM_CLASS_LARVA ||
		 pItem->GetItemClass() == ITEM_CLASS_PUPA ||
		 pItem->GetItemClass() == ITEM_CLASS_COMPOS_MEI)
		&& HostPotionHalfPrice())
	{
		finalPrice /= 2;
	}

	// The tax-change event scales what the shop charges.
	if (type == NPC_TO_PC)
	{
		finalPrice = finalPrice * HostShopTaxPercent() / 100;
	}

	// Nothing is free.
	if (finalPrice==0)
	{
		return 1;
	}

	// A skull is worth half to a vampire and three quarters to an Ousters.
	if (pItem->GetItemClass()==ITEM_CLASS_SKULL)
	{
		int race = HostRace();

		if (race==RACE_VAMPIRE)
		{
			finalPrice >>= 1;
		}
		else if (race==RACE_OUSTERS)
		{
			finalPrice = finalPrice * 75 / 100;
		}
	}

		
	// Then the head-price rate the server sent.
	if(pItem->GetItemClass() == ITEM_CLASS_SKULL)
	{
		finalPrice    = finalPrice * g_pUserInformation->HeadPrice / 100;
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

