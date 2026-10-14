//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNPCAskHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNPCAsk.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "TalkBox.h"
#include "DialogPresentationHost.h"
#include "MNPCScriptTable.h"
#include "SystemAvailabilities.h"

//////////////////////////////////////////////////////////////////////
//
// Run when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCNPCAskHandler::execute ( GCNPCAsk * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	

	if (PlayerState::Creature()==NULL
		|| !ZoneState::Exists()
		|| g_pNPCScriptTable==NULL
		|| g_pPCTalkBox==NULL)
	{
		DEBUG_ADD_ERR("[Error] Some Object is NULL");
		return;
	}


	if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK)
	{
		PlayerState::SetWaitVerifyNULL();
		
		// NPC
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );
		
//		pPacket->

		if (pCreature!=NULL || (pPacket->getNPCID() >= 634 && pPacket->getNPCID() <= 637) )
		{		
			unsigned int CreatureType;

			if(pPacket->getNPCID() >= 634 && pPacket->getNPCID() <= 637 )
			{
				CreatureType = pPacket->getNPCID();
			} else
			{
				CreatureType= CreatureState::GetCreatureType(pCreature);
			}
			
			//---------------------------------------------------
			// It goes into the PC talk box.
			//---------------------------------------------------
			// Clear what was there.
			g_pPCTalkBox->Release();

			DEBUG_ADD("TalkBoxRel");

			//---------------------------------------------------
			// normal
			//---------------------------------------------------
			g_pPCTalkBox->SetType( PCTalkBox::NORMAL );

			
			int scriptID = pPacket->getScriptID();

			//---------------------------------------------------
			// Set up the PC talk box.
			//---------------------------------------------------
			// It is called SetContent, but it is the subject. - -;
			g_pPCTalkBox->SetContent( g_pNPCScriptTable->GetSubject(scriptID, 0) );
			g_pPCTalkBox->SetNPCID( pPacket->getObjectID() );
			g_pPCTalkBox->SetCreatureType( CreatureType );
			g_pPCTalkBox->SetScriptID( scriptID );

			DEBUG_ADD("SetScript");

			//---------------------------------------------------
			// Add each string.
			//---------------------------------------------------
			int contentSize = g_pNPCScriptTable->GetContentSize( scriptID );

			g_pPCTalkBox->m_AnswerIDMap.clear();

			for (int i=0; i<contentSize; i++)
			{
				// Into the PC talk box.
				// An empty answer is left out like a refused one: the talk box
				// would hand the dialog a NULL menu string.
				const char* pAnswer = g_pNPCScriptTable->GetContent( scriptID, i );
				if( pAnswer != NULL && pAnswer[0] != '\0'
					&& g_pSystemAvailableManager->ScriptFiltering( scriptID, i ) )
				{
					g_pPCTalkBox->AddString( pAnswer );
					g_pPCTalkBox->m_AnswerIDMap.push_back( i );
				}				
			}

			DEBUG_ADD("SetContent");
			
			DialogPresentation::PopupPCTalkDlg();
		}

		//---------------------------------------------------
		// Pop up the dialog.
		//---------------------------------------------------
		//POINT point = ConvertPositionMapToScreen(pCreature->GetX(), pCreature->GetY());

		//point.y -= pCreature->GetHeight();

		//g_pUIDialog->PopupPCTalkDlg(point.x, point.y - size*20-100);							
	}
	else
	{
		DEBUG_ADD_ERR("[Error] WaitVerifyMode is Not WAIT_VERIFY_NPC_ASK");
	}

	

	__END_CATCH
}
