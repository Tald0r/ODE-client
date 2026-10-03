// MStopZoneWallEffectGenerator.h
#ifndef __MSTOPZONWALLEEFFECTGENERATOR_H__
#define __MSTOPZONWALLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MStopWallEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Borrowed services. Queue consumes every effect and returns true only when
// it retains the submitted effect alive.
struct MStopWallEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MStopWallEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneWallEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneWallEffectGenerator() {}
		~MStopZoneWallEffectGenerator() {}
		static const MStopWallEffectHost* SetHost(const MStopWallEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_WALL; }
		// The first accepted effect owns the original target; later ones get copies.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MStopWallEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MStopWallEffectHost* s_pHost;
};

#endif
