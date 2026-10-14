//--------------------------------------------------------------------------------
//
// Filename    : GCAddVampireCorpseHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddVampireCorpse.h"
#include "CreatureTypeSelection.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddVampireCorpseHandler::execute ( GCAddVampireCorpse * pPacket , Player * pPlayer )

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
		const PCVampireInfo3 & vi = pPacket->getVampireInfo();

		//----------------------------------------	
		// Is the creature already there?
		//----------------------------------------	
		MCreature* pCreature = ZoneState::GetCreatureOnly( vi.getObjectID() );
		
		//---------------------------------------------------------
		//
		//					Not in the zone
		//
		//---------------------------------------------------------
		if (pCreature==NULL)
		{
			// Is there a corpse already?
			MItem* pItem = ZoneState::GetItem( vi.getObjectID() );

			//---------------------------------------------------------
			//
			// Make a new corpse
			//
			//---------------------------------------------------------
			if (pItem==NULL)
			{
				//Color_t getHairColor () const throw () { return m_Colors[VAMPIRE_COLOR_HAIR]; }
				//Color_t getSkinColor () const throw () { return m_Colors[VAMPIRE_COLOR_SKIN]; }
				//Color_t getCoatColor ( ColorType colorType = MAIN_COLOR ) const throw () { return m_Colors[VAMPIRE_COLOR_COAT1 + (int)colorType]; }

				//----------------------------------------	
				//
				// Make the dead creature.
				//
				//----------------------------------------	
				DEBUG_ADD("New VampireCorpse");
				
				MCreature*	pDeadCreature = ZoneState::NewCreatureWear();
				if (pDeadCreature == NULL)
				{
					return;
				}
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
				}
				
				// creatureType: 2 for a male vampire, 3 for a female one
				//--------------------------------------------------
				// Set the CreatureType
				//--------------------------------------------------
				Shape_t shape	= vi.getShape();
				bool bMale		= vi.getSex()==MALE;
				ItemType_t coatType = vi.getCoatType();

				if (vi.getCompetence()==0)
				{
					CreatureState::SetCompetence( pDeadCreature, 0 );
					CreatureState::SetCreatureType( pDeadCreature, CREATURETYPE_VAMPIRE_OPERATOR );
					CreatureState::SetMale( pDeadCreature, bMale );
				}
				else
				{
					CreatureState::SetCreatureType( pDeadCreature, GetVampireCreatureType( shape, bMale, coatType ) );
				}

				CreatureState::SetMale( pDeadCreature, bMale );
				
				// it means little, but
				if (CreatureState::GetCreatureType( pDeadCreature )==CREATURETYPE_BAT)
				{
					CreatureState::SetFlyingCreature( pDeadCreature );
				}
				else
				{
					CreatureState::SetGroundCreature( pDeadCreature );
				}

				CreatureState::SetID( pDeadCreature, vi.getObjectID() );
				CreatureState::SetPosition( pDeadCreature, vi.getX(), vi.getY() );
				CreatureState::SetServerPosition( pDeadCreature, vi.getX(), vi.getY() );
				CreatureState::SetDirection( pDeadCreature, vi.getDir() );
				CreatureState::SetCurrentDirection( pDeadCreature, vi.getDir() );

				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
					pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
				}
				
				// Turn it into a corpse.
				CreatureState::SetCorpse( pDeadCreature );
				CreatureState::SetName( pDeadCreature, vi.getName().c_str() );

				// for now
				CreatureState::SetGuildNumber( pDeadCreature, vi.getGuildID() );

				CreatureState::SetWeaponSpeed( pDeadCreature, vi.getAttackSpeed() );
				if (MStatus* pStatus = CreatureState::Status( pDeadCreature ))
				{
					pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
				}

				// colours
				CreatureState::SetBodyColor1( pDeadCreature, vi.getSkinColor() );
				CreatureState::SetBodyColor2( pDeadCreature, vi.getCoatColor() );
				

				//--------------------------------------------------
				// [ TEST CODE ]
				//--------------------------------------------------
				// set the coat's colour
				//--------------------------------------------------
				/*
				if (pDeadCreature->IsMale())
				{
					CreatureState::SetBodyColor2( pDeadCreature, 91 );
				}
				else
				{
					CreatureState::SetBodyColor2( pDeadCreature, 38 );
				}
				*/

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
				pCorpse->SetID( vi.getObjectID() );
				pCorpse->SetItemType( 0 );
				pCorpse->SetPosition( vi.getX(), vi.getY() );		

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
					TYPE_OBJECTID oldItemID = ZoneState::GetItemID( vi.getX(), vi.getY() );

					DEBUG_ADD_FORMAT("Remove old Item id=%d", oldItemID);
						
					if (ZoneState::RemoveItem( oldItemID ))
					{
						// Add it again.
						if (!ZoneState::AddItem( pCorpse, false ))
						{
							DEBUG_ADD_FORMAT_ERR("[Error] Can't add Corpse to Zone, too. id=%d, xy=(%d, %d)", vi.getObjectID(), vi.getX(), vi.getY());
							
							delete pCorpse;
						}
					}
					else
					{
						// When the item there cannot be removed
						DEBUG_ADD_FORMAT_ERR("[Error] Can't remove old Item. id=%d, xy=(%d, %d)", oldItemID, vi.getX(), vi.getY());
						
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
			const PCVampireInfo3 & vi = pPacket->getVampireInfo();

			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ADVANCEMENT_CLASS_LEVEL, vi.getAdvancementLevel() );
			}

			Shape_t shape	= vi.getShape();
			bool bMale		= vi.getSex()==MALE;
			ItemType_t coatType = vi.getCoatType();

			int creatureType = GetVampireCreatureType( shape, bMale, coatType );

			if (CreatureState::GetCreatureType( pCreature )!=creatureType)
			{
				if (vi.getCompetence()==0)
				{
					CreatureState::SetCompetence( pCreature, 0 );
					CreatureState::SetCreatureType( pCreature, CREATURETYPE_VAMPIRE_OPERATOR );
					CreatureState::SetMale( pCreature, bMale );
				}
				else
				{
					CreatureState::SetCreatureType( pCreature, creatureType);
				}

				CreatureState::SetGroundCreature( pCreature );
			}

			CreatureState::MovePosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetServerPosition( pCreature, vi.getX(), vi.getY() );
			CreatureState::SetDirection( pCreature, vi.getDir() );
			CreatureState::SetCurrentDirection( pCreature, vi.getDir() );

		//	pCreature->SetStatus( MODIFY_MAX_HP, vi.getMaxHP() );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_CURRENT_HP, 0 );
			}

			CreatureState::SetWeaponSpeed( pCreature, vi.getAttackSpeed() );
			if (MStatus* pStatus = CreatureState::Status( pCreature ))
			{
				pStatus->SetStatus( MODIFY_ALIGNMENT, vi.getAlignment() );
			}

			// for now
			CreatureState::SetGuildNumber( pCreature, vi.getGuildID() );

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
