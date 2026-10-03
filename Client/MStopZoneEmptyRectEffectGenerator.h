//----------------------------------------------------------------------
// MStopZoneEmptyRectEffectGenerator.h
//----------------------------------------------------------------------
#ifndef __MSTOPZONEEMPTYRECTEFFECTGENERATOR_H__
#define __MSTOPZONEEMPTYRECTEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MEmptyRectEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

struct MEmptyRectEffectBounds
{
	TYPE_SECTORPOSITION width = 0;
	TYPE_SECTORPOSITION height = 0;
};

// Borrowed services. Bounds are read before constructing any effects. Queue
// consumes every effect and returns true only when it retains it alive.
struct MEmptyRectEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MEmptyRectEffectSprite& sprite) = nullptr;
	bool (*Bounds)(MEmptyRectEffectBounds& bounds) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneEmptyRectEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneEmptyRectEffectGenerator() {}
		~MStopZoneEmptyRectEffectGenerator() {}
		static const MEmptyRectEffectHost* SetHost(const MEmptyRectEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_EMPTY_RECT; }
		// The first accepted effect takes the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MEmptyRectEffectSprite& sprite);
		static bool ReadBounds(MEmptyRectEffectBounds& bounds);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MEmptyRectEffectHost* s_pHost;
};

#endif
