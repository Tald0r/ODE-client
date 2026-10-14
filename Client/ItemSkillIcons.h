#pragma once

class MItem;

// Some items change the player's skill icons while carried: a slayer's
// holy water, portal items, bombs, mines and bomb materials, and a
// vampire's portal items and other vampire goods. Gaining or losing one
// refreshes which skills the player can use (BonusSkills::
// RefreshAvailableSkills); any other item, or no player, changes nothing.
void CheckItemForSkillIcon(const MItem* pItem);
