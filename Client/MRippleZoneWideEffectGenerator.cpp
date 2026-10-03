// MRippleZoneWideEffectGenerator.cpp
#include "Client_PCH.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include <utility>

const MWideRippleEffectHost* MRippleZoneWideEffectGenerator::s_pHost = nullptr;

const MWideRippleEffectHost* MRippleZoneWideEffectGenerator::SetHost(const MWideRippleEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MRippleZoneWideEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MRippleZoneWideEffectGenerator::ReadBounds(MWideRippleEffectBounds& bounds)
{
	bounds = {};
	return s_pHost && s_pHost->Bounds && s_pHost->Bounds(bounds);
}

bool MRippleZoneWideEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MRippleZoneWideEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;

	MFixedZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;

	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	int i;
	int n=(egInfo.power<<1) - 1;

	TYPE_SECTORPOSITION x=sX, y=sY;
	int cX, cY;
	WorldTileGeometry::Step(x, y, egInfo.direction);

	switch (egInfo.direction)
	{
		case DIRECTION_LEFT : case DIRECTION_RIGHT :
			y = y-(egInfo.power-1);
			cX	= 0;
			cY	= 1;
		break;

		case DIRECTION_UP : case DIRECTION_DOWN :
			x = x-(egInfo.power-1);
			cX	= 1;
			cY	= 0;
		break;

		case DIRECTION_LEFTUP : case DIRECTION_RIGHTDOWN :
			x = x-(egInfo.power-1);
			y = y+(egInfo.power-1);
			cX	= 1;
			cY	= -1;
		break;

		case DIRECTION_LEFTDOWN : case DIRECTION_RIGHTUP :
			x = x-(egInfo.power-1);
			y = y-(egInfo.power-1);
			cX	= 1;
			cY	= 1;
		break;

		default:
			return false;
	}

	for (i=0; i<n; i++)
	{
		MWideRippleEffectBounds bounds;
		if (!ReadBounds(bounds)) return bOK;
		if (x >= bounds.width || y >= bounds.height)
			continue;

		auto effect = std::make_unique<MEffect>(bltType);
		MEffect* pEffect = effect.get();

		pEffect->SetFrameID( frameID, maxFrame );

		pEffect->SetPosition(x, y);

		pEffect->SetDirection( egInfo.direction );

		pEffect->SetZ(egInfo.z0);
		pEffect->SetStepPixel(egInfo.step);
		pEffect->SetCount( egInfo.count, egInfo.linkCount );

		pEffect->SetPower(static_cast<BYTE>(egInfo.power + 1));

		bool bAdd = QueueEffect(std::move(effect));

		if (bAdd)
		{
			if (i==(egInfo.power-1))
			{
				bOK = true;

				pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );
			}
			else
			{
				pEffect->SetLink( egInfo.nActionInfo, NULL );
			}
		}

		x += cX;
		y += cY;
	}

	return bOK;
}
