//----------------------------------------------------------------------
// test_trade_manager.cpp
//----------------------------------------------------------------------
//
// The trade manager (docs/RESTRUCTURING.md task 4.2, second slice):
// the exchange between the player's inventory and wallet and the
// other side's offer. What it needs from the executable is one
// millisecond clock, carried by the item host installed here by hand;
// the player's inventory and wallet are the globals the executable
// also creates.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "gamemodel_world.h"
#include "MTradeManager.h"
#include "MSortedItemManager.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "MGameDef.h"

namespace {

int		s_Alive = 0;
DWORD	s_Frame = 0;
MonotonicClock::TimePoint	s_Now;

int		DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void	RefreshAffect(MItem*)			{}
void	PlayItemSound(TYPE_SOUNDID)		{}

// A host that carries the clock and nothing else of note.
const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							NULL, NULL, NULL, NULL, NULL, NULL };

struct TradeWorld : GameModelWorld
{
	TradeWorld()
	{
		s_Alive = 0;
		s_Now = MonotonicClock::TimePoint();

		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 3);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(1, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).SetGrid(2, 2);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 2).SetGrid(3, 1);
		g_pClientConfig->TRADE_ACCEPT_DELAY_TIME = 5000;

		// The player's side: a small grid and a wallet with a ceiling.
		g_pInventory = new MInventory;
		g_pInventory->Init(3, 2);
		g_pMoneyManager = new MMoneyManager;
		g_pMoneyManager->SetMoneyLimit(1000);
		g_pMoneyManager->SetMoney(100);

		MItem::SetHost(&s_Host);
	}

	~TradeWorld()
	{
		delete g_pMoneyManager;		g_pMoneyManager = NULL;
		delete g_pInventory;		g_pInventory = NULL;
	}
};

struct Sword : public MItem
{
	Sword(TYPE_OBJECTID id, TYPE_ITEMTYPE type)	{ SetID(id); SetItemType(type); s_Alive++; }
	~Sword()									{ s_Alive--; }
	ITEM_CLASS	GetItemClass() const			{ return ITEM_CLASS_SWORD; }
};

} // namespace

//----------------------------------------------------------------------
// Lifetime
//----------------------------------------------------------------------
TEST(TradeManager, InitBuildsTheOtherSideOverThePlayersInventory)
{
	TradeWorld world;
	MTradeManager trade;

	CHECK(trade.GetMyInventory() == NULL);
	CHECK_EQ(false, trade.CanTrade());			// nothing to trade with yet
	CHECK_EQ(false, trade.Trade());

	trade.Init();
	CHECK(trade.GetMyInventory() == g_pInventory);
	CHECK(trade.GetOtherInventory() != NULL);
	CHECK_EQ(TRADE_INVENTORY_WIDTH, (int)trade.GetOtherInventory()->GetWidth());
	CHECK_EQ(TRADE_INVENTORY_HEIGHT, (int)trade.GetOtherInventory()->GetHeight());
	CHECK(trade.GetMyMoneyManager() != NULL);
	CHECK(trade.GetOtherMoneyManager() != NULL);
	CHECK_EQ(false, trade.IsAcceptMyTrade());
	CHECK_EQ(false, trade.IsAcceptOtherTrade());
	CHECK(trade.GetOtherID() == OBJECTID_NULL);

	// Release deletes the other side's items and both trade wallets; the
	// player's inventory is not the manager's to delete, only to forget.
	Sword* offered = new Sword(1, 0);
	CHECK(trade.GetOtherInventory()->AddItem(offered));
	trade.Release();
	CHECK_EQ(0, s_Alive);
	CHECK(trade.GetOtherInventory() == NULL);
	CHECK(trade.GetMyMoneyManager() == NULL);
	CHECK(trade.GetOtherMoneyManager() == NULL);
	CHECK(trade.GetMyInventory() == NULL);
	CHECK(g_pInventory != NULL);
}

