//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRemoveStoreItemHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRemoveStoreItem.h"
#include "MStorage.h"
#include "MPlayer.h"
#include "VS_UI.h"


//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCRemoveStoreItemHandler::execute ( GCRemoveStoreItem * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	//__BEGIN_DEBUG_EX
	
 
	if(gC_vs_ui.inventory_mode == 1 || gC_vs_ui.inventory_mode == 2)
	{
		
		if(g_pStorage2 != NULL && g_pStorage2->GetCuropenid() !=0)
		{
			if(pPacket->getOwnerObjectID() == g_pStorage2->GetCuropenid())
			{
				const MItem* pStorageItem = g_pStorage2->GetItem( pPacket->getIndex() );
				
				if (pStorageItem!=NULL) 
				{
					g_pStorage2->SetCurrent(0);
					g_pStorage2->RemoveItem(pPacket->getIndex());
				}
			}	
		}
		
	}
	

	//__END_DEBUG_EX 
	__END_CATCH
}
