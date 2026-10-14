//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddVampireHandler.cc
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddVampire.h"
#include "CommunicationPresentationHost.h"
#include "CreatureTypeSelection.h"
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

void GCAddVampireHandler::execute ( GCAddVampire * pPacket , Player * pPlayer )

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
		const PCVampireInfo3 & vi = pPacket->getVampireInfo ();

		MCreature* pCreature = ZoneState::GetCreature(vi.getObjectID());

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
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
			}

			CreatureState::SetName( pCreature, vi.getName().c_str() );

			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();

			if (vi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_OPERATOR );
				CreatureState::SetMale( pCreature, bMale );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );
			}

			CreatureState::SetMale( pCreature, bMale );

			if (CreatureState::GetCreatureType( pCreature )==CREATURETYPE_BAT)
			{
				CreatureState::SetFlyingCreature( pCreature );
			}
			else
			{
				CreatureState::SetGroundCreature( pCreature );
			}

			CreatureState::SetID( pCreature, vi.getObjectID() );
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetDirection( pCreature, vi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, vi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );
 
			// colours
			
			if( CreatureState::GetCreatureType( pCreature ) == CREATURETYPE_WER_WOLF )
			{
				CreatureState::SetBodyColor1( pCreature, vi.getCoatColor() );
				CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );
			} else
			{
				CreatureState::SetBodyColor1( pCreature, vi.getSkinColor() );
				CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );
			}
			// add by sonic 2006.10.29
			CreatureState::SetMasterEffectType( pCreature, vi.getMasterEffectColor() );
			// 2006.10.29 sonic add end
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
				pStatus->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, vi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, vi.getRank() );
			}

			//vi.getName()
			// colour information

			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );
			
			// for now
			CreatureState::SetGuildNumber( pCreature, vi.getGuildID() );
			CreatureState::SetUnionGuildID( pCreature, vi.getUnionID() );
			if( vi.getBatColor() != 0 )
				CreatureState::SetBatColor( pCreature, vi.getBatColor() );
			else
				CreatureState::SetBatColor( pCreature, 0xFFFF );
			
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());
			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsVampire( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
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
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
			}
			//--------------------------------------------------
			// Set the CreatureType
			//--------------------------------------------------
			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();

			if (vi.getCompetence()==0)
			{
				CreatureState::SetCompetence( pCreature, 0 );
				CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_OPERATOR );
				CreatureState::SetMale( pCreature, bMale );
			}
			else
			{
				CreatureState::SetCreatureType( pCreature, GetVampireCreatureType( shape, bMale, coatType ) );
			}

			CreatureState::SetMale( pCreature, bMale );

			if (CreatureState::GetCreatureType( pCreature )==CREATURETYPE_BAT)
			{
				CreatureState::SetFlyingCreature( pCreature );
			}
			else
			{
				CreatureState::SetGroundCreature( pCreature );
			}
			//pCreature->SetGroundCreature();

			// for now
			CreatureState::SetGuildNumber( pCreature, vi.getGuildID() );
			CreatureState::SetUnionGuildID( pCreature, vi.getUnionID() );
			
			//pCreature->SetAction(ACTION_MOVE);
			CreatureState::MovePosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );			
			CreatureState::SetDirection( pCreature, vi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, vi.getDir() );
			CreatureState::SetAction( pCreature, ACTION_STAND );

			// colours
			if( CreatureState::GetCreatureType( pCreature ) == CREATURETYPE_WER_WOLF )
			{
				CreatureState::SetBodyColor1( pCreature, vi.getCoatColor() );
				CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );
			} else
			{
				CreatureState::SetBodyColor1( pCreature, vi.getSkinColor() );
				CreatureState::SetBodyColor2( pCreature, vi.getCoatColor() );
			}

			// add by sonic 2006.10.29
			CreatureState::SetMasterEffectType( pCreature, vi.getMasterEffectColor() );
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
				pStatus->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
				pStatus->SetStatus( MODIFY_CURRENT_HP, vi.getCurrentHP() );
				pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
				pStatus->SetStatus( MODIFY_RANK, vi.getRank() );
			}
			
			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );
			if( vi.getBatColor() != 0 )
				CreatureState::SetBatColor( pCreature, vi.getBatColor() );
			else
				CreatureState::SetBatColor( pCreature, 0xFFFF );
			if(IsNPCType( pCreature ) == false)
				CreatureState::SetPersnalShop( pCreature, pPacket->getStoreOutlook().isOpen());
			
			if(pPacket->getStoreOutlook().isOpen()&& PlayerState::Creature() != NULL && CreatureState::IsVampire( PlayerState::Creature() )== true && IsNPCType( pCreature ) == false)
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
			
			//--------------------------------------------------
			// 0 normally, 1 through a portal
			//--------------------------------------------------
			if (pPacket->getFromFlag()==1)
			{
				WorldEvents::ComeFromPortal( pCreature );
			}
			
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

	// [Help] when a vampire appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_VAMPIRE );
//	__END_HELP_EVENT

	__END_CATCH
}
