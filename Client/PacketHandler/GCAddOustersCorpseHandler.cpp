//--------------------------------------------------------------------------------
//
// Filename    : GCAddOustersCorpseHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddOustersCorpse.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MStatus.h"
#include "ZoneStateHost.h"


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddOustersCorpseHandler::execute ( GCAddOustersCorpse * pPacket , Player * pPlayer )

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
		DEBUG_ADD_ERR("[Error] GCAddOusterCorpseHandler. The zone is NULL.");
	}
	//------------------------------------------------------
	// Normal..
	//------------------------------------------------------
	else
	{	
		const PCOustersInfo3 & oi = pPacket->getOustersInfo();
		
		//----------------------------------------	
		// Is the creature already there?
		//----------------------------------------	
		MCreature* pCreature = ZoneState::GetCreatureOnly( oi.getObjectID() );
		
		//---------------------------------------------------------
		//					Not in the zone
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			// Is there a corpse already?
			MItem* pItem = ZoneState::GetItem( oi.getObjectID() );
			
			//---------------------------------------------------------
			// Make a new corpse
			//---------------------------------------------------------
			if (pItem==NULL)
			{
				//----------------------------------------	
				// Make the dead creature.
				//----------------------------------------	
				DEBUG_ADD("[GCAddOustersCorpseHandler] New OustersCorpse");				
				MCreature*	pDeadCreature = ZoneState::NewCreatureWear();
				if (pDeadCreature == NULL)
				{
					return;
				}
				//add by zdj 2005.6.24
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, oi.getAdvancementLevel() );
				}

				//pCreature->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, oi.getAdvancementLevel() );
				
				ItemType_t		coatType	= oi.getCoatType();

				if (oi.getCompetence()==0)
				{
					CreatureState::SetCompetence( pDeadCreature, 0 );
					CreatureState::SetCreatureType( pDeadCreature, CREATURETYPE_OUSTERS_OPERATOR );
				}
				else
				{
					CreatureState::SetCreatureType( pDeadCreature, GetOustersCreatureType( coatType ));
				}
				
				CreatureState::SetID( pDeadCreature, oi.getObjectID() );
				CreatureState::SetPosition( pDeadCreature, oi.getX(), oi.getY() );
				CreatureState::SetServerPosition( pDeadCreature, oi.getX(), oi.getY() );
				CreatureState::SetDirection( pDeadCreature, oi.getDir() );
				CreatureState::SetCurrentDirection( pDeadCreature, oi.getDir() );
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_MAX_HP, oi.getMaxHP() );
					pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
				}
				// Turn it into a corpse.
				CreatureState::SetCorpse( pDeadCreature );
				CreatureState::SetName( pDeadCreature, oi.getName().c_str() );
				// for now
				CreatureState::SetGuildNumber( pDeadCreature, oi.getGuildID() );
				
				CreatureState::SetWeaponSpeed( pDeadCreature, oi.getAttackSpeed() );
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_ALIGNMENT, oi.getAlignment() );
				}
				// colours
				CreatureState::SetBodyColor1( pDeadCreature, oi.getHairColor() );
				CreatureState::SetBodyColor2( pDeadCreature, oi.getCoatColor() );

				CreatureState::SetAddonToOusters3( pDeadCreature, &oi );
				
				//----------------------------------------	
				// Make the corpse item.
				//----------------------------------------	
				// The corpse owns the creature from here on.
				MItem* pCorpse = ZoneState::NewCorpse( pDeadCreature );
				if (pCorpse == NULL)
				{
					ZoneState::DeleteCreature( pDeadCreature );
					return;
				}
				
				pCorpse->SetID( oi.getObjectID() );
				pCorpse->SetItemType( 0 );
				pCorpse->SetPosition( oi.getX(), oi.getY() );				
				pCorpse->SetNumber( pPacket->getTreasureCount() );
				
				//----------------------------------------
				// Add the item to the zone
				//----------------------------------------
				if (!ZoneState::AddItem( pCorpse, false ))
				{
					//---------------------------------------------------------
					// When it could not be added
					// Remove the item there and add it again.
					//---------------------------------------------------------
					TYPE_OBJECTID oldItemID = ZoneState::GetItemID( oi.getX(), oi.getY() );
					
					DEBUG_ADD_FORMAT("Remove old Item id=%d", oldItemID);
					
					if (ZoneState::RemoveItem( oldItemID ))
					{
						// Add it again.
						if (!ZoneState::AddItem( pCorpse, false ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't add Corpse to Zone, too. id=%d, xy=(%d, %d)", oi.getObjectID(), oi.getX(), oi.getY());
							
							delete pCorpse;
						}
					}
					else
					{
						// When the item there cannot be removed
						DEBUG_ADD_FORMAT_ERR("[Error] Can't remove old Item. id=%d, xy=(%d, %d)", oldItemID, oi.getX(), oi.getY());
						
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
			const PCOustersInfo3 & oi = pPacket->getOustersInfo();
		
			ItemType_t coatType = oi.getCoatType();
			
			int creatureType = GetOustersCreatureType( coatType );
			
			if (CreatureState::GetCreatureType( pCreature )!=creatureType)
			{
				if (oi.getCompetence()==0)
				{
					CreatureState::SetCompetence( pCreature, 0 );
					CreatureState::SetCreatureType( pCreature, CREATURETYPE_OUSTERS_OPERATOR );
				}
				else
				{
					CreatureState::SetCreatureType( pCreature, creatureType);
				}
				
				CreatureState::SetGroundCreature( pCreature );
			}			
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, oi.getAdvancementLevel() );
			}
			CreatureState::MovePosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetServerPosition( pCreature, oi.getX(), oi.getY() );
			CreatureState::SetDirection( pCreature, oi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, oi.getDir() );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
			}
			
			CreatureState::SetWeaponSpeed( pCreature, oi.getAttackSpeed() );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ALIGNMENT, oi.getAlignment() );
			}

			CreatureState::SetGuildNumber( pCreature, oi.getGuildID() );
			
			//---------------------------------------------------------
			// The creature must die.
			//---------------------------------------------------------
			if (!CreatureState::IsDead( pCreature ))
			{
				CreatureState::SetStop( pCreature );
				CreatureState::SetDead( pCreature );
			}			
			CreatureState::SetItemCount( pCreature, pPacket->getTreasureCount() );
			// Only a creature that wears gear is dressed, as the old dynamic_cast
			// to MCreatureWear allowed.
			CreatureState::SetAddonToOusters3( pCreature, &oi );
		}
	}		
	__END_CATCH
}
