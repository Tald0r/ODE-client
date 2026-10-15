//---------------------------------------------------------------------------
// ClientFunction.cpp
//---------------------------------------------------------------------------
// The whisper target, whether the player stands somewhere safe, and the
// inventory effects (task 4.236: gamemodel's since then; the UI's
// translucent box went into SpriteLib's AlphaBox.cpp and VS_UI_Base.cpp,
// and the blood bible's gear effect into UiRuntime). Where an item made
// from another goes (task 4.237, GameMain's until then).
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "ClientFunction.h"
#include "SkillDef.h"
#include "InventoryEffectTarget.h"
#include "MInventory.h"
#include "MItem.h"
#include "MEffectGeneratorTable.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "UserInformation.h"
#include "PlayerStateHost.h"
#include "DebugLog.h"
#include <memory>
#include <utility>

//---------------------------------------------------------------------------
// Get Whisper ID
//---------------------------------------------------------------------------
const char*
GetWhisperID()
{
	return g_pUserInformation->WhisperID.GetString();
}

//---------------------------------------------------------------------------
// Is Player In Safe Position
//---------------------------------------------------------------------------
// 0 not, 1 safe for the player's race, 2 safe for everyone (0 without a
// player).
//---------------------------------------------------------------------------
BOOL
IsPlayerInSafePosition()
{
	return PlayerState::IsInSafeSector();
}

//---------------------------------------------------------------------------
// Add an inventory effect at the item's grid position.
//---------------------------------------------------------------------------
void
AddNewInventoryEffect(TYPE_OBJECTID id, TYPE_ACTIONINFO ai, DWORD delayFrame, DWORD value)
{
	DEBUG_ADD("AddNewInventoryEffect");
	if (ai >= g_pActionInfoTable->GetSize()) return;

	const MItem* item = g_pInventory->GetItemToModify(id);
	if (item == nullptr || !(*g_pActionInfoTable)[ai].IsTargetItem())
	{
		DEBUG_ADD("No such item or Not Target Item");
		return;
	}

	const int x = item->GetGridX();
	const int y = item->GetGridY();
	const auto phases = (*g_pActionInfoTable)[ai].GetSize();
	if (phases == 0) return;

	std::unique_ptr<MActionResultNode> action;
	switch (ai)
	{
		case MAGIC_ENCHANT_OPTION_PLUS:
			if (g_pInventory->GetItem(id) != nullptr)
				action = std::make_unique<MActionResultNodeChangeItemOptionInInventory>(id, value);
			break;

		case MAGIC_ENCHANT_OPTION_NULL:
		{
			MItem* inventoryItem = g_pInventory->GetItem(id);
			if (inventoryItem != nullptr)
			{
				if (inventoryItem->GetItemClass() != ITEM_CLASS_PET_ITEM)
					action = std::make_unique<MActionResultNodeChangeItemOptionInInventory>(id, value);
				else if (PlayerState::IsItemCheckBufferItemToItem())
				{
					MItem* mouseItem = PlayerState::GetItemCheckBuffer();
					PlayerState::ClearItemCheckBuffer();
					delete mouseItem;
				}
			}
			break;
		}
		case MAGIC_ENCHANT_REMOVE_ITEM:
			action = std::make_unique<MActionResultNodeRemoveItemInInventory>(id);
			break;
		case MAGIC_TRANS_ITEM_OK:
			action = std::make_unique<MActionResultNodeChangeItemGenderInInventory>(id);
			break;
	}

	// Generate consumes the target and can execute its result or delete it
	// immediately. Finish result construction before that ownership transfer.
	auto target = PrepareInventoryEffectTarget(static_cast<BYTE>(phases), x, y, id, delayFrame, std::move(action));
	g_pEffectGeneratorTable->Generate(x, y, 0, 0, 1, ai, target.release());

	DEBUG_ADD_FORMAT("[AddNewInventoryEffect] ai=%d, item id=%d", ai, id);
}

//-----------------------------------------------------------------------------
// GetMakeItemFitPosition
//-----------------------------------------------------------------------------
//
// Used when making holy water, bombs or mines from an item.
//
// Where the item made from pItem (itemClass, itemType) goes in the
// inventory, in fitPoint: only when it returns true; false means there is
// no room for it.
//-----------------------------------------------------------------------------
bool
GetMakeItemFitPosition(MItem* pItem, ITEM_CLASS itemClass, int itemType, POINT& fitPoint)
{
	bool bFindPos = false;	// is there room?

	// create a temporary item to find the position it would fit into.
	MItem* pResultItem = MItem::NewItem( itemClass );

	if (pResultItem == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] GetMakeItemFitPosition: invalid item class %d", itemClass);
		return false;
	}

	pResultItem->SetItemType( itemType );

	if (g_pInventory->GetFitPosition(pResultItem, fitPoint))
	{
		bFindPos = true;

		MItem* pOldItem = g_pInventory->GetItem(static_cast<BYTE>(fitPoint.x), static_cast<BYTE>(fitPoint.y));

		// Going into an empty cell while only one is left:
		// it changes where it is.
		if (pOldItem==NULL
			&& pItem->GetNumber()==1)
		{
			fitPoint.x = pItem->GetGridX();
			fitPoint.y = pItem->GetGridY();

			bFindPos = true;
		}
	}
	else
	{
		// No empty cell, but only one left: it changes where it is.
		if (pItem->GetNumber()==1)
		{
			fitPoint.x = pItem->GetGridX();
			fitPoint.y = pItem->GetGridY();

			bFindPos = true;
		}
	}

	// free the temporary item
	delete pResultItem;

	return bFindPos;
}
