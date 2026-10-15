//----------------------------------------------------------------------
// MNPC.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MNPC.h"
#include "MNPCTable.h"
#include "MShopTemplateTable.h"
#include "MShopShelf.h"
#include "NPCShopBuilder.h"
#include "PlayerStateHost.h"

// The NPCs' allocator pool (task 4.244: MemoryPool.cpp's until then).
MemoryPool g_NPCCreatureMemoryPool( sizeof( MNPC ), 5 );

namespace {
const NPCShopHost shopHost{
	.CreateItem = MItem::NewItem,
	.IsFemale = []() { MCreature* pPlayer = PlayerState::Creature(); return pPlayer != NULL && pPlayer->IsFemale(); },
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
	// Delete the one there was.
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
	// Get the NPC's shop.
	//-----------------------------------------------------------
	MShop* pShop = m_pShop;

	if (pShop==NULL)
	{
		// No shop: make one.
		pShop = new MShop;
		pShop->Init( MShopShelf::MAX_SHELF );

		// Give it to the NPC..
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

	// Bathory
	if (m_CreatureType==217
		// Tepez
		|| m_CreatureType==366)
	{
		//--------------------------------------------------------
		// When it had no need to turn..
		// turn it now and then, when it is bored.. - -;
		//--------------------------------------------------------			
		if (//not a player
			m_CreatureType >= 4
			// alive..
			&& m_bAlive
			// standing
			&& m_Action==ACTION_STAND
			// with nowhere to move
			&& m_listMoveBuffer.size()==0
			// at the end of its stand..
			&& m_ActionCount>=m_ActionCountMax-1
			// at random.. - -;
			&& (rand() % 5)==0)
		{
			// look at the player, at random.
			MCreature* pPlayer = PlayerState::Creature();
			if (pPlayer != NULL)
			{
				SetDirectionToPosition( pPlayer->GetX(), pPlayer->GetY() );
			}
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
