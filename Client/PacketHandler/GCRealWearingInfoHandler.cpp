//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRealWearingInfoHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
// include files
#include "Gpackets/GCRealWearingInfo.h"
#include "BonusSkillHost.h"
#include "DebugLog.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRealWearingInfoHandler::execute ( GCRealWearingInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	DWORD flag = pPacket->getInfo();

	DEBUG_ADD_FORMAT("[GCRealWearingInfo] flag=0x%08X (bit n = gear slot n is effective)", (unsigned)flag);

	if (PlayerState::Creature()==NULL)
	{
		DEBUG_ADD_ERR("[Error] Player is NULL");
	}
	else
	{

		//------------------------------------------------------------
		// Slayer
		//------------------------------------------------------------
		switch(PlayerState::GetRace())
		{
		case RACE_SLAYER:
		{
			bool bResetSkills = false;

			MItem *pWeapon = g_pSlayerGear->GetItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND);
			
			if(pWeapon != NULL)
			{
				if(flag & 0x10)	// the weapon's values apply
				{
					if(pWeapon->IsAffectStatus() == false)
						bResetSkills = true;
				}
				else	// the weapon's values do not apply
				{
					if(pWeapon->IsAffectStatus() == true)
						bResetSkills = true;
				}
			}

			int num = (int)MSlayerGear::MAX_GEAR_SLAYER;
			DWORD bit = 1;

			//-------------------------------------------------------
			// For each slot, whether its item's values apply.
			//-------------------------------------------------------
			for (int i=0; i<num; i++)
			{
				const MItem* pItem = g_pSlayerGear->GetItem( (MSlayerGear::GEAR_SLAYER)i );
				
				if (pItem!=NULL)
				{
					// 2004, 10, 26, sobeit add start
					if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN)
						continue;
					// 2004, 10, 26, sobeit add end

					int itemID = pItem->GetID();

					MItem* pModifyItem = g_pSlayerGear->GetItemToModify( itemID );

					if (pModifyItem!=NULL)
					{
						//-------------------------------------------------------
						// The values apply
						//-------------------------------------------------------
						if (flag & bit)
						{
							pModifyItem->SetAffectStatus();
							PlayerState::SetAddonItem(pModifyItem);
						}
						//-------------------------------------------------------
						// The values do not apply
						//-------------------------------------------------------
						else
						{
							DEBUG_ADD_FORMAT("[GCRealWearingInfo] slayer slot %d item id=%d class=%d type=%d not effective - addon removed",
								i, itemID, (int)pModifyItem->GetItemClass(), (int)pModifyItem->GetItemType());
							pModifyItem->UnSetAffectStatus();
							PlayerState::RemoveAddonItem(pModifyItem);
						}
					}
				}

				bit <<= 1;
			}

			if(bResetSkills)
				BonusSkills::RefreshAvailableSkills();

		}
		break;

		case RACE_VAMPIRE:
		//------------------------------------------------------------
		// Vampire
		//------------------------------------------------------------
		{
			int num = (int)MVampireGear::MAX_GEAR_VAMPIRE;
			DWORD bit = 1;

			//-------------------------------------------------------
			// For each slot, whether its item's values apply.
			//-------------------------------------------------------
			for (int i=0; i<num; i++)
			{
				const MItem* pItem = g_pVampireGear->GetItem( (MVampireGear::GEAR_VAMPIRE)i );

				if (pItem!=NULL)
				{
					// 2004, 10, 26, sobeit add start
					if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN)
						continue;
					// 2004, 10, 26, sobeit add end
					int itemID = pItem->GetID();

					MItem* pModifyItem = g_pVampireGear->GetItemToModify( itemID );

					if (pModifyItem!=NULL)
					{
						//-------------------------------------------------------
						// The values apply
						//-------------------------------------------------------
						if (flag & bit)
						{
							pModifyItem->SetAffectStatus();
						}
						//-------------------------------------------------------
						// The values do not apply
						//-------------------------------------------------------
						else
						{
							pModifyItem->UnSetAffectStatus();
						}
					}
				}

				bit <<= 1;
			}
		}
		break;

		case RACE_OUSTERS:
			{
				int num = (int)MOustersGear::MAX_GEAR_OUSTERS;
				DWORD bit = 1;
				
				//-------------------------------------------------------
				// For each slot, whether its item's values apply.
				//-------------------------------------------------------
				for (int i=0; i<num; i++)
				{
					const MItem* pItem = g_pOustersGear->GetItem( (MOustersGear::GEAR_OUSTERS)i );
					
					if (pItem!=NULL)
					{
						// 2004, 10, 26, sobeit add start
						if(pItem->GetItemClass() == ITEM_CLASS_BLOOD_BIBLE_SIGN)
							continue;
						// 2004, 10, 26, sobeit add end
						int itemID = pItem->GetID();
						
						MItem* pModifyItem = g_pOustersGear->GetItemToModify( itemID );
						
						if (pModifyItem!=NULL)
						{
							//-------------------------------------------------------
							// The values apply
							//-------------------------------------------------------
							if (flag & bit)
							{
								pModifyItem->SetAffectStatus();
							}
							//-------------------------------------------------------
							// The values do not apply
							//-------------------------------------------------------
							else
							{
								pModifyItem->UnSetAffectStatus();
							}
						}
					}
					
					bit <<= 1;
				}
			}
			break;
		default:
			break;
		}

		PlayerState::CalculateStatus();
	}


	__END_CATCH
}
