//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNPCSayHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNPCSay.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"
#include "MChatManager.h"
#include "MNPCScriptTable.h"

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCNPCSayHandler::execute ( GCNPCSay * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
		// Debug Message

	//------------------------------------------------------
	// Zone이 아직 생성되지 않은 경우
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// 정상.. 
	//------------------------------------------------------
	else
	{
		//---------------------------------------------------------------
		// 캐릭터 머리 위에 채팅을 띄운다.
		//---------------------------------------------------------------
		MCreature*	pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature==NULL)
		{
			// 그런 캐릭터가 없으면 무시한다.
		}
		else
		{		
			//-------------------------------------------------
			// 정상적인 대화
			//-------------------------------------------------
			if (g_pChatManager->IsAcceptID( CreatureState::GetName(pCreature) ))
			{
				int scriptID = pPacket->getScriptID();
				int subjectID = pPacket->getSubjectID();

				const char* pString = g_pNPCScriptTable->GetSubject( scriptID, subjectID );

				if (pString!=NULL)
				{
					CreatureState::SetChatString(pCreature, pString );
				}
			}
			//-------------------------------------------------
			// 대화 거부로 설정되어 있다면 출력안한다.
			//-------------------------------------------------
			else
			{
				CreatureState::SetChatString(pCreature, "......" );
			}

			// history에 추가
			//char temp[128];
			//sprintf(temp, "%s> %s", pCreature->GetName(), str);
			//UI_AddChatToHistory( temp );
		}
	}

	__END_CATCH
}
