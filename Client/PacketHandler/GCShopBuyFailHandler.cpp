//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopBuyFailHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopBuyFail.h"
#include "DialogPresentationHost.h"
#include "TempInformation.h"
#include "MGameStringTable.h"

void GCShopBuyFailHandler::execute ( GCShopBuyFail * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	// mode를 없앤다.
	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

	// 거래를 다시 활성화한다.
	DialogPresentation::UnlockItemTrade();

	switch (pPacket->getCode())
	{
		case GC_SHOP_BUY_FAIL_NOT_ENOUGH_BLACK_STAR :
		case GC_SHOP_BUY_FAIL_NOT_ENOUGH_RED_STAR :
		case GC_SHOP_BUY_FAIL_NOT_ENOUGH_BLUE_STAR :
		case GC_SHOP_BUY_FAIL_NOT_ENOUGH_GREEN_STAR :
		case GC_SHOP_BUY_FAIL_NOT_ENOUGH_CYAN_STAR :
			//pPacket->getAmount()	// 부족한 개수
	
		default :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_BUY_NO_ITEM].GetString() );
	}


	__END_CATCH
}
