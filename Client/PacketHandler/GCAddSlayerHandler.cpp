//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddSlayerHandler.cc
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddSlayer.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// MCreature::IsNPC: whether the creature table's row for its type is an
// NPC's.
static bool IsNPCType(MCreature* pCreature)
{
	return (*g_pCreatureTable)[CreatureState::GetCreatureType(pCreature)].IsNPC();
}

void GCAddSlayerHandler::execute ( GCAddSlayer * pPacket , Player * pPlayer )

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
		const PCSlayerInfo3 & si = pPacket->getSlayerInfo();
			
		DEBUG_ADD_FORMAT("[AddSlayer] id=%d, xy=(%d,%d), dir=%d", si.getObjectID(), si.getX(), si.getY(), si.getDir());				
		
		MCreature* pCreature = ZoneState::GetCreature(si.getObjectID());

		//--------------------------------------------------
		// A new creature: add it
		// It is a creature that wears gear.
		//--------------------------------------------------
		if (pCreature==NULL)
		{
			MCreature* pCreatureWear = ZoneState::NewCreatureWear();
			if (pCreatureWear == NULL)
			{
				return;
			}
			if (MStatus* pStatus = CreatureState::Status( pCreatureWear ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, si.getAdvancementLevel() );
			}
			pCreature = pCreatureWear;

			if (si.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreatureWear, 0 );
				CreatureState::SetCreatureType( pCreatureWear, CREATURETYPE_SLAYER_OPERATOR );
				CreatureState::SetMale( pCreatureWear, si.getSex()==MALE );
			}
			else
			{
				CreatureState::SetCreatureType( pCreatureWear, (si.getSex()==MALE)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE );
			}

			CreatureState::SetGuildNumber( pCreatureWear, si.getGuildID() );
			CreatureState::SetUnionGuildID( pCreatureWear, si.getUnionID() );

			CreatureState::SetGroundCreature( pCreatureWear );
			CreatureState::SetID( pCreatureWear, si.getObjectID() );
			//pCreatureWear->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pCreatureWear, si.getX(), si.getY() );
			CreatureState::SetServerPosition( pCreatureWear, si.getX(), si.getY() );
			CreatureState::SetDirection( pCreatureWear, si.getDir() );
			CreatureState::SetCurrentDirection( pCreatureWear, si.getDir() );
			CreatureState::SetAction( pCreatureWear, ACTION_STAND );

			// skin colour
			CreatureState::SetBodyColor1( pCreatureWear, si.getSkinColor() );

			CreatureState::SetMasterEffectType( pCreature, si.getMasterEffectColor());

			if (MStatus* pStatus = CreatureState::Status( pCreatureWear ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, si.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, si.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, si.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, si.getRank() );
			}

			// the name
			CreatureState::SetName( pCreatureWear, si.getName().c_str() );

			// the colour information

			// its gear --> for now, in basic clothes.
			//----------------------------------------	
			// Put the gear on.
			//----------------------------------------	
			CreatureState::SetAddonToSlayer3( pCreatureWear, &si );
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());

			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsSlayer( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
			{
				if(!pPacket->getStoreOutlook().getSign().empty())
					CreatureState::SetPersnalString( pCreature, pPacket->getStoreOutlook().getSign().c_str(), CommunicationPresentation::GetChattingColor() );
				else
					CreatureState::SetPersnalString( pCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor() );

			}

			if (!ZoneState::AddCreature( pCreatureWear ))
			{
				ZoneState::DeleteCreature( pCreatureWear );
				pCreatureWear = NULL;
				pCreature = NULL;
			}
		}
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, si.getAdvancementLevel() );
			}
			CreatureState::SetGuildNumber( pCreature, si.getGuildID() );
			CreatureState::SetUnionGuildID( pCreature, si.getUnionID() );

			CreatureState::SetGroundCreature( pCreature );
			
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::MovePosition( pCreature, si.getX(), si.getY() );
			CreatureState::SetServerPosition( pCreature, si.getX(), si.getY() );
			CreatureState::SetDirection( pCreature, si.getDir() );
			CreatureState::SetCurrentDirection( pCreature, si.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// skin colour
			CreatureState::SetBodyColor1( pCreature, si.getSkinColor() );
			CreatureState::SetMasterEffectType( pCreature, si.getMasterEffectColor());

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_MAX_HP, si.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, si.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, si.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, si.getRank() );
			}
			
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());
			
			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsSlayer( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
			{
				if(!pPacket->getStoreOutlook().getSign().empty())
					CreatureState::SetPersnalString( pCreature, pPacket->getStoreOutlook().getSign().c_str(), CommunicationPresentation::GetChattingColor() );
				else
					CreatureState::SetPersnalString( pCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor() );

			}

		}

		if (pCreature!=NULL)
		{
			//--------------------------------------------------
			// Attach the effects..
			//--------------------------------------------------
			WorldEvents::SetEffectInfo( pCreature, pPacket->getEffectInfo(), 0 );
			
			// the pet
			if(pPacket->getPetInfo() != NULL)
				WorldEvents::SetPetInfo(pPacket->getPetInfo(), CreatureState::GetID( pCreature ));
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

	// [Help] when a slayer appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_SLAYER );
//	__END_HELP_EVENT


	__END_CATCH
}
