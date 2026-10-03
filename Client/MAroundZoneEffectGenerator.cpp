// MAroundZoneEffectGenerator.cpp
#include "Client_PCH.h"
#include "MAroundZoneEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "MViewDef.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

const MAroundZoneEffectHost* MAroundZoneEffectGenerator::s_pHost = nullptr;

const MAroundZoneEffectHost* MAroundZoneEffectGenerator::SetHost(const MAroundZoneEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MAroundZoneEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

bool MAroundZoneEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect, DWORD delay)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect), delay);
}

int MAroundZoneEffectGenerator::OffsetCoordinate(int coordinate, int offset)
{
	return static_cast<int>(std::clamp<std::int64_t>(static_cast<std::int64_t>(coordinate) + offset,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

bool
MAroundZoneEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	bool bOK = false;

	MEffectTarget* pTarget = egInfo.pEffectTarget;

	// The chosen variant persists between attempts; positions reset each time.
	int est = egInfo.effectSpriteType;

	int num = 0;
	int dwWaitCount = 0;
	if(est == EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2)
		num = 1;
	else if(est == EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1)
		num = 5;
	else if(est == EFFECTSPRITETYPE_SPIT_STREAM)
		num = 6;
	else if(est == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW)
		num = 3;
	else
		num = rand()%2 + 2;

	for (int TempCount=0; TempCount<num; TempCount++)
	{
		dwWaitCount = 0;
		POINT pixelPoint = { egInfo.x1, egInfo.y1 };

		if (egInfo.effectSpriteType == EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2)
		{
			est = egInfo.effectSpriteType + rand()%2;

			switch(egInfo.step%4)
			{
			case 0:
				pixelPoint.x = OffsetCoordinate(pixelPoint.x, -(rand()%(TILE_X<<1)) - 24);
				pixelPoint.y = OffsetCoordinate(pixelPoint.y, -(rand()%(TILE_Y<<1)) - 24);
				break;

			case 1:
				pixelPoint.x = OffsetCoordinate(pixelPoint.x, -(rand()%(TILE_X<<1)) - 24);
				pixelPoint.y = OffsetCoordinate(pixelPoint.y, +(rand()%(TILE_Y<<1)) + 24);
				break;

			case 2:
				pixelPoint.x = OffsetCoordinate(pixelPoint.x, +(rand()%(TILE_X<<1)) + 24);
				pixelPoint.y = OffsetCoordinate(pixelPoint.y, -(rand()%(TILE_Y<<1)) - 24);
				break;

			case 3:
				pixelPoint.x = OffsetCoordinate(pixelPoint.x, +(rand()%(TILE_X<<1)) + 24);
				pixelPoint.y = OffsetCoordinate(pixelPoint.y, +(rand()%(TILE_Y<<1)) + 24);
				break;

			}

		}

		else if (est==EFFECTSPRITETYPE_GUN_DUST_1)
		{
			switch (rand()%3)
			{
				case 0 :
					est = EFFECTSPRITETYPE_GUN_DUST_1;
				break;

				case 1 :
					est = EFFECTSPRITETYPE_GUN_DUST_2;
				break;

				case 2 :
					est = EFFECTSPRITETYPE_GUN_DUST_3;
				break;
			}

			pixelPoint.x = OffsetCoordinate(pixelPoint.x, (rand()%(TILE_X<<1)) - TILE_X);
			pixelPoint.y = OffsetCoordinate(pixelPoint.y, (rand()%(TILE_Y<<1)) - TILE_Y);
		}
		else if (est==EFFECTSPRITETYPE_MOLE_SHOT_1 )
		{
			est = EFFECTSPRITETYPE_MOLE_SHOT_1+(rand()%5);
			pixelPoint.x = OffsetCoordinate(pixelPoint.x, (rand()%(TILE_X<<1)) - TILE_X);
			pixelPoint.y = OffsetCoordinate(pixelPoint.y, (rand()%(TILE_Y<<1)) - TILE_Y);
		}
		else if( est == EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1)
		{
			est = EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1+(rand()%5);
			pixelPoint.x = OffsetCoordinate(pixelPoint.x, rand()%(TILE_X<<1));
			pixelPoint.y = OffsetCoordinate(pixelPoint.y, rand()%(TILE_Y<<1));
		}
		else if(est == EFFECTSPRITETYPE_SPIT_STREAM)
		{
			int DirX = 0, DirY = 0;
			pixelPoint.x = egInfo.x0;
			pixelPoint.y = egInfo.y0;
			switch (egInfo.direction)
			{
				case DIRECTION_LEFTDOWN		: DirX = -1;	DirY = +1;	break;
				case DIRECTION_RIGHTUP		: DirX = +1;	DirY = -1;	break;
				case DIRECTION_LEFTUP		: DirX = -1;	DirY = -1;	break;
				case DIRECTION_RIGHTDOWN	: DirX = +1;	DirY = +1;	break;
				case DIRECTION_LEFT			: DirX = -1;				break;
				case DIRECTION_DOWN			:				DirY = +1;	break;
				case DIRECTION_UP			:				DirY = -1;	break;
				case DIRECTION_RIGHT		: DirX = +1;				break;
			}
			pixelPoint.x = OffsetCoordinate(pixelPoint.x, (TempCount + 1) * 24 * DirX);
			pixelPoint.y = OffsetCoordinate(pixelPoint.y, (TempCount + 1) * 24 * DirY);
			dwWaitCount = (TempCount);
		}
		else if(est == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW)
		{
			pixelPoint.x = egInfo.x0;
			pixelPoint.y = egInfo.y0;
		}

		MAroundZoneEffectSprite sprite;
		if (!ReadSprite(static_cast<TYPE_EFFECTSPRITETYPE>(est), sprite)) continue;
		const BYTE bltType = sprite.bltType;
		const TYPE_FRAMEID frameID = sprite.frameID;
		const int maxFrame = sprite.maxFrames;

		auto effect = std::make_unique<MEffect>(bltType);
		MEffect* pEffect = effect.get();

		pEffect->SetFrameID( frameID, static_cast<BYTE>(maxFrame) );

		pEffect->SetPixelPosition(pixelPoint.x, pixelPoint.y, egInfo.z0);

		pEffect->SetStepPixel(egInfo.step);

		pEffect->SetCount( maxFrame, egInfo.linkCount );

		if(est == EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW)
		{
			pEffect->SetMulti(true);
			pEffect->SetDirection((egInfo.direction + (TempCount-1) + 8)%8);
		}
		else
			pEffect->SetDirection( egInfo.direction );

		pEffect->SetPower(egInfo.power);

		if(dwWaitCount)
		{
			pEffect->SetWaitFrame(dwWaitCount);
			pEffect->SetCount(static_cast<DWORD>(dwWaitCount) + static_cast<DWORD>(maxFrame), egInfo.linkCount);
			pEffect->SetMulti(true);
		}
		// The first accepted effect takes the original; later copies use the destination.
		if (QueueEffect(std::move(effect), static_cast<DWORD>(dwWaitCount)))
		{
			if (!bOK)
			{
				pEffect->SetLink( egInfo.nActionInfo, pTarget );

				bOK = true;
			}
			else
			{
				if (pTarget==NULL)
				{
					pEffect->SetLink( egInfo.nActionInfo, NULL );
				}
				else
				{
					MEffectTarget* pEffectTarget2 = new MEffectTarget( *pTarget );
					pEffect->SetLink( egInfo.nActionInfo, pEffectTarget2 );
					pEffectTarget2->Set( egInfo.x1, egInfo.y1, egInfo.z1, egInfo.creatureID );
				}
			}
		}
	}

	return bOK;
}
