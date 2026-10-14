//////////////////////////////////////////////////////////////////////
//
// Filename    : GCBloodBibleListHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
// include files
#include "Gpackets/GCBloodBibleList.h"


#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "TalkBox.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "TempInformation.h"

//////////////////////////////////////////////////////////////////////
//
// Run when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCBloodBibleListHandler::execute ( GCBloodBibleList * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	
	if (PlayerState::Creature()==NULL
		|| !ZoneState::Exists()
		|| g_pPCTalkBox==NULL)
	{
		DEBUG_ADD_ERR("[Error] Some Object is NULL");
		return;
	}

	PlayerState::SetWaitVerifyNULL();

	g_pPCTalkBox->Release();		// 

	std::vector<ItemType_t>	BloodBibleList = pPacket->getList();

	g_pPCTalkBox->SetType( PCTalkBox::BLOOD_BIBLE_SIGN );

	char str[192];
	char str2[192];
	for(int i = 0; static_cast<size_t>(i)< BloodBibleList.size(); i++)
	{
		// The two arguments are table lookups indexed by a packet field.
		// CTypeTable::operator[] range-checks that (it has done so in
		// every build since e65ab7a), but what it returns out of range is
		// a default-constructed MString, whose GetString() is NULL - and
		// a NULL passed to a %s is undefined, not merely empty.
		// GetGameString() answers "" instead.
		SafeFormat::Format(str2, GetGameString(UI_STRING_MESSAGE_RENT_BLOOD_BIBLE2),
			GetGameString(UI_STRING_MESSAGE_BLOOD_BIBLE_ARMEGA+BloodBibleList[i]),
			GetGameString(STRING_MESSAGE_BLOOD_BIBLE_BONUS_ARMEGA+BloodBibleList[i]));

		// str2 can now be a full 191 bytes, and "%3d " adds at least four
		// more, so the old sprintf into str[192] could run four bytes
		// past it. The format here is a literal, so this is about the
		// bound rather than about C19.
		SafeFormat::Format(str, "%3d %s", BloodBibleList[i], str2);
		g_pPCTalkBox->AddString( str );
	}
	
	// Add the way out.
	std::string szMsg;
	szMsg += "999";
	szMsg += GetGameString(UI_STRING_MESSAGE_RENT_LATER_BLOOD_BIBLE);
	g_pPCTalkBox->AddString( szMsg.c_str() );


	snprintf(str, sizeof(str), "%s", GetGameString(UI_STRING_MESSAGE_RENT_BLOOD_BIBLE));

	g_pPCTalkBox->SetContent( str );

	DialogPresentation::PopupPCTalkDlg();
	g_pTempInformation->SetMode(TempInformation::MODE_SKILL_LEARN);


	__END_CATCH
}
