#include "NPCShopBuilder.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MShopTemplateTable.h"
#include "MNPCTable.h"
#include "DebugLog.h"

#include <memory>

bool BuildNPCShopShelf(MShop& shop, const NPC_INFO* npc,
	MShopTemplateTable* templates, bool mysterious, const NPCShopHost& host)
{
	const auto shelfType = mysterious ? MShopShelf::SHELF_UNKNOWN : MShopShelf::SHELF_FIXED;
	if (static_cast<unsigned int>(shelfType) >= shop.GetSize()) return false;
	if (!npc)
	{
		std::unique_ptr<MShopShelf> empty(MShopShelf::NewShelf(shelfType));
		if (shop.SetShelf(shelfType, empty.get())) empty.release();
		return false;
	}

	MShopShelf* shelf = shop.GetShelf(shelfType);
	if (!shelf)
	{
		std::unique_ptr<MShopShelf> created(MShopShelf::NewShelf(shelfType));
		if (!shop.SetShelf(shelfType, created.get())) return false;
		shelf = created.release();
	}
	else
		shelf->Release();

	int stock = 0;
	for (const auto id : npc->ListShopTemplateID)
	{
		if (stock == SHOP_SHELF_SLOT) break;
		const auto* definition = templates ? templates->GetData(id) : nullptr;
		if (!definition || definition->Type != shelfType
			|| definition->Class < 0 || definition->Class >= MAX_ITEM_CLASS)
			continue;

		for (int type = definition->MinType;
			type <= definition->MaxType && stock < SHOP_SHELF_SLOT; ++type)
		{
			const auto itemClass = static_cast<ITEM_CLASS>(definition->Class);
			std::unique_ptr<MItem> item(host.CreateItem ? host.CreateItem(itemClass) : nullptr);
			if (!item)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Shop template: invalid item class %d", itemClass);
				continue;
			}
			if (mysterious && host.IsFemale && host.IsFemale()
				&& (itemClass == ITEM_CLASS_COAT || itemClass == ITEM_CLASS_TROUSER
					|| itemClass == ITEM_CLASS_VAMPIRE_COAT))
			{
				++type;
				if (type > definition->MaxType) break;
			}

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
			if (shelf->AddItem(item.get()))
			{
				item.release();
				++stock;
			}
		}
	}
	if (stock != 0) shelf->SetEnable();
	else shelf->SetDisable();
	return true;
}
