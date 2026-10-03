// MStopZoneMultipleEffectGenerator.cpp
#include "Client_PCH.h"
#include "MStopZoneMultipleEffectGenerator.h"
#include "MEffect.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include "MEventQueue.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {

int OffsetCoordinate(int coordinate, int offset)
{
	const std::int64_t value = static_cast<std::int64_t>(coordinate) + offset;
	return static_cast<int>(std::clamp<std::int64_t>(value,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

} // namespace

const MStopMultipleEffectHost* MStopZoneMultipleEffectGenerator::s_pHost = nullptr;

const MStopMultipleEffectHost* MStopZoneMultipleEffectGenerator::SetHost(const MStopMultipleEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MStopZoneMultipleEffectGenerator::ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopMultipleEffectSprite& sprite)
{
	sprite = {};
	return s_pHost && s_pHost->Sprite && s_pHost->Sprite(type, sprite);
}

void MStopZoneMultipleEffectGenerator::AddEvent(MEvent& event)
{
	if (s_pHost && s_pHost->AddEvent) s_pHost->AddEvent(event);
}

bool MStopZoneMultipleEffectGenerator::QueueEffect(std::unique_ptr<MEffect> effect)
{
	return s_pHost && s_pHost->Queue && s_pHost->Queue(std::move(effect));
}

bool
MStopZoneMultipleEffectGenerator::Generate( const EFFECTGENERATOR_INFO& egInfo )
{
	MStopMultipleEffectSprite sprite;
	if (!ReadSprite(egInfo.effectSpriteType, sprite)) return false;
	const BYTE bltType = sprite.bltType;
	const TYPE_FRAMEID frameID = sprite.frameID;
	const BYTE maxFrame = static_cast<BYTE>(sprite.maxFrames);

	bool bOK = false;

	int x, y;

	MEffectTarget*	pEffectTarget2;

	int numEffectPhase = 4;

	int randY = 50;
	int randX = TILE_X;
	const int numEffect = 4;

	if(egInfo.nActionInfo == SKILL_ACID_STORM_WIDE || egInfo.nActionInfo == SKILL_POISON_STORM_WIDE)
	{
		randY = 100;
		randX = TILE_X*2;
		numEffectPhase = 6;
	}

	MEvent event;
	event.eventID = EVENTID_METEOR_SHAKE;
	event.eventType = EVENTTYPE_ZONE;
	event.eventDelay = 2300;
	event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
	event.parameter3 = 1;
	AddEvent(event);

	int ex[numEffect];
	int ey[numEffect];

	int effectCount = 0;

	// Draw all four positions before submitting any effect in the phase.
	for (int i=0; i<numEffectPhase; i++)
	{
		int n = 0;

		ex[n] = OffsetCoordinate(egInfo.x0, -(rand()%randX) - 24);
		ey[n] = OffsetCoordinate(egInfo.y0, -(rand()%randY));

		n++;
		ex[n] = OffsetCoordinate(egInfo.x0, -(rand()%randX) - 24);
		ey[n] = OffsetCoordinate(egInfo.y0, rand()%randY);

		n++;
		ex[n] = OffsetCoordinate(egInfo.x0, rand()%randX + 24);
		ey[n] = OffsetCoordinate(egInfo.y0, -(rand()%randY));

		n++;
		ex[n] = OffsetCoordinate(egInfo.x0, rand()%randX + 24);
		ey[n] = OffsetCoordinate(egInfo.y0, rand()%randY);

		for (int j=0; j<numEffect; j++)
		{
			x = ex[j];
			y = ey[j];

			auto effect = std::make_unique<MEffect>(bltType);
			MEffect* pEffect = effect.get();
			pEffect->SetDelayFrame(effectCount);

			pEffect->SetFrameID( frameID, maxFrame );

			pEffect->SetPixelPosition( x, y, egInfo.z0 );

			pEffect->SetDirection( egInfo.direction );

			pEffect->SetCount( egInfo.count+effectCount, egInfo.linkCount );
			pEffect->SetMulti( true );

			// Stagger every attempt, even when the queue rejects it.
			effectCount += 2;

			pEffect->SetPower(egInfo.power);

			if (QueueEffect(std::move(effect)))
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
						pEffectTarget2->Set( x, y, egInfo.z0, egInfo.creatureID );
					}
				}
			}
		}
	}

	return bOK;
}
