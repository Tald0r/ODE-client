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
#include "ClientDef.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MGameStringTable.h"
#include "MItemOptionTable.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRemoveFromGearHandler::execute ( GCRemoveFromGear * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	

	int slotID = pPacket->getSlotID();

	//----------------------------------------------------------------------
	// Slayer인 경우
	//----------------------------------------------------------------------
	switch(g_pPlayer->GetRace())
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
			// game message 출력
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
				ADDON_HELM,			//GEAR_SLAYER_HELM = 0,			// 모자
					ADDON_NULL,			//GEAR_SLAYER_NECKLACE,			// 목걸이
					ADDON_COAT,			//GEAR_SLAYER_COAT,				// 상의
					ADDON_LEFTHAND,		//GEAR_SLAYER_LEFTHAND,			// 왼손
					ADDON_RIGHTHAND,	//GEAR_SLAYER_RIGHTHAND,			// 오른손
					ADDON_NULL,			//GEAR_SLAYER_GLOVE,				// 장갑
					ADDON_NULL,			//GEAR_SLAYER_BELT,				// 혁대
					ADDON_TROUSER,		//GEAR_SLAYER_TROUSER,			// 하의			
					ADDON_NULL,			//GEAR_SLAYER_BRACELET1,			// 팔찌1
					ADDON_NULL,			//GEAR_SLAYER_BRACELET2,			// 팔찌2
					ADDON_NULL,			//GEAR_SLAYER_RING1,				// 반지1
					ADDON_NULL,			//GEAR_SLAYER_RING2,				// 반지2
					ADDON_NULL,			//GEAR_SLAYER_RING3,				// 반지3
					ADDON_NULL,			//GEAR_SLAYER_RING4,				// 반지4
					ADDON_NULL,			//GEAR_SLAYER_SHOES,				// 신발
			};
		
			// RemoveItem has already cleared both aliases of a two-hand item.
			// Use the removed item to select its right-hand visual addon.
			if (slotID == MSlayerGear::GEAR_SLAYER_LEFTHAND && pRemovedItem->IsGearSlotTwoHand())
				slotID = MSlayerGear::GEAR_SLAYER_RIGHTHAND;

			//----------------------------------------------------------
			// 복장을 바꿔준다.
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
				g_pPlayer->SetStop();

				#ifdef	OUTPUT_DEBUG				
					if (g_pPlayer->RemoveAddon( addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						const MCreatureWear::ADDON_INFO& addonInfo = g_pPlayer->GetAddonInfo( addonSlotID );
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d, AddonFrameID=%d", addonSlotID, addonInfo.FrameID);
					}				
				#else
					g_pPlayer->RemoveAddon( addonSlotID );
				#endif
			}

			// item정보 제거
			UI_RemoveDescriptor( (void*)pRemovedItem );

			//----------------------------------------------------------
			// item을 없앤다.
			//----------------------------------------------------------
			delete pRemovedItem;
		}
	}
	break;

	case RACE_VAMPIRE:
	//----------------------------------------------------------------------
	// Vampire인 경우
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
			// game message 출력
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
			// 복장을 바꿔준다.
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
				g_pPlayer->SetStop();

				#ifdef	OUTPUT_DEBUG				
					if (g_pPlayer->RemoveAddon( addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						const MCreatureWear::ADDON_INFO& addonInfo = g_pPlayer->GetAddonInfo( addonSlotID );
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d, AddonFrameID=%d", addonSlotID, addonInfo.FrameID);
					}				
				#else
					g_pPlayer->RemoveAddon( addonSlotID );
				#endif
			}

			// item정보 제거
			UI_RemoveDescriptor( (void*)pRemovedItem );

			//----------------------------------------------------------
			// item을 없앤다.
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
			// game message 출력
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
				ADDON_NULL,			//GEAR_OUSTERS_CIRCLET,			// 서클릿
				ADDON_COAT,			//GEAR_OUSTERS_COAT,				// 옷
				ADDON_LEFTHAND,		//GEAR_OUSTERS_LEFTHAND,			// 왼손
				ADDON_RIGHTHAND,	//GEAR_OUSTERS_RIGHTHAND,			// 오른손
				ADDON_TROUSER,			//GEAR_OUSTERS_BOOTS,				// 신발
				ADDON_NULL,			//GEAR_OUSTERS_ARMSBAND1,			// 암스밴드1
				ADDON_NULL,			//GEAR_OUSTERS_ARMSBAND2,			// 암스밴드2
				ADDON_NULL,			//GEAR_OUSTERS_RING1,				// 링1
				ADDON_NULL,			//GEAR_OUSTERS_RING2,				// 링2
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE1,			// 목걸이1
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE2,			// 목걸이2
				ADDON_NULL,			//GEAR_OUSTERS_NECKLACE3,			// 목걸이3
				ADDON_NULL,			//GEAR_OUSTERS_STONE1,			// 정령석1
				ADDON_NULL,			//GEAR_OUSTERS_STONE2,			// 정령석2
				ADDON_NULL,			//GEAR_OUSTERS_STONE3,			// 정령석3
				ADDON_NULL,			//GEAR_OUSTERS_STONE4,			// 정령석4
			};

			// RemoveItem has already cleared both aliases of a two-hand item.
			// Use the removed item to select its right-hand visual addon.
			if (slotID == MOustersGear::GEAR_OUSTERS_LEFTHAND && pRemovedItem->IsGearSlotTwoHand())
				slotID = MOustersGear::GEAR_OUSTERS_RIGHTHAND;

			//----------------------------------------------------------
			// 복장을 바꿔준다.
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
				g_pPlayer->SetStop();

				#ifdef	OUTPUT_DEBUG				
					if (g_pPlayer->RemoveAddon( addonSlotID ))
					{
						DEBUG_ADD_FORMAT("[OK] RemoveAddon. Slot=%d", addonSlotID);
					}
					else
					{
						const MCreatureWear::ADDON_INFO& addonInfo = g_pPlayer->GetAddonInfo( addonSlotID );
						DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. Slot=%d, AddonFrameID=%d", addonSlotID, addonInfo.FrameID);
					}				
				#else
					g_pPlayer->RemoveAddon( addonSlotID );
				#endif
			}

			// item정보 제거
			UI_RemoveDescriptor( (void*)pRemovedItem );

			//----------------------------------------------------------
			// item을 없앤다.
			//----------------------------------------------------------
			delete pRemovedItem;
		}
	}
	break;
	default:
		break;
	}

//	// [도움말] 아이템이 부서진 경우
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_ITEM_BROKEN );
//	__END_HELP_EVENT


	__END_CATCH
}
