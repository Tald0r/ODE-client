//-----------------------------------------------------------------------------
// MPriceManager.h
//-----------------------------------------------------------------------------

#ifndef __MPRICEMANAGER_H__
#define __MPRICEMANAGER_H__

class MItem;

// A price shown as stars: number stars of the given type.
struct STAR_ITEM_PRICE {
	int type;
	int number;
};

//-----------------------------------------------------------------------------
// MPriceHost - what the price manager needs from the executable
// (docs/RESTRUCTURING.md task 4.2). The prices compile into gamemodel,
// which cannot see the player, the event manager or the skill set, so
// the executable installs these once at start-up (GameInit.cpp); a test
// binary installs its own, or none - and without one a price carries
// no player, event or skill adjustment.
//
// The last three are inputs of the server's price rule (decore::
// itemPrice) that the server never sends: the executable answers them
// with the documented defaults below, which are also the answers
// without a host. The crown moon card's price is not among them: the
// server announces it (NOTICE_EVENT_CROWN_PRICE, SetEventItemPrice).
// Nor is the castle tax: the packets that open the shop carry its
// ratio (SetShopTaxRatio). The host once carried the tax-change
// notice's percentage (EVENTID_TAX_CHANGE, ShopTaxPercent), which
// taxed every buy price a second time; task 4.12's slice 5 removed it,
// so no notice can reach a price.
//-----------------------------------------------------------------------------
struct MPriceHost {
	int		(*Race)();					// RACE_SLAYER, RACE_VAMPIRE or RACE_OUSTERS (RaceType.h); -1 for none of them
	int		(*Level)();
	int		(*StatSum)();				// STR + DEX + INT as worn
	int		(*BasicStatSum)();			// STR + DEX + INT before gear and affects
	bool	(*IsPotionHalfPrice)();		// the premium half-price event, or the NEMA blood bible
	bool	(*IsGambleHalfPrice)();		// the JAVE blood bible
	bool	(*IsCreateTypeGame)(const MItem* pItem);	// the game gave the item away (CREATE_TYPE_GAME), so it sells for 1; default false
	bool	(*IsPayPlaying)();			// the player pays, which the premium half price needs (GamePlayer::isPayPlaying); default true
	int		(*PotionPriceRatio)();		// the Blood Bible potion price percentage (OPTION_POTION_PRICE), 0 for none; default 0
};

class MPriceManager {
	public :
		enum TRADE_TYPE
		{
			NPC_TO_PC,		// the item's buy price, at m_MarketCondSell (a purchase is GetPurchasePrice)
			PC_TO_NPC,		// the player sells, at m_MarketCondBuy
			REPAIR,			// the player repairs
			SILVERING,		// the player has the item silver-coated
		};

	public :
		MPriceManager(); 	
		~MPriceManager();

		//-------------------------------------------------------		
		// Get Item Price
		//-------------------------------------------------------		
		int			GetItemPrice(MItem* pItem, TRADE_TYPE type);
		void		GetItemPrice(MItem* pItem, STAR_ITEM_PRICE& price);
		int			GetMysteriousPrice(MItem* pItem) const;

		//-------------------------------------------------------
		// A shop purchase: what buying count of pItem costs, with the
		// castle tax, as the server's buy handler charges it; a
		// motorcycle, which that handler prices on its own, is one
		// item's price and untaxed. The buy check and every buy price
		// the shop shows ask here.
		//-------------------------------------------------------
		unsigned	GetPurchasePrice(MItem* pItem, int count);

		// The MarketCondSell of the packets that open the shop's racks
		// (GCShopVersion, GCShopList, GCShopListMysterious): the
		// castle's item tax ratio for this player, a percentage; 100
		// or below taxes nothing. When the ratio is 100, GCShopVersion
		// sends the NPC's own market condition in its place, which is
		// 100 at every shop in the server's seed and so taxes nothing
		// either. The ratio is no market condition, and the sell
		// dialog's market condition (GCShopMarketCondition) is not it.
		void		SetShopTaxRatio(int ratio)			{ m_ShopTaxRatio = ratio; }

		//-------------------------------------------------------
		// The market conditions, buy and sell as the NPC sees them
		//-------------------------------------------------------
		void		SetMarketCondBuy(int buy)			{ m_MarketCondBuy = buy; }
		void		SetMarketCondSell(int sell)			{ m_MarketCondSell = sell; }

		int			GetMarketCondBuy() const			{ return m_MarketCondBuy; }
		int			GetMarketCondSell() const			{ return m_MarketCondSell; }

		void		SetEventItemPrice(int Price)		{ m_EventFixPrice = Price; }

		static void					SetHost(const MPriceHost* pHost)	{ s_pHost = pHost; }

	protected :
		// The host's answers, and what they are without one: no race,
		// no level, no stats, no discount, and the defaults of the
		// three the server never sends.
		static int		HostRace()				{ return s_pHost!=NULL ? s_pHost->Race() : -1; }
		static int		HostLevel()				{ return s_pHost!=NULL ? s_pHost->Level() : 0; }
		static int		HostStatSum()			{ return s_pHost!=NULL ? s_pHost->StatSum() : 0; }
		static int		HostBasicStatSum()		{ return s_pHost!=NULL ? s_pHost->BasicStatSum() : 0; }
		static bool		HostPotionHalfPrice()	{ return s_pHost!=NULL && s_pHost->IsPotionHalfPrice(); }
		static bool		HostGambleHalfPrice()	{ return s_pHost!=NULL && s_pHost->IsGambleHalfPrice(); }
		static bool		HostIsCreateTypeGame(const MItem* pItem)	{ return s_pHost!=NULL && s_pHost->IsCreateTypeGame(pItem); }
		static bool		HostIsPayPlaying()		{ return s_pHost==NULL || s_pHost->IsPayPlaying(); }
		static int		HostPotionPriceRatio()	{ return s_pHost!=NULL ? s_pHost->PotionPriceRatio() : 0; }


		// The item's price, one at the NPC's market condition
		int					PriceAt(MItem* pItem, TRADE_TYPE type, int marketCondSell);

		// The market conditions, as the NPC sees them
		int					m_MarketCondBuy;		// when the NPC buys (25)
		int					m_MarketCondSell;		// when the NPC sells (100), as the sell dialog last said
		int					m_ShopTaxRatio;			// the castle's tax on a purchase, a percentage (100)
		int					m_EventFixPrice;		// the event item price the server sets

		static const MPriceHost*	s_pHost;
};

extern MPriceManager*		g_pPriceManager;

#endif

