//----------------------------------------------------------------------
// MAttackZoneEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MAttackZoneEffectGenerator.h"
#include "MLinearEffect.h"
#include "WorldTileGeometry.h"
#include "DirectionSelection.h"
#include "MViewDef.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"
#include <cmath>
#include <utility>

const MZoneAttackEffectHost* MAttackZoneEffectGenerator::s_pHost = nullptr;

const MZoneAttackEffectHost* MAttackZoneEffectGenerator::SetHost(const MZoneAttackEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAttackZoneEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAttackZoneEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MAttackZoneEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	int est = egInfo.effectSpriteType;

	MZoneAttackEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	auto effect = std::make_unique<MLinearEffect>(sprite.bltType);
	MLinearEffect* pEffect = effect.get();
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	// Source pixel position.
	int sx = egInfo.x0;
	int sy = egInfo.y0;
	int sz = egInfo.z0;

	// Destination pixel position.
	int tx = egInfo.x1;
	int ty = egInfo.y1;
	int tz = egInfo.z1;

	if(egInfo.nActionInfo == SKILL_HALO)
	{
		int sTileX = WorldTileGeometry::PixelToTileX(sx);
		int sTileY = WorldTileGeometry::PixelToTileY(sy);
		int eTileX = WorldTileGeometry::PixelToTileX(tx);
		int eTileY = WorldTileGeometry::PixelToTileY(ty);

		int TempDir = SelectFacingDirection(sTileX, sTileY, eTileX, eTileY);
		switch (TempDir)
		{
			case DIRECTION_LEFTDOWN		: eTileX-=3;	eTileY+=3;	break;
			case DIRECTION_RIGHTUP		: eTileX+=3;	eTileY-=3;	break;
			case DIRECTION_LEFTUP		: eTileX-=3;	eTileY-=3;	break;
			case DIRECTION_RIGHTDOWN	: eTileX+=3;	eTileY+=3;	break;
			case DIRECTION_LEFT			: eTileX-=3;				break;
			case DIRECTION_DOWN			:			eTileY+=3;	break;
			case DIRECTION_UP			:			eTileY-=3;	break;
			case DIRECTION_RIGHT		: eTileX+=3;				break;
		}
		pEffect->SetMulti(true);

		tx = WorldTileGeometry::TileToPixelX(eTileX);
		ty = WorldTileGeometry::TileToPixelY(eTileY);
	}
	//------------------------------------------------------------
	// Wind-divider range adjustment.
	//------------------------------------------------------------
	if (est==EFFECTSPRITETYPE_WIND_DIVIDER_1
		|| est==EFFECTSPRITETYPE_WIND_DIVIDER_2
		|| est==EFFECTSPRITETYPE_WIND_DIVIDER_3)
	{
		// Travel a distance determined by speed and duration.
		int movePixel = egInfo.step * egInfo.count;

		int cx = sx-tx;
		int cy = sy-ty;

		if (cx==0 || cy==0)
		{
			//---------------------------------------------
			// Convert the source to sector coordinates.
			//---------------------------------------------
			TYPE_SECTORPOSITION	sX, sY;
			sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(sx));
			sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(sy));

			//---------------------------------------------
			// Step one sector in the supplied direction.
			//---------------------------------------------
			TYPE_SECTORPOSITION x=sX, y=sY;
			WorldTileGeometry::Step(x,y, egInfo.direction);

			//---------------------------------------------
			// Convert the selected sector back to pixels.
			//---------------------------------------------
			tx = WorldTileGeometry::TileToPixelX( x );
			ty = WorldTileGeometry::TileToPixelY( y );

			// Recompute the displacement.
			cx = sx - tx;
			cy = sy - ty;
		}

		int currentPixel = static_cast<int>(sqrt(cx*cx + cy*cy));

		if (currentPixel==0)
		{
		}
		else
		{
			tx = sx - (cx * movePixel / currentPixel);
			ty = sy - (cy * movePixel / currentPixel);

			MEffectTarget* pTarget = egInfo.pEffectTarget;
			if (pTarget!=NULL)
			{
				int tx2 = sx - (cx * movePixel);
				int ty2 = sy - (cy * movePixel);

				pTarget->Set(tx2, ty2, tz, pTarget->GetID());
			}
		}
	}

	pEffect->SetFrameID( frameID, maxFrame );

	// Configure the source position.
	pEffect->SetPixelPosition( sx, sy, sz );

	// Select the linear target.
	pEffect->SetTarget( tx, ty, tz, egInfo.step );

	// The supplied facing overrides linear target selection.
	pEffect->SetDirection( egInfo.direction );

	// Keep finite lifetime and independent link timing.
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	// Preserve power.
	pEffect->SetPower(egInfo.power);

	if (QueueEffect(std::move(effect)))
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

		return true;
	}

	return false;

}
