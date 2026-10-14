//////////////////////////////////////////////////////////////////////
//
// Filename    : GCDropItemToZoneHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCDropItemToZone.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "UserInformation.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCDropItemToZoneHandler::execute ( GCDropItemToZone * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

		DEBUG_ADD_FORMAT("[Drop Item] class=%d, type=%d, optSize=%d, id=%d, xy=(%d,%d)",
											(int)pPacket->getItemClass(), 
											(int)pPacket->getItemType(), 
											(int)pPacket->getOptionTypeSize(), 
											(int)pPacket->getObjectID(), 
											(int)pPacket->getX(), 
											(int)pPacket->getY());
		

		// dropping = TRUE
		WorldEvents::AddItemToZone( (GCAddItemToZone*) pPacket, true );		

		TYPE_OBJECTID id = pPacket->getDropPetOID();
		if(id != 0)	// something was dropped -_-
		{
			MCreature *pCreature = ZoneState::GetCreature(id);
			if(pCreature != NULL)
			{
				TYPE_OBJECTID petID = CreatureState::GetPetID(pCreature);

				if(petID != OBJECTID_NULL)
				{
					MCreature *pPet = ZoneState::GetFakeCreature(petID);

					if(pPet != NULL)
					{
						CreatureState::SetAction(pPet, ACTION_DRAINED);		// a pet drained is a pet barking.

						MPetItem *pPetItem = CreatureState::GetPetItem(pPet);
						if(pPetItem != NULL)
						{
							if(pPetItem->GetItemType()<3) // not a second-generation pet
							{
								if(pPetItem->GetNumber() >= 30)
								{
									WorldEvents::PlaySoundAt(SOUND_WORLD_DOG_GRRR, false, pPacket->getX(), pPacket->getY());
								}
								else
								{
									WorldEvents::PlaySoundAt(SOUND_WORLD_DOG_BARK, false, pPacket->getX(), pPacket->getY());
								}
							}
							else if(pPetItem->GetItemType() == 3) // a slayer's pet
							{	// the slayer's pet (a tank) sounds when looting.
								TYPE_SOUNDID soundID = (*g_pCreatureTable)[CreatureState::GetCreatureType(pPet)].GetActionSound( ACTION_DRAINED );
								if (soundID!=SOUNDID_NULL  )
								{
									WorldEvents::PlaySoundAt( soundID,
												false,
												CreatureState::GetX(pPet), CreatureState::GetY(pPet));
								}

							}
						}
					}
				}
			}
		}

	__END_CATCH
}
