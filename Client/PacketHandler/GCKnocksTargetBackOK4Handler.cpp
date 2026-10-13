//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKnocksTargetBackOK4Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKnocksTargetBackOK4.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCKnocksTargetBackOK4Handler::execute ( GCKnocksTargetBackOK4 * pPacket , Player * pPlayer )

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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );

		// Creature가 knockback당함
		if (pCreature != NULL)
		{			
			unsigned short x = pPacket->getX();
			unsigned short y = pPacket->getY();

			// 물러나는방향(direction)적용..
			// 이동한 후의 좌표가 온다.
			//MCreature::GetPositionToDirection(x, y, pPacket->getDir());

			// Creature의 좌표를 바로 바꾼다.
			CreatureState::MovePosition(pCreature, x, y );
			CreatureState::SetServerPosition(pCreature, x, y );
			CreatureState::SetStop(pCreature);

			// 결과 바로 표현
			CreatureState::PacketSpecialActionResult(pCreature, 
								static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType() + (*g_pActionInfoTable).GetMinResultActionInfo()),
								CreatureState::GetID(pCreature),
								CreatureState::GetX(pCreature),
								CreatureState::GetY(pCreature)
			);			
		}
	}


	__END_CATCH
}
