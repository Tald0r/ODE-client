//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackArmsOK4Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackArmsOK4.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "MSlayerGear.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackArmsOK4Handler::execute ( GCAttackArmsOK4 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

			// message

	//------------------------------------------------------
	// Zone이 아직 생성되지 않은 경우
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
		return;
	}	

	//------------------------------------------------------
	// 대상이 되는 creature를 얻는다.
	//------------------------------------------------------
	MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );
	
	if (pTargetCreature==NULL)
	{
		// 그런 creature가 없을 경우
		DEBUG_ADD_FORMAT("There's no such creature : TargetID=%d, Skill=%d", pPacket->getTargetObjectID(), SKILL_ATTACK_MELEE);
		
		return;
	}

	//------------------------------------------------------
	// target creature가 결과를 당하는 모습을 바로 보여준다.
	//------------------------------------------------------
	
	if( pPacket->getSkillType() == SKILL_JABBING_VEIN || pPacket->getSkillType() == SKILL_MOLE_SHOT ||
					pPacket->getSkillType() == SKILL_TRIDENT || pPacket->getSkillType() == SKILL_QUICK_FIRE ||
					pPacket->getSkillType() == SKILL_ULTIMATE_BLOW || pPacket->getSkillType() == SKILL_HARPOON_BOMB)
	{
		CreatureState::PacketSpecialActionResult(pTargetCreature, 
					static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType()+g_pActionInfoTable->GetMinResultActionInfo()),
					CreatureState::GetID(pTargetCreature),
					CreatureState::GetX(pTargetCreature),
					CreatureState::GetY(pTargetCreature)
		);
	}
	else
	{
		CreatureState::PacketSpecialActionResult(pTargetCreature, 
					RESULT_SKILL_ATTACK_GUN_SR,				//pPacket->getSkillType()+g_ActionInfoTable.GetMinResultActionInfo()
					CreatureState::GetID(pTargetCreature),
					CreatureState::GetX(pTargetCreature),
					CreatureState::GetY(pTargetCreature)
		);
	}




	__END_CATCH
}
