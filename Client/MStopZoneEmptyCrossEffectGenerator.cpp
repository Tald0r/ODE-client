//----------------------------------------------------------------------
// MStopZoneEmptyCrossEffectGenerator.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MStopZoneEmptyCrossEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include <utility>

const MFixedZoneEffectHost* MStopZoneEmptyCrossEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MStopZoneEmptyCrossEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneEmptyCrossEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneEmptyCrossEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneEmptyCrossEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK;

	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;

	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	auto effect = std::make_unique<MEffect>(bltType);
	MEffect* pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX-1, sY);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(egInfo.power);

	bOK = QueueEffect(std::move(effect));

	if (bOK)
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
	}

	MEffectTarget*	pEffectTarget2;

	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX+1, sY);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(egInfo.power);

	if (QueueEffect(std::move(effect)))
	{
		if (egInfo.pEffectTarget == NULL)
		{
			pEffect->SetLink( egInfo.nActionInfo, NULL );
		}
		else
		{
			pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
			pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
			pEffectTarget2->Set( egInfo.x0+TILE_X, egInfo.y0, egInfo.z0, egInfo.creatureID );
		}
	}

	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX, sY-1);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count , egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(egInfo.power);

	if (QueueEffect(std::move(effect)))
	{
		if (egInfo.pEffectTarget == NULL)
		{
			pEffect->SetLink( egInfo.nActionInfo, NULL );
		}
		else
		{
			pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
			pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
			pEffectTarget2->Set( egInfo.x0, egInfo.y0-TILE_Y, egInfo.z0, egInfo.creatureID );
		}
	}

	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX, sY+1);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count , egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(egInfo.power);

	if (QueueEffect(std::move(effect)))
	{
		if (egInfo.pEffectTarget == NULL)
		{
			pEffect->SetLink( egInfo.nActionInfo, NULL );
		}
		else
		{
			pEffectTarget2 = new MEffectTarget(*egInfo.pEffectTarget);
			pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
			pEffectTarget2->Set( egInfo.x0, egInfo.y0+TILE_Y, egInfo.z0, egInfo.creatureID );
		}
	}

	// Only the first arm takes the original target; the result reports that slot.
	return bOK;
}
