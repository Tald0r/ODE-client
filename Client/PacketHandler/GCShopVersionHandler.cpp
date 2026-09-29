//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopVersionHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopVersion.h"
#include "ClientDef.h"
#include "MNPC.h"
#include "MShopShelf.h"
#include "ServerInfo.h"
#include "UIFunction.h"
#include "MPriceManager.h"
void GCShopVersionHandler::execute ( GCShopVersion * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	

	//------------------------------------------------------
	// Zone이 아직 생성되지 않은 경우
	//------------------------------------------------------
	if (g_pZone==NULL)
	{
		// message
		DEBUG_ADD("[Error] Zone is Not Init.. yet.");			
	}
	//------------------------------------------------------
	// 정상.. 
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = g_pZone->GetCreature( pPacket->getObjectID() );

		//------------------------------------------------------
		// 그런 creature가 없는 경우
		//------------------------------------------------------
		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT("[Error] There is no such Creature id=%d", pPacket->getObjectID());
		}
		//------------------------------------------------------
		// NPC인 경우
		//------------------------------------------------------
		else if (pCreature->GetClassType()==MCreature::CLASS_NPC)
		{
			MNPC* pNPC = (MNPC*)pCreature;

			//------------------------------------------------------
			// NPC의 상점을 얻는다.
			//------------------------------------------------------
			MShop* pShop = pNPC->GetShop();

			if (pShop==NULL)
			{
				// 상점이 없는 경우 --> 생성.
				pShop = new MShop;
				pShop->Init( MShopShelf::MAX_SHELF );
				
				// NPC에 상점 설정..
				pNPC->SetShop( pShop );				
			}

			//------------------------------------------------------
			// default로 normal 상점에 접근하게 한다.
			//------------------------------------------------------
			pShop->SetCurrent( 0 );

			//------------------------------------------------------
			// normal item 선반을 생성한다.
			//------------------------------------------------------
			pNPC->CreateFixedShelf();
			pNPC->CreateFixedShelf(true);	// mysterious -_-;


			//------------------------------------------------------
			// 각 shelf의 version 비교..
			//------------------------------------------------------
			BOOL bSameAll = TRUE;
			for (ShopRackType_t i=0; i<SHOP_RACK_TYPE_MAX; i++)
			{
				//------------------------------------------------------
				// normal item인 경우는 체크할 필요없다. 
				// --> client에 이미 정보가 있으므로
				//------------------------------------------------------
				if (i!=SHOP_RACK_SPECIAL)
				{
					continue;
				}

				MShopShelf* pShopShelf = pShop->GetShelf( i );

				//------------------------------------------------------
				// 선반이 없는 경우 --> 생성
				//------------------------------------------------------
				if (pShopShelf==NULL)
				{
					// i is SHOP_RACK_SPECIAL here, which is SHELF_SPECIAL
					// (both 1): inside the factory table, never NULL.
					pShopShelf = MShopShelf::NewShelf( i );

					pShop->SetShelf( (MShopShelf::SHELF_TYPE)i, pShopShelf );
				}

				unsigned int serverVersion = pPacket->getVersion( i );
				unsigned int clientVersion = pShopShelf->GetVersion();

				//------------------------------------------------------
				// version이 다르면 item std::list를 요청한다.
				//------------------------------------------------------
				if (serverVersion!=clientVersion)
				{
					// version이 다른 것이 있다고 체크
					bSameAll = FALSE;

						// item std::list 요청 packet
						CGShopRequestList	_CGShopRequestList;
						_CGShopRequestList.setObjectID( pNPC->GetID() );
						_CGShopRequestList.setRackType( i );

						g_pSocket->sendPacket( &_CGShopRequestList );						
				}
			}

			// The castle's tax ratio for this player, or the NPC's market
			// condition when that ratio is 100 (MPriceManager::SetShopTaxRatio).
			g_pPriceManager->SetShopTaxRatio( pPacket->getMarketCondSell() );
			
			//------------------------------------------------------
			// 모든 shelf의 version이 같으면..
			// 바로 상점을 띄운다.
			//------------------------------------------------------
			if (bSameAll)
			{
				//------------------------------------------------------
				// 정상적으로 된 경우
				// --> 상점을 실행한다.
				//------------------------------------------------------
				UI_RunShop();
				UI_SetShop( pShop );		// shop 설정				
			}
			
		}
		//------------------------------------------------------
		// NPC가 아닌 경우
		//------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT("[Error] The Creature is Not NPC. id=%d", pPacket->getObjectID());
		}
	}



	__END_CATCH
}
