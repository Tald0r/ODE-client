//////////////////////////////////////////////////////////////////////
//
// Filename    : GCExecuteElementHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCExecuteElement.h"

#include "Client.h"
#include "VS_UI.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCExecuteElementHandler::execute ( GCExecuteElement * pGCExecuteElement , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//		__BEGIN_DEBUG_EX
		

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
#ifdef __DEBUG_OUTPUT__
		DEBUG_ADD_FORMAT("[GCExecuteElementHandler] QuestID=%d, bCondition=%d, index = %d", pGCExecuteElement->getQuestID(), pGCExecuteElement->getCondition(), pGCExecuteElement->getIndex());
#endif
		gC_vs_ui.PushGQuestExcuteElement(pGCExecuteElement->getQuestID(), pGCExecuteElement->getCondition(), pGCExecuteElement->getIndex());

	}
	


//	__END_DEBUG_EX 
	__END_CATCH
}
