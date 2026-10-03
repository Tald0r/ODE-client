// MStopZoneRectEffectGenerator.cpp
#include "Client_PCH.h"
#include "MStopZoneRectEffectGenerator.h"
#include "MEffect.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
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

const MRectZoneEffectHost* MStopZoneRectEffectGenerator::s_pHost = nullptr;

const MRectZoneEffectHost* MStopZoneRectEffectGenerator::SetHost(const MRectZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneRectEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MRectZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MStopZoneRectEffectGenerator::ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count)
{
	count = 0;
	return s_pHost && s_pHost->MaxFrames && s_pHost->MaxFrames(blt, frameID, count);
}

bool MStopZoneRectEffectGenerator::ReadBounds(MRectZoneEffectBounds& bounds)
{
	bounds = {};
	return s_pHost && s_pHost->Bounds && s_pHost->Bounds(bounds);
}

bool MStopZoneRectEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect, DWORD delay)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect), delay);
}

bool
MStopZoneRectEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;
	bool bAdd;

	MRectZoneEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	TYPE_FRAMEID frameID = sprite.frameID;
	BYTE			power = egInfo.power;

	// Darkness follows the resolved frame ID, including the previous phase.
	BOOL bDarkness = FALSE, bGrayDarkness = FALSE, bSharpHail = FALSE;
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
			if (frameID>=EFFECTSPRITETYPE_DARKNESS_1_1
				&& frameID<=EFFECTSPRITETYPE_DARKNESS_1_5)
			{
				frameID = EFFECTSPRITETYPE_DARKNESS_1_1 + rand()%5;
			}
			else if (frameID >= EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 &&
				frameID <= EFFECTSPRITETYPE_GRAY_DARKNESS_1_5 )
			{
				frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 + rand()%5;
			}
		}

		if( frameID >= EFFECTSPRITETYPE_DARKNESS_1_1 &&
			frameID <= EFFECTSPRITETYPE_DARKNESS_3_5 )
			bDarkness = TRUE;
		else
			bGrayDarkness = TRUE;
		if(egInfo.nActionInfo == RESULT_MAGIC_DARKNESS_WIDE ||
			egInfo.nActionInfo == RESULT_SKILL_WIDE_GRAY_DARKNESS )
			power = 2;
	}
	else if(frameID == EFFECTSPRITETYPE_SHARP_HAIL_DROP_1)
	{
		bSharpHail = TRUE;
		power = 2;
	}

	TYPE_SECTORPOSITION	sX, sY;
	sX = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileX(egInfo.x0));
	sY = static_cast<TYPE_SECTORPOSITION>(WorldTileGeometry::PixelToTileY(egInfo.y0));

	int frameCount;
	if (!ReadMaxFrames(bltType, frameID, frameCount)) return false;
	const BYTE maxFrame = static_cast<BYTE>(frameCount);

	// Submit the center before reading zone bounds.
	auto effect = std::make_unique<MEffect>(bltType);
	MEffect* pEffect = effect.get();

	pEffect->SetFrameID( frameID, maxFrame );

	pEffect->SetPosition(sX, sY);
	pEffect->SetZ(egInfo.z0);
	pEffect->SetStepPixel(egInfo.step);
	pEffect->SetCount( egInfo.count , egInfo.linkCount );

	pEffect->SetDirection( egInfo.direction );

	pEffect->SetPower(power);

	bAdd = QueueEffect(std::move(effect), 0);

	if (bAdd)
	{
		pEffect->SetLink( egInfo.nActionInfo, egInfo.pEffectTarget );

		bOK = true;
	}

	// These actions change surrounding power only, after center submission.
	if(egInfo.nActionInfo == SKILL_WIDE_ICE_FIELD)
	{
		power = 2;
	}
	else if(egInfo.nActionInfo == SKILL_LAND_MINE_EXPLOSION)
	{
		power = 3;

	}

	MRectZoneEffectBounds bounds;
	if (!ReadBounds(bounds)) return bOK;

	int sX1 = sX-power;
	int sY1 = sY-power;
	int sX2 = sX+power;
	int sY2 = sY+power;

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

	DWORD TempDelay = 0;
	int x, y;
	for (y=sY1; y<=sY2; y++)
	{
		for (x=sX1; x<=sX2; x++)
		{
			if (x==sX && y==sY)
				continue;

			TempDelay = 0;
			// Surrounding darkness/hail tiles draw fresh variants before any delay.
			if (bDarkness)
			{
				frameID = EFFECTSPRITETYPE_DARKNESS_1_1 + rand()%5;
			}
			else if( bGrayDarkness )
			{
				frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_1_1 + rand()%5;
			}
			else if ( bSharpHail )
			{
				frameID = EFFECTSPRITETYPE_SHARP_HAIL_DROP_1 + rand()%3;
			}

			effect = std::make_unique<MEffect>(bltType);
			pEffect = effect.get();

			pEffect->SetFrameID( frameID, maxFrame );

			pEffect->SetPosition(x, y);
			pEffect->SetZ(egInfo.z0);
			pEffect->SetStepPixel(egInfo.step);
			pEffect->SetCount( egInfo.count, egInfo.linkCount );

			pEffect->SetDirection( egInfo.direction );

			pEffect->SetPower(power);

			 if(egInfo.nActionInfo == SKILL_LAND_MINE_EXPLOSION || bSharpHail )
			 {
				TempDelay = rand()%16;
				pEffect->SetWaitFrame(TempDelay);
				pEffect->SetCount( egInfo.count + TempDelay, egInfo.linkCount );

			 }
			bAdd = QueueEffect(std::move(effect), TempDelay);

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
