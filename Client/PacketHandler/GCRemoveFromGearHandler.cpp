//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRemoveFromGearHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRemoveFromGear.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MGameStringTable.h"
#include "MItemOptionTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRemoveFromGearHandler::execute ( GCRemoveFromGear * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		
	

	int slotID = pPacket->getSlotID();

	//----------------------------------------------------------------------
	// A slayer.
	//----------------------------------------------------------------------
	switch(PlayerState::GetRace())
	{
	case RACE_SLAYER:
	{
		MItem* pRemovedItem = g_pSlayerGear->RemoveItem( slotID );

		
		if (pRemovedItem==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No Removed Item in Slot=%d", slotID);
		}
		else
		{
			//----------------------------------------------------------
			// Say so in the game messages.
			//----------------------------------------------------------

			if(pRemovedItem->GetItemClass() != ITEM_CLASS_COUPLE_RING && pRemovedItem->GetItemClass() != ITEM_CLASS_VAMPIRE_COUPLE_RING)
			{
				char str[128]; 
				
				if (pRemovedItem->IsEmptyItemOptionList() )
				{
					snprintf(str, sizeof(str), "%s %s",					
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				else
				{
					std::string option_name;
					for(int i = 0; i < pRemovedItem->GetItemOptionListCount(); i++)
					{
						option_name += pRemovedItem->GetItemOptionName(i);
						if(i != pRemovedItem->GetItemOptionListCount())
							option_name += " ";
					}
					snprintf(str, sizeof(str), "%s%s %s",					
						option_name.c_str(),	
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				
				g_pGameMessage->Add(str);
			}
			
			
			
			int addonSlot[] = 
			{
				ADDON_HELM,			//GEAR_SLAYER_HELM = 0,			// hat
					ADDON_NULL,			//GEAR_SLAYER_NECKLACE,			// necklace
					ADDON_COAT,			//GEAR_SLAYER_COAT,				// top
					ADDON_LEFTHAND,		//GEAR_SLAYER_LEFTHAND,			// left hand
					ADDON_RIGHTHAND,	//GEAR_SLAYER_RIGHTHAND,			// right hand
					ADDON_NULL,			//GEAR_SLAYER_GLOVE,				// gloves
					ADDON_NULL,			//GEAR_SLAYER_BELT,				// belt
					ADDON_TROUSER,		//GEAR_SLAYER_TROUSER,			// trousers			
					ADDON_NULL,			//GEAR_SLAYER_BRACELET1,			// bracelet 1
					ADDON_NULL,			//GEAR_SLAYER_BRACELET2,			// bracelet 2
					ADDON_NULL,			//GEAR_SLAYER_RING1,				// ring 1
					ADDON_NULL,			//GEAR_SLAYER_RING2,				// ring 2
					ADDON_NULL,			//GEAR_SLAYER_RING3,				// ring 3
					ADDON_NULL,			//GEAR_SLAYER_RING4,				// ring 4
					ADDON_NULL,			//GEAR_SLAYER_SHOES,				// shoes
			};
		
			// RemoveItem has already cleared both aliases of a two-hand item.
			// Use the removed item to select its right-hand visual addon.
			if (slotID == MSlayerGear::GEAR_SLAYER_LEFTHAND && pRemovedItem->IsGearSlotTwoHand())
				slotID = MSlayerGear::GEAR_SLAYER_RIGHTHAND;

			//----------------------------------------------------------
			// Change the clothes drawn.
			//----------------------------------------------------------
			// addonSlot maps a gear slot to the visual addon it drives,
			// but slotID ranges over ALL of them: RemoveItem above
			// bounds it to m_Size, which is
			// MAX_GEAR_SLAYER (27), against fifteen entries here.
			// Unequipping a ZAP, a PDA, a shoulder or a blood bible
			// therefore read past a stack array and handed the result
			// to RemoveAddon. Those slots drive no addon, which is what
			// ADDON_NULL says, so that is the answer past the end.
			const int nAddonSlots = (int)(sizeof(addonSlot) / sizeof(addonSlot[0]));
			int addonSlotID = (slotID >= 0 && slotID < nAddonSlots)
					? addonSlot[slotID]
					: ADDON_NULL;

			if (addonSlotID != ADDON_NULL)
			{
				CreatureState::SetStop(pPlayerCreature);

				#ifdef	OUTPUT_DEBUG				
					if (CreatureState::RemoveAddon(pPlayerCreature, addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d", addonSlotID);
					}				
				#else
					CreatureState::RemoveAddon(pPlayerCreature, addonSlotID );
				#endif
			}

			// Drop the item's description.
			DialogPresentation::RemoveDescriptor( pRemovedItem );

			//----------------------------------------------------------
			// Free the item.
			//----------------------------------------------------------
			delete pRemovedItem;
		}
	}
	break;

	case RACE_VAMPIRE:
	//----------------------------------------------------------------------
	// A vampire.
	//----------------------------------------------------------------------
	{
		MItem* pRemovedItem = g_pVampireGear->RemoveItem( slotID );

		if (pRemovedItem==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No Removed Item in Slot=%d", slotID);
		}
		else
		{
			//----------------------------------------------------------
			// Say so in the game messages.
			//----------------------------------------------------------
			if(pRemovedItem->GetItemClass() != ITEM_CLASS_COUPLE_RING && pRemovedItem->GetItemClass() != ITEM_CLASS_VAMPIRE_COUPLE_RING)
			{
				char str[128]; 
				
				if (pRemovedItem->IsEmptyItemOptionList())
				{
					snprintf(str, sizeof(str), "%s %s",					
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				else
				{
					std::string option_name;
					for(int i = 0; i < pRemovedItem->GetItemOptionListCount(); i++)
					{
						option_name += pRemovedItem->GetItemOptionName(i);
						if(i != pRemovedItem->GetItemOptionListCount())
							option_name += " ";
					}
					snprintf(str, sizeof(str), "%s%s %s",					
						option_name.c_str(),	
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				
				g_pGameMessage->Add(str);
			}

			// RemoveItem has already cleared both aliases of a two-hand item.
			// Use the removed item to select its right-hand visual addon.
			if (slotID == MVampireGear::GEAR_VAMPIRE_LEFTHAND && pRemovedItem->IsGearSlotTwoHand())
				slotID = MVampireGear::GEAR_VAMPIRE_RIGHTHAND;

			//----------------------------------------------------------
			// Change the clothes drawn.
			//----------------------------------------------------------
			// Vampire coats and hands are the slots with visual addons.
			int addonSlotID = ADDON_NULL;
			switch (slotID)
			{
			case MVampireGear::GEAR_VAMPIRE_COAT: addonSlotID = ADDON_COAT; break;
			case MVampireGear::GEAR_VAMPIRE_LEFTHAND: addonSlotID = ADDON_LEFTHAND; break;
			case MVampireGear::GEAR_VAMPIRE_RIGHTHAND: addonSlotID = ADDON_RIGHTHAND; break;
			}

			if (addonSlotID != ADDON_NULL)
			{
				CreatureState::SetStop(pPlayerCreature);

				#ifdef	OUTPUT_DEBUG				
					if (CreatureState::RemoveAddon(pPlayerCreature, addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d", addonSlotID);
					}				
				#else
					CreatureState::RemoveAddon(pPlayerCreature, addonSlotID );
				#endif
			}

			// Drop the item's description.
			DialogPresentation::RemoveDescriptor( pRemovedItem );

			//----------------------------------------------------------
			// Free the item.
			//----------------------------------------------------------
			delete pRemovedItem;
		}
	}
	break;

	case RACE_OUSTERS:
	{
		MItem* pRemovedItem = g_pOustersGear->RemoveItem( slotID );

		if (pRemovedItem==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No Removed Item in Slot=%d", slotID);
		}
		else
		{
			//----------------------------------------------------------
			// Say so in the game messages.
			//----------------------------------------------------------
			if(1)
			{
				char str[128]; 
				
				if (pRemovedItem->IsEmptyItemOptionList())
				{
					snprintf(str, sizeof(str), "%s %s",					
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				else
				{
					std::string option_name;
					for(int i = 0; i < pRemovedItem->GetItemOptionListCount(); i++)
					{
						option_name += pRemovedItem->GetItemOptionName(i);
						if(i != pRemovedItem->GetItemOptionListCount())
							option_name += " ";
					}
					snprintf(str, sizeof(str), "%s%s %s",					
						option_name.c_str(),	
						pRemovedItem->GetName(),
						(*g_pGameStringTable)[STRING_MESSAGE_ITEM_BROKEN].GetString());
				}
				
				g_pGameMessage->Add(str);
			}

			int addonSlot[] = 
			{
				ADDON_NULL,			//GEAR_OUSTERS_CIRCLET,			// circlet
				ADDON_COAT,			//GEAR_OUSTERS_COAT,				// clothes
				ADDON_LEFTHAND,		//GEAR_OUSTERS_LEFTHAND,			// left hand
				ADDON_RIGHTHAND,	//GEAR_OUSTERS_RIGHTHAND,			// right hand
				ADDON_TROUSER,			//GEAR_OUSTERS_BOOTS,				// shoes
				ADDON_NULL,			//GEAR_OUSTERS_ARMSBAND1,			// armsband 1
				ADDON_NULL,			//GEAR_OUSTERS_ARMSBAND2,			// armsband 2
				ADDON_NULL,			//GEAR_OUSTERS_RING1,				// ring 1
				ADDON_NULL,			//GEAR_OUSTERS_RING2,				// ring 2
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE1,			// necklace 1
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE2,			// necklace 2
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE3,			// necklace 3
				ADDON_NULL,			//GEAR_OUSTERS_STONE1,			// spirit stone 1
				ADDON_NULL,			//GEAR_OUSTERS_STONE2,			// spirit stone 2
				ADDON_NULL,			//GEAR_OUSTERS_STONE3,			// spirit stone 3
				ADDON_NULL,			//GEAR_OUSTERS_STONE4,			// spirit stone 4
			};

			// RemoveItem has already cleared both aliases of a two-hand item.
			// Use the removed item to select its right-hand visual addon.
			if (slotID == MOustersGear::GEAR_OUSTERS_LEFTHAND && pRemovedItem->IsGearSlotTwoHand())
				slotID = MOustersGear::GEAR_OUSTERS_RIGHTHAND;

			//----------------------------------------------------------
			// Change the clothes drawn.
			//----------------------------------------------------------
			// addonSlot maps a gear slot to the visual addon it drives,
			// but slotID ranges over ALL of them: RemoveItem above
			// bounds it to m_Size, which is
			// MAX_GEAR_OUSTERS (28), against sixteen entries here.
			// Unequipping a ZAP, a PDA, a shoulder or a blood bible
			// therefore read past a stack array and handed the result
			// to RemoveAddon. Those slots drive no addon, which is what
			// ADDON_NULL says, so that is the answer past the end.
			const int nAddonSlots = (int)(sizeof(addonSlot) / sizeof(addonSlot[0]));
			int addonSlotID = (slotID >= 0 && slotID < nAddonSlots)
					? addonSlot[slotID]
					: ADDON_NULL;

			if (addonSlotID != ADDON_NULL)
			{
				CreatureState::SetStop(pPlayerCreature);

				#ifdef	OUTPUT_DEBUG				
					if (CreatureState::RemoveAddon(pPlayerCreature, addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d", addonSlotID);
					}				
				#else
					CreatureState::RemoveAddon(pPlayerCreature, addonSlotID );
				#endif
			}

			// Drop the item's description.
			DialogPresentation::RemoveDescriptor( pRemovedItem );

			//----------------------------------------------------------
			// Free the item.
			//----------------------------------------------------------
			delete pRemovedItem;
		}
	}
	break;
	default:
		break;
	}

//	// [Help] when an item breaks
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_ITEM_BROKEN );
//	__END_HELP_EVENT


	__END_CATCH
}
