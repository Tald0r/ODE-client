//////////////////////////////////////////////////////////////////////
//
// Filename    : GCLearnSkillReady1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCLearnSkillReady.h"
#include "ClientDef.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "MGameStringTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCLearnSkillReadyHandler::execute ( GCLearnSkillReady * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	// 임시로 skill관련 정보를 저장한다.
	//g_TempInformation.Mode		=	TempInformation::MODE_SKILL_LEARN;
	//g_TempInformation.Value1	=	pPacket->getSkillDomainType();

	// 새로 배울 Skill이 있다고 표시한다.
	int domainType = pPacket->getSkillDomainType();

	// SkillDomainType_t is a BYTE and GCLearnSkillReady::read does not
	// bound it, so this is 0..255 against MAX_SKILLDOMAIN domains.
	// (*g_pSkillManager)[] survives that - MSkillManager is a
	// CTypeTable, which range-checks in every build - but
	// SKILLDOMAIN_NAME below is a plain int array and would read past
	// its end, then hand the garbage to the string table as an id.
	if (domainType < 0 || domainType >= MAX_SKILLDOMAIN)
	{
		DEBUG_ADD_FORMAT("[PacketError-GCLearnSkillReadyHandler] domain out of range: %d", domainType);
		return;
	}

	if (auto* entry = g_pSkillManager->GetMutable(domainType)) {
		entry->SetNewSkill();
	}

	// SKILLDOMAIN_NAME holds string table ids, not strings, so look the name up
	// before formatting it.
	g_pGameMessage->AddSafeFormat( "You can learn a new %s skill.",
		(*g_pGameStringTable)[SKILLDOMAIN_NAME[domainType]].GetString() );

	// levelup했다고 뭔가 보여준다. 뭘까.... --;
	//UI_LevelUp();

	// [도움말] Skill배울 수 있을 때
//	__BEGIN_HELP_EVENT
//		switch ( domainType )
//		{
//			case SKILL_DOMAIN_BLADE :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_BLADE );
//			break;
//
//			case SKILL_DOMAIN_SWORD :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_SWORD );
//			break;
//
//			case SKILL_DOMAIN_GUN :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_GUN );
//			break;
//
//			case SKILL_DOMAIN_ENCHANT :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_ENCHANT );
//			break;
//
//			case SKILL_DOMAIN_HEAL :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_HEAL );
//			break;
//			
//			case SKILL_DOMAIN_VAMPIRE :
////				ExecuteHelpEvent( HE_SKILL_CAN_LEARN_VAMPIRE );
//			break;
//		}
//	__END_HELP_EVENT
	

	__END_CATCH
}
