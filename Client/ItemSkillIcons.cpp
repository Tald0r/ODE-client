#include "Client_PCH.h"
#include "ItemSkillIcons.h"
#include "BonusSkillHost.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//-----------------------------------------------------------------------------
// Check Item For SkillIcon
//-----------------------------------------------------------------------------
// Each item ought to say through a virtual member whether it changes a
// skill icon; the item classes are listed here instead.
//-----------------------------------------------------------------------------
void
CheckItemForSkillIcon(const MItem* pItem)
{
	// The player's race; without a player no icon changes.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}

	ITEM_CLASS itemClass = pItem->GetItemClass();

	// Some item classes change the skill icons.
	if ((CreatureState::IsSlayer(pPlayerCreature)
		&& (itemClass==ITEM_CLASS_HOLYWATER
			|| itemClass==ITEM_CLASS_SLAYER_PORTAL_ITEM
			|| itemClass==ITEM_CLASS_BOMB
			|| itemClass==ITEM_CLASS_MINE
			|| itemClass==ITEM_CLASS_BOMB_MATERIAL))

		|| (CreatureState::IsVampire(pPlayerCreature)
		&& (itemClass==ITEM_CLASS_VAMPIRE_PORTAL_ITEM
			|| itemClass==ITEM_CLASS_VAMPIRE_ETC))
		)
	{
		BonusSkills::RefreshAvailableSkills();
	}
}
