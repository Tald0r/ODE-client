//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddNPCHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddNPC.h"
#include "DebugLog.h"
#include "EffectSpriteTypeDef.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MGuildType.h"
#include "MStatus.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddNPCHandler::execute ( GCAddNPC * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// message

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
		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

		//--------------------------------------------------
		// A new creature is added
		//--------------------------------------------------
		if (pCreature==NULL)
		{			
			int npcID = pPacket->getNPCID();

			MCreature* pNPC = ZoneState::NewNPC();
			if (pNPC == NULL)
			{
				return;
			}

			int zoneID	= (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);

			// The EVE guild mark
			int creatureType = npcID;
			int spriteType = (*g_pCreatureTable)[creatureType].SpriteTypes[0];			
			
			const CREATURESPRITETABLE_INFO& spriteInfo = (*g_pCreatureSpriteTable)[spriteType];

			if ((spriteInfo.IsNPCSprite()
				&& spriteInfo.IsSlayerSprite())
				|| spriteInfo.IsSlayerSprite())
			{
				// slayer NPC
				CreatureState::SetGuildNumber( pNPC, GUILDID_EVE );
			} else
			if( spriteInfo.IsOustersSprite() )
			{
				CreatureState::SetGuildNumber( pNPC, GUILDID_OUSTERS );
			}
			else if (zoneID==1007 || zoneID==1114 || zoneID==1115)
			{
				// Tepez
				CreatureState::SetGuildNumber( pNPC, GUILDID_TEPEZ );
			}
			else
			{
				// Bathory
				CreatureState::SetGuildNumber( pNPC, GUILDID_BATHORY );
			}

			//pNPC->SetCreatureType( 0 );
			CreatureState::SetCreatureType( pNPC, npcID );
			CreatureState::SetGroundCreature( pNPC );
			CreatureState::SetID( pNPC, pPacket->getObjectID() );
			//pNPC->SetAction(ACTION_MOVE);
			CreatureState::SetPosition( pNPC, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pNPC, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pNPC, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pNPC, pPacket->getDir() );
			CreatureState::SetAction( pNPC, ACTION_STAND );

			if(zoneID>= 1500 && zoneID<=1506)
				CreatureState::SetName( pNPC, pPacket->getName().c_str() );
			else
				CreatureState::SetName( pNPC, (*g_pCreatureTable)[creatureType].Name );

			CreatureState::SetBodyColor1( pNPC, pPacket->getMainColor() );
			CreatureState::SetBodyColor2( pNPC, pPacket->getSubColor() );

			// NPC ID
			CreatureState::SetNPCID( pNPC, pPacket->getNPCID() );
			
			//
			MStatus* pStatus = CreatureState::Status( pNPC );
			if (pStatus != NULL)
			{
				pStatus->SetStatus( MODIFY_MAX_HP, 100 );
			}

//			_MinTrace("AddNPC : %s Dir:%d Pos(%d,%d) CreatureType : %d\n",pNPC->GetName(), pPacket->getDir(), pPacket->getX(), pPacket->getY(), pPacket->getNPCID() );
			//------------------------------------------------------------
			// Bathory.. hard-coded (-_-);
			//------------------------------------------------------------
			if (npcID==217)
			{
				if (pStatus != NULL)
				{
					pStatus->SetStatus( MODIFY_CURRENT_HP, 10 );
					pStatus->SetStatus( MODIFY_ALIGNMENT, -10000 );	// means little
				}
			}
			else
			{
				if (pStatus != NULL)
				{
					pStatus->SetStatus( MODIFY_CURRENT_HP, 100 );
				}
			}

			if (!ZoneState::AddCreature( pNPC ))
			{
				ZoneState::DeleteCreature( pNPC );
				pNPC = NULL;
			}
			else
			{			
				//------------------------------------------------------------
				// A slayer NPC is dressed.
				//------------------------------------------------------------
				CreatureState::SetAddonToNPC( pNPC, npcID );

				//------------------------------------------------------------
				// Load it if it is not loaded.
				//------------------------------------------------------------
//				LoadCreatureType( pPacket->getNPCID() );

				const int npcType = CreatureState::GetCreatureType( pNPC );
				if( npcType == 638 || npcType == 657)
					CreatureState::AttachLastingEffect( pNPC, EFFECTSPRITETYPE_AMATA );
				else
				if( npcType == 639 || npcType == 654)
					CreatureState::AttachLastingEffect( pNPC, EFFECTSPRITETYPE_RIPATY );
			}
		}
		//--------------------------------------------------
		// A creature already there
		//--------------------------------------------------
		else
		{
			if (CreatureState::IsNPC( pCreature ))
			{
				// Already there..
				CreatureState::SetGroundCreature( pCreature );
				
				//pCreature->SetAction(ACTION_MOVE);
				CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
				CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );
				CreatureState::SetDirection( pCreature, pPacket->getDir() );
				CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
				CreatureState::SetAction( pCreature, ACTION_STAND );

				CreatureState::SetBodyColor1( pCreature, pPacket->getMainColor() );
				CreatureState::SetBodyColor2( pCreature, pPacket->getSubColor() );

				//
				//pCreature->SetStatus( MODIFY_MAX_HP, 100 );
				//pCreature->SetStatus( MODIFY_CURRENT_HP, 100 );
			}
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] The Creature is Not NPC. id=%d", pPacket->getObjectID());
			}
		}
	}

	// [Help] when an NPC appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_CREATURE_APPEAR_NPC );
//	__END_HELP_EVENT


	__END_CATCH
}
