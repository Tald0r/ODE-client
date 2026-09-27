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
#include "ClientDef.h"
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
	if (pGCGetDamage->getObjectID()==g_pPlayer->GetID())
	{
		g_pPlayer->PacketSpecialActionResult( 
					static_cast<TYPE_ACTIONINFO>(SKILL_ATTACK_MELEE + (*g_pActionInfoTable).GetMinResultActionInfo()),
					g_pPlayer->GetID(),
					g_pPlayer->GetX(),
					g_pPlayer->GetY()
			);
	}
	else
	{
		//------------------------------------------------------
		// Zone이 아직 생성되지 않은 경우
		//------------------------------------------------------
		if (g_pZone==NULL)
		{
			// message
			DEBUG_ADD("[Error] Zone is Not Init.. yet.");			
		}
		//------------------------------------------------------
		// 정상.. 
		//------------------------------------------------------
		else
		{
			MCreature* pCreature = g_pZone->GetCreature( pGCGetDamage->getObjectID() );

			// Creature에게 Damage 입힘
			if (pCreature != NULL)
			{
				// SKILL_ATTACK_MELEE에 대한 결과를 표현해준다.
				pCreature->PacketSpecialActionResult( 
								static_cast<TYPE_ACTIONINFO>(SKILL_ATTACK_MELEE + (*g_pActionInfoTable).GetMinResultActionInfo()),
								pCreature->GetID(),
								pCreature->GetX(),
								pCreature->GetY()
				);
				
			}
		}
	}

	__END_CATCH
}
