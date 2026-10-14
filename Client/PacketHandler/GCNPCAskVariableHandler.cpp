//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNPCAskVariableHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "MNPCScriptTable.h"
#include "Gpackets/GCNPCAskVariable.h"

#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "TalkBox.h"
#include "DialogPresentationHost.h"
#include "SystemAvailabilities.h"


//////////////////////////////////////////////////////////////////////
//
// Run when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCNPCAskVariableHandler::execute ( GCNPCAskVariable * pPacket , Player * pPlayer )

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

		if (pCreature!=NULL)
		{			
			//---------------------------------------------------
			// It goes into the PC talk box.
			//---------------------------------------------------
			// Clear what was there.
			g_pPCTalkBox->Release();

			DEBUG_ADD("TalkBoxRelease - GCNPCAskVariableHandler");

			//---------------------------------------------------
			// normal
			//---------------------------------------------------
			g_pPCTalkBox->SetType( PCTalkBox::NORMAL );

			
			int scriptID = pPacket->getScriptID();

			//---------------------------------------------------
			// Set up the PC talk box.
			//---------------------------------------------------
			// It is called SetContent, but it is the subject. - -;
			std::string title;
			g_pNPCScriptTable->GetSubjectParameter(scriptID, 0, pPacket->getScriptParameters(), title);
			g_pPCTalkBox->SetContent( title.c_str() );
			g_pPCTalkBox->SetNPCID( pPacket->getObjectID() );
			g_pPCTalkBox->SetCreatureType( CreatureState::GetCreatureType(pCreature) );
			g_pPCTalkBox->SetScriptID( scriptID );

			DEBUG_ADD("SetScript - GCNPCAskVariableHandler");

			//---------------------------------------------------
			// Add each string.
			//---------------------------------------------------
			int contentSize = g_pNPCScriptTable->GetContentSize( scriptID );

			g_pPCTalkBox->m_AnswerIDMap.clear();
			
			for (int i=0; i<contentSize; i++)
			{
				// Into the PC talk box.
				std::string str;
				g_pNPCScriptTable->GetContentParameter( scriptID, i ,pPacket->getScriptParameters(),str);
				// An empty answer is left out like a refused one: the talk box
				// would hand the dialog a NULL menu string.
				if( !str.empty() && g_pSystemAvailableManager->ScriptFiltering( scriptID, i ) )
				{
					g_pPCTalkBox->AddString( str.c_str() );
					g_pPCTalkBox->m_AnswerIDMap.push_back( i );
				}
				DEBUG_ADD(str.c_str());				
			}
			DEBUG_ADD("SetContent - GCNPCAskVariableHandler");			
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
