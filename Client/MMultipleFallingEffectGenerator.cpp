//----------------------------------------------------------------------
// MMultipleFallingEffectGenerator.cpp
//----------------------------------------------------------------------
// Generate phases of four falling projectiles.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MMultipleFallingEffectGenerator.h"
#include "MLinearEffect.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include <cstdlib>
#include <utility>

const MMultipleFallingEffectHost* MMultipleFallingEffectGenerator::s_pHost = nullptr;

const MMultipleFallingEffectHost* MMultipleFallingEffectGenerator::SetHost(const MMultipleFallingEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MMultipleFallingEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MMultipleFallingEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MMultipleFallingEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MMultipleFallingEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MMultipleFallingEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	bool bOK = false;

	int x, y, z;
	int ez1 = egInfo.z1 - TILE_Y;
	int zt = ez1; // Destination height below the supplied endpoint.

	MEffectTarget*	pEffectTarget2;

	//---------------------------------------------
	// Select the phase count and spread.
	//---------------------------------------------
	int numEffectPhase = 4;

	int baseZ = 350;

	int randY = 50;
	int randX = TILE_X;
	const int numEffect = 4;

	if(egInfo.nActionInfo == SKILL_ACID_STORM_WIDE || egInfo.nActionInfo == SKILL_POISON_STORM_WIDE)
	{
		randY = 100;
		randX = TILE_X*2;
		numEffectPhase = 6;
	}
	else if(egInfo.nActionInfo == SKILL_ICE_HAIL)
	{
		randY = 24*2;
		randX = TILE_X*2;
		numEffectPhase = 13;
	}
	else if(egInfo.nActionInfo == SKILL_WIDE_ICE_HAIL)
	{
		randY = 24*4;
		randX = TILE_X*4;
		numEffectPhase = 25;
	}

	int ex[numEffect];
	int ey[numEffect];
	int ez[numEffect];

	int dropCount = egInfo.count;
	const int phaseUpper = 150;	// Additional height per phase.
	const int dropCountInc = phaseUpper / egInfo.step;

	// Each phase creates four projectiles.
	for (int i=0; i<numEffectPhase; i++)
	{
		int n = 0;

		ex[n] = egInfo.x0 - rand()%randX - 24;
		ey[n] = egInfo.y0 - rand()%randY;
		ez[n] = baseZ + rand()%50;

		n++;
		ex[n] = egInfo.x0 - rand()%randX - 24;
		ey[n] = egInfo.y0 + rand()%randY;
		ez[n] = baseZ + rand()%50;

		n++;
		ex[n] = egInfo.x0 + rand()%randX + 24;
		ey[n] = egInfo.y0 - rand()%randY;
		ez[n] = baseZ + rand()%50;

		n++;
		ex[n] = egInfo.x0 + rand()%randX + 24;
		ey[n] = egInfo.y0 + rand()%randY;
		ez[n] = baseZ + rand()%50;

		baseZ		+= phaseUpper;
		dropCount	+= dropCountInc;

		for (int j=0; j<numEffect; j++)
		{
			x = ex[j];
			y = ey[j];
			z = ez[j];

			auto effect = std::make_unique<MLinearEffect>(bltType);
			MLinearEffect* pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );

			// Begin at the sampled source pixel position.
			pEffect->SetPixelPosition( x, y, egInfo.z0+z );

			// Linear target selection computes the final facing.
			pEffect->SetDirection( egInfo.direction );

			// All projectiles fall to the shared destination height.
			pEffect->SetTarget( x, y, zt, egInfo.step );

			// Phase duration increases before submission.
			pEffect->SetCount( dropCount, egInfo.linkCount );

			// Preserve power.
			pEffect->SetPower(egInfo.power);

			// The queue consumes the effect even on rejection.
			if (QueueEffect(std::move(effect)))
			{
				if (!bOK)
				{
					pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
					bOK = true;
				}
				else
				{
					// Later accepted shots receive independent target copies.
					if (egInfo.pEffectTarget == NULL)
					{
						pEffect->SetLink( egInfo.nActionInfo, NULL );
					}
					else
					{
						pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
						pEffectTarget2->Set( x, y, zt, egInfo.creatureID );
					}
				}
			}
		}
	}

	return bOK;
}
