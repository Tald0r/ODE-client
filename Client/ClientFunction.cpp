//---------------------------------------------------------------------------
// ClientFunction.cpp
//---------------------------------------------------------------------------
// The whisper target, whether the player stands somewhere safe, and the
// inventory effects (task 4.236: gamemodel's since then; the UI's
// translucent box went into SpriteLib's AlphaBox.cpp and VS_UI_Base.cpp,
// and the blood bible's gear effect into UiRuntime).
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "ClientFunction.h"
#include "SkillDef.h"
#include "InventoryEffectTarget.h"
#include "MInventory.h"
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
