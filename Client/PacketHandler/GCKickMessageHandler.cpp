//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKickMessageHandler.cc
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKickMessage.h"

	#include "MGameStringTable.h"
	#include "MEventQueue.h"
	#include "PlayerStateHost.h"
	#include "WorldEventsHost.h"


//////////////////////////////////////////////////////////////////////
//
// Runs when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCKickMessageHandler::execute ( GCKickMessage * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	
	switch( pPacket->getType() )
	{
	case KICK_MESSAGE_PAY_TIMEOUT :
		{
			MEvent event;
			event.eventID = EVENTID_FORCE_LOGOUT_BY_PREMIUM;
			event.eventDelay = int( pPacket->getSeconds() * 1000 );
			event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
			event.eventType = EVENTTYPE_ZONE;
			event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_BY_PAYTIME);
			event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_AFTER_SECOND);
						
			WorldEvents::AddEvent(event);
		}
		break;
	case KICK_MESSAGE_EXPIRE_FREEPLAY :
		{
			MEvent event;
			event.eventID = EVENTID_FORCE_LOGOUT_BY_PREMIUM;
			event.eventDelay = int( pPacket->getSeconds() * 1000 );
			event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
			event.eventType = EVENTTYPE_ZONE;
			if(PlayerState::GetRace() == RACE_SLAYER)
				event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_BY_FREEPLAY_LEVEL);
			else
				event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_BY_LEVEL);
			event.m_StringsID.push_back(STRING_MESSAGE_LOGOUT_AFTER_SECOND);

			WorldEvents::AddEvent(event);
		}
		break;
	}		

	__END_CATCH
}
