//-----------------------------------------------------------------------------
// UIDialog.cpp
//-----------------------------------------------------------------------------
// The NPC talk box and the message box (task 4.238: gamemodel's since then).
// The windows are DialogWindow's; the player, the other windows and the
// party window are reached through PlayerState, DialogPresentation,
// TradePresentation and CommunicationPresentation.
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "../basic/CrtCompat.h"
#include "../basic/SafeFormat.h"

namespace {
constexpr size_t TalkMessageCapacity = 2048;
constexpr size_t FreeMessageCapacity = 256;
}

#include "UIDialog.h"
#include "TalkBox.h"
#include "MStringArray.h"
#include "MCreatureTable.h"
#include "TempInformation.h"
#include "ClientMode.h"
#include "GameSocket.h"
#include "PlayerStateHost.h"
#include "DialogPresentationHost.h"
#include "TradePresentationHost.h"
#include "CommunicationPresentationHost.h"
#include "../basic/DebugLog.h"
#include "Packet/ClientPlayer.h"
#include "Packet/Cpackets/CGNPCAskAnswer.h"
#include "Packet/Cpackets/CGLearnSkill.h"
#include "Packet/Cpackets/CGSelectQuest.h"
#include "Packet/Cpackets/CGSelectBloodBible.h"

#include <cstdlib>
#include <cstring>

bool g_bPartyRunning = false;		// whether the party window was up when the talk box opened

using namespace std;

//-----------------------------------------------------------------------------
// global
//-----------------------------------------------------------------------------
UIDialog*			g_pUIDialog = NULL;

//-----------------------------------------------------------------------------
// static
//-----------------------------------------------------------------------------
DWORD				UIDialog::s_LockGameInput	= 0;

//-----------------------------------------------------------------------------
//
// constructor / destructor
//
//-----------------------------------------------------------------------------
UIDialog::UIDialog()
{
	m_pPCTalkDlg = NULL;

	m_pMessageDlg = NULL;

	for (int i=0; i<MAX_MESSAGE; i++)
	{
		m_ppDlgMessage[i] = NULL;
	}
}

UIDialog::~UIDialog()
{
	Release();
}

//-----------------------------------------------------------------------------
//
//	member functions
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void
UIDialog::Init()
{

	//-------------------------------------------------------------
	// MESSAGE_PCTALK
	//-------------------------------------------------------------
	m_ppDlgMessageSize[MESSAGE_PCTALK] = 1;
	m_ppDlgMessage[MESSAGE_PCTALK] = new char* [1];
	for (int i=0; i<m_ppDlgMessageSize[MESSAGE_PCTALK]; i++)
	{
		m_ppDlgMessage[MESSAGE_PCTALK][i] = new char [TalkMessageCapacity];
	}



	//-------------------------------------------------------------
	// MESSAGE_FREE
	//-------------------------------------------------------------
	m_ppDlgMessageSize[MESSAGE_FREE] = 2;
	m_ppDlgMessage[MESSAGE_FREE] = new char* [2];
	m_ppDlgMessage[MESSAGE_FREE][0] = new char [FreeMessageCapacity];
	m_ppDlgMessage[MESSAGE_FREE][1] = new char [FreeMessageCapacity];
}

//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
void
UIDialog::Release()
{
	// PC Talk Dlg
	if (m_pPCTalkDlg!=NULL)
	{
		DialogWindow::Delete( m_pPCTalkDlg );
		m_pPCTalkDlg = NULL;
	}

	// Message Dlg
	if (m_pMessageDlg!=NULL)
	{
		DialogWindow::Delete( m_pMessageDlg );
		m_pMessageDlg = NULL;
	}

	// dlg message
	for (int i=0; i<MAX_MESSAGE; i++)
	{
		if (m_ppDlgMessage[i]!=NULL)
		{
			for (int j=0; j<m_ppDlgMessageSize[i]; j++)
			{
				if (m_ppDlgMessage[i][j]!=NULL)
				{
					delete [] m_ppDlgMessage[i][j];
				}
			}

			delete [] m_ppDlgMessage[i];

			m_ppDlgMessage[i] = NULL;
		}
	}


	// The game's input is free again.
	s_LockGameInput	= 0;
}

