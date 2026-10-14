//--------------------------------------------------------------------------------
//
// Filename    : GCAddMonsterCorpseHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddMonsterCorpse.h"
#include "DebugLog.h"
#include "GameInfoPresentationHost.h"
#include "MCreatureTable.h"
#include "MHelpDef.h"
#include "MItem.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddMonsterCorpseHandler::execute ( GCAddMonsterCorpse * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	

	// Make the creature, put it in a corpse, and add that to the zone.
	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// Normal..
	//------------------------------------------------------
	else
	{	
		//----------------------------------------	
		// Is the creature already there?
		//----------------------------------------	
		MCreature* pCreature = ZoneState::GetCreatureOnly( pPacket->getObjectID() );
		
		static bool IsIKilled = false;
		
		// 2005, 1, 18 , sobeit add start - the corpses of monsters shaped as slayers, vampires and ousters are ignored (vampires would not matter.. anyway..)
		// it only does not crash when gcaddslayercorps or gcaddousterscorps comes... anyway... ignored for one reason or another..
		int MonsterType = pPacket->getMonsterType();
		if(MonsterType == 735 || MonsterType == 736 ||MonsterType == 737 || (MonsterType>= 792 && MonsterType<=800))
			return ;
		// 2005, 1, 18 , sobeit add end
		//---------------------------------------------------------
		//
		//					Not in the zone
		//
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			// Is there a corpse already?
			MItem* pItem = ZoneState::GetItem( pPacket->getObjectID() );

			//---------------------------------------------------------
			//
			// Make a new corpse
			//
			//---------------------------------------------------------
			if (pItem==NULL)
			{
				DEBUG_ADD("New MonsterCorpse");
				
				//----------------------------------------	
				//
				// Make the dead creature.
				//
				//----------------------------------------	
				MCreature*	pCreature = ZoneState::NewCreature();
				if (pCreature == NULL)
				{
					return;
				}
				
				CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType() );
				CreatureState::SetGroundCreature( pCreature );
				CreatureState::SetID( pCreature, pPacket->getObjectID() );
				CreatureState::SetPosition( pCreature, pPacket->getX(), pPacket->getY() );
				CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
				CreatureState::SetDirection( pCreature, pPacket->getDir() );
				CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );
				
				if (MStatus* pStatus = CreatureState::Status( pCreature ))
				{
					pStatus->SetStatus( MODIFY_MAX_HP, 100 );
					pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
				}

				// Turn it into a corpse.
				CreatureState::SetCorpse( pCreature );
				//pCreature->SetName( (*g_pCreatureTable)[pPacket->getMonsterType()].Name.GetString() );
				CreatureState::SetName( pCreature, pPacket->getMonsterName().c_str() );
				CreatureState::SetDrainCreatureID( pCreature, pPacket->getLastKiller() );

				// 2004, 5, 7 sobeit add start - a monster I killed - the blood-drain help
				if(PlayerState::Creature() != NULL && pPacket->getLastKiller() == PlayerState::GetID() && !IsIKilled)
				{
					GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_KILL);
					IsIKilled = true;
				}
				// 2004, 5, 7 sobeit add end

				// for now..
				CreatureState::SetGuildNumber( pCreature, 1 );

				
				// with no head -_-;
				if (!pPacket->gethasHead())
				{
					CreatureState::RemoveHead( pCreature );
				}

				//----------------------------------------	
				//
				// Make the corpse item.
				//
				//----------------------------------------	
				// The corpse owns the creature from here on.
				MItem* pCorpse = ZoneState::NewCorpse( pCreature );
				if (pCorpse == NULL)
				{
					ZoneState::DeleteCreature( pCreature );
					return;
				}
				pCorpse->SetID( pPacket->getObjectID() );
				pCorpse->SetItemType( 0 );
				pCorpse->SetPosition( pPacket->getX(), pPacket->getY() );		

				//---------------------------------------------------------
				// the number of items in the corpse
				//---------------------------------------------------------
				pCorpse->SetNumber( pPacket->getTreasureCount() );


				//----------------------------------------
				// Add the item to the zone
				//----------------------------------------
				if (ZoneState::AddItem( pCorpse, false ))
				{
					//------------------------------------------------------------
					// Load it if it is not loaded.
					//------------------------------------------------------------
//					LoadCreatureType( pPacket->getMonsterType() );			
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Can't add Corpse to Zone. id=%d, xy=(%d, %d)", pPacket->getObjectID(), pPacket->getX(), pPacket->getY());
					
					//---------------------------------------------------------
					// When it could not be added
					// Remove the item there and add it again.
					//---------------------------------------------------------
					TYPE_OBJECTID oldItemID = ZoneState::GetItemID( pPacket->getX(), pPacket->getY() );

					DEBUG_ADD_FORMAT("Remove old Item id=%d", oldItemID);
						
					if (ZoneState::RemoveItem( oldItemID ))
					{
						// Add it again.
						if (!ZoneState::AddItem( pCorpse, false ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't add Corpse to Zone, too. id=%d, xy=(%d, %d)", pPacket->getObjectID(), pPacket->getX(), pPacket->getY());
							
							delete pCorpse;
						}
					}
					else
					{
						// When the item there cannot be removed
						DEBUG_ADD_FORMAT_ERR("[Error] Can't remove old Item. id=%d, xy=(%d, %d)", oldItemID, pPacket->getX(), pPacket->getY());
						
						delete pCorpse;
					}								
				}
				
				// A Christmas tree takes an effect.
				if(CreatureState::GetCreatureType( pCreature ) == 482 || CreatureState::GetCreatureType( pCreature ) == 650)
				{
					if(CreatureState::GetCreatureType( pCreature ) == 482)
						CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_CLIENTONLY_XMAS_TREE, 0xFFFF );
					// The words in white, MCreature::SetChatString's default colour.
					CreatureState::SetChatString( pCreature, CreatureState::GetName( pCreature ), 0x00FFFFFF );
				}
			}
			//---------------------------------------------------------
			//
			// A corpse already there
			//
			//---------------------------------------------------------
			else
			{
				#ifdef	OUTPUT_DEBUG
					if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
					{
						// The corpse is already there
						DEBUG_ADD("[Collide] Already Exist the Corpse");
					}					
				#endif								

				//---------------------------------------------------------
				// the number of items in the corpse
				//---------------------------------------------------------
				pItem->SetNumber( pPacket->getTreasureCount() );
			}

		}
		//---------------------------------------------------------
		//
		//				Already in the zone
		//
		//---------------------------------------------------------		
		else
		{
			DEBUG_ADD("[Collide] exist Creature");

			if (CreatureState::GetCreatureType( pCreature )!=pPacket->getMonsterType())
			{
				CreatureState::SetCreatureType( pCreature, pPacket->getMonsterType());
				CreatureState::SetGroundCreature( pCreature );
			}

			CreatureState::MovePosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetServerPosition( pCreature, pPacket->getX(), pPacket->getY() );
			CreatureState::SetDirection( pCreature, pPacket->getDir() );
			CreatureState::SetCurrentDirection( pCreature, pPacket->getDir() );

			//pCreature->SetStatus( MODIFY_MAX_HP, 100 );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
			}

			//---------------------------------------------------------
			// The creature must die.
			//---------------------------------------------------------
			if (!CreatureState::IsDead( pCreature ))
			{
				CreatureState::SetStop( pCreature );
				CreatureState::SetDead( pCreature );
			}

			CreatureState::SetItemCount( pCreature, pPacket->getTreasureCount() );

			// for now..
			CreatureState::SetGuildNumber( pCreature, 1 );
			CreatureState::SetDrainCreatureID( pCreature, pPacket->getLastKiller() );

			// 2004, 5, 7 sobeit add start - a monster I killed - the blood-drain help
			if(PlayerState::Creature() != NULL && pPacket->getLastKiller() == PlayerState::GetID() && !IsIKilled)
			{
				GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_KILL);
				IsIKilled = true;
			}
			// 2004, 5, 7 sobeit add end
			// with no head -_-;
			if (!pPacket->gethasHead())
			{
				CreatureState::RemoveHead( pCreature );
			}
		}

	}

	// [Help] when a corpse appears
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_ITEM_APPEAR_CORPSE );	
//	__END_HELP_EVENT


	__END_CATCH
}
