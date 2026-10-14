//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillFailed2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillFailed2.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillFailed2Handler::execute ( GCSkillFailed2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	
	// message

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
		return;
	}	

	//------------------------------------------------------
	// The creatures involved.
	//------------------------------------------------------
	MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );
	MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );
	
	//------------------------------------------------------
	// The creature acts.
	//------------------------------------------------------
	int skilltype = pPacket->getSkillType();
	
//	if(skilltype == SKILL_BLOOD_DRAIN)
//	{
//		DEBUG_ADD("blood drain failed");
//	}
	if( skilltype >= g_pActionInfoTable->GetSize() )
		return;
	
	if(pTargetCreature == NULL && pCreature != NULL && CreatureState::IsOusters(pCreature) && skilltype == SKILL_ABSORB_SOUL)
	{
		pTargetCreature = ZoneState::GetItemAsCorpseCreature( pPacket->getTargetObjectID() );
	}

	if (pCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), SKILL_ATTACK_MELEE);				
		
		return;
	}

	//------------------------------------------------------
	// The attack skill depends on the weapon in hand.
	//------------------------------------------------------
	if (skilltype==SKILL_ATTACK_MELEE
		|| skilltype==SKILL_ATTACK_ARMS)
	{
		skilltype = CreatureState::GetBasicActionInfo(pCreature);
	}

	//
	if (static_cast<DWORD>(skilltype) >= g_pActionInfoTable->GetMinResultActionInfo())
	{
		DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", skilltype);
		return;
	}
	
	if( pPacket->getGrade() > 0 && (*g_pActionInfoTable)[skilltype].IsUseActionStep() )
		skilltype = (*g_pActionInfoTable)[skilltype].GetActionStep( pPacket->getGrade() - 1 );


	DEBUG_ADD_FORMAT("SkillType = %d", skilltype);

	if (CreatureState::IsOusters(pCreature) && skilltype==SKILL_ABSORB_SOUL)
	{
		CreatureState::SetStopAbsorbSoul(pCreature);
	}
	
	// 2005, 1, 10, sobeit add start - a failed grenade throw by monster 784 shows nothing..
	if(skilltype == SKILL_GRENADE_ATTACK) 
		return;
	// 2005, 1, 10, sobeit add end

	//------------------------------------------------------
	// Other
	//------------------------------------------------------
	if ((*g_pActionInfoTable)[skilltype].IsTargetOther())
	{		
		if (skilltype==SKILL_BLOOD_DRAIN || skilltype == SKILL_BITE_OF_DEATH )
		{
			CreatureState::SetStopBloodDrain(pCreature);
			//DEBUG_ADD("blood drain test");
		}
		
		// Nobody was hit.  
		if (pTargetCreature==NULL)
		{
			// No such creature.
			DEBUG_ADD_FORMAT("There's no such creature : TargetID=%d, Skill=%d", pPacket->getTargetObjectID(), SKILL_ATTACK_MELEE);

			//------------------------------------------------------
			// The creature acts.
			//------------------------------------------------------
			CreatureState::PacketSpecialActionToNobody(pCreature, skilltype,
							CreatureState::GetX(pCreature),
							CreatureState::GetY(pCreature)						
			);			
			
			return;
		}
		
		// Used on itself?
		if (pCreature==pTargetCreature)
		{
			CreatureState::PacketSpecialActionToSelf(pCreature, skilltype, 				
						NULL
			);

			return;
		}
		
		//------------------------------------------------------
		// The acting creature faces its target.
		//------------------------------------------------------
		CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pTargetCreature), CreatureState::GetY(pTargetCreature) );

		CreatureState::PacketSpecialActionToOther(pCreature, skilltype, 
						pPacket->getTargetObjectID(), 
						NULL
		);		
		
	}
	//------------------------------------------------------
	// Self
	//------------------------------------------------------
	else if ((*g_pActionInfoTable)[skilltype].IsTargetSelf())
	{
		CreatureState::PacketSpecialActionToSelf(pCreature, skilltype, 				
						NULL
		);
	}
	//------------------------------------------------------
	// Zone
	//------------------------------------------------------
	else if ((*g_pActionInfoTable)[skilltype].IsTargetZone())
	{
		CreatureState::PacketSpecialActionToSector(pCreature, skilltype, 
						CreatureState::GetX(pCreature),
						CreatureState::GetY(pCreature),
						NULL
		);
	}


	__END_CATCH
}
