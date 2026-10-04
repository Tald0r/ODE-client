//-----------------------------------------------------------------------------
// MTradeManager.cpp
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MTradeManager.h"
#include "MSortedItemManager.h"
#include "MGameDef.h"
#include "DebugLog.h"

#define SAFE_DELETE(x)		{ if(x!=NULL) delete x; x=NULL; }

#include "ClientConfig.h"

//-----------------------------------------------------------------------------
// Item위치를 기억하기 위한 것..
//-----------------------------------------------------------------------------
class ItemPosition {
	public :
		MItem*			pItem;
		int				gridX;
		int				gridY;

	public :
		ItemPosition(MItem* _pItem, int _gridX, int _gridY)
		{
			pItem	= _pItem;
			gridX	= _gridX;
			gridY	= _gridY;
		}
};

class ItemPositionMap : public std::map<TYPE_OBJECTID, ItemPosition*> {
	public :
		ItemPositionMap() {}
		~ItemPositionMap()
		{
			iterator iItemPosition = begin();

			while (iItemPosition!=end())
			{
				ItemPosition* pItemPosition = iItemPosition->second;

				delete pItemPosition;

				iItemPosition++;
			}
		}
};


//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
MTradeManager*		g_pTradeManager = NULL;

//-----------------------------------------------------------------------------
//
// constructor / destructor
//
//-----------------------------------------------------------------------------
MTradeManager::MTradeManager()
{
	m_OtherID	= OBJECTID_NULL;

	m_pMyInventory = NULL;
	m_pOtherInventory = NULL;
	
	m_pMyMoney = NULL;
	m_pOtherMoney = NULL;

	m_bAcceptMyTrade = false;
	m_bAcceptOtherTrade = false;
	
	m_NextAcceptTime = MonotonicClock::TimePoint();
}

MTradeManager::~MTradeManager()
{
	Release();
}

//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void				
MTradeManager::Init()
{
	Release();

	//--------------------------------------------------------------
	// Inventory 초기화
	//--------------------------------------------------------------
	m_pMyInventory = g_pInventory;	//new MInventory;
	m_pOtherInventory = new MInventory;
	
	//m_pMyInventory->Init(TRADE_INVENTORY_WIDTH, TRADE_INVENTORY_HEIGHT);
	m_pOtherInventory->Init(TRADE_INVENTORY_WIDTH, TRADE_INVENTORY_HEIGHT);


	//--------------------------------------------------------------
	// Money Manager
	//--------------------------------------------------------------
	m_pMyMoney = new MMoneyManager;
	m_pOtherMoney = new MMoneyManager;

	m_bAcceptMyTrade = false;
	m_bAcceptOtherTrade = false;

	m_OtherID	= OBJECTID_NULL;
}

//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
void				
MTradeManager::Release()
{
	// The player's own inventory: not ours to delete.
	m_pMyInventory = NULL;

	if (m_pOtherInventory != NULL)
	{
		delete m_pOtherInventory;
		m_pOtherInventory = NULL;
	}

	if (m_pMyMoney != NULL)
	{
		delete m_pMyMoney;
		m_pMyMoney = NULL;
	}

	if (m_pOtherMoney != NULL)
	{
		delete m_pOtherMoney;
		m_pOtherMoney = NULL;
	}

	m_bAcceptMyTrade = false;
	m_bAcceptOtherTrade = false;

	m_NextAcceptTime = MonotonicClock::TimePoint();
}

//-----------------------------------------------------------------------------
// Is AcceptTime
//-----------------------------------------------------------------------------
// The accept delay runs on the item host's frame stamp; without
// one (a test binary) there is no delay.
//-----------------------------------------------------------------------------
bool
MTradeManager::IsAcceptTime() const
{
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	return pClock==NULL || *pClock >= m_NextAcceptTime;
}

//-----------------------------------------------------------------------------
// Set NextAcceptTime
//-----------------------------------------------------------------------------
void
MTradeManager::SetNextAcceptTime()
{
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	m_NextAcceptTime = (pClock!=NULL ? *pClock : MonotonicClock::TimePoint()) + MonotonicClock::Millis(g_pClientConfig->TRADE_ACCEPT_DELAY_TIME);
}

