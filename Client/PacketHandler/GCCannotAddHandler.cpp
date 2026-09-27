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
#include "ClientDef.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCCannotAddHandler::execute ( GCCannotAdd * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		


	//-----------------------------------------------------------------
	// 처리할려는 item buffer의 상태에 따라서..
	//-----------------------------------------------------------------

	//-----------------------------------------------------------------
	// Pickup to Inventory
	//-----------------------------------------------------------------
	if (g_pPlayer->IsItemCheckBufferPickupToInventory())
	{
		DEBUG_ADD("Can't Drop Item to Inventory");
		
		// inventory에 못 넣는 경우..
		// buffer만 제거시키면 된다.
		g_pPlayer->ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// Pickup to Mouse
	//-----------------------------------------------------------------
	else if (g_pPlayer->IsItemCheckBufferPickupToMouse())
	{
		DEBUG_ADD("Can't Pickup Item to Mouse");
		
		// buffer만 제거시키면 된다.
		g_pPlayer->ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// Drop to Zone
	//-----------------------------------------------------------------
	else if (g_pPlayer->IsItemCheckBufferDropToZone())
	{
		DEBUG_ADD("Can't Drop Item to Zone");
		
		// 다시 item을 mouse에 붙인다.
		MItem* pItem = g_pPlayer->GetItemCheckBuffer();

		UI_PickUpItem( pItem );

		// buffer 제거		
		g_pPlayer->ClearItemCheckBuffer();
	}
	//-----------------------------------------------------------------
	// inventory에서 아이템을 못 분리한다고 검증.
	//-----------------------------------------------------------------
	else if (g_pPlayer->IsItemCheckBufferPickupSomeFromInventory())
	{
		DEBUG_ADD("Can't PIckupSome From Inventory");

		g_pPlayer->ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// Drop to RelicTable
	//-----------------------------------------------------------------
	else if (g_pPlayer->IsItemCheckBufferDropToRelicTable())
	{
		DEBUG_ADD("Can't Drop to RelicTable");

		g_pPlayer->ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// Drop to Creature
	//-----------------------------------------------------------------
	else if (g_pPlayer->IsItemCheckBufferDropToCreature())
	{
		DEBUG_ADD("Can't Drop to Creature");

		g_pPlayer->ClearItemCheckBuffer();		
	}
	//-----------------------------------------------------------------
	// 다른 경우..
	//-----------------------------------------------------------------
	else
	{
		// No check buffer covers this rejection. CGAddMouseToGear is the
		// usual source: the client has already applied the equip locally
		// and nothing here reverts it.
		DEBUG_ADD("[GCCannotAdd] rejection with no item check buffer - a gear equip refused by the server is not reverted locally");

		g_pPlayer->ClearItemCheckBuffer();		
	}



	__END_CATCH
}
