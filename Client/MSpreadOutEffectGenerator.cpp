// MSpreadOutEffectGenerator.cpp
#include "Client_PCH.h"
#include "MSpreadOutEffectGenerator.h"
#include "MLinearEffect.h"
#include "WorldTileGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

const MFixedZoneEffectHost* MSpreadOutEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MSpreadOutEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MSpreadOutEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MSpreadOutEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MSpreadOutEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;

	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	MEffectTarget* pTarget = egInfo.pEffectTarget;

	int sx = egInfo.x0;
	int sy = egInfo.y0;
	int sz = 0;

	int tx, ty, tz=sz;

	MLinearEffect*	pEffect;

	// Emit one trajectory for each direction, starting from source pixels at z=0.
	for (int d=0; d<8; d++)
	{
		int movePixel = egInfo.step;

		TYPE_SECTORPOSITION	sX, sY;
		sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(sx));
		sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(sy));

		TYPE_SECTORPOSITION x=sX, y=sY;
		WorldTileGeometry::Step(x, y, static_cast<BYTE>(d));

		tx = WorldTileGeometry::TileToPixelX(x);
		ty = WorldTileGeometry::TileToPixelY(y);

		// Wrapped sector neighbors can be far from the original pixel source.
		const auto cx = static_cast<std::int64_t>(sx) - tx;
		const auto cy = static_cast<std::int64_t>(sy) - ty;
		const auto currentPixel = static_cast<std::int64_t>(std::sqrt(
			static_cast<double>(cx) * cx + static_cast<double>(cy) * cy));

		if (currentPixel==0)
		{
		}
		else
		{
			movePixel = static_cast<int>((float)movePixel * (1.0f - fabs((float)cy / (float)(2.0f*currentPixel))));

			const auto movePixelStep = static_cast<std::int64_t>(movePixel) * egInfo.count;

			tx = static_cast<int>(std::clamp<std::int64_t>(sx - cx * movePixelStep / currentPixel,
				(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
			ty = static_cast<int>(std::clamp<std::int64_t>(sy - cy * movePixelStep / currentPixel,
				(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
		}

		auto effect = std::make_unique<MLinearEffect>(bltType);
		pEffect = effect.get();

		pEffect->SetFrameID( frameID, maxFrame );

		pEffect->SetPixelPosition( sx, sy, sz );
		pEffect->SetTarget( tx, ty, tz, egInfo.step );

		pEffect->SetStepPixel( egInfo.step );
		pEffect->SetCount( egInfo.count, egInfo.linkCount );

		pEffect->SetDirection( d );

		pEffect->SetPower(egInfo.power);

		// Slot zero alone owns the original target and determines the result.
		if (d==0)
		{
			bOK = QueueEffect(std::move(effect));

			if (bOK)
			{
				if (pTarget == NULL)
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
				}
				else
				{
					pEffect->SetLink( egInfo.nActionInfo, pTarget );
					pTarget->Set( tx, ty, tz, egInfo.creatureID );
				}
			}
		}
		else
		{
			if (QueueEffect(std::move(effect)))
			{
				if (pTarget==NULL)
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
				}
				else
				{
					MEffectTarget* pEffectTarget2 = new MEffectTarget( *pTarget );
					pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
					pEffectTarget2->Set( tx, ty, tz, egInfo.creatureID );
				}
			}
		}
	}

	return bOK;
}
