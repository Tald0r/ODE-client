//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowItemOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowItemOK3.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowItemOK3Handler::execute ( GCThrowItemOK3 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	int skillType = MAGIC_THROW_HOLY_WATER;

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
			// [ TEST CODE ]
			//
			// Make the result and hand it over.
			//
			// The target is hit by the user's skill.
			// [ TEST CODE ]
			MActionResult* pResult = new MActionResult;

			//DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			pResult->Add( new MActionResultNodeActionInfo( 
										skillType,//pPacket->getSkillType(), 
										pPacket->getObjectID(), 
										pPacket->getTargetObjectID(), 
										CreatureState::GetX(pTargetCreature),
										CreatureState::GetY(pTargetCreature)
									)
						);
										//delayFrame ) );

			// The user faces the target.
			CreatureState::SetDirectionToPosition(pUserCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature));
			//pTargetCreature->SetDirectionToPosition(pUserCreature->GetX(), pUserCreature->GetY());

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther(pUserCreature, skillType,
								pPacket->getTargetObjectID(),
								pResult			// result
			);		
		}
	}		


	__END_CATCH
}
