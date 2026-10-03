// Stationary variants and delayed effects around source or destination pixels.
#ifndef __MAROUNDZONEEFFECTGENERATOR_H__
#define __MAROUNDZONEEFFECTGENERATOR_H__

#include "MEffectGenerator.h"
#include <memory>

struct MAroundZoneEffectSprite
{
	BYTE bltType = 0;
	TYPE_FRAMEID frameID = 0;
	int maxFrames = 0;
};

// Metadata is read after each attempt's variant and position selection.
// Queue consumes every effect and returns true only when it retains it alive.
struct MAroundZoneEffectHost
{
	bool (*Sprite)(TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& sprite) = nullptr;
	bool (*Queue)(std::unique_ptr<MEffect> effect, DWORD delay) = nullptr;
};

class MAroundZoneEffectGenerator : public MEffectGenerator {
	public:
		MAroundZoneEffectGenerator() {}
		~MAroundZoneEffectGenerator() {}
		static const MAroundZoneEffectHost* SetHost(const MAroundZoneEffectHost* host);
		TYPE_EFFECTGENERATORID GetID() { return EFFECTGENERATORID_AROUND_ZONE; }
		// The first accepted effect owns the original target; later ones own copies.
		// Missing metadata skips that attempt and preserves earlier acceptance.
		bool Generate(const EFFECTGENERATOR_INFO& egInfo);

	private:
		static bool ReadSprite(TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& sprite);
		static bool QueueEffect(std::unique_ptr<MEffect> effect, DWORD delay);
		static const MAroundZoneEffectHost* s_pHost;
};

#endif
