//----------------------------------------------------------------------
// OrbitEffectPolicy.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "OrbitEffectPolicy.h"
#include "EffectSpriteTypeDef.h"

//----------------------------------------------------------------------
// ConsultsPreviousOrbitStep
//----------------------------------------------------------------------
bool
ConsultsPreviousOrbitStep(unsigned short spriteType, bool hasAttachedEffect)
{
	return spriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK
		|| spriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL
		|| spriteType == EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL
		|| (spriteType == EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL
			&& hasAttachedEffect);
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
