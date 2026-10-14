//////////////////////////////////////////////////////////////////////
//
// Filename    : GCLearnSkillOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCLearnSkillOK.h"
#include "../basic/CMessageArray.h"
#include "BonusSkillHost.h"
#include "GameMessages.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"
#include "TempInformation.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCLearnSkillOKHandler::execute ( GCLearnSkillOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

		
		
	// Clear the mode;
	// otherwise the player could keep learning.
	//g_TempInformation.Mode = TempInformation::MODE_NULL;
	g_pTempInformation->SetMode(TempInformation::MODE_NULL);
						

	int domainType	= pPacket->getSkillDomainType();
	int skillType	= pPacket->getSkillType();
	auto* domain = g_pSkillManager->GetMutable(domainType);
	auto* skill = g_pSkillInfoTable->GetMutable(skillType);
	if (domain == nullptr || skill == nullptr)
		return;


	// Learn it.
	MSkillDomain& swordDomain = *domain;

	swordDomain.SetNewSkill();
	bool bLearn = swordDomain.LearnSkill( (ACTIONINFO)skillType );
	
	// An ousters raises the level of a skill it has already learned; without
	// a player there is no ousters.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	const bool bOusters = pPlayerCreature != NULL && CreatureState::IsOusters(pPlayerCreature);
	if(bLearn == true || !bOusters)
	{
		// Add it to the skills usable now.
		g_pSkillAvailable->AddSkill( (ACTIONINFO)skillType );
		
		SKILLINFO_NODE& skillInfo = *skill;
		
		skillInfo.SetExpLevel( 1 );
		skillInfo.SetSkillExp( 0 );
		
		skillInfo.SetDelayTime( skillInfo.maxDelay );
		skillInfo.SetEnable( true );
		
		skillInfo.SetNextAvailableTime();
	}
	else if( bOusters )
	{
		// An ousters that has already learned the skill:
		SKILLINFO_NODE& skillInfo = *skill;
		skillInfo.SetExpLevel( skillInfo.GetExpLevel()+1 );
		
	}
	
	// DEBUG_ADD_FORMAT( "[Error] TempInformation Mode is Not SKILL_LEARN : %d", (int)g_TempInformation.Mode);

	//------------------------------------------------------------
	// Check again which skills are usable.
	//------------------------------------------------------------
	BonusSkills::RefreshAvailableSkills();
	
	//------------------------------------------------------------
	// Say that the skill was learned.
	//------------------------------------------------------------
	g_pGameMessage->AddSafeFormat(GetGameString(STRING_LEARN_SKILL), (*g_pSkillInfoTable)[skillType].GetHName());

	// [Help] when a skill is learned
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_SKILL_LEARNED );
//	__END_HELP_EVENT


	__END_CATCH
}
