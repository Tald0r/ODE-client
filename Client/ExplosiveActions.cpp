#include "Client_PCH.h"
#include "ExplosiveActions.h"
#include "SkillDef.h"

int
GetMineActionInfo(int itemType)
{
	const int maxMine = 5;

	const int mineSkillID[maxMine] = {
		MINE_ANKLE_KILLER,
		MINE_POMZ,
		MINE_AP_C1,
		MINE_DIAMONDBACK,		
		MINE_SWIFT_EX
	};

	if (itemType>=0 && itemType<maxMine)
	{		
		return mineSkillID[itemType];
	}

	return -1;
}

int
GetBombActionInfo(int itemType)
{
	const int maxBomb = 5;

	const int bombSkillID[maxBomb] = {
		BOMB_SPLINTER,
		BOMB_ACER,
		BOMB_BULLS,
		BOMB_STUN,
		BOMB_CROSSBOW,		
	};

	if (itemType>=0 && itemType<maxBomb)
	{		
		return bombSkillID[itemType];
	}

	return -1;
}
