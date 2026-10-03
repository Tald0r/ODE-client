//----------------------------------------------------------------------
// MStopZoneCrossEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopZoneCrossEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include <utility>

const MCrossZoneEffectHost* MStopZoneCrossEffectGenerator::s_pHost = nullptr;

const MCrossZoneEffectHost* MStopZoneCrossEffectGenerator::SetHost(const MCrossZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneCrossEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MCrossZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneCrossEffectGenerator::ReadBounds(MCrossZoneEffectBounds& bounds)
{
	bounds = {};
	return s_pHost && s_pHost->Bounds && s_pHost->Bounds(bounds);
}

bool MStopZoneCrossEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneCrossEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	// Success means at least one effect was accepted. The first takes the
	// caller target, so success also reports its ownership transfer.
	bool bOK = false, bAdd = false;

	MCrossZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	BYTE			power = egInfo.power;

	if(egInfo.nActionInfo == SAND_CROSS)
		power = 3;

	//---------------------------------------------
	// Convert pixel coordinates to map tiles.
	//---------------------------------------------
	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	auto effect = std::make_unique<MEffect>(bltType);
	MEffect* pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX, sY);		// Tile coordinates
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);		// Retain the step for the next effect.
	pEffect->SetCount( egInfo.count, egInfo.linkCount );			// Lifetime in frames

	// Direction
	pEffect->SetDirection( egInfo.direction );

	// Power
	pEffect->SetPower(egInfo.power);

	// Submit to the zone.
	bAdd = QueueEffect(std::move(effect));

	if (bAdd)
	{
		// Link the next effect.
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

		bOK = true;
	}

	MCrossZoneEffectBounds bounds;
	if (!ReadBounds(bounds)) return bOK;

	int sX1 = sX-power;
	int sY1 = sY-power;
	int sX2 = sX+power;
	int sY2 = sY+power;

	//------------------------------------------------------
	// Clip the arms to the zone bounds.
	//------------------------------------------------------
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

	// Generate one effect per cross tile.
	MEffectTarget*	pEffectTarget2;

	int x, y;
	for (y=sY1; y<=sY2; y++)
	{
		for (x=sX1; x<=sX2; x++)
		{
			if ((x==sX && y==sY) || !(x == sX || y == sY))
				continue;

			effect = std::make_unique<MEffect>(bltType);
			pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );

			pEffect->SetPosition(x, y);		// Tile coordinates
			pEffect->SetZ(egInfo.z0);
			pEffect->SetStepPixel(egInfo.step);		// Retain the step for the next effect.
			pEffect->SetCount( egInfo.count, egInfo.linkCount );			// Lifetime in frames

			// Direction
			pEffect->SetDirection( egInfo.direction );

			// Power
			pEffect->SetPower(power);

			// Submit to the zone.
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
					// Link the next effect.
					if (egInfo.pEffectTarget == NULL)
					{
						pEffect->SetLink( egInfo.nActionInfo, NULL );
					}
					else
					{
						pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
						pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
						pEffectTarget2->Set( egInfo.x1+TILE_X*(x-sX1-1),
												egInfo.y1+TILE_Y*(y-sY1-1),
												egInfo.z0,
												egInfo.creatureID );
					}
				}
			}

		}
	}

	return bOK;
}
