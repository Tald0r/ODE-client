//////////////////////////////////////////////////////////////////////
//
// Filename    : GCUnionOfferListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUnionOfferList.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "VS_UI.h"
#include "DebugInfo.h"
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCUnionOfferListHandler::execute ( GCUnionOfferList * pPacket , Player * pPlayer )

{
	__BEGIN_TRY// __BEGIN_DEBUG_EX
	(void)pPlayer;
	

		
	std::list<SingleGuildUnionOffer*>::const_iterator itr = pPacket->getUnionOfferList().begin();
	std::list<SingleGuildUnionOffer*>::const_iterator enditr = pPacket->getUnionOfferList().end();

	while(itr != enditr)
	{
		Assert( *itr != NULL );
		SingleGuildUnionOffer* offerlist = (*itr);
		SIZE windowSize;
		windowSize.cx = 500; 
		windowSize.cy = 250;
		char sztemp[2048];
		memset(sztemp,0,2048); 

		if(offerlist->getGuildType() == SingleGuildUnionOffer::JOIN)
			SafeFormat::Format(sztemp, GetGameString(UI_STRING_MESSAGE_TOTAL_UNION_JOIN_MSG), offerlist->getGuildName().c_str() );
		else if(offerlist->getGuildType() == SingleGuildUnionOffer::QUIT)
			SafeFormat::Format(sztemp, GetGameString(UI_STRING_MESSAGE_TOTAL_UNION_DEPORT_MSG), offerlist->getGuildName().c_str() );
		else
		{
			DEBUG_ADD_WAR("[GCUnionOfferListHandler] - SingleGuildUnionOffer guild type error");
			return;
		}
		
		gC_vs_ui.AddMail(0, 9, windowSize, offerlist->getGuildMaster().c_str() , sztemp , offerlist->getDate(), 0, offerlist->getGuildID(), offerlist->getGuildType());
		gC_vs_ui.AddMailContents(0, 9, sztemp ); 

		itr++;
	}

//	__END_DEBUG_EX
 __END_CATCH
}
