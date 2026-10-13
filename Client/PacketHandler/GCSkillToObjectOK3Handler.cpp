//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToObjectOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToObjectOK3.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "Packet/WireHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToObjectOK3Handler::execute ( GCSkillToObjectOK3 * pPacket , Player * pPlayer )

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
		
		// The creature is hit.
		if (pUserCreature != NULL)
		{
			int skillID = pPacket->getSkillType();

			if( g_pActionInfoTable->GetSize() <= skillID )
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
				SendBugReport("[Error:GCSTOOK3H] Exceed SkillType %d", skillID );
				return;
			}


			// A skill that follows the current weapon.
			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction())
			{
				skillID = CreatureState::GetBasicActionInfo(pUserCreature);

				if (static_cast<DWORD>(skillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", skillID);
					return;
				}
			}
			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
				skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);
	
			// The user faces the target.
			CreatureState::SetDirectionToPosition(pUserCreature, pPacket->getTargetX(), pPacket->getTargetY());
			
			// Only show the skill used in that direction.
			CreatureState::PacketSpecialActionToNobody(pUserCreature, skillID, 
								pPacket->getTargetX(),
								pPacket->getTargetY()
			);		
		}
	}	


	__END_CATCH
}
