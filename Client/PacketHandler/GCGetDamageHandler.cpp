//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGetDamageHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGetDamage.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCGetDamageHandler::execute ( GCGetDamage * pGCGetDamage , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// message

	//------------------------------------------------------
	// Player가 Damage를 받았으면..
	//------------------------------------------------------
	if (pGCGetDamage->getObjectID()==PlayerState::GetID())
	{
		CreatureState::PacketSpecialActionResult( PlayerState::Creature(),
					static_cast<TYPE_ACTIONINFO>(SKILL_ATTACK_MELEE + (*g_pActionInfoTable).GetMinResultActionInfo()),
					PlayerState::GetID(),
					CreatureState::GetX(PlayerState::Creature()),
					CreatureState::GetY(PlayerState::Creature())
			);
	}
	else
	{
		//------------------------------------------------------
		// Zone이 아직 생성되지 않은 경우
		//------------------------------------------------------
		if (!ZoneState::Exists())
		{
			// message
			DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		}
		//------------------------------------------------------
		// 정상.. 
		//------------------------------------------------------
		else
		{
			MCreature* pCreature = ZoneState::GetCreature( pGCGetDamage->getObjectID() );

			// Creature에게 Damage 입힘
			if (pCreature != NULL)
			{
				// SKILL_ATTACK_MELEE에 대한 결과를 표현해준다.
				CreatureState::PacketSpecialActionResult(pCreature, 
								static_cast<TYPE_ACTIONINFO>(SKILL_ATTACK_MELEE + (*g_pActionInfoTable).GetMinResultActionInfo()),
								CreatureState::GetID(pCreature),
								CreatureState::GetX(pCreature),
								CreatureState::GetY(pCreature)
				);
				
			}
		}
	}

	__END_CATCH
}
