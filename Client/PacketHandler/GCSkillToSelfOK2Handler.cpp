//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToSelfOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToSelfOK2.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "Packet/WireHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToSelfOK2Handler::execute ( GCSkillToSelfOK2 * pPacket , Player * pPlayer )

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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );


		//------------------------------------------------------
		//
		// The creature uses the skill. 
		//
		//------------------------------------------------------
		if (pCreature != NULL)
		{	
			// [ TEST CODE ]
			//
			// Make the result and hand it over.
			//
			// Someone used the skill on itself.
			// [ TEST CODE ]

			int skillID = pPacket->getSkillType();
			
			if( g_pActionInfoTable->GetSize() <= skillID )
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
				SendBugReport("[Error:GCSTSOK2H] Exceed SkillType %d", skillID );

				return;
			}			
			
			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
				skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);
				
			MActionResult* pResult = new MActionResult;

			DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			pResult->Add( new MActionResultNodeActionInfo( 
									skillID, 
									CreatureState::GetID(pCreature), 
									CreatureState::GetID(pCreature), 
									CreatureState::GetX(pCreature),
									CreatureState::GetY(pCreature),
									delayFrame ) );

			//------------------------------------------------------
			// Add the effect status, if any.
			//------------------------------------------------------
			EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
			
			if (es!=EFFECTSTATUS_NULL)
			{
				pResult->Add( new MActionResultNodeAddEffectStatus( CreatureState::GetID(pCreature), 
																		es, 
																		delayFrame ) );
			}

			//------------------------------------------------------
			//
			// Apply the skill's own result too, if it has one.
			//
			//------------------------------------------------------
			MActionResultNode* pActionResultNode = CreateActionResultNode(pCreature, skillID);

			//------------------------------------------------------
			// Apply it too, if there is one.
			//------------------------------------------------------
			if (pActionResultNode!=NULL)
			{
				pResult->Add( pActionResultNode );
			}

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToSelf(pCreature, pPacket->getSkillType(),
									pResult	// result
				);
		}
	
	}


	__END_CATCH
}
