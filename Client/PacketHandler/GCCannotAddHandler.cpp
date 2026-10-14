//////////////////////////////////////////////////////////////////////
//
// Filename    : GCCannotAddHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCCannotAdd.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCCannotAddHandler::execute ( GCCannotAdd * pPacket , Player * pPlayer )

{
	__BEGIN_TRY

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
	(void)pPacket;
	(void)pPlayer;
		


	//-----------------------------------------------------------------
	// By what the item waiting on the server was for.
	//-----------------------------------------------------------------

	//-----------------------------------------------------------------
	// Pickup to Inventory
	const PlayerState::ITEM_CHECK_BUFFER status = PlayerState::GetItemCheckBufferStatus();

	//-----------------------------------------------------------------
	if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY)
	{
		DEBUG_ADD("Can't Drop Item to Inventory");
		
		// It cannot go into the inventory;
		// only the buffer is cleared.
		PlayerState::ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// Pickup to Mouse
	//-----------------------------------------------------------------
	else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE)
	{
		DEBUG_ADD("Can't Pickup Item to Mouse");
		
		// only the buffer is cleared.
		PlayerState::ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// Drop to Zone
	//-----------------------------------------------------------------
	else if (status == PlayerState::ITEM_CHECK_BUFFER_DROP_TO_ZONE)
	{
		DEBUG_ADD("Can't Drop Item to Zone");
		
		// The item goes back on the mouse.
		MItem* pItem = PlayerState::GetItemCheckBuffer();

		DialogPresentation::PickUpItem( pItem );

		// Clear the buffer.		
		PlayerState::ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// The server says the stack cannot be split in the inventory.
	//-----------------------------------------------------------------
	else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY)
	{
		DEBUG_ADD("Can't PIckupSome From Inventory");

		PlayerState::ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// Drop to RelicTable
	//-----------------------------------------------------------------
	else if (status == PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE)
	{
		DEBUG_ADD("Can't Drop to RelicTable");

		PlayerState::ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// Drop to Creature
	//-----------------------------------------------------------------
	else if (status == PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE)
	{
		DEBUG_ADD("Can't Drop to Creature");

		PlayerState::ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// Anything else.
	//-----------------------------------------------------------------
	else
	{
		// No check buffer covers this rejection. CGAddMouseToGear is the
		// usual source: the client has already applied the equip locally
		// and nothing here reverts it.
		DEBUG_ADD("[GCCannotAdd] rejection with no item check buffer - a gear equip refused by the server is not reverted locally");

		PlayerState::ClearItemCheckBuffer();		
	}



	__END_CATCH
}