//-----------------------------------------------------------------------------
// Refuse MyTrade
//-----------------------------------------------------------------------------
void				
MTradeManager::RefuseMyTrade()						
{ 
	if (m_bAcceptMyTrade)
	{
		SetNextAcceptTime();

		m_bAcceptMyTrade = false;
	}
}

//-----------------------------------------------------------------------------
// Refuse OtherTrade
//-----------------------------------------------------------------------------
void				
MTradeManager::RefuseOtherTrade()					
{ 
	if (m_bAcceptOtherTrade)
	{
		SetNextAcceptTime();

		m_bAcceptOtherTrade = false;
	}
}

//-----------------------------------------------------------------------------
// Can Trade
//-----------------------------------------------------------------------------
// 상대방의 item이 현재 내 inventory에 모두 들어갈수 있는지 체크한다.
//-----------------------------------------------------------------------------
bool				
MTradeManager::CanTrade() const
{
	if (m_pOtherInventory==NULL)
	{
		return false;
	}

	//-----------------------------------------------------------
	// 돈 액수 체크
	//-----------------------------------------------------------
	if (!g_pMoneyManager->CanAddMoney( m_pOtherMoney->GetMoney() ))
	{
		// 돈 한계를 넘어서 교환이 안되는 경우
		return false;
	}

	//-----------------------------------------------------------
	// 간단한 체크 : 개수 한계를 넘어갈 때.
	// 즉, 두 inventory의 개수 합이.. gridWidth*gridHeight를 넘는 경우
	//-----------------------------------------------------------
	// 내가 교환할거는 빼줘야 한다.
	//if (g_pInventory->GetItemNum() + m_pOtherInventory->GetItemNum()
		//> g_pInventory->GetWidth()*g_pInventory->GetHeight())
	//{
	//	return false;
	//}

	//-----------------------------------------------------------
	//
	//			나의 Temp Inventory를 만든다.
	//
	//-----------------------------------------------------------
	

	MInventory	tempInventory;
	
	tempInventory.Init( g_pInventory->GetWidth(), g_pInventory->GetHeight() );

	//-----------------------------------------------------------
	// g_pInventory ---> tempInventory
	//-----------------------------------------------------------
	// pointer만 잠시 저장해둔다.
	//-----------------------------------------------------------
	bool bOK = true;		// 제대로 다 들어가는가?

	g_pInventory->SetBegin();

	while ( g_pInventory->IsNotEnd() )
	{
		const MItem* pItem = g_pInventory->Get();

		//-----------------------------------------------------------
		// 내 inventory의 아이템 중에서.. 
		// 교환 하지 않을 것들만 temp inventory에 추가한다.
		//-----------------------------------------------------------
		if (!pItem->IsTrade())
		{
			//-----------------------------------------------------------
			// 이게 안될 수가 있을까?? - -;
			//-----------------------------------------------------------
			if (!tempInventory.AddItem( (MItem*)pItem, pItem->GetGridX(), pItem->GetGridY() ))
			{
				bOK = false;
				break;
			}
		}

		g_pInventory->Next();
	}

	//-----------------------------------------------------------
	// m_pOtherInventory의 원래 위치를 기억해둬야 한다.
	//-----------------------------------------------------------	
	ItemPositionMap		mapItemPosition;	

	//-----------------------------------------------------------
	//
	// 상대방 inventory의 모든 item이 
	// 내 inventory(tempInventory)에 들어갈 수 있는지 체크한다.
	//
	//-----------------------------------------------------------
	if (bOK)
	{			
		MSortedItemManager	SIM;

		//-----------------------------------------------------------
		// m_pOtherInventory의 원래 위치를 기억해둬야 한다.
		// 동시에.. 크기별로 Sort한다.
		//-----------------------------------------------------------
		m_pOtherInventory->SetBegin();

		while ( m_pOtherInventory->IsNotEnd() )
		{
			MItem* pItem = m_pOtherInventory->Get();

			ItemPosition* pItemPosition = new ItemPosition( (MItem*)pItem, pItem->GetGridX(), pItem->GetGridY() );
			mapItemPosition.insert( ItemPositionMap::value_type(pItem->GetID(), pItemPosition) );

			//---------------------------------------------------
			// 크기별로 sort한다.
			//---------------------------------------------------
			SIM.AddItem( pItem );

			m_pOtherInventory->Next();
		}	

		//---------------------------------------------------------------
		//
		//			SIM --> tempInventory
		//
		//---------------------------------------------------------------
		MSortedItemManager::iterator iItem = SIM.begin();

		while (iItem != SIM.end())
		{
			MItem* pItem = iItem->second;

			//-----------------------------------------------------
			// 추가가 안 된다면.. 자리가 없어서 교환 불가능
			//-----------------------------------------------------
			if (!tempInventory.AddItem( pItem ))
			{				
				bOK = false;
				
				break;
			}

			iItem ++;
		}

		SIM.Clear();
	}

	//-----------------------------------------------------------
	//
	//		tempInventory를 비워야 한다.
	//
	//-----------------------------------------------------------
	tempInventory.SetBegin();

	while ( tempInventory.IsNotEnd() )
	{
		const MItem* pItem = tempInventory.Get();

		tempInventory.Next();

		//-----------------------------------------------------------
		// 이게 안될 수가 있을까?? - -;
		//-----------------------------------------------------------
		if (!tempInventory.RemoveItem( pItem->GetGridX(), pItem->GetGridY() ))
		{			
			// 안되믄 죽음이다.

			// 이거는 그냥 pointer만 저장한거기 때문에
			// delete하면 안된다.
		}
	}

	//-----------------------------------------------------------
	// 상대방 inventory의 아이템들을 
	// 원래 위치로 되돌린다.
	//-----------------------------------------------------------
	m_pOtherInventory->SetBegin();

	while ( m_pOtherInventory->IsNotEnd() )
	{
		const MItem* pItem = m_pOtherInventory->Get();

		//-----------------------------------------------------------
		// m_pOtherInventory의 원래 위치를 기억해둬야 한다.
		//-----------------------------------------------------------
		ItemPositionMap::iterator iItemPosition = mapItemPosition.find( pItem->GetID() );

		if (iItemPosition == mapItemPosition.end())
		{
			// 나머지는 안 바뀐거라고 보면 된다..
			break;
		}

		ItemPosition* pItemPosition = iItemPosition->second;

		MItem* pOriginalItem = pItemPosition->pItem;
		pOriginalItem->SetGridXY( pItemPosition->gridX, pItemPosition->gridY );

		m_pOtherInventory->Next();
	}

	// tempInventory에는 아무것도 없어야 한다.

	return bOK;
}

