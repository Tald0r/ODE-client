#ifndef DARKEDEN_NPC_SHOP_BUILDER_H
#define DARKEDEN_NPC_SHOP_BUILDER_H

#include "MItem.h"

class MShop;
class MShopTemplateTable;
class NPC_INFO;

// Borrowed services for one build. Item families with live use actions stay
// executable-owned. Missing factories produce no items, a missing gender
// query means nonfemale, and missing portal actions are skipped.
struct NPCShopHost
{
	MItem* (*CreateItem)(ITEM_CLASS itemClass) = nullptr;
	bool (*IsFemale)() = nullptr;
	void (*SetPortalDestination)(MItem& item, int zone,
		TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) = nullptr;
};

// The caller owns the initialized shop; its shelf owns the generated items.
// Returns false when NPC metadata or the requested shelf slot is missing.
// A missing template table produces an empty, disabled shelf.
bool BuildNPCShopShelf(MShop& shop, const NPC_INFO* npc,
	MShopTemplateTable* templates, bool mysterious, const NPCShopHost& host);

#endif
