//----------------------------------------------------------------------
// MAttackCreatureHomingEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttackCreatureHomingEffectGenerator.h"
#include "MHomingEffect.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <utility>

const MCreatureParabolaEffectHost* MAttackCreatureHomingEffectGenerator::s_pHost = nullptr;

const MCreatureParabolaEffectHost* MAttackCreatureHomingEffectGenerator::SetHost(const MCreatureParabolaEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

// Consumes the effect; true only when the zone keeps it.
bool MAttackCreatureHomingEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

//----------------------------------------------------------------------
// Generate
//----------------------------------------------------------------------
bool
MAttackCreatureHomingEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MCreatureParabolaEffectSprite sprite;
	if (s_pHost == NULL || s_pHost->Sprite == NULL || !s_pHost->Sprite( egInfo.effectSpriteType, sprite ))
	{
		return false;
	}
	BLT_TYPE		bltType = static_cast<BLT_TYPE>(sprite.bltType);
	TYPE_FRAMEID	frameID	= sprite.frameID;

	// the creature's position
	int cx, cy, cz;	

	// the target position in pixels
	MCreatureParabolaPosition creature;
	if (s_pHost->Creature == NULL || !s_pHost->Creature( egInfo.creatureID, creature ))
	{
		return false;
	}

	int currentAngle, stepAngle;

	const int angles[] =
	{
		180,		// left
		225,
		270,
		315,
		0,
		45,
		90,
		135
	};

	// the starting angle
	currentAngle = angles[egInfo.direction];

	// the turning angle
	stepAngle = 13;

	int fixZPos = TILE_Y;
	int TempEndFrame = egInfo.count;
	int TempDir = egInfo.direction;
	if(egInfo.nActionInfo == SKILL_CLIENT_HALO_ATTACK)
	{
		stepAngle = 30;
		if(egInfo.direction == 2 || egInfo.direction == 6 )
		{
			stepAngle = 45;
			TempEndFrame = 8;
		}
		if(rand()%2)
			TempDir = (TempDir+2)%8;
		else
			TempDir = (TempDir+6)%8;
		currentAngle = angles[TempDir];
	}

	int maxFrame;
	if (s_pHost->MaxFrames == NULL || !s_pHost->MaxFrames( sprite.bltType, frameID, maxFrame ))
	{
		return false;
	}

	// create the homing effect
	MHomingEffect* pEffect = new MHomingEffect(bltType, currentAngle, stepAngle);	

	pEffect->SetFrameID( frameID, maxFrame );

	cx = WorldTileGeometry::TileToPixelX( creature.x );
	cy = WorldTileGeometry::TileToPixelY( creature.y );
	cz = creature.z + fixZPos;	// raised about a tile.

	// the launch position in pixels - raised about a tile.
	pEffect->SetPixelPosition( egInfo.x0, egInfo.y0, egInfo.z0+fixZPos );	

	// the direction
	pEffect->SetDirection( egInfo.direction );

	pEffect->SetTraceCreatureID( egInfo.creatureID );	

	// the target position in pixels
	pEffect->SetTarget( cx,cy,cz, egInfo.step );

	// the frames it lasts (with a target it hardly matters - -;)
	pEffect->SetCount( TempEndFrame, egInfo.linkCount );

	// power
	pEffect->SetPower(egInfo.power);

	pEffect->SetMulti(true);

	if (QueueEffect( std::unique_ptr<MEffect>(pEffect) ))
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
		return true;
	}

	return false;
}
