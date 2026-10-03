// MStopZoneSelectableEffectGenerator.cpp
#include "Client_PCH.h"
#include "MStopZoneSelectableEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include <cstdlib>
#include <utility>

const MSelectableZoneEffectHost* MStopZoneSelectableEffectGenerator::s_pHost = nullptr;

const MSelectableZoneEffectHost* MStopZoneSelectableEffectGenerator::SetHost(const MSelectableZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneSelectableEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneSelectableEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MStopZoneSelectableEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneSelectableEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MSelectableZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	const bool repeatFrame = sprite.repeatFrame;

	int direction = egInfo.direction;

	// Darkness variants follow the resolved frame ID, including a previous phase.
	if ((frameID>=EFFECTSPRITETYPE_DARKNESS_1_1
		&& frameID<=EFFECTSPRITETYPE_DARKNESS_3_5) ||
		(frameID >= EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 &&
		frameID <= EFFECTSPRITETYPE_GRAY_DARKNESS_3_5) )
	{
		if (egInfo.pPreviousEffect!=NULL)
		{
			TYPE_FRAMEID oldFrameID = (egInfo.pPreviousEffect)->GetFrameID();

			frameID = oldFrameID + 5;
		}
		else
		{
			if (frameID>=EFFECTSPRITETYPE_DARKNESS_2_1
				&& frameID<=EFFECTSPRITETYPE_DARKNESS_2_5)
			{
				frameID = EFFECTSPRITETYPE_DARKNESS_2_1 + rand()%5;
			}
			else if (frameID>=EFFECTSPRITETYPE_DARKNESS_1_1
				&& frameID<=EFFECTSPRITETYPE_DARKNESS_1_5)
			{
				frameID = EFFECTSPRITETYPE_DARKNESS_1_1 + rand()%5;
			}
			else if (frameID>=EFFECTSPRITETYPE_DARKNESS_3_1
				&& frameID<=EFFECTSPRITETYPE_DARKNESS_3_5)
			{
				frameID = EFFECTSPRITETYPE_DARKNESS_3_1 + rand()%5;
			}
			else if (frameID>=EFFECTSPRITETYPE_GRAY_DARKNESS_2_1
				&& frameID<=EFFECTSPRITETYPE_GRAY_DARKNESS_2_5)
			{
				frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 + rand()%5;
			}
			else if (frameID>=EFFECTSPRITETYPE_GRAY_DARKNESS_1_1
				&& frameID<=EFFECTSPRITETYPE_GRAY_DARKNESS_1_5)
			{
				frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 + rand()%5;
			}
			else if (frameID>=EFFECTSPRITETYPE_GRAY_DARKNESS_3_1
				&& frameID<=EFFECTSPRITETYPE_GRAY_DARKNESS_3_5)
			{
				frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_3_1 + rand()%5;
			}
		}
	}

	// Sword-wave frames rotate the supplied direction.
	if (frameID==EFFECTSPRITETYPE_SWORD_WAVE_1)
	{
		direction = (direction + 1) % 8;
	}
	else if (frameID==EFFECTSPRITETYPE_SWORD_WAVE_2)
	{
		direction = (direction + 6) % 8;
	}
	else if (frameID==EFFECTSPRITETYPE_SWORD_WAVE_3)
	{
		direction = (direction + 1) % 8;
	}

	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	int maxFrame;
	if (!ReadMaxFrames(bltType, frameID, maxFrame)) return false;

	auto effect = std::make_unique<MSelectableEffect>(bltType);
	MEffect* pEffect = effect.get();

	pEffect->SetFrameID( frameID, static_cast<BYTE>(maxFrame) );

	pEffect->SetPosition(sX, sY);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count, egInfo.linkCount );

	pEffect->SetDirection( direction );

	pEffect->SetPower(egInfo.power);

	bool bAdd = QueueEffect(std::move(effect));

	// Link first; accepted repeating effects then consume an animation-start draw.
	if (bAdd)
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

		if (repeatFrame)
		{
			if (maxFrame!=0)
			{
				int num = rand() % maxFrame;

				for (int nf=0; nf<num; nf++)
				{
					pEffect->NextFrame();
				}
			}
		}
	}

	return bAdd;

}
