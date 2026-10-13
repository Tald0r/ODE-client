//////////////////////////////////////////////////////////////////////
//
// Filename    : GCBloodDrainOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCBloodDrainOK3.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCBloodDrainOK3Handler::execute ( GCBloodDrainOK3 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The normal case. 
	//------------------------------------------------------
	else
	{
		MCreature* pUserCreature = ZoneState::GetCreature( pPacket->getObjectID() );
		MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );		

		// The creature is hit.
		if (pUserCreature != NULL && pTargetCreature != NULL)
		{
			// Tell the target who drains its blood.
			CreatureState::SetDrainCreatureID(pTargetCreature, pPacket->getObjectID() );

			CreatureState::ClearStopBloodDrain(pUserCreature);

			// [ TEST CODE ]
			//
			// Make the result and hand it over.
			//
			// The target is hit by the user's skill.
			// [ TEST CODE ]
			MActionResult* pResult = new MActionResult;

			//DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			pResult->Add( new MActionResultNodeActionInfo( 
											SKILL_BLOOD_DRAIN,
											pPacket->getObjectID(), 
											pPacket->getTargetObjectID(), 
											CreatureState::GetX(pTargetCreature),
											CreatureState::GetY(pTargetCreature)
											 ) );

			// The user faces the target.
			CreatureState::SetDirectionToPosition(pUserCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature));
			//pTargetCreature->SetDirectionToPosition(pUserCreature->GetX(), pUserCreature->GetY());

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther(pUserCreature, SKILL_BLOOD_DRAIN,
								pPacket->getTargetObjectID(),
								pResult			// result
			);		

			int delayFrame = 691200;	// 12 hours of 16 frames a second: 12*60*60*16
			CreatureState::AddEffectStatus(pTargetCreature, EFFECTSTATUS_BLOOD_DRAIN, delayFrame );
		}
	}	


	__END_CATCH
}
