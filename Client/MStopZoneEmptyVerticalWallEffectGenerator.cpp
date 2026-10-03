//----------------------------------------------------------------------
// MStopZoneEmptyVerticalWallEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopZoneEmptyVerticalEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include <utility>

const MEmptyWallEffectHost* MStopZoneEmptyVerticalWallEffectGenerator::s_pHost = nullptr;

const MEmptyWallEffectHost* MStopZoneEmptyVerticalWallEffectGenerator::SetHost(const MEmptyWallEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneEmptyVerticalWallEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneEmptyVerticalWallEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

// Per direction: initial tile displacement, then the tile increment.
const int g_WallVerticalDirValue[8][4] = {
	{ -1, 0, 1, 0 },
	{ -1, 1, 1, -1 },
	{ 0, -1, 0, 1 },
	{ -1, -1, 1, 1 },
	{ -1, 0, 1, 0 },
	{ 1, -1, -1, 1 },
	{ 0, -1, 0, 1 },
	{ -1, -1, 1, 1 },
};

bool
MStopZoneEmptyVerticalWallEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;

	MEmptyWallEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;

	int sX1, sY1;

	// Mines start from the source tile; other actions use the destination.
	if (egInfo.nActionInfo>=MINE_ANKLE_KILLER
		&& egInfo.nActionInfo<=MINE_COBRA)
	{
		sX1 = WorldTileGeometry::PixelToTileX(egInfo.x0);
		sY1 = WorldTileGeometry::PixelToTileY(egInfo.y0);
	}
	else
	{
		sX1 = WorldTileGeometry::PixelToTileX(egInfo.x1);
		sY1 = WorldTileGeometry::PixelToTileY(egInfo.y1);
	}

	int lookDirection = egInfo.direction;
	if (lookDirection >= 8) return false;

	int stepMulti = (egInfo.step>>1);
	int sX = sX1 + g_WallVerticalDirValue[lookDirection][0] * stepMulti;
	int sY = sY1 + g_WallVerticalDirValue[lookDirection][1] * stepMulti;
	int cX = g_WallVerticalDirValue[lookDirection][2];
	int cY = g_WallVerticalDirValue[lookDirection][3];

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	// Copied targets start at the destination pixels and advance on every iteration.
	int x = egInfo.x1,
		y = egInfo.y1;

	for (int i=0; i<egInfo.step; i++)
	{
		// Skip the middle index, including the upper middle for an even step count.
		if (i!=stepMulti)
		{
			auto effect = std::make_unique<MEffect>(bltType);
			MEffect* pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );

			pEffect->SetPosition(sX, sY);
			pEffect->SetZ(egInfo.z0);
			pEffect->SetStepPixel(egInfo.step);
			pEffect->SetCount( egInfo.count , egInfo.linkCount );

			pEffect->SetDirection( egInfo.direction );

			pEffect->SetPower(egInfo.power);

			bool bAdd = QueueEffect(std::move(effect));

			if (bAdd)
			{
				if (!bOK)
				{
					pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

					bOK = true;
				}
				else if (egInfo.pEffectTarget == NULL)
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
				}
				else
				{
					MEffectTarget* pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
					pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
					pEffectTarget2->Set( x, y, egInfo.z0, egInfo.creatureID );
				}
			}
		}

		sX += cX;
		sY += cY;

		x += TILE_X*cX;
		y += TILE_Y*cY;
	}

	return bOK;
}
