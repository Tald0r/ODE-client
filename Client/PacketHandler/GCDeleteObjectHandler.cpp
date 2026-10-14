//--------------------------------------------------------------------------------
//
// Filename    : GCDeleteObjectHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCDeleteObject.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCDeleteObjectHandler::execute ( GCDeleteObject * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
	}	
	//------------------------------------------------------
	// Normal: some object is removed; which one is found
	// by trying.
	//------------------------------------------------------
	else
	{	
		TYPE_OBJECTID objectID = pPacket->getObjectID();

		if (PlayerState::GetItemCheckBufferStatus()==PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE)
		{
			//---------------------------------------------
			// the item being dropped
			//---------------------------------------------
			MItem* pItem = PlayerState::GetItemCheckBuffer();

			if(pItem->GetID() == objectID)DialogPresentation::DropItem();
			PlayerState::ClearItemCheckBuffer();
		}

		//------------------------------------------------------
		// A creature?
		//------------------------------------------------------
		if ( ZoneState::RemoveCreature( objectID ) )
		{
			DEBUG_ADD_FORMAT("Remove Creature OK. ID=%d", objectID);
		}
		//------------------------------------------------------
		// An item?
		//------------------------------------------------------
		else if ( ZoneState::RemoveItem( objectID ) )
		{			
			DEBUG_ADD_FORMAT("Remove Item OK. ID=%d", objectID);
		}
		//------------------------------------------------------
		// Neither
		//------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Can't Remove Object. ID=%d", objectID);
		}
	}


	__END_CATCH
}
