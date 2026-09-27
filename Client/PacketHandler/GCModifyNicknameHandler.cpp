//////////////////////////////////////////////////////////////////////
//
// Filename    : GCModifyNicknameHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCModifyNickname.h"
#include "ClientDef.h"
#include "VS_UI.h"
#include "MGameStringTable.h"
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCModifyNicknameHandler::execute ( GCModifyNickname * pPacket , Player * pPlayer )

{
	__BEGIN_TRY /*__BEGIN_DEBUG_EX*/
	(void)pPlayer;
	

		int CreatureID = pPacket->getObjectID();
		
		if (g_pZone==NULL)
		{
			// message
			DEBUG_ADD("[Error] Zone is Not Init.. yet.");			
			return;
		}
		
		MCreature* pCreature = g_pZone->GetCreature(CreatureID);

		if (pCreature != NULL)
		{
			NicknameInfo TempNick	= pPacket->getNicknameInfo();
			// getNickname() returns std::string by value, so its c_str() died at
			// the end of the assignment below and szNickName reached SetNickName
			// dangling. The custom name is bound here instead; the index branch
			// points at the string table, which outlives this function.
			std::string customNickname;
			const char* szNickName;
			if(TempNick.getNicknameType() == NicknameInfo::NICK_CUSTOM_FORCED ||
				TempNick.getNicknameType() == NicknameInfo::NICK_CUSTOM)
			{
				customNickname = TempNick.getNickname();
				szNickName = customNickname.c_str();
			}
			else // 닉네임 인덱스가 있을 때
			{
				DWORD TempIndex = TempNick.getNicknameIndex();
				if(TempIndex >= static_cast<DWORD>(g_pNickNameStringTable->GetSize()))
					TempIndex = 0;
				szNickName = (*g_pNickNameStringTable)[TempIndex].GetString();
			}

			pCreature->SetNickName(TempNick.getNicknameType(), (char*)szNickName);
//			if(pCreature->GetID() == g_pPlayer->GetID())
//			{
//				g_char_slot_ingame.m_NickNameType = TempNick.getNicknameType();
//				g_char_slot_ingame.m_NickName = szNickName;
//			}

		}


	/*__END_DEBUG_EX */__END_CATCH
}
