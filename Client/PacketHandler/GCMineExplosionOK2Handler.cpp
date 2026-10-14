//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMineExplosionOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMineExplosionOK2.h"
#include "DebugLog.h"
#include "ExplosiveActions.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMineExplosionOK2Handler::execute ( GCMineExplosionOK2 * pPacket , Player * pPlayer )

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
		int skillID = GetMineActionInfo( pPacket->getItemType() );

		if (skillID >= 0)
		{
			//int resultActionInfo = skillID + (*g_pActionInfoTable).GetMinResultActionInfo();

			//------------------------------------------------------
			// Make the result.
			//------------------------------------------------------
			MActionResult* pResult = new MActionResult;
		
			DWORD delayFrame = 16;//ConvertDurationToFrame( pPacket->getDuration() );

			int size = pPacket->getCListNum();

			for (int i=0; i<size; i++)
			{
				//pResult->Add( new MActionResultNodeActionInfo( 
				//					OBJECTID_NULL,		// the user; may it be missing?
				//					pPacket->popCListElement(), 
				//					skillType,
				//					delayFrame ) );
				//------------------------------------------------------
				//
				// Apply the skill's own result, if it has one.
				//
				//------------------------------------------------------
				int targetID = pPacket->popCListElement();

				MCreature* pTargetCreature = ZoneState::GetCreature( targetID );

				if (pTargetCreature!=NULL)
				{
					//------------------------------------------------------
					// Add the effect status, if any.
					//------------------------------------------------------
					EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
								
					if (es!=EFFECTSTATUS_NULL)
					{
						pResult->Add( new MActionResultNodeAddEffectStatus( targetID, es, delayFrame ) );
					}

					MActionResultNode* pActionResultNode = NULL;

					/*
					switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
					{
						//------------------------------------------------------
						// Run another action.
						//------------------------------------------------------
						case ACTIONRESULTNODE_ACTIONINFO :
							pActionResultNode =  new MActionResultNodeActionInfo( 
																(*g_pActionInfoTable)[skillID].GetActionResultValue(),
																OBJECTID_NULL, // no user
																targetID, 
																pTargetCreature->GetX(),
																pTargetCreature->GetY(),
																delayFrame);
						break;

						//------------------------------------------------------
						// Burrow
						//------------------------------------------------------
						case ACTIONRESULTNODE_CREATURE_BURROW :
							pActionResultNode = new MActionResultNodeCreatureBurrow( targetID );
						break;
					}
					*/
					pActionResultNode =  new MActionResultNodeActionInfo( 
																skillID, //resultActionInfo,
																targetID,
																targetID, 
																CreatureState::GetX(pTargetCreature),
																CreatureState::GetY(pTargetCreature),
																delayFrame);

					//------------------------------------------------------
					// Apply it too, if there is one.
					//------------------------------------------------------
					if (pActionResultNode!=NULL)
					{
						pResult->Add( pActionResultNode );
					}
				}
			}

			//------------------------------------------------------
			// Apply it straight to the sector.
			//------------------------------------------------------		
			int direction = pPacket->getDir();

			WorldEvents::SkillToSector(skillID, 
							pPacket->getX(),
							pPacket->getY(),
							direction, 
							delayFrame,
							pResult);
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Mine Type is Wrong. type = %d", pPacket->getItemType());
		}
	}


	__END_CATCH
}
