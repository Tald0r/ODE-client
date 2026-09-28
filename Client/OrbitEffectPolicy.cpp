//----------------------------------------------------------------------
// OrbitEffectPolicy.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "OrbitEffectPolicy.h"
#include "EffectSpriteTypeDef.h"
#include <cstdlib>

//----------------------------------------------------------------------
// ConsultsPreviousOrbitStep
//----------------------------------------------------------------------
bool
ConsultsPreviousOrbitStep(unsigned short spriteType, bool hasAttachedEffect)
{
	return hasAttachedEffect
		&& (spriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK
			|| spriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL
			|| spriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL
			|| spriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL);
}

//----------------------------------------------------------------------
// ClearsBeforeAttach
//----------------------------------------------------------------------
bool
ClearsBeforeAttach(unsigned short spriteType)
{
	return spriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK
		|| spriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL;
}

//----------------------------------------------------------------------
// RandomIndex
//----------------------------------------------------------------------
int
RandomIndex(int randomValue, int count)
{
	// RAND_MAX/count rounds down, so dividing by it would give count for
	// the top values of rand(); scaling against RAND_MAX + 1 keeps the
	// result below count. The product needs 64 bits where RAND_MAX is
	// 2^31 - 1.
	return (int)((long long)randomValue * count / ((long long)RAND_MAX + 1));
}
