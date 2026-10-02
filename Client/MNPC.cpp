//----------------------------------------------------------------------
// MNPC.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MNPC.h"
#include "MNPCTable.h"
#include "MShopTemplateTable.h"
#include "MShopShelf.h"
#include "MPlayer.h"
#include "NPCShopBuilder.h"

namespace {
const NPCShopHost shopHost{
	.CreateItem = MItem::NewItem,
	.IsFemale = []() { return g_pPlayer && g_pPlayer->IsFemale(); },
	.SetPortalDestination = [](MItem& item, int zone,
		TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		static_cast<MVampirePortalItem&>(item).SetZone(zone, x, y);
	},
};
}

//----------------------------------------------------------------------
// 
// constructor / destructor
//
//----------------------------------------------------------------------
MNPC::MNPC()
{
	m_NPCID = 0;

	m_pShop = NULL;
}

MNPC::~MNPC()
{
	if (m_pShop!=NULL)
	{
		delete m_pShop;
	}
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Get NPC Info
//----------------------------------------------------------------------
NPC_INFO*			
MNPC::GetNPCInfo(TYPE_OBJECTID id) const
{
	return (*g_pNPCTable).GetData( id ); 
}

//----------------------------------------------------------------------
// Set Shop
//----------------------------------------------------------------------
void				
MNPC::SetShop(MShop* pShop)
{
	// 기존에 있던걸 지운다.
	if (m_pShop!=NULL)
	{
		delete m_pShop;
	}

	m_pShop = pShop;
}

//-----------------------------------------------------------------------------
// CreateItemFromShopTemplate
//-----------------------------------------------------------------------------
BOOL
MNPC::CreateFixedShelf(bool bMysterious)
{
	NPC_INFO* pInfo = (*g_pNPCTable).GetData( m_NPCID );

	//-----------------------------------------------------------
	// NPC의 상점을 얻는다.
	//-----------------------------------------------------------
	MShop* pShop = m_pShop;

	if (pShop==NULL)
	{
		// 상점이 없으면 상점을 만든다.
		pShop = new MShop;
		pShop->Init( MShopShelf::MAX_SHELF );

		// NPC에 상점 설정..
		m_pShop = pShop;
	}

	return BuildNPCShopShelf(*pShop, pInfo, g_pShopTemplateTable, bMysterious, shopHost);
}

//-----------------------------------------------------------------------------
// Action
//-----------------------------------------------------------------------------
void
MNPC::Action()
{
	MCreature::Action();

	// 바토리인 경우
	if (m_CreatureType==217
		// 테페즈인 경우
		|| m_CreatureType==366)
	{
		//--------------------------------------------------------
		// 방향을 바꿀 필요가 없던 경우에..
		// 심심할때마다 한번씩 방향 바꿔주기.. - -;
		//--------------------------------------------------------			
		if (//Player가 아니고
			m_CreatureType >= 4
			// 살아 있고..
			&& m_bAlive
			// 정지상태이고
			&& m_Action==ACTION_STAND
			// 움직일곳이 없고
			&& m_listMoveBuffer.size()==0
			// 정지동작의 끝에..
			&& m_ActionCount>=m_ActionCountMax-1
			// random하게.. - -;
			&& (rand() % 5)==0)
		{
			// 랜덤하게 player를 바라본다.
			SetDirectionToPosition( g_pPlayer->GetX(), g_pPlayer->GetY() );
		}
	}
	if(m_CreatureType >= 636 && m_CreatureType <= 639 )
	{
		if(m_ActionCount == 0 && !m_listEffect.empty())
		{
			ATTACHEFFECT_LIST::iterator itr = m_listEffect.begin();
			ATTACHEFFECT_LIST::iterator endItr = m_listEffect.end();
			
			while(itr != endItr )
			{
				MAttachEffect *pEffect =  *itr;
				
				pEffect->SetFrameID( pEffect->GetFrameID(), pEffect->GetMaxFrame() );
				itr++;
			}
		}
	}	
}
