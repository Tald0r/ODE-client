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
#include "DebugLog.h"
#include "VsUiPresentationHost.h"
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
		
		// A 500 by 250 letter on the guild-union mail tab (0), as kind 9.
		VsUiPresentation::AddMail(0, 9, 500, 250, offerlist->getGuildMaster().c_str() , sztemp , offerlist->getDate(), false, offerlist->getGuildID(), offerlist->getGuildType());
		VsUiPresentation::AddMailContents(0, 9, sztemp ); 

		itr++;
	}

//	__END_DEBUG_EX
 __END_CATCH
}