//----------------------------------------------------------------------
// The accept delay
//----------------------------------------------------------------------
TEST(TradeManager, RefusingStartsTheAcceptDelayOnTheClock)
{
	TradeWorld world;
	MTradeManager trade;
	trade.Init();

	CHECK(trade.IsAcceptTime());

	// Refusing without having accepted starts no delay.
	trade.RefuseMyTrade();
	CHECK(trade.IsAcceptTime());

	s_Now = MonotonicClock::FromMillis(10000);
	trade.AcceptMyTrade();
	CHECK(trade.IsAcceptMyTrade());
	trade.RefuseMyTrade();
	CHECK_EQ(false, trade.IsAcceptMyTrade());
	CHECK_EQ(false, trade.IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(14999);
	CHECK_EQ(false, trade.IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(15000);
	CHECK(trade.IsAcceptTime());

	// The other side's refusal restarts it.
	trade.AcceptOtherTrade();
	trade.RefuseOtherTrade();
	CHECK_EQ(false, trade.IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(20000);
	CHECK(trade.IsAcceptTime());

	// Without a clock there is no delay - neither with no host at all
	// nor with a host that carries none.
	MItem::SetHost(NULL);
	trade.AcceptMyTrade();
	trade.RefuseMyTrade();
	CHECK(trade.IsAcceptTime());
	const MItemHost clockless = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, NULL,
									NULL, NULL, NULL, NULL, NULL, NULL };
	MItem::SetHost(&clockless);
	CHECK(trade.IsAcceptTime());
	// With the clock back, a refusal counts again.
	MItem::SetHost(&s_Host);
	trade.AcceptMyTrade();
	trade.RefuseMyTrade();
	CHECK_EQ(false, trade.IsAcceptTime());		// 20000 + 5000 on the clock at 20000
	s_Now = MonotonicClock::FromMillis(25000);
	CHECK(trade.IsAcceptTime());
}

//----------------------------------------------------------------------
// Feasibility
//----------------------------------------------------------------------
TEST(TradeManager, CanTradeNeedsRoomForTheOffersAndForTheMoney)
{
	TradeWorld world;
	MTradeManager trade;
	trade.Init();

	// The player keeps a 2x2 sword (cells 0..1 x 0..1) and offers a 1x1.
	Sword* kept = new Sword(1, 1);
	Sword* given = new Sword(2, 0);
	given->SetTrade();
	CHECK(g_pInventory->AddItem(kept, 0, 0));
	CHECK(g_pInventory->AddItem(given, 2, 0));

	// The other side offers two 1x1 swords: they fit in the free column.
	Sword* offerA = new Sword(3, 0);
	Sword* offerB = new Sword(4, 0);
	CHECK(trade.GetOtherInventory()->AddItem(offerA));
	CHECK(trade.GetOtherInventory()->AddItem(offerB));
	CHECK(trade.CanTrade());

	// The check must leave both inventories as they were.
	CHECK(g_pInventory->GetItem(0, 0) == kept);
	CHECK(g_pInventory->GetItem(2, 0) == given);
	CHECK_EQ(2, g_pInventory->GetItemNum());
	CHECK_EQ(2, trade.GetOtherInventory()->GetItemNum());
	// The offers sat at (0,0) and (0,1) on the other side, went to (2,0)
	// and (2,1) in the scratch grid, and are back where they were.
	CHECK_EQ(0, (int)offerA->GetGridX());
	CHECK_EQ(0, (int)offerA->GetGridY());
	CHECK_EQ(0, (int)offerB->GetGridX());
	CHECK_EQ(1, (int)offerB->GetGridY());
	CHECK(trade.GetOtherInventory()->GetItem(0, 0) == offerA);
	CHECK(trade.GetOtherInventory()->GetItem(0, 1) == offerB);

	// A third 1x1 would not fit beside the kept sword.
	Sword* offerC = new Sword(5, 0);
	CHECK(trade.GetOtherInventory()->AddItem(offerC));
	CHECK_EQ(false, trade.CanTrade());
	delete trade.GetOtherInventory()->RemoveItem((TYPE_OBJECTID)5);
	CHECK(trade.CanTrade());

	// Money the player's wallet cannot take blocks the trade too.
	trade.GetOtherMoneyManager()->SetMoney(901);		// 100 held, limit 1000
	CHECK_EQ(false, trade.CanTrade());
	trade.GetOtherMoneyManager()->SetMoney(900);
	CHECK(trade.CanTrade());

	trade.Release();
	CHECK_EQ(2, s_Alive);		// the player's two swords are still theirs
}

//----------------------------------------------------------------------
// The exchange
//----------------------------------------------------------------------
TEST(TradeManager, TradeSwapsItemsAndMoneyOnceBothAccepted)
{
	TradeWorld world;
	MTradeManager trade;
	trade.Init();

	Sword* kept = new Sword(1, 0);
	Sword* given = new Sword(2, 0);
	given->SetTrade();
	CHECK(g_pInventory->AddItem(kept, 0, 0));
	CHECK(g_pInventory->AddItem(given, 1, 0));
	Sword* offer = new Sword(3, 2);					// 3x1: needs the free row
	CHECK(trade.GetOtherInventory()->AddItem(offer));
	trade.GetOtherMoneyManager()->SetMoney(250);
	trade.GetMyMoneyManager()->SetMoney(40);

	// Both must have accepted.
	CHECK_EQ(false, trade.Trade());
	trade.AcceptMyTrade();
	CHECK_EQ(false, trade.Trade());
	trade.AcceptOtherTrade();
	CHECK(trade.Trade());

	// The offered sword is gone, the other's arrived and is no longer
	// marked for trade; the money moved into the player's wallet and
	// both trade wallets are empty; the acceptances are consumed.
	CHECK(g_pInventory->GetItem((TYPE_OBJECTID)2) == NULL);
	CHECK(g_pInventory->GetItem((TYPE_OBJECTID)3) == offer);
	CHECK_EQ(false, (bool)offer->IsTrade());
	CHECK(g_pInventory->GetItem(0, 1) == offer);
	CHECK(g_pInventory->GetItem(0, 0) == kept);
	CHECK_EQ(2, g_pInventory->GetItemNum());
	CHECK_EQ(0, trade.GetOtherInventory()->GetItemNum());
	CHECK_EQ(350, g_pMoneyManager->GetMoney());
	CHECK_EQ(0, trade.GetOtherMoneyManager()->GetMoney());
	CHECK_EQ(0, trade.GetMyMoneyManager()->GetMoney());
	CHECK_EQ(false, trade.IsAcceptMyTrade());
	CHECK_EQ(false, trade.IsAcceptOtherTrade());
	CHECK_EQ(2, s_Alive);

	trade.Release();
	CHECK_EQ(2, s_Alive);
}

TEST(TradeManager, CancelRefundsTheMoneyThePlayerOffered)
{
	TradeWorld world;
	MTradeManager trade;
	trade.Init();
	trade.GetMyMoneyManager()->SetMoney(60);
	trade.GetOtherMoneyManager()->SetMoney(500);

	CHECK(trade.CancelTrade());
	CHECK_EQ(160, g_pMoneyManager->GetMoney());
	CHECK_EQ(0, trade.GetMyMoneyManager()->GetMoney());
	CHECK_EQ(0, trade.GetOtherMoneyManager()->GetMoney());
}

//----------------------------------------------------------------------
// The size-ordered map the exchange packs with
//----------------------------------------------------------------------
TEST(SortedItemManager, OrdersBiggerFootprintsFirstThenById)
{
	TradeWorld world;
	MSortedItemManager sorted;

	Sword* small = new Sword(9, 0);		// 1x1
	Sword* big = new Sword(1, 1);		// 2x2
	Sword* wide = new Sword(5, 2);		// 3x1
	Sword* small2 = new Sword(4, 0);
	CHECK(sorted.AddItem(small));
	CHECK(sorted.AddItem(big));
	CHECK(sorted.AddItem(wide));
	CHECK(sorted.AddItem(small2));
	CHECK_EQ(false, sorted.AddItem(small));	// the same key twice

	MSortedItemManager::iterator it = sorted.begin();
	CHECK(it->second == big);	++it;
	CHECK(it->second == wide);	++it;
	CHECK(it->second == small2);	++it;		// among equals, the lower id
	CHECK(it->second == small);	++it;
	CHECK(it == sorted.end());

	sorted.Release();
	CHECK_EQ(0, s_Alive);
	CHECK(sorted.empty());
}

//----------------------------------------------------------------------
// The legacy wrap
//----------------------------------------------------------------------
TEST(TradeManager, AcceptDelayAcrossTheLegacyWrapStillWaits)
{
	// The DWORD sum wrapped: (2^32 - 1000) + 5000 is 4000 mod 2^32, and
	// "clock >= 4000" allowed the next accept at once.
	const DWORD dw_now = (DWORD)(0x100000000ull - 1000);
	CHECK(dw_now >= (DWORD)(dw_now + 5000));

	TradeWorld world;
	MTradeManager trade;
	trade.Init();

	s_Now = MonotonicClock::FromMillis(0x100000000ull - 1000);
	trade.AcceptMyTrade();
	trade.RefuseMyTrade();
	CHECK_EQ(false, trade.IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(0x100000000ull + 3999);
	CHECK_EQ(false, trade.IsAcceptTime());
	s_Now = MonotonicClock::FromMillis(0x100000000ull + 4000);
	CHECK(trade.IsAcceptTime());
}