//-----------------------------------------------------------------------------
// Close PCTalk Dlg
//-----------------------------------------------------------------------------
void
UIDialog::ClosePCTalkDlg()
{
	if (m_pPCTalkDlg!=NULL)
	{
		DialogWindow::Delete( m_pPCTalkDlg );
		m_pPCTalkDlg = NULL;

		UnSetLockInputPCTalk();

		// If the party window was up... it comes back.
		if (g_bPartyRunning)
		{
			CommunicationPresentation::RunParty();
			g_bPartyRunning = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Close FreeMessage Dlg
//-----------------------------------------------------------------------------
void
UIDialog::CloseMessageDlg()
{
	if (m_pMessageDlg!=NULL)
	{
		DialogWindow::Delete( m_pMessageDlg );
		m_pMessageDlg = NULL;

		UnSetLockInputMessage();
	}
}

//-----------------------------------------------------------------------------
// Hide PCTalk Dlg
//-----------------------------------------------------------------------------
void
UIDialog::HidePCTalkDlg()
{
	if (m_pPCTalkDlg!=NULL)
	{
		DialogWindow::Finish( m_pPCTalkDlg );
	}
}

//-----------------------------------------------------------------------------
// Show PCTalk Dlg
//-----------------------------------------------------------------------------
void
UIDialog::ShowPCTalkDlg()
{
	if (m_pPCTalkDlg!=NULL)
	{
		DialogWindow::Start( m_pPCTalkDlg );
	}
}

//-----------------------------------------------------------------------------
// The NPC talk box's choice
//-----------------------------------------------------------------------------
void
UIDialog::ProcessPCTalkDlg(DialogWindow::Window* pDlg, id_t id)
{
	//-----------------------------------------------------------
	// When something is picked,
	// send the choice to the server.
	//-----------------------------------------------------------
	// (a roundabout way.. - -;)
	g_pPCTalkBox->SetAnswerID(id);

	int answerID;
	//--------------------------------------------------
	// when the dialog was 'left'..
	//--------------------------------------------------
	//if (id==DIALOG_EXECID_EXIT)
	//{
	//	answerID = 99;		// - -;;

		//UnSetLockInputPCTalk();
	//}
	//--------------------------------------------------
	// a plain choice..
	//--------------------------------------------------
	//else
	{
		answerID = g_pPCTalkBox->GetAnswerID();
	}


	//--------------------------------------------------
	// Send only when nothing awaits verification.
	//--------------------------------------------------
	if (g_Mode == MODE_GAME)
	{
		if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NULL)
		{
			//--------------------------------------------------
			// and no other window is up..
			//--------------------------------------------------
			if (!DialogPresentation::IsRunningStorage()
				&& !TradePresentation::IsRunningExchange()
				&& !DialogPresentation::IsRunningShop()
				&& !DialogPresentation::IsRunningBookcase()
				&& !DialogPresentation::IsRunningRepair()
				&& !DialogPresentation::IsRunningSell()
				&& !DialogPresentation::IsRunningSwapAdvancementItem())
			{
				switch (g_pPCTalkBox->GetType())
				{
					//--------------------------------------------------
					// a plain talk
					//--------------------------------------------------
					case PCTalkBox::NORMAL :
					{
						answerID = g_pPCTalkBox->MapMenuAnswer(answerID);

						CGNPCAskAnswer _CGNPCAskAnswer;

						_CGNPCAskAnswer.setObjectID( g_pPCTalkBox->GetNPCID() );
						_CGNPCAskAnswer.setScriptID( g_pPCTalkBox->GetScriptID() );
						_CGNPCAskAnswer.setAnswerID( answerID );

						g_pSocket->sendPacket( &_CGNPCAskAnswer );

						//--------------------------------------------------
						// Wait for the server's answer.
						//--------------------------------------------------
						PlayerState::SetWaitVerify( PlayerState::WAIT_VERIFY_NPC_ASK );
					}
					break;

					//--------------------------------------------------
					// learning a skill
					//--------------------------------------------------
					case PCTalkBox::SKILL_LEARN :
					{
						if (id!=DialogWindow::EXECID_EXIT)
						{
							if (id==999)
							{
								// leaving
								DialogWindow::Run( pDlg, DialogWindow::EXECID_EXIT );

								UnSetLockInputPCTalk();

								PlayerState::SetWaitVerifyNULL();

								g_pTempInformation->SetMode(TempInformation::MODE_NULL);
							}
							else if (g_pTempInformation->GetMode()==TempInformation::MODE_SKILL_LEARN)
							{
								g_pTempInformation->SetMode(TempInformation::MODE_NULL);

								CGLearnSkill _CGLearnSkill;

								//_CGLearnSkill.setObjectID( (*g_pPCTalkBox).GetNPCID() );
								_CGLearnSkill.setSkillType( static_cast<SkillType_t>(id) );
								_CGLearnSkill.setSkillDomainType( static_cast<SkillDomainType_t>((*g_pTempInformation).Value1) );

								g_pSocket->sendPacket( &_CGLearnSkill );

								//--------------------------------------------------
								// Wait for the server's answer.
								//--------------------------------------------------
								//g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_NPC_ASK );

								// leave the dialog
								DialogWindow::Run( pDlg, DialogWindow::EXECID_EXIT );

								PlayerState::SetWaitVerifyNULL();

								UnSetLockInputPCTalk();

								g_pTempInformation->SetMode(TempInformation::MODE_NULL);
							}
							else
							{
								DEBUG_ADD_ERR("[Error] Mode is Not SKILL_LEARN");
							}
						}
					}
					break;
					case PCTalkBox::SELECT_QUEST :
					{
						if ( id != DialogWindow::EXECID_EXIT )
						{
							if( id == 9999 )
							{
								DialogWindow::Run( pDlg, DialogWindow::EXECID_EXIT );
								UnSetLockInputPCTalk();
								PlayerState::SetWaitVerifyNULL();
								g_pTempInformation->SetMode(TempInformation::MODE_NULL);
							} else if (g_pTempInformation->GetMode() == TempInformation::MODE_SELECT_QUEST )
							{
								CGSelectQuest _CGSelectQuest;

								_CGSelectQuest.setQuestID ( id );
								_CGSelectQuest.setNPCObjectID( g_pPCTalkBox->GetNPCID() );
								g_pSocket->sendPacket( &_CGSelectQuest );

								DialogWindow::Run( pDlg, DialogWindow::EXECID_EXIT );
								PlayerState::SetWaitVerifyNULL();
								UnSetLockInputPCTalk();
								g_pTempInformation->SetMode( TempInformation::MODE_NULL );
							} else
							{
								DEBUG_ADD_ERR("[Error] Mode is Not SELECT_QUEST");
							}
						}
					}
					break;
					//--------------------------------------------------
					// signing a blood bible
					//--------------------------------------------------
					case PCTalkBox::BLOOD_BIBLE_SIGN :
					{
						if (id!=DialogWindow::EXECID_EXIT)
						{
							if (id==999)
							{
								// leaving
								DialogWindow::Run( pDlg, DialogWindow::EXECID_EXIT );

								UnSetLockInputPCTalk();

								PlayerState::SetWaitVerifyNULL();

								g_pTempInformation->SetMode(TempInformation::MODE_NULL);
							}
							else if (g_pTempInformation->GetMode()==TempInformation::MODE_SKILL_LEARN)
							{
							//	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

								CGSelectBloodBible _CGSelectBloodBible;

								//_CGLearnSkill.setObjectID( (*g_pPCTalkBox).GetNPCID() );
								_CGSelectBloodBible.setBloodBibleID( static_cast<ItemType_t>(id) );

								g_pSocket->sendPacket( &_CGSelectBloodBible );

								//--------------------------------------------------
								// Wait for the server's answer.
								//--------------------------------------------------
								//g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_NPC_ASK );

								// leave the dialog
//								pDlg->Run( DIALOG_EXECID_EXIT );
//
//								g_pPlayer->SetWaitVerifyNULL();
//
//								UnSetLockInputPCTalk();
//
//								g_pTempInformation->SetMode(TempInformation::MODE_NULL);
							}
							else
							{
								DEBUG_ADD_ERR("[Error] Mode is Not SKILL_LEARN");
							}
						}
					}
					break;
				}
			}
		}
	}

	//-------------------------------------------------------------
	// Escape or just leaving.. the verification... hmm.. uneasy.. - -;
	//-------------------------------------------------------------
	if (id==DialogWindow::EXECID_EXIT)
	{
		UnSetLockInputPCTalk();

		PlayerState::SetWaitVerifyNULL();

		g_pTempInformation->SetMode(TempInformation::MODE_NULL);

		// If the party window was up... it comes back.
		if (g_bPartyRunning)
		{
			CommunicationPresentation::RunParty();
			g_bPartyRunning = false;
		}
	}
}

//-----------------------------------------------------------------------------
// PopupPCTalkDlg
//-----------------------------------------------------------------------------
void
UIDialog::PopupPCTalkDlg(int x, int y)
{
	(void)x;
	(void)y;

	if(g_bActiveGame == false)
		return;

	//---------------------------------------------------------
	// Free the dialog that was there.
	//---------------------------------------------------------
	if (m_pPCTalkDlg!=NULL)
	{
		DialogWindow::Delete( m_pPCTalkDlg );
		m_pPCTalkDlg = NULL;
	}

	// Was the party window up?
	if (DialogPresentation::IsRunningParty() || g_bPartyRunning)
	{
		g_bPartyRunning = true;
	}
	else
	{
		g_bPartyRunning = false;
	}

	//---------------------------------------------------------
	// Close the other dialogs
	//---------------------------------------------------------
	DEBUG_ADD("UID-CloseUIDlg");

//	gC_vs_ui.CloseInfo();
//	gC_vs_ui.CloseShop();
//	gC_vs_ui.ServerDisconnectMessage();
	DialogPresentation::CloseAllDialog();

	DEBUG_ADD("UID_COK");

	CloseMessageDlg();

	//---------------------------------------------------------
	// the size
	//---------------------------------------------------------
	int msgSize = static_cast<int>(g_pPCTalkBox->size());

	if (msgSize==0)
	{
		return;
	}

	//---------------------------------------------------------
	// make the dialog
	//---------------------------------------------------------
	DEBUG_ADD("newNPCDLG");

	m_pPCTalkDlg = DialogWindow::NewTalk(g_pPCTalkBox->GetCreatureType(),
							(*g_pCreatureTable)[g_pPCTalkBox->GetCreatureType()].Name.GetString());

	//---------------------------------------------------------
	// the title
	//---------------------------------------------------------


	//---------------------------------------------------------
	// the menu..
	//---------------------------------------------------------
	DEBUG_ADD("newDLGMENU");
	DialogWindow::MenuItem* pMenu = new DialogWindow::MenuItem[msgSize];

	MStringList::iterator iString = g_pPCTalkBox->begin();

	int i=0;

	switch (g_pPCTalkBox->GetType())
	{
		//-----------------------------------------------------
		// a plain talk
		//-----------------------------------------------------
		case PCTalkBox::NORMAL :
			while (iString != g_pPCTalkBox->end())
			{
				MString* pString = *iString;

				// the menu line
				pMenu[i].id			= i+1;						// ID+1
				pMenu[i].text		= pString->GetString() != NULL ? pString->GetString() : "";		// its text (an empty one is NULL)

				// next..
				iString++;
				i++;
			}
		break;

		//-----------------------------------------------------
		// learning a skill
		//-----------------------------------------------------
		// a line is "%3d%s". A stopgap!~~~
		//-----------------------------------------------------
		case PCTalkBox::SKILL_LEARN :
		case PCTalkBox::BLOOD_BIBLE_SIGN:
			while (iString != g_pPCTalkBox->end())
			{
				MString* pString = *iString;
				DEBUG_ADD(pString->GetString());

				char strID[80];
				char strName[80];

				if (pString->GetLength() < 3) {
					++iString;
					continue;
				}
				Basic::CopyBounded(strID, pString->GetString(), 3);
				strID[3] = '\0';
				SafeFormat::Copy(strName, pString->GetString()+3);

				// the name alone
				*pString = strName;

				// the menu line
				pMenu[i].id			= atoi(strID);
				pMenu[i].text		= pString->GetString() != NULL ? pString->GetString() : "";		// its text (an empty one is NULL)

				// next..
				iString++;
				i++;
			}
		break;
		case PCTalkBox::SELECT_QUEST :
			while (iString != g_pPCTalkBox->end())
			{
				MString* pString = *iString;
				DEBUG_ADD(pString->GetString());

				char strID[80];
				char strName[80];

				if (pString->GetLength() < 4) {
					++iString;
					continue;
				}
				Basic::CopyBounded(strID, pString->GetString(), 4);
				strID[4] = '\0';
				SafeFormat::Copy(strName, pString->GetString()+4);

				// the name alone
				*pString = strName;

				// the menu line
				pMenu[i].id			= atoi(strID);
				pMenu[i].text		= pString->GetString() != NULL ? pString->GetString() : "";		// its text (an empty one is NULL)

				// next..
				iString++;
				i++;
			}
			break;
	}


	//---------------------------------------------------------
	// set the menu
	//---------------------------------------------------------
	DEBUG_ADD("setMENU");
	DialogWindow::SetMenu(m_pPCTalkDlg, pMenu, i);

	DEBUG_ADD("spMenu");
	SafeFormat::Copy(m_ppDlgMessage[MESSAGE_PCTALK][0], TalkMessageCapacity, g_pPCTalkBox->GetContent());

	//const char* pFirstString = m_ppDlgMessage[MESSAGE_PCTALK][0];

	DEBUG_ADD("setMsg");

	DialogWindow::SetMessage(m_pPCTalkDlg, m_ppDlgMessage[MESSAGE_PCTALK], 1, false);

	//---------------------------------------------------------
	// delete
	//---------------------------------------------------------
	delete [] pMenu;

	//---------------------------------------------------------
	// start the dialog..
	//---------------------------------------------------------
	DEBUG_ADD("startTalk");
	DialogWindow::Start( m_pPCTalkDlg );

	//---------------------------------------------------------
	// Block the game's input.
	//---------------------------------------------------------
	SetLockInputPCTalk();
}

//-----------------------------------------------------------------------------
// Popup Free MessageDlg
//-----------------------------------------------------------------------------
void
UIDialog::PopupFreeMessageDlg(const char* msg, int x, int y, WORD fButton, bool IsAfterExit)
{
	(void)IsAfterExit;
	int msgID= MESSAGE_FREE;

	//-------------------------------------------------------------
	// MESSAGE_FREE
	//-------------------------------------------------------------
	int msgSize = static_cast<int>(strlen(msg));

	int dlgSizeY, numStr;

	//-------------------------------------------------------------
	// over 40 characters
	//-------------------------------------------------------------
	// (crude, but it does)
	if (msgSize > 40)
	{
		dlgSizeY = 1;
		char* str = (char*)msg + 40;

		int i;
		for (i=40; i>0; i--)
		{
			if (*str==' ')
			{
				break;
			}

			str--;
		}

		Basic::CopyBounded(m_ppDlgMessage[msgID][0], msg, i);
		m_ppDlgMessage[msgID][0][i] = '\0';
		//------------------------------------------
		//yckou
		if(i==0)
			SafeFormat::Copy(m_ppDlgMessage[msgID][1], FreeMessageCapacity, str);
		else
			SafeFormat::Copy(m_ppDlgMessage[msgID][1], FreeMessageCapacity, str+1);
		//end yckou
		//------------------------------------------
		numStr = 2;
	}
	//-------------------------------------------------------------
	// 40 characters or fewer
	//-------------------------------------------------------------
	else
	{
		dlgSizeY = 0;
		SafeFormat::Copy(m_ppDlgMessage[msgID][0], FreeMessageCapacity, msg);
		numStr = 1;
	}

	//---------------------------------------------------------
	// Free the dialog that was there.
	//---------------------------------------------------------
	if (m_pMessageDlg!=NULL)
	{
		DialogWindow::Delete( m_pMessageDlg );
		m_pMessageDlg = NULL;
	}

	//---------------------------------------------------------
	// make the dialog
	//---------------------------------------------------------
	m_pMessageDlg = DialogWindow::NewMessage(x, y, dlgSizeY, fButton);

	DialogWindow::SetMessage(m_pMessageDlg, m_ppDlgMessage[msgID], numStr, true);


	//---------------------------------------------------------
	// start the dialog..
	//---------------------------------------------------------
	DialogWindow::Start( m_pMessageDlg );

	//---------------------------------------------------------
	// Block the game's input.
	//---------------------------------------------------------
	SetLockInputMessage();
}
//-----------------------------------------------------------------------------
// ProcessCannotBuyDlg
//-----------------------------------------------------------------------------
void
UIDialog::ProcessMessageDlg(DialogWindow::Window* pDlg, id_t id)
{
	(void)pDlg;
	(void)id;
	//if (id==DIALOG_OK)
	//{
		UnSetLockInputMessage();
	//}

}
