//----------------------------------------------------------------------
// MStopZoneCrossEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONECROSSEFFECTGENERATOR_H__
#define __MSTOPZONECROSSEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MCrossZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

struct MCrossZoneEffectBounds
{
	TYPE_SECTORPOSITION width = 0;
	TYPE_SECTORPOSITION height = 0;
};

// Borrowed services. Bounds are read after submitting the center. Queue
// consumes every effect and returns true only when it retains it alive.
struct MCrossZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MCrossZoneEffectSprite& sprite) = nullptr;
	bool (*Bounds)(MCrossZoneEffectBounds& bounds) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneCrossEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneCrossEffectGenerator() {}
		~MStopZoneCrossEffectGenerator() {}
		static const MCrossZoneEffectHost* SetHost(const MCrossZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_CROSS; }
		// The first accepted effect takes the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MCrossZoneEffectSprite& sprite);
		static bool ReadBounds(MCrossZoneEffectBounds& bounds);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MCrossZoneEffectHost* s_pHost;
};

#endif
