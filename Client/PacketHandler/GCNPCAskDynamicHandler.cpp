//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNPCAskDynamicHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNPCAskDynamic.h"
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
void GCNPCAskDynamicHandler::execute ( GCNPCAskDynamic * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	if (PlayerState::Creature() == NULL)
	{
		DEBUG_ADD_ERR("[Error] Player is NULL");
		return;
	}
	

	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// Normal..
	//------------------------------------------------------
	else
	{
		if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK)
		{
			PlayerState::SetWaitVerifyNULL();

			int size = pPacket->getContentsCount();

			if (size==0)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] No Answer String.. ScriptID=%d", (int)pPacket->getScriptID());
			}
			else
			{
				// NPC
				MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

				if (pCreature==NULL)
				{
					// No such character: ignore it.
				}
				else
				{
					//---------------------------------------------------
					// It goes into the PC talk box.
					//---------------------------------------------------
					// Clear what was there.
					g_pPCTalkBox->Release();

					//---------------------------------------------------
					// normal
					//---------------------------------------------------
					g_pPCTalkBox->SetType( PCTalkBox::NORMAL );

					//---------------------------------------------------
					// Set up the PC talk box.
					//---------------------------------------------------
					g_pPCTalkBox->SetContent( pPacket->getSubject().c_str() );
					g_pPCTalkBox->SetNPCID( pPacket->getObjectID() );
					g_pPCTalkBox->SetCreatureType( CreatureState::GetCreatureType(pCreature) );
					g_pPCTalkBox->SetScriptID( pPacket->getScriptID() );

					//---------------------------------------------------
					// Add each string.
					//---------------------------------------------------

					g_pPCTalkBox->m_AnswerIDMap.clear();
					for (int i=0; i<size; i++)
					{
						// Into the PC talk box.
						// Every answer leaves the packet, so a refused one takes its
						// text with it. An empty one is left out like a refused one:
						// the talk box would hand the dialog a NULL menu string.
						const std::string answer = pPacket->popContent();
						if( !answer.empty() && g_pSystemAvailableManager->ScriptFiltering( pPacket->getScriptID(), i ) )
						{
							g_pPCTalkBox->AddString( answer.c_str() );
							g_pPCTalkBox->m_AnswerIDMap.push_back( i );
						}
					}
					
					DialogPresentation::PopupPCTalkDlg();
				}

				//---------------------------------------------------
				// Pop up the dialog.
				//---------------------------------------------------
				//POINT point = ConvertPositionMapToScreen(pCreature->GetX(), pCreature->GetY());

				//point.y -= pCreature->GetHeight();

				//g_pUIDialog->PopupPCTalkDlg(point.x, point.y - size*20-100);				
			}
		}
		else
		{
			DEBUG_ADD_ERR("[Error] WaitVerifyMode is Not WAIT_VERIFY_NPC_ASK");
		}
	}


	__END_CATCH
}
