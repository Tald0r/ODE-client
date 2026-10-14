//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRemoveEffectHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRemoveEffect.h"
#include "DebugLog.h"
#include "WorldEventsHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MEffectStatusDef.h"
#include "MEffectStatusTable.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "SoundDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRemoveEffectHandler::execute ( GCRemoveEffect * pPacket , Player * pPlayer )

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
		int CreatureID = pPacket->getObjectID();
		
		MCreature* pCreature = ZoneState::GetCreature(CreatureID);

		// Not standing: the creature lying in the corpse with this ID (the
		// relic shelf is one).
		if (pCreature == NULL)
		{
			pCreature = ZoneState::GetCorpseCreature(CreatureID);
		}

		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature. id=%d", pPacket->getObjectID());
		}
		else
		{	
			int size = pPacket->getListNum();

			for (int i=0; i<size; i++)
			{
				WORD effectStatus = pPacket->popFrontListElement();

				switch( effectStatus )
				{
					case EFFECTSTATUS_KEEP_SWEEPER :                      // 304
					case EFFECTSTATUS_KEEP_SWEEPER_2 :                    // 305
					case EFFECTSTATUS_KEEP_SWEEPER_3 :                    // 306
					case EFFECTSTATUS_KEEP_SWEEPER_4 :                    // 307
					case EFFECTSTATUS_KEEP_SWEEPER_5 :                    // 308
					case EFFECTSTATUS_KEEP_SWEEPER_6 :                    // 309
					case EFFECTSTATUS_KEEP_SWEEPER_7 :                    // 310
					case EFFECTSTATUS_KEEP_SWEEPER_8 :                    // 311
					case EFFECTSTATUS_KEEP_SWEEPER_9 :                    // 312
					case EFFECTSTATUS_KEEP_SWEEPER_10 :                   // 313
					case EFFECTSTATUS_KEEP_SWEEPER_11 :                   // 314
					case EFFECTSTATUS_KEEP_SWEEPER_12 :                   // 315
						WorldEvents::PlaySound( SOUND_SWEEPER_END );
						break;
				}

				if (CreatureState::RemoveEffectStatus(pCreature, (EFFECTSTATUS)effectStatus ),0)
				{					
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Failed to Remove EffectStatus %d", effectStatus);
				}
				if( PlayerState::GetID() == CreatureState::GetID(pCreature) ) // the player itself
				{
					ACTIONINFO actionInfo = (ACTIONINFO) (*g_pEffectStatusTable)[effectStatus].OriginalActionInfo;
					switch(actionInfo)
					{
						//						case MAGIC_SACRIFICE :
					case MAGIC_AURA_SHIELD :
						if (auto* entry = g_pSkillInfoTable->GetMutable(actionInfo)) {
							entry->SetAvailableTime();
						}
						break;						
					default:
						break;
					}
				}

				if( effectStatus == EFFECTSTATUS_FADE_OUT) 
					CreatureState::SetVisibleSoon(pCreature);
				
//				if( effectStatus == EFFECTSTATUS_REDIANCE && pCreature->IsSlayer() )
//				{
//					MCreatureWear* pCreatureWear = dynamic_cast<MCreatureWear*>( pCreature );
////					_MinTrace("Basic ActionInfo : %d %s\n",pCreatureWear->GetID(), pCreatureWear->GetName() );
//					
//					const MCreatureWear::ADDON_INFO &addonInfo = pCreatureWear->GetAddonInfo( ADDON_RIGHTHAND );
//					
//					if( addonInfo.bAddon )
//					{
////						_MinTrace("Basic ActionInfo : %d %d %d\n",addonInfo.ItemClass,addonInfo.ItemType);
//						if( addonInfo.ItemClass < g_pItemTable->GetSize() &&
//							addonInfo.ItemType < g_pItemTable[addonInfo.ItemClass].GetSize() &&
//							(*g_pItemTable)[addonInfo.ItemClass][addonInfo.ItemType].UseActionInfo != ACTIONINFO_NULL )
//							pCreature->SetBasicActionInfo( (*g_pItemTable)[addonInfo.ItemClass][addonInfo.ItemType].UseActionInfo );
//						else
//							pCreature->SetBasicActionInfo( SKILL_ATTACK_MELEE );
//					} else
//						pCreature->SetBasicActionInfo( SKILL_ATTACK_MELEE );
//				}
			}
		}
	}


	__END_CATCH
}