//-----------------------------------------------------------------------------
// Trade
//-----------------------------------------------------------------------------
// 상대방의 item을 내 inventory로 넣는다.
//-----------------------------------------------------------------------------
bool				
MTradeManager::Trade()
{
	if (// 교환확인을... 한명이라도 안 눌렀거나
		!m_bAcceptMyTrade || !m_bAcceptOtherTrade
		// init안된 경우
		|| m_pMyInventory==NULL
		|| m_pOtherInventory==NULL
		|| m_pMyMoney==NULL
		|| m_pOtherMoney==NULL)
	{
		#ifdef OUTPUT_DEBUG
			DEBUG_ADD_FORMAT_ERR("[Error] Trade Failed! %d %d %d %d %d %d"
								,!m_bAcceptMyTrade
								,!m_bAcceptOtherTrade
								,m_pMyInventory==NULL
								,m_pOtherInventory==NULL
								,m_pMyMoney==NULL
								,m_pOtherMoney==NULL);			
		#endif

		// trade될리가 없당.
		return false;
	}

	//---------------------------------------------------------------
	//
	//				교환이 안되는 경우
	//
	//---------------------------------------------------------------
	if (!CanTrade())
	{
		DEBUG_ADD_ERR("[Error] Can't Trade");
		
		return false;
	}

	MSortedItemManager	SIM;

	//---------------------------------------------------------------
	//
	//	내 inventory에서 교환할려는 아이템을 모두 제거한다.
	//
	//---------------------------------------------------------------
	DEBUG_ADD("[Trade] Remove Trading Items");
	
	m_pMyInventory->SetBegin();

	while ( m_pMyInventory->IsNotEnd() )
	{
		const MItem* pItem = m_pMyInventory->Get();

		m_pMyInventory->Next();

		//-----------------------------------------------------------
		// 실제로 제거한다.
		//-----------------------------------------------------------
		if (pItem->IsTrade())
		{
			DEBUG_ADD_FORMAT("[Trade] Remove: id=%d. xy=(%d, %d)", pItem->GetID(), pItem->GetX(), pItem->GetY() );
			
			MItem* pRemovedItem = m_pMyInventory->RemoveItem( pItem->GetGridX(), pItem->GetGridY() );

			if (pRemovedItem!=NULL)
			{
				delete pRemovedItem;
			}
			else
			{
				// 콩가루~~
				DEBUG_ADD_ERR("[Error] Remove Error");
			}

		}				
	}

	//---------------------------------------------------------------
	//
	//		m_pOtherInventory --> SortedItemManager
	//
	//---------------------------------------------------------------
	DEBUG_ADD("[Trade] Move OtherInventory --> SortedItemManager");
	
	m_pOtherInventory->SetBegin();

	while ( m_pOtherInventory->IsNotEnd() )
	{
		const MItem* pItem = m_pOtherInventory->Get();
		
		m_pOtherInventory->Next();

		//-----------------------------------------------------------
		// OtherInventory에서 실제로 제거한다.
		//-----------------------------------------------------------
		MItem* pRemovedItem = m_pOtherInventory->RemoveItem( pItem->GetGridX(), pItem->GetGridY() );

		if (pRemovedItem!=NULL)
		{
			//-----------------------------------------------------------
			// player의 inventory에 추가한다.
			//-----------------------------------------------------------
			if (SIM.AddItem( pRemovedItem ))
			{
				//-------------------------------------------------------
				// 제대로 추가된 경우
				//-------------------------------------------------------				
			}
			else
			{
				// -_-;; 대책없는 경우
				delete pRemovedItem;	// memory leak이나 막자 - -;;
			}
		}
		else	
		{
			// 콩가루 - -;;
			// 대책없다.. 개된다.. - -;;
		}

	}

	//---------------------------------------------------------------
	//
	//			SIM --> g_pInventory
	//
	//---------------------------------------------------------------
	DEBUG_ADD("[Trade] SIM --> Inventory");
		
	MSortedItemManager::iterator iItem = SIM.begin();

	while (iItem != SIM.end())
	{
		MItem* pItem = iItem->second;

		if (g_pInventory->AddItem( pItem ))
		{
			pItem->UnSetTrade();	// -_-;

			// Event GiftBox 아이템인 경우는 녹색 --> 빨간색으로
			if (pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX)
			{
				if(pItem->GetItemType()==0)
				{
					pItem->SetItemType( 1 );
				}
				else if(pItem->GetItemType() >= 2 && pItem->GetItemType() <= 5)
				{
					pItem->SetItemType(6);
				}
				// 2004, 4, 28 sobeit add 마켓 이벤트 박스 start
				else if(pItem->GetItemType() >= 16 && pItem->GetItemType() <= 18)
				{  
					int TempItemType = pItem->GetItemType() + 3; // 검은색 박스로 바꾸기 위해 
					pItem->SetItemType(TempItemType);
				}
				// 2004, 4, 28 sobeit add 마켓 이벤트 박스 end
			}
		}
		else
		{
			// - -;
//			delete pItem;
			SAFE_DELETE( pItem );
		}

		iItem ++;
	}

	SIM.Clear();


	//-----------------------------------------------------------
	//
	//				돈 교환
	//
	//-----------------------------------------------------------
	DEBUG_ADD("[Trade] Money");
		

	if (g_pMoneyManager->AddMoney( m_pOtherMoney->GetMoney() ))
	{
		m_pOtherMoney->SetMoney( 0 );
		m_pMyMoney->SetMoney( 0 );
	}
	else
	{
		// 돈 한계 넘는 경우.. - -;
	}

	m_bAcceptMyTrade = false;
	m_bAcceptOtherTrade = false;
	
	return true;
}

//-----------------------------------------------------------------------------
// Cancel Trade
//-----------------------------------------------------------------------------
// The trade is called off: the money the player put up goes back to
// the wallet. The offered items stay where they are, flagged, until the
// next trade start clears them (GameUI).
//-----------------------------------------------------------------------------
bool
MTradeManager::CancelTrade()
{
	if (g_pMoneyManager->AddMoney( m_pMyMoney->GetMoney() ))
	{
		m_pOtherMoney->SetMoney( 0 );
		m_pMyMoney->SetMoney( 0 );
	}

	return true;
}