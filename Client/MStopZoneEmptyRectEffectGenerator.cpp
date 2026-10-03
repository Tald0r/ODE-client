//----------------------------------------------------------------------
// MStopZoneEmptyRectEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopZoneEmptyRectEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

int OffsetTargetCoordinate(int coordinate, int offset)
{
	const std::int64_t value = static_cast<std::int64_t>(coordinate) + offset;
	return static_cast<int>(std::clamp<std::int64_t>(value,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

} // namespace

const MEmptyRectEffectHost* MStopZoneEmptyRectEffectGenerator::s_pHost = nullptr;

const MEmptyRectEffectHost* MStopZoneEmptyRectEffectGenerator::SetHost(const MEmptyRectEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneEmptyRectEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MEmptyRectEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneEmptyRectEffectGenerator::ReadBounds(MEmptyRectEffectBounds& bounds)
{
	bounds = {};
	return s_pHost && s_pHost->Bounds && s_pHost->Bounds(bounds);
}

bool MStopZoneEmptyRectEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneEmptyRectEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;
	bool bAdd;

	MEmptyRectEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;

	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	MEmptyRectEffectBounds bounds;
	if (!ReadBounds(bounds)) return false;

	int sX1 = sX-egInfo.power;
	int sY1 = sY-egInfo.power;
	int sX2 = sX+egInfo.power;
	int sY2 = sY+egInfo.power;

	if (sX1 < 0)
	{
		sX1 = 0;
	}

	if (sX2 >= bounds.width)
	{
		sX2 = bounds.width-1;
	}

	if (sY1 < 0)
	{
		sY1 = 0;
	}

	if (sY2 >= bounds.height)
	{
		sY2 = bounds.height-1;
	}

	MEffectTarget*	pEffectTarget2;

	int x, y;
	for (y=sY1; y<=sY2; y++)
	{
		for (x=sX1; x<=sX2; x++)
		{
			// Fill the clipped rectangle except for the source tile.
			if (x==sX && y==sY)
				continue;

			auto effect = std::make_unique<MEffect>(bltType);
			MEffect* pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );

			pEffect->SetPosition(x, y);
			pEffect->SetZ(egInfo.z0);
			pEffect->SetStepPixel(egInfo.step);
			pEffect->SetCount( egInfo.count, egInfo.linkCount );

			pEffect->SetDirection( egInfo.direction );

			pEffect->SetPower(egInfo.power);

			bAdd = QueueEffect(std::move(effect));

			if (bAdd)
			{
				// The first accepted effect takes the original target.
				if (!bOK)
				{
					pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

					bOK = true;
				}
				else
				{
					if (egInfo.pEffectTarget == NULL)
					{
						pEffect->SetLink( egInfo.nActionInfo, NULL );
					}
					else
					{
						pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
						pEffectTarget2->Set( OffsetTargetCoordinate(egInfo.x1, TILE_X*(x-sX1-1)),
												OffsetTargetCoordinate(egInfo.y1, TILE_Y*(y-sY1-1)),
												egInfo.z0,
												egInfo.creatureID );
					}
				}
			}
		}
	}

	return bOK;
}
