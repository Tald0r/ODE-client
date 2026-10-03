// MStopZoneRandomEffectGenerator.cpp
#include "Client_PCH.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <utility>

const MFixedZoneEffectHost* MStopZoneRandomEffectGenerator::s_pHost = nullptr;

const MFixedZoneEffectHost* MStopZoneRandomEffectGenerator::SetHost(const MFixedZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneRandomEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneRandomEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneRandomEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
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

	int x, y;

	// Draw each quadrant immediately before constructing and submitting it.
	std::unique_ptr<MEffect> effect;
	MEffect* pEffect;
	x = sX - (rand()%3 + 1);
	y = sY - (rand()%3 + 1);
	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(x, y);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(egInfo.power);

	// Only the first slot takes the original target and determines the result.
	bOK = QueueEffect(std::move(effect));

	if (bOK)
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
	}

	// Later accepted slots receive unchanged target copies.
	MEffectTarget* pEffectTarget2;
	x = sX + (rand()%3 + 1);
	y = sY - (rand()%3 + 1);
	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(x, y);
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
		}
	}

	x = sX + (rand()%3 + 1);
	y = sY + (rand()%3 + 1);
	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(x, y);
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
		}

	}

	x = sX - (rand()%3 + 1);
	y = sY + (rand()%3 + 1);

	effect = std::make_unique<MEffect>(bltType);
	pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(x, y);
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
		}
	}

	return bOK;
}
