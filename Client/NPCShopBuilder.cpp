#include "NPCShopBuilder.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MShopTemplateTable.h"
#include "MNPCTable.h"
#include "DebugLog.h"

bool BuildNPCShopShelf(MShop& shop, const NPC_INFO* npc,
	MShopTemplateTable* templates, bool mysterious, const NPCShopHost& host)
{
	const auto shelfType = mysterious ? MShopShelf::SHELF_UNKNOWN : MShopShelf::SHELF_FIXED;
	if (!npc)
	{
		shop.SetShelf(shelfType, MShopShelf::NewShelf(shelfType));
		return false;
	}

	MShopShelf* shelf = shop.GetShelf(shelfType);
	if (!shelf)
	{
		shelf = MShopShelf::NewShelf(shelfType);
		shop.SetShelf(shelfType, shelf);
	}
	else
		shelf->Release();

	bool enabled = false;
	for (const auto id : npc->ListShopTemplateID)
	{
		const auto* definition = templates->GetData(id);
		if (!definition || static_cast<MShopShelf::SHELF_TYPE>(definition->Type) != shelfType)
			continue;

		for (int type = definition->MinType; type <= definition->MaxType; ++type)
		{
			const auto itemClass = static_cast<ITEM_CLASS>(definition->Class);
			MItem* item = host.CreateItem ? host.CreateItem(itemClass) : nullptr;
			if (!item)
			{
				DEBUG_ADD_FORMAT("[Error] Shop template: invalid item class %d", itemClass);
				continue;
			}
			enabled = true;
			if (mysterious && host.IsFemale && host.IsFemale()
				&& (itemClass == ITEM_CLASS_COAT || itemClass == ITEM_CLASS_TROUSER
					|| itemClass == ITEM_CLASS_VAMPIRE_COAT))
				++type;

			item->SetItemType(type);
			item->SetGrade(4);
			if (mysterious) item->UnSetIdentified();
			item->SetCurrentDurability(item->GetMaxDurability());
			if (item->IsChargeItem()) item->SetNumber(item->GetMaxNumber());

			if (item->GetItemClass() == ITEM_CLASS_VAMPIRE_PORTAL_ITEM
				&& host.SetPortalDestination)
			{
				switch (type)
				{
				case 3: case 4: case 5:
					host.SetPortalDestination(*item, 1003, 50, 70);
					break;
				case 6: case 7: case 8:
					host.SetPortalDestination(*item, 1007, 62, 65);
					break;
				case 9: case 10: case 11:
					host.SetPortalDestination(*item, 61, 102, 220);
					break;
				}
			}
			shelf->AddItem(item);
		}
	}
	if (enabled) shelf->SetEnable();
	else shelf->SetDisable();
	return true;
}
