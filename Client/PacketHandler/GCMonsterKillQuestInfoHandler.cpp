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
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCMonsterKillQuestInfoHandler::execute ( GCMonsterKillQuestInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
		
	while(! pPacket->empty() )
	{
		std::unique_ptr<GCMonsterKillQuestInfo::QuestInfo> pInfo( pPacket->popQuestInfo() );
//		GCMonsterKillQuestInfo::QuestInfo* pInfo = pPacket->popQuestInfo();	
		std::string str;
		//pInfo->sType
		str = (*g_pCreatureTable)[pInfo->sType].Name;
		g_pQuestInfoManager->SetInfo (pInfo->questID, pInfo->goal, pInfo->timeLimit, str);		
//		delete pInfo;
	}
	

	__END_CATCH
}
