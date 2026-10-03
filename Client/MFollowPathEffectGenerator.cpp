// MFollowPathEffectGenerator.cpp
#include "Client_PCH.h"
#include "MFollowPathEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"
#include "WorldTileGeometry.h"
#include <utility>
#include <vector>

typedef std::vector<POINT> FOLLOW_PATH;

// Wild Typhoon rebuilds this shared cache; other actions reuse its current paths.
FOLLOW_PATH			FollowPath[8];

void	MakePathWildTyphoon()
{
	int i;
	POINT pt;

	for(i=0;i<8;i++)
		FollowPath[i].clear();

	for(i=0;i<8;i+=2)
	{
		if( i == 2 || i == 6 )
		{
			pt.x = i == 6 ? 1 : -1;
			pt.y = 0;
		} else
		{
			pt.x = 0;
			pt.y = i == 0 ? -1 : 1;
		}
		FollowPath[i].push_back( pt );

		if( i == 2 || i == 6 )
		{
			pt.x = 0;
			pt.y = i == 6 ? -2 : 2;
		} else
		{
			pt.y =0;
			pt.x = i == 0 ? -2 : 2;
		}
		FollowPath[i].push_back( pt );

		if( i == 2 || i == 6 )
		{
			pt.x = i == 6 ? -2 : 2;
			pt.y = 0;
		} else
		{
			pt.x = 0;
			pt.y = i == 0 ? 2 : -2;
		}
		FollowPath[i].push_back( pt );

		if( i == 2 || i == 6 )
		{
			pt.x  =0;
			pt.y = i == 6 ? 2 : -2;
		} else
		{
			pt.x = i == 0 ? 2 : -2;
			pt.y = 0;
		}
		FollowPath[i].push_back( pt );

		if( i == 2 || i == 6 )
		{
			pt.x = i == 6 ? 1 : -1;
			pt.y = i == 6 ? -1 : 1;
		} else
		{
			pt.x = i == 0 ? -1 : 1;
			pt.y = i == 0 ? -1 : 1;
		}
		FollowPath[i].push_back( pt );
	}

	for(i=1;i<8;i+=2)
	{
		if( i == 1 || i == 5 ){
			pt.x = 0;
			pt.y = i == 1 ? 2 : -2;
		} else {
			pt.x = i == 3 ? 2 : -2;
			pt.y = 0;
		}
		FollowPath[i].push_back( pt );

		if(i == 1 || i == 5 ) {
			pt.x = i == 1 ? -2 : 2;
			pt.y = 0;
		} else {
			pt.x = 0;
			pt.y = i == 3 ? 2 : -2;
		}
		FollowPath[i].push_back( pt );

		if(i == 1 || i == 5 ) {
			pt.x = 0;
			pt.y = i == 1 ? -2 : 2;
		} else {
			pt.x = i == 3 ? -2 : 2;
			pt.y = 0;
		}
		FollowPath[i].push_back( pt );

		if(i == 1 || i == 5 ) {
			pt.x = i == 1 ? 1 : -1;
			pt.y = 0;
		} else {
			pt.x = 0;
			pt.y = i == 3 ? -1 : 1;
		}
		FollowPath[i].push_back( pt );

		if( i == 1 || i == 5 ) {
			pt.x = 0;
			pt.y = i == 1 ? 1 : -1;
		} else {
			pt.x = i == 3 ? 1 : -1;
			pt.y = 0;
		}
		FollowPath[i].push_back( pt );
	}
}

const MFixedZoneEffectHost* MFollowPathEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MFollowPathEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MFollowPathEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MFollowPathEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MFollowPathEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);
	int currentPhase = egInfo.pEffectTarget != NULL ?egInfo.pEffectTarget->GetCurrentPhase() : -1;
	MEffectTarget*	pTarget = egInfo.pEffectTarget;

	if( egInfo.nActionInfo == SKILL_WILD_TYPHOON )
		MakePathWildTyphoon();

	BYTE Dir = egInfo.direction;

	if( static_cast<size_t>(currentPhase-2) >= FollowPath[Dir].size() || currentPhase < 0 )
		return false;

	auto effect = std::make_unique<MLinearEffect>(bltType);
	MLinearEffect* pEffect = effect.get();

	TYPE_SECTORPOSITION tX,tY;
	tX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	tY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	int sx,sy,sz;
	sx = egInfo.x0;
	sy = egInfo.y0;
	sz = egInfo.z0;

	// Narrow the source tile before adding the signed path offset.
	int tx = WorldTileGeometry::TileToPixelX(tX+FollowPath[Dir][currentPhase-2].x);
	int ty = WorldTileGeometry::TileToPixelY(tY+FollowPath[Dir][currentPhase-2].y);
	int tz = egInfo.z1;

	pEffect->SetFrameID( frameID, maxFrame );
	pEffect->SetPixelPosition( sx, sy, sz );

	pEffect->SetTarget( tx, ty, tz, egInfo.step );
	pEffect->SetDirection( Dir );
	pEffect->SetCount( egInfo.count, egInfo.linkCount );
	pEffect->SetPower(egInfo.power);

	if (QueueEffect(std::move(effect)))
	{
		if( pTarget != NULL)
		{
			pEffect->SetLink( egInfo.nActionInfo, pTarget );
			pTarget->Set( tx, ty, tz, egInfo.creatureID );
		}
		return true;
	}

	return false;

}
