//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToInventoryOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToInventoryOK2.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToInventoryOK2Handler::execute ( GCSkillToInventoryOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		



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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );
			
		// Creature가 casting동작 취함..
		if (pCreature != NULL)
		{
			int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			//------------------------------------------------------------
			// Delay Frame 설정
			//------------------------------------------------------------
			CreatureState::SetEffectDelayFrame(pCreature, pPacket->getSkillType(), delayFrame );


			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToInventoryItem(pCreature, 
								pPacket->getSkillType()								
			);
		}
	}	
	


	__END_CATCH
}
