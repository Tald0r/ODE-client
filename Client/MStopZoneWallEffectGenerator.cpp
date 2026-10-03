// MStopZoneWallEffectGenerator.cpp
#include "Client_PCH.h"
#include "MStopZoneWallEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

int TargetCoordinate(std::int64_t value)
{
	return static_cast<int>(std::clamp<std::int64_t>(value,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

} // namespace

// Direction rows contain start X/Y offsets and per-tile X/Y increments.
const int g_WallDirValue[8][4] = {
	{ 0, -1, 0, 1 },
	{ -1, -1, 1, 1 },
	{ -1, 0, 1, 0 },
	{ 1, -1, -1, 1 },
	{ 0, -1, 0, 1 },
	{ -1, -1, 1, 1 },
	{ -1, 0, 1, 0 },
	{ -1, 1, 1, -1 }
};

const MStopWallEffectHost* MStopZoneWallEffectGenerator::s_pHost = nullptr;

const MStopWallEffectHost* MStopZoneWallEffectGenerator::SetHost(const MStopWallEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneWallEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopWallEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneWallEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneWallEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;

	MStopWallEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;

	const int sX1 = WorldTileGeometry::PixelToTileX(egInfo.x1);
	const int sY1 = WorldTileGeometry::PixelToTileY(egInfo.y1);

	int lookDirection = egInfo.direction;
	if (lookDirection >= 8) return false;

	// Include every tile; even counts start half a step-count before the destination.
	int stepMulti = (egInfo.step>>1);
	int sX = sX1 + g_WallDirValue[lookDirection][0] * stepMulti;
	int sY = sY1 + g_WallDirValue[lookDirection][1] * stepMulti;
	int cX = g_WallDirValue[lookDirection][2];
	int cY = g_WallDirValue[lookDirection][3];

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	std::int64_t x = egInfo.x1,
		y = egInfo.y1;

	for (int i=0; i<egInfo.step; i++)
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
			// The first accepted effect takes the original target.
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
				pEffectTarget2->Set( TargetCoordinate(x), TargetCoordinate(y), egInfo.z0, egInfo.creatureID );
			}
		}

		sX += cX;
		sY += cY;

		// Target pixels advance even after rejection and the final submission.
		x += TILE_X*cX;
		y += TILE_Y*cY;
	}

	return bOK;
}
