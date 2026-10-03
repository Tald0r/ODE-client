// Selectable stationary generation with borrowed sprite and zone services.
#ifndef __MSTOPZONESELECTABLEEFFECTGENERATOR_H__
#define __MSTOPZONESELECTABLEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MSelectableZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	bool repeatFrame = false;
};

// MaxFrames resolves the final frame after variant selection. Queue consumes
// every effect and returns true only when it retains the effect alive.
struct MSelectableZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& sprite) = nullptr;
	bool (*MaxFrames)(BYTE blt, TYPE_FRAMEID frameID, int& count) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect) = nullptr;
};

class MStopZoneSelectableEffectGenerator : public MEffectGenerator {
	public:
		MStopZoneSelectableEffectGenerator() {}
		~MStopZoneSelectableEffectGenerator() {}
		static const MSelectableZoneEffectHost* SetHost(const MSelectableZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_STOP_ZONE_SELECTABLE; }
		// Only an accepted effect takes ownership of the original target.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& sprite);
		static bool ReadMaxFrames(BYTE blt, TYPE_FRAMEID frameID, int& count);
		static bool QueueEffect(std::unique_ptr<MEffect> effect);
		static const MSelectableZoneEffectHost* s_pHost;
};

#endif
