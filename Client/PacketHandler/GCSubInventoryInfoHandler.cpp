//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSubInventoryInfoHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSubInventoryInfo.h"
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
		#include "ClientDef.h"
		#include "UIFunction.h"
	#endif

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
		extern void SetSubInventoryInfo(InventoryInfo* pInventoryInfo);
	#endif
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCSubInventoryInfoHandler::execute ( GCSubInventoryInfo * pPacket , Player * pPlayer )

{
	(void)pPacket;
	(void)pPlayer;

	
	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 藤속관櫓관
		if(NULL == g_pZone || NULL == g_pPlayer)
			return ;
		MItem* pItem = g_pPlayer->GetItemCheckBuffer();
		if(NULL == pItem)
			return;

		g_pPlayer->ClearItemCheckBuffer();
		UI_RunSubInventory(pItem);
		
		SetSubInventoryInfo(pPacket->getInventoryInfo());
	#endif

}
