//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddEffectToTileHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddEffectToTile.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "DirectionSelection.h"
#include "MEffectStatusTable.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddEffectToTileHandler::execute ( GCAddEffectToTile * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPlayer;
		
		
	// Exceptions
	MCreature* const pPlayerCreature = PlayerState::Creature();

	// A trap is only shown to slayers.
	if( pPacket->getEffectID() == EFFECTSTATUS_TRAP_INSTALLED && (pPlayerCreature == NULL || !CreatureState::IsSlayer(pPlayerCreature)) )
		return;

	EFFECTSTATUS EffectStatusType = (EFFECTSTATUS)pPacket->getEffectID();
	int skillType	= (*g_pEffectStatusTable)[ EffectStatusType ].ActionInfo;
	
	// 2004, 9, 3, sobeit add start
	if(EffectStatusType == EFFECTSTATUS_TURRET_LASER && pPlayerCreature != NULL) // a siege trap: a beam the attackers do not see
	{
		if(PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) || 
			PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) || 
			PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) || 
			PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) || 
			PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5) )
			return;
	}
	// 2006.10.29 sonic add (the rest of the note is garbled)
	if(EffectStatusType == EFFECTSTATUS_DRAGON_TORNADO || EffectStatusType == EFFECTSTATUS_DRAGON_TORNADO_CHILD)
	{
		WorldEvents::SetDragonTornado(EffectStatusType, pPacket->getObjectID(), pPacket->getX(), pPacket->getY());
		return;
	}
	// 2004, 9, 3, sobeit add end
	//------------------------------------------------------------
	// When there is a skill tied to the effect..
	//------------------------------------------------------------
	if (skillType!=ACTIONINFO_NULL)
	{
		int x			= pPacket->getX();
		int y			= pPacket->getY();
		int delayFrame	= ConvertDurationToFrame( pPacket->getDuration() );
		
//		switch(skillType)
//		{
//		case SKILL_CLIENT_TRAP_ICICLE_DROP:
//			delayFrame = 16;
//			break;
//		case SKILL_CLIENT_TRAP_ICICLE_AUGER:
//			delayFrame = 14;
//			break;
//		case SKILL_CLIENT_TRAP_ICICLE_LARGE_AUGER:
//			delayFrame = 12;
//			break;
//		}
		WorldEvents::ExecuteActionInfoFromMainNode(
				skillType,		// the value itself is a RESULT_ACTIONINFO.
			
				x, y, 0,
				DIRECTION_DOWN,
				
				OBJECTID_NULL,
				x, y, 0,
				
				delayFrame,	
				
				NULL, true, 0, 0
		);
	}
	else
	{
		DEBUG_ADD_FORMAT_ERR("[Error] No ActionInfo with EffectStatusID=%d", pPacket->getEffectID());
	}
	
	switch( EffectStatusType )
	{
	case EFFECTSTATUS_TRYING :
		WorldEvents::ExecuteActionInfoFromMainNode(
			REGEN_TOWER_TRING_POSITION_LIGHT,
			pPacket->getX(),
			pPacket->getY(),
			0,
			DIRECTION_DOWN,
			OBJECTID_NULL,
			pPacket->getX(),
			pPacket->getY(),
			0,
			ConvertDurationToFrame( pPacket->getDuration() ),
			NULL, true, 0, 0
			);
		break;
		
	case EFFECTSTATUS_GROUND_ELEMENTAL_AURA:
		if( ZoneState::Exists() )
		{
			ZoneState::SetGroundElemental(pPacket->getX(), pPacket->getY());
		}
		break;

	case EFFECTSTATUS_DARKNESS_FORBIDDEN:
		if( ZoneState::Exists() )
		{
			ZoneState::SetDarknessForbidden(pPacket->getX(), pPacket->getY());
		}
		break;
	case EFFECTSTATUS_LUCKY:
		WorldEvents::ExecuteActionInfoFromMainNode(
			RESULT_LUCKY_AURA,
			pPacket->getX(),
			pPacket->getY(),
			0,
			DIRECTION_DOWN,
			OBJECTID_NULL,
			pPacket->getX(),
			pPacket->getY(),
			0,
			ConvertDurationToFrame( pPacket->getDuration() ),
			NULL, true, 0, 0
			);
		break;
	case EFFECTSTATUS_MISFORTUNE:
		WorldEvents::ExecuteActionInfoFromMainNode(
			RESULT_MISFORTUNE_AURA,
			pPacket->getX(),
			pPacket->getY(),
			0,
			DIRECTION_DOWN,
			OBJECTID_NULL,
			pPacket->getX(),
			pPacket->getY(),
			0,
			ConvertDurationToFrame( pPacket->getDuration() ),
			NULL, true, 0, 0
			);
		break;
	case EFFECTSTATUS_FURY_OF_GNOME:
		if( ZoneState::Exists() )
		{
			ZoneState::SetFuryOfGnome(pPacket->getX(), pPacket->getY());
		}
		break;

	case EFFECTSTATUS_HALO:
		if( ZoneState::Exists() )
		{
			MCreature * pTargetCreature = ZoneState::GetCreature(pPacket->getObjectID());
			// The view (g_pTopView, which it once also checked) outlives every zone.
			if(pTargetCreature != NULL)
			{
				int TempDir = SelectFacingDirection(pPacket->getX(), pPacket->getY(), CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature));
//				if(rand()%2)
//					TempDir = (TempDir+2)%8;
//				else
//					TempDir = (TempDir+6)%8;
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_HALO_ATTACK, pPacket->getX(), pPacket->getY(), 0,TempDir,	CreatureState::GetID(pTargetCreature),	
					CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature), 0, 12, NULL, true, 0, 0);	
			}
		}
		break;

	default:
		break;
	}


	__END_CATCH
}
