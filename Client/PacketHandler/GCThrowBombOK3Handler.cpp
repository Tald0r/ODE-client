//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowBombOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowBombOK3.h"
#include "DebugLog.h"
#include "ExplosiveActions.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowBombOK3Handler::execute ( GCThrowBombOK3 * pPacket , Player * pPlayer )

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


		// The creature uses something on a tile.
		if (pCreature != NULL)
		{			
			int skillID = GetBombActionInfo( pPacket->getItemType() );//pPacket->getSkillType();	

			if (skillID < 0)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] no match skillType. itemType=%d", pPacket->getItemType());
				return;
			}

			int useSkillID = skillID;

			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction())
			{
				useSkillID = CreatureState::GetBasicActionInfo(pCreature);

				if (static_cast<DWORD>(useSkillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", useSkillID);
					return;
				}
			}

			// Make the result.
			MActionResult* pResult = new MActionResult;
		
			// (meaningless)
			DWORD delayFrame = 16;//ConvertDurationToFrame( pPacket->getDuration() );

			int targetID = CreatureState::GetID(pCreature);
			if ((*g_pActionInfoTable)[skillID].GetActionResultID()==ACTIONRESULTNODE_ACTIONINFO)
			{
				targetID = OBJECTID_NULL;
			}

			pResult->Add( new MActionResultNodeActionInfo( 
											skillID,
											CreatureState::GetID(pCreature), 
											targetID,	//pCreature->GetID(), 
											pPacket->getX(),
											pPacket->getY(),
											delayFrame ) 
							);


			int size = pPacket->getCListNum();

			for (int i=0; i<size; i++)
			{
				//pResult->Add( new MActionResultNodeActionInfo( 
				//					pCreature->GetID(), 
				//					pPacket->popCListElement(), 
				//					pPacket->getSkillType(),
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

					switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
					{
						//------------------------------------------------------
						// Run another action.
						//------------------------------------------------------
						case ACTIONRESULTNODE_ACTIONINFO :
							pActionResultNode =  new MActionResultNodeActionInfo( 
																useSkillID,	//(*g_pActionInfoTable)[skillID].GetActionResultValue(),
																CreatureState::GetID(pCreature), 
																targetID, 
																CreatureState::GetX(pTargetCreature),
																CreatureState::GetY(pTargetCreature),
																delayFrame);
						break;

						//------------------------------------------------------
						// Burrow
						//------------------------------------------------------
						default :
							pActionResultNode = CreateActionResultNode(pTargetCreature, useSkillID);
					}

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
			// Face the cell.
			//------------------------------------------------------
			CreatureState::SetDirectionToPosition(pCreature, pPacket->getX(), pPacket->getY());
			
			//------------------------------------------------------
			// The range is used as the direction.
			//------------------------------------------------------
			CreatureState::SetDirection(pCreature, pPacket->getDir() );

			//------------------------------------------------------
			// Set how it acts.
			//------------------------------------------------------
			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToSector(pCreature, skillID, 
								pPacket->getX(), pPacket->getY(),
								pResult		// result
			);		

		
		}
	}



	__END_CATCH
}
