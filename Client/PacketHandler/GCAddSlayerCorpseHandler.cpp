//--------------------------------------------------------------------------------
//
// Filename    : GCAddSlayerCorpseHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddSlayerCorpse.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddSlayerCorpseHandler::execute ( GCAddSlayerCorpse * pPacket , Player * pPlayer )

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
		const PCSlayerInfo3 & si = pPacket->getSlayerInfo();	
			
		//----------------------------------------	
		// Is the creature already there?
		//----------------------------------------	
		MCreature* pCreature = ZoneState::GetCreatureOnly( si.getObjectID() );
		
		//---------------------------------------------------------
		//
		//					Not in the zone
		//
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			// Is there a corpse already?
			MItem* pItem = ZoneState::GetItem( si.getObjectID() );

			//---------------------------------------------------------
			//
			// Make a new corpse
			//
			//---------------------------------------------------------
			if (pItem==NULL)
			{
				DEBUG_ADD("New SlayerCorpse");
				
				//----------------------------------------	
				//
				// Make the dead creature.
				//
				//----------------------------------------	
				MCreature*	pDeadCreature = ZoneState::NewCreatureWear();
				if (pDeadCreature == NULL)
				{
					return;
				}
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, si.getAdvancementLevel() );
				}
				
				//----------------------------------------	
				// Put the gear on.
				//----------------------------------------					
				if (si.getCompetence()==0)
				{
					CreatureState::SetCompetence( pDeadCreature, 0 );
					CreatureState::SetCreatureType( pDeadCreature, CREATURETYPE_SLAYER_OPERATOR );
					CreatureState::SetMale( pDeadCreature, si.getSex()==MALE );
				}
				else
				{
					CreatureState::SetCreatureType( pDeadCreature, (si.getSex()==MALE)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE );
				}

				CreatureState::SetGuildNumber( pDeadCreature, si.getGuildID() );

				// creatureType: 0 for a male slayer, 1 for a female one
				CreatureState::SetGroundCreature( pDeadCreature );
				CreatureState::SetID( pDeadCreature, si.getObjectID() );
				CreatureState::SetPosition( pDeadCreature, si.getX(), si.getY() );
				CreatureState::SetServerPosition( pDeadCreature, si.getX(), si.getY() );
				CreatureState::SetDirection( pDeadCreature, si.getDir() );
				CreatureState::SetCurrentDirection( pDeadCreature, si.getDir() );

				// skin colour
				CreatureState::SetBodyColor1( pDeadCreature, si.getSkinColor() );

				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_MAX_HP, si.getMaxHP() );
					pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
				}
				
				CreatureState::SetAddonToSlayer3( pDeadCreature, &si );

				// Turn it into a corpse.
				CreatureState::SetCorpse( pDeadCreature );
				CreatureState::SetName( pDeadCreature, si.getName().c_str() );

				//----------------------------------------	
				//
				// Make the corpse item.
				//
				//----------------------------------------	
				// The corpse owns the creature from here on.
				MItem* pCorpse = ZoneState::NewCorpse( pDeadCreature );
				if (pCorpse == NULL)
				{
					ZoneState::DeleteCreature( pDeadCreature );
					return;
				}
				pCorpse->SetID( si.getObjectID() );
				pCorpse->SetItemType( 0 );
				pCorpse->SetPosition( si.getX(), si.getY() );	
				
				pCorpse->SetNumber( pPacket->getTreasureCount() );

				//----------------------------------------
				// Add the item to the zone
				//----------------------------------------
				if (!ZoneState::AddItem( pCorpse, false ))
				{
					DEBUG_ADD_ERR("[Error] Can't Add to Zone.");
					
					//---------------------------------------------------------
					// When it could not be added
					// Remove the item there and add it again.
					//---------------------------------------------------------
					TYPE_OBJECTID oldItemID = ZoneState::GetItemID( si.getX(), si.getY() );

					DEBUG_ADD_FORMAT("Remove old Item id=%d", oldItemID);
						
					if (ZoneState::RemoveItem( oldItemID ))
					{
						// Add it again.
						if (!ZoneState::AddItem( pCorpse, false ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't add Corpse to Zone, too. id=%d, xy=(%d, %d)", si.getObjectID(), si.getX(), si.getY());
							
							delete pCorpse;
						}
					}
					else
					{
						// When the item there cannot be removed
						DEBUG_ADD_FORMAT_ERR("[Error] Can't remove old Item. id=%d, xy=(%d, %d)", oldItemID, si.getX(), si.getY());
						
						delete pCorpse;
					}	
				}				
			}
			//---------------------------------------------------------
			//
			// A corpse already there
			//
			//---------------------------------------------------------
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Already Exist Another Item : class=%d type=%d", (int)pItem->GetItemClass(), (int)pItem->GetItemType());
				
				if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
				{
					//---------------------------------------------------------					
					// The corpse is already there
					// the number of items in the corpse
					//---------------------------------------------------------
					pItem->SetNumber( pPacket->getTreasureCount() );
				}
				else
				{
					DEBUG_ADD("The Item is Not Corpse");
				}
			}
		}
		//---------------------------------------------------------
		//
		//				Already in the zone
		//
		//---------------------------------------------------------		
		else
		{
			DEBUG_ADD_FORMAT("Already Exist Creature. Set Dead id=%d", si.getObjectID());

			const PCSlayerInfo3 & si = pPacket->getSlayerInfo();

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, si.getAdvancementLevel() );
			}

			int creatureType = ((si.getSex()==MALE)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE);

			if (CreatureState::GetCreatureType( pCreature )!=creatureType)
			{
				if (si.getCompetence()==0)
				{
					CreatureState::SetCompetence( pCreature, 0 );
					CreatureState::SetCreatureType( pCreature, CREATURETYPE_SLAYER_OPERATOR );
					CreatureState::SetMale( pCreature, si.getSex()==MALE );
				}
				else
				{
					CreatureState::SetCreatureType( pCreature, creatureType);
				}

				CreatureState::SetGroundCreature( pCreature );
			}

			CreatureState::SetGuildNumber( pCreature, si.getGuildID() );
			
			CreatureState::MovePosition( pCreature, si.getX(), si.getY() );
			CreatureState::SetServerPosition( pCreature, si.getX(), si.getY() );
			CreatureState::SetDirection( pCreature, si.getDir() );
			CreatureState::SetCurrentDirection( pCreature, si.getDir() );
			
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
		}
	}


	__END_CATCH
}
