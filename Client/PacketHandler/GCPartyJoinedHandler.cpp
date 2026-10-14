//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPartyJoinedHandler.cpp
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCPartyJoined.h"
#include "MParty.h"
#include "UserInformation.h"
#include "TempInformation.h"

#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"



void GCPartyJoinedHandler::execute (GCPartyJoined * pPacket , Player * pPlayer)
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	if (PlayerState::Creature()==NULL
		|| g_pParty==NULL
		|| g_pUserInformation==NULL
		|| g_pTempInformation==NULL
		|| !ZoneState::Exists())
	{
		DEBUG_ADD_WAR("GCPartyJoinedHandler Failed");
		return;
	}

	int previousSize = g_pParty->GetSize();
	
	//g_pParty->Release();
	g_pParty->UnSetPlayerParty();
	MParty* pOldParty = g_pParty;

	g_pParty = new MParty;
	g_pParty->SetJoinTime( pOldParty->GetJoinTime() );
	

	int count = pPacket->getMemberInfoCount();

	struct in_addr sa;
				
	for (int i=0; i<count; i++)
	{
		PARTY_MEMBER_INFO* pInfo = pPacket->popMemberInfo();

		if (pInfo!=NULL)
		{
			if (g_pUserInformation->CharacterID!=pInfo->name.c_str())
			{			
				PARTY_INFO* pNewInfo = new PARTY_INFO;

				pNewInfo->Name		= pInfo->name.c_str();
				pNewInfo->bMale		= (pInfo->sex==1);				
				pNewInfo->hairStyle	= CommunicationPresentation::GetFaceStyle(pNewInfo->bMale, pInfo->hair_style);

#ifdef PLATFORM_WINDOWS
				sa.S_un.S_addr = pInfo->ip;
#else
				sa.s_addr = pInfo->ip;
#endif
				pNewInfo->IP		= inet_ntoa( sa );

				//---------------------------------------------------------
				// Was it in the party before..
				//---------------------------------------------------------
				PARTY_INFO* pOldInfo = pOldParty->GetMemberInfo( pInfo->name.c_str() );
					
				if (pOldInfo!=NULL)
				{
					pNewInfo->HP	= pOldInfo->HP;
					pNewInfo->MaxHP = pOldInfo->MaxHP;
					pNewInfo->zoneID = pOldInfo->zoneID;
					pNewInfo->zoneX = pOldInfo->zoneX;
					pNewInfo->zoneY = pOldInfo->zoneY;
					pNewInfo->guildID = pOldInfo->guildID;
				}			

				//---------------------------------------------------------
				// Is it in this zone?
				//---------------------------------------------------------
				if (ZoneState::Exists())
				{
					int creatureID = ZoneState::GetCreatureID( pInfo->name.c_str(), 1 );

					if (static_cast<unsigned int>(creatureID)==OBJECTID_NULL)
					{
						pNewInfo->bInSight = false;						
						pNewInfo->zoneID = 60002;
					}
					else
					{
						MCreature* pCreature = ZoneState::GetCreature( creatureID );
						
						if (pCreature!=NULL)
						{
							pNewInfo->bInSight = true;

							const MStatus* pStatus = CreatureState::Status(pCreature);
							if (pStatus != NULL)
							{
								pNewInfo->HP = static_cast<WORD>(pStatus->GetHP());
								pNewInfo->MaxHP = static_cast<WORD>(pStatus->GetMAX_HP());
							}
							pNewInfo->zoneID = (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);
							pNewInfo->zoneX = static_cast<BYTE>(CreatureState::GetServerX(pCreature));
							pNewInfo->zoneY = static_cast<BYTE>(CreatureState::GetServerY(pCreature));
							pNewInfo->guildID = static_cast<WORD>(CreatureState::GetGuildNumber(pCreature));

							CreatureState::SetPlayerParty(pCreature);
						}
						else
						{
							pNewInfo->bInSight = false;
						}
					}
				}
				else
				{
					pNewInfo->bInSight = false;
					pNewInfo->zoneID = 60002;
				}

				if (!g_pParty->AddMember( pNewInfo ))
				{
					delete pNewInfo;
				}
			}

			delete pInfo;
		}
	}

	CommunicationPresentation::ClosePartyCancel();
	CommunicationPresentation::ClosePartyAsk();
	CommunicationPresentation::ClosePartyRequest();

	if (previousSize!=g_pParty->GetSize())
	{
		CommunicationPresentation::RunParty();
	}	

	// Nobody before, and members now:
	// the player has joined a party.
	if (previousSize==0)
	{
		g_pParty->SetJoinTime();
	}

	//---------------------------------------------------------------
	// Tell the party about the player.
	//---------------------------------------------------------------
	PlayerState::SendInfoToParty();

	delete pOldParty;

	

	__END_CATCH
}
