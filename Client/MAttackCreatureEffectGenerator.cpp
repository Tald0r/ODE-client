//----------------------------------------------------------------------
// MAttackCreatureEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttackCreatureEffectGenerator.h"
#include "MGuidanceEffect.h"
#include "SkillDef.h"
#include <utility>

const MCreatureParabolaEffectHost* MAttackCreatureEffectGenerator::s_pHost = nullptr;

const MCreatureParabolaEffectHost* MAttackCreatureEffectGenerator::SetHost(const MCreatureParabolaEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

// Consumes the effect; true only when the zone keeps it.
bool MAttackCreatureEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttackCreatureEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MCreatureParabolaEffectSprite sprite;
	if (s_pHost == NULL || s_pHost->Sprite == NULL || !s_pHost->Sprite( egInfo.effectSpriteType, sprite ))
	{
		return false;
	}
	BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
	TYPE_FRAMEID	frameID	= sprite.frameID;

	int effectCount = 0;	
	if( egInfo.nActionInfo == STEP_SKILL_KASAS_ARROW_2 || egInfo.nActionInfo == STEP_SKILL_BLAZE_BOLT_2 ||
		egInfo.nActionInfo == STEP_SKILL_EARTHS_TEETH_2 )
		effectCount = 1;
	else
	if( egInfo.nActionInfo == STEP_SKILL_KASAS_ARROW_3 || egInfo.nActionInfo == STEP_SKILL_BLAZE_BOLT_3 ||
		egInfo.nActionInfo == STEP_SKILL_EARTHS_TEETH_3 )
		effectCount = 2;

	//---------------------------------------------
	// the target position in pixels
	//---------------------------------------------
	MCreatureParabolaPosition creature;
	if (s_pHost->Creature == NULL || !s_pHost->Creature( egInfo.creatureID, creature ))
	{
		return false;
	}

	int maxFrame;
	if (s_pHost->MaxFrames == NULL || !s_pHost->MaxFrames( sprite.bltType, frameID, maxFrame ))
	{
		return false;
	}

	MGuidanceEffect* pEffect = new MGuidanceEffect(bltType);	
	
	pEffect->SetFrameID( frameID, maxFrame );		// effect 0, max 3 frames

	// the launch position in pixels
	pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 );
	pEffect->SetDirection( egInfo.direction );

	// the target creature
	pEffect->SetTraceCreatureID( egInfo.creatureID );
	pEffect->SetStepPixel( egInfo.step );

	// the frames it lasts (with a target it hardly matters - -;)
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	// power
	pEffect->SetPower(egInfo.power);

	if (QueueEffect( std::unique_ptr<MEffect>(pEffect) ))
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
		if( effectCount == 0)
			return true;
	} else
		return false;

	if( effectCount > 0)
	{
		int z=-12;
		while(effectCount)
		{
			MGuidanceEffect* pEffect = new MGuidanceEffect(bltType);
			pEffect->SetFrameID( frameID, maxFrame );
			if( egInfo.direction == 2 || egInfo.direction == 6 )
			{
				pEffect->SetPixelPosition( egInfo.x0+z, egInfo.y0, egInfo.z0 );
			} else
				pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0 - z );
			z+=24;
			pEffect->SetDirection( egInfo.direction );
			pEffect->SetTraceCreatureID( egInfo.creatureID );
			pEffect->SetStepPixel( egInfo.step );
			pEffect->SetCount( egInfo.count, egInfo.linkCount );
			pEffect->SetPower( egInfo.power );
			if( QueueEffect( std::unique_ptr<MEffect>(pEffect) ) )
			{
				if( egInfo.pEffectTarget != NULL )
				{
					MEffectTarget *pTarget = new MEffectTarget ( *egInfo.pEffectTarget );								
					pEffect->SetLink( egInfo.nActionInfo, pTarget );				
				}
			} else
				return false;
			effectCount--;
		}
		return true;
	}
	return false;
}
