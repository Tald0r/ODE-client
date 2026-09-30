//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMonsterKillQuestInfoHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMonsterKillQuestInfo.h"
#include "MMonsterKillQuestInfo.h"
#include "MCreatureTable.h"

#include <memory>

//////////////////////////////////////////////////////////////////////
//
// Runs when the client receives this packet from the server: each entry
// sets a monster-kill quest's goal, time limit and creature name.
//
//////////////////////////////////////////////////////////////////////
void GCMonsterKillQuestInfoHandler::execute ( GCMonsterKillQuestInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
		
	while(! pPacket->empty() )
	{
		std::unique_ptr<GCMonsterKillQuestInfo::QuestInfo> pInfo( pPacket->popQuestInfo() );

		// The creature to kill is the server's sType. The creature table
		// answers a type it does not hold with an empty row, and a row can
		// have no name; an empty MString keeps no storage, so the name is
		// NULL then, and a NULL assigned to a std::string reads address 0.
		// The quest keeps an empty name instead.
		const char* pName = (*g_pCreatureTable)[pInfo->sType].Name.GetString();
		g_pQuestInfoManager->SetInfo (pInfo->questID, pInfo->goal, pInfo->timeLimit, pName == NULL ? "" : pName);
	}
	

	__END_CATCH
}
