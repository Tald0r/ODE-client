//--------------------------------------------------------------------------------
//
// Filename    : GCAddOustersHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddOusters.h"
#include "CommunicationPresentationHost.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// MCreature::IsNPC: whether the creature table's row for its type is an
// NPC's.
static bool IsNPCType(MCreature* pCreature)
{
	return (*g_pCreatureTable)[CreatureState::GetCreatureType(pCreature)].IsNPC();
}

void GCAddOustersHandler::execute ( GCAddOusters * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
		
	
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
		const PCOustersInfo3 & oi = pPacket->getOustersInfo ();

		MCreature* pCreature = ZoneState::GetCreature(oi.getObjectID());

		//--------------------------------------------------
		// A new creature: add it
		//--------------------------------------------------
		if (pCreature==NULL)
		{
			pCreature = ZoneState::NewCreatureWear();
			if (pCreature == NULL)
			{
				return;
			}
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, oi.getAdvancementLevel() );
			}

			CreatureState::SetName( pCreature, oi.getName().c_str() );

			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			ItemType_t coatType = oi.getCoatType();

			if (oi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_OUSTERS_OPERATOR );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetOustersCreatureType( coatType ) );
			}

			CreatureState::SetID( pCreature, oi.getObjectID() );
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetServerPosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetDirection( pCreature, oi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, oi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			CreatureState::SetBodyColor1( pCreature, oi.getHairColor() );
			CreatureState::SetBodyColor2( pCreature, oi.getCoatColor() );
	
			// add by sonic 2006.10.29
			CreatureState::SetMasterEffectType( pCreature, oi.getMasterEffectColor() );
			// end
			//--------------------------------------------------
			// [ TEST CODE ]
			//--------------------------------------------------
			// set the coat's colour
			//--------------------------------------------------
			/*
			if (pCreature->IsMale())
			{
				CreatureState::SetBodyColor2( pCreature, 91 );
			}
			else
			{
				CreatureState::SetBodyColor2( pCreature, 38 );
			}
			*/

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, oi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, oi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, oi.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, oi.getRank() );
			}

			//oi.getName()
			// colour information

			CreatureState::SetWeaponSpeed( pCreature, oi.getAttackSpeed() );
			
			// for now
			CreatureState::SetGuildNumber( pCreature, oi.getGuildID() );
			CreatureState::SetUnionGuildID( pCreature, oi.getUnionID() );
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());
			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsOusters( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
			{
				if(!pPacket->getStoreOutlook().getSign().empty())
					CreatureState::SetPersnalString( pCreature, pPacket->getStoreOutlook().getSign().c_str(), CommunicationPresentation::GetChattingColor() );
				else
					CreatureState::SetPersnalString( pCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor() );
			}


			if (!ZoneState::AddCreature( pCreature ))
			{
				ZoneState::DeleteCreature( pCreature );
				pCreature = NULL;
			}
			
		}
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			ItemType_t coatType = oi.getCoatType();

			if (oi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_OUSTERS_OPERATOR );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetOustersCreatureType( coatType ) );
			}

			// for now
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, oi.getAdvancementLevel() );
			}
			CreatureState::SetGuildNumber( pCreature, oi.getGuildID() );
			CreatureState::SetUnionGuildID( pCreature, oi.getUnionID() );
			
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::MovePosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetServerPosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetDirection( pCreature, oi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, oi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			CreatureState::SetBodyColor1( pCreature, oi.getHairColor() );
			CreatureState::SetBodyColor2( pCreature, oi.getCoatColor() );
			CreatureState::SetMasterEffectType( pCreature, oi.getMasterEffectColor() );

	
			//--------------------------------------------------
			// [ TEST CODE ]
			//--------------------------------------------------
			// set the coat's colour
			//--------------------------------------------------
			/*
			if (pCreature->IsMale())
			{
				CreatureState::SetBodyColor2( pCreature, 91 );
			}
			else
			{
				CreatureState::SetBodyColor2( pCreature, 38 );
			}
			*/

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, oi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, oi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, oi.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, oi.getRank() );
			}

			CreatureState::SetWeaponSpeed( pCreature, oi.getAttackSpeed() );
			
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());

			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsOusters( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
			{
				if(!pPacket->getStoreOutlook().getSign().empty())
					CreatureState::SetPersnalString( pCreature, pPacket->getStoreOutlook().getSign().c_str(), CommunicationPresentation::GetChattingColor() );
				else
					CreatureState::SetPersnalString( pCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor() );
 
//				pCreature->SetPersnalString((char *)pPacket->getStoreOutlook().getSign().c_str());
//				pCreature->SetPersnalShop(pPacket->getStoreOutlook().isOpen());
			}

			
		}

		if (pCreature!=NULL)
		{
			//--------------------------------------------------
			// Attach the effects..
			//--------------------------------------------------
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );
			
			CreatureState::SetAddonToOusters3( pCreature, &oi );

			// the pet
			if(pPacket->getPetInfo() != NULL)
				WorldEvents::SetPetInfo(pPacket->getPetInfo(), CreatureState::GetID( pCreature ));

			DEBUG_ADD_FORMAT("[GCADDOUSTERS] ID = %d", (int)CreatureState::GetID( pCreature ));
			
			NicknameInfo* _tempNick = pPacket->getNicknameInfo();
			if(_tempNick != NULL)
			{
				// a custom nickname
				if(_tempNick->getNicknameType() == NicknameInfo::NICK_CUSTOM_FORCED ||
				   _tempNick->getNicknameType() == NicknameInfo::NICK_CUSTOM)
				{
					CreatureState::SetNickName( pCreature, _tempNick->getNicknameType(), (char*)_tempNick->getNickname().c_str());
					
				}
				else // a nickname by its index
				{
					int TempIndex = _tempNick->getNicknameIndex();
					if(TempIndex >= g_pNickNameStringTable->GetSize())
						TempIndex = 0;
					CreatureState::SetNickName( pCreature, _tempNick->getNicknameType(), (char*)(*g_pNickNameStringTable)[TempIndex].GetString());
				}
			}

		}
	}


	__END_CATCH
}
