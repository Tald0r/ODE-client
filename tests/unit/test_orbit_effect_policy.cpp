#include "test_framework.h"
#include "Client/OrbitEffectPolicy.h"
#include "Client/EffectSpriteTypeDef.h"

static const unsigned short s_ElementalTypes[] =
{
	EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL,
	EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK,
	EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL,
	EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL,
};

TEST(OrbitEffectPolicy, ElementalsWithoutAnAttachedEffectKeepTheirRandomStep)
{
	for (unsigned short type : s_ElementalTypes)
	{
		CHECK(!ConsultsPreviousOrbitStep(type, false));
	}
}

TEST(OrbitEffectPolicy, ElementalsContinueTheAttachedOrbitStep)
{
	for (unsigned short type : s_ElementalTypes)
	{
		CHECK(ConsultsPreviousOrbitStep(type, true));
	}
}

TEST(OrbitEffectPolicy, OtherSpriteTypesNeverConsultThePreviousStep)
{
	CHECK(!ConsultsPreviousOrbitStep(EFFECTSPRITETYPE_SUMMON_HELICOPTER, true));
	CHECK(!ConsultsPreviousOrbitStep(EFFECTSPRITETYPE_SUMMON_HELICOPTER, false));
}

TEST(OrbitEffectPolicy, OnlyAttackAndHealClearTheAttachedEffects)
{
	CHECK(ClearsBeforeAttach(EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK));
	CHECK(ClearsBeforeAttach(EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL));
	CHECK(!ClearsBeforeAttach(EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL));
	CHECK(!ClearsBeforeAttach(EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL));
	CHECK(!ClearsBeforeAttach(EFFECTSPRITETYPE_SUMMON_HELICOPTER));
}
